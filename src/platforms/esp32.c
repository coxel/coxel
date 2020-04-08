#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../platform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/ledc.h"
#include "bootloader_random.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "sdmmc_cmd.h"

char key_get_standard_input();
extern uint32_t palette[16];
int gfx_getpixel(struct gfx* gfx, int x, int y);

#define TFT_MISO	12
#define TFT_MOSI	23
#define TFT_SCLK	18
#define TFT_CS		27
#define TFT_DC		32
#define TFT_RST		5
#define TFT_BL		4

#define SD_MISO		2
#define SD_MOSI		15
#define SD_SCLK		14
#define SD_CS		13
#ifdef CONFIG_IDF_TARGET_ESP32S2
#define SDSPI_DMA_CHAN    host.slot
#else
#define SDSPI_DMA_CHAN    1
#endif

#define BUTTON_0	38
#define BUTTON_1	37
#define BUTTON_2	39
#define PARALLEL_LINES 16

static uint32_t pressed = 0;
static void IRAM_ATTR gpio_isr_handler(void* arg) {
	uint32_t gpio_num = (uint32_t)arg;
	pressed = gpio_num;
}

/* This function is called (in irq context!) just before a transmission starts. */
/* It will set the D/C line to the value indicated in the user field. */
void lcd_spi_pre_transfer_callback(spi_transaction_t* t) {
	int dc = (int)t->user;
	gpio_set_level(TFT_DC, dc);
}

void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd) {
	esp_err_t ret;
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));       //Zero out the transaction
	t.length = 8;                     //Command is 8 bits
	t.tx_buffer = &cmd;               //The data is the cmd itself
	t.user = (void*)0;                //D/C needs to be set to 0
	ret = spi_device_polling_transmit(spi, &t);  //Transmit!
	assert(ret == ESP_OK);            //Should have had no issues.
}

void lcd_data(spi_device_handle_t spi, const uint8_t* data, int len)
{
	esp_err_t ret;
	spi_transaction_t t;
	if (len == 0) return;             //no need to send anything
	memset(&t, 0, sizeof(t));       //Zero out the transaction
	t.length = len * 8;                 //Len is in bytes, transaction length is in bits.
	t.tx_buffer = data;               //Data
	t.user = (void*)1;                //D/C needs to be set to 1
	ret = spi_device_polling_transmit(spi, &t);  //Transmit!
	assert(ret == ESP_OK);            //Should have had no issues.
}

typedef struct {
	uint8_t cmd;
	uint8_t data[16];
	uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

DRAM_ATTR static const lcd_init_cmd_t lcd_init_cmds[] = {
	/* Power contorl B, power control = 0, DC_ENA = 1 */
	{0xCF, {0x00, 0x83, 0X30}, 3},
	/* Power on sequence control,
	 * cp1 keeps 1 frame, 1st frame enable
	 * vcl = 0, ddvdh=3, vgh=1, vgl=2
	 * DDVDH_ENH=1
	 */
	{0xED, {0x64, 0x03, 0x12, 0x81}, 4},
	/* Driver timing control A,
	 * non-overlap=default +1
	 * EQ=default - 1, CR=default
	 * pre-charge=default - 1
	 */
	{0xE8, {0x85, 0x01, 0x79}, 3},
	/* Power control A, Vcore=1.6V, DDVDH=5.6V */
	{0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
	/* Pump ratio control, DDVDH=2xVCl */
	{0xF7, {0x20}, 1},
	/* Driver timing control, all=0 unit */
	{0xEA, {0x00, 0x00}, 2},
	/* Power control 1, GVDD=4.75V */
	{0xC0, {0x26}, 1},
	/* Power control 2, DDVDH=VCl*2, VGH=VCl*7, VGL=-VCl*3 */
	{0xC1, {0x11}, 1},
	/* VCOM control 1, VCOMH=4.025V, VCOML=-0.950V */
	{0xC5, {0x35, 0x3E}, 2},
	/* VCOM control 2, VCOMH=VMH-2, VCOML=VML-2 */
	{0xC7, {0xBE}, 1},
	/* Memory access contorl, MX=MY=0, MV=1, ML=0, BGR=1, MH=0 */
	{0x36, {0x28}, 1},
	/* Pixel format, 16bits/pixel for RGB/MCU interface */
	{0x3A, {0x55}, 1},
	/* Frame rate control, f=fosc, 120Hz fps */
	{0xB1, {0x00, 0x10}, 2},
	/* Enable 3G, disabled */
	{0xF2, {0x08}, 1},
	/* Gamma set, curve 1 */
	{0x26, {0x01}, 1},
	/* Positive gamma correction */
	{0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0x87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
	/* Negative gamma correction */
	{0xE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
	/* Column address set, SC=0, EC=0xEF */
	{0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
	/* Page address set, SP=0, EP=0x013F */
	{0x2B, {0x00, 0x00, 0x01, 0x3f}, 4},
	/* Memory write */
	{0x2C, {0}, 0},
	/* Entry mode set, Low vol detect disabled, normal display */
	{0xB7, {0x07}, 1},
	/* Display function control */
	{0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
	/* Sleep out */
	{0x11, {0}, 0x80},
	/* Display on */
	{0x29, {0}, 0x80},
	{0, {0}, 0xFF},
};

NORETURN void platform_error(const char* msg) {
	printf("%s\n", msg);
	vTaskDelay(5000 / portTICK_RATE_MS);
	esp_restart();
}

NORETURN void platform_exit(int code) {
	printf("Called platform_exit(%d)\n", code);
	vTaskDelay(5000 / portTICK_RATE_MS);
	esp_restart();
}

void* platform_malloc(int size) {
	return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

void platform_free(void* ptr) {
	heap_caps_free(ptr);
}

void platform_copy(const char* ptr, int len) {
}

int platform_paste(char* ptr, int len) {
	return 0;
}

uint32_t platform_seed() {
	return esp_random();
}

void* platform_open(const char* filename, uint32_t* filesize) {
	char buf[300] = "/sdcard/";
	if (strlen(filename) > 256)
		return 0;
	strcpy(buf + 8, filename);
	struct stat st;
	if (stat(buf, &st))
		return 0;
	*filesize = st.st_size;
	return fopen(buf, "rb");
}

void* platform_create(const char* filename) {
	char buf[300] = "/sdcard/";
	if (strlen(filename) > 256)
		return 0;
	strcpy(buf + 8, filename);
	return fopen(buf, "rw");
}

int platform_read(void* file, char* data, int len) {
	return fread(data, 1, len, (FILE*)file);
}

int platform_write(void* file, const char* data, int len) {
	return fwrite(data, 1, len, (FILE*)file);
}

void platform_close(void* file) {
	fclose((FILE*)file);
}

char key_get_input() {
	return key_get_standard_input();
}

static struct spi_transaction_t spi_trans_cmd(uint8_t cmd) {
	struct spi_transaction_t trans;
	memset(&trans, 0, sizeof(trans));
	trans.flags = SPI_TRANS_USE_TXDATA;
	trans.user = (void*)0;
	trans.length = 8;
	trans.tx_data[0] = cmd;
	return trans;
}

static struct spi_transaction_t spi_trans_data_2(uint16_t param1, uint16_t param2) {
	struct spi_transaction_t trans;
	memset(&trans, 0, sizeof(trans));
	trans.flags = SPI_TRANS_USE_TXDATA;
	trans.user = (void*)1;
	trans.length = 32;
	trans.tx_data[0] = param1 >> 8;
	trans.tx_data[1] = param1 & 0xFF;
	trans.tx_data[2] = param2 >> 8;
	trans.tx_data[3] = param2 & 0xFF;
	return trans;
}

static struct spi_transaction_t spi_trans_data(const void* buf, int len) {
	struct spi_transaction_t trans;
	memset(&trans, 0, sizeof(trans));
	trans.user = (void*)1;
	trans.length = len * 8;
	trans.tx_buffer = buf;
	return trans;
}

static void paint(spi_device_handle_t spi) {
	esp_err_t err;
	spi_transaction_t init_trans[] = {
		/* Column address set */
		spi_trans_cmd(0x2A),
		spi_trans_data_2(80, 239),
		/* Page address set */
		spi_trans_cmd(0x2B),
		spi_trans_data_2(48, 191),
		/* Memory write */
		spi_trans_cmd(0x2C),
	};
	/* Prepare for writing */
	for (int i = 0; i < sizeof(init_trans) / sizeof(spi_transaction_t); i++) {
		err = spi_device_queue_trans(spi, &init_trans[i], portMAX_DELAY);
		assert(err == ESP_OK);
	}
	int init_finished = 0;
	spi_transaction_t line_trans;
	uint16_t line[160];
	for (int y = 0; y < 144; y++) {
		/* Wait for last transaction to finish */
		if (y) {
			/* Wait for finish */
			if (!init_finished) {
				for (int i = 0; i < sizeof(init_trans) / sizeof(spi_transaction_t); i++) {
					spi_transaction_t* wt = &init_trans[i];
					err = spi_device_get_trans_result(spi, &wt, portMAX_DELAY);
					assert(err == ESP_OK);
				}
				init_finished = 1;
			}
			spi_transaction_t* wt = &line_trans;
			err = spi_device_get_trans_result(spi, &wt, portMAX_DELAY);
			assert(err == ESP_OK);
		}
		for (int x = 0; x < 160; x++) {
			uint32_t color = palette[gfx_getpixel(console_getgfx(), x, y)];
			/* RGB888 to RGB565 */
			uint16_t color16 = ((color & 0xF80000) >> 8) | ((color & 0xFC00) >> 5) | ((color & 0xF8) >> 3);
			line[x] = (color16 >> 8) | (color16 << 8);
		}
		line_trans = spi_trans_data(line, 160 * 2);
		err = spi_device_queue_trans(spi, &line_trans, portMAX_DELAY);
		assert(err == ESP_OK);
	}
	/* Wait for last transaction to finish */
	spi_transaction_t* wt = &line_trans;
	err = spi_device_get_trans_result(spi, &wt, portMAX_DELAY);
	assert(err == ESP_OK);
}

void app_main() {
	printf("Initializing bootloader random number generator...\n");
	bootloader_random_enable();

	printf("Initializing button input...\n");
	{
		gpio_config_t io_conf;
		io_conf.intr_type = GPIO_PIN_INTR_NEGEDGE;
		io_conf.mode = GPIO_MODE_INPUT;
		io_conf.pin_bit_mask = (1LL << BUTTON_0) | (1LL << BUTTON_1) | (1LL << BUTTON_2);
		io_conf.pull_up_en = 1;
		io_conf.pull_down_en = 0;
		gpio_config(&io_conf);
	}

	gpio_install_isr_service(0);
	gpio_isr_handler_add(BUTTON_0, gpio_isr_handler, (void*)BUTTON_0);
	gpio_isr_handler_add(BUTTON_1, gpio_isr_handler, (void*)BUTTON_1);
	gpio_isr_handler_add(BUTTON_2, gpio_isr_handler, (void*)BUTTON_2);

	printf("Initializing SPI bus...\n");
	esp_err_t ret;
	spi_device_handle_t spi;
	spi_bus_config_t buscfg = {
		.miso_io_num = TFT_MISO,
		.mosi_io_num = TFT_MOSI,
		.sclk_io_num = TFT_SCLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 240 * 320 * 2 + 8,
	};
	spi_device_interface_config_t devcfg = {
		.clock_speed_hz = 10 * 1000 * 1000,
		.mode = 0,
		.spics_io_num = TFT_CS,
		.queue_size = 7,
		.pre_cb = lcd_spi_pre_transfer_callback,
	};
	ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
	if (ret != 0)
		printf("spi_bus_initialize() error.\n");
	ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
	if (ret != 0)
		printf("spi_bus_add_device() error.\n");

	printf("Initializing display...\n");
	gpio_set_direction(TFT_DC, GPIO_MODE_OUTPUT);
	gpio_set_direction(TFT_RST, GPIO_MODE_OUTPUT);

	/* Reset the display */
	gpio_set_level(TFT_RST, 0);
	vTaskDelay(100 / portTICK_RATE_MS);
	gpio_set_level(TFT_RST, 1);
	vTaskDelay(100 / portTICK_RATE_MS);

	int cmd = 0;
	while (lcd_init_cmds[cmd].databytes != 0xFF) {
		lcd_cmd(spi, lcd_init_cmds[cmd].cmd);
		lcd_data(spi, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes & 0x1F);
		if (lcd_init_cmds[cmd].databytes & 0x80) {
			vTaskDelay(100 / portTICK_RATE_MS);
		}
		cmd++;
	}

	printf("Configuring backlight PWM...\n");
	int duty = 3;
	ledc_timer_config_t ledc_timer = {
		.duty_resolution = 3,
		.freq_hz = 10000000, /* 10 MHz */
		.speed_mode = LEDC_TIMER_0,
		.timer_num = LEDC_HIGH_SPEED_MODE,
		.clk_cfg = LEDC_AUTO_CLK,
	};
	ledc_channel_config_t ledc_channel = {
		.channel = LEDC_CHANNEL_0,
		.duty = duty,
		.gpio_num = TFT_BL,
		.speed_mode = LEDC_HIGH_SPEED_MODE,
		.hpoint = 0,
		.timer_sel = LEDC_TIMER_0,
	};
	ledc_timer_config(&ledc_timer);
	ledc_channel_config(&ledc_channel);

	printf("Configuring SDMMC...\n");
	sdmmc_card_t* card;
	sdmmc_host_t host = SDSPI_HOST_DEFAULT();
	host.slot = VSPI_HOST;
	sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
	slot_config.gpio_miso = SD_MISO;
	slot_config.gpio_mosi = SD_MOSI;
	slot_config.gpio_sck = SD_SCLK;
	slot_config.gpio_cs = SD_CS;
	slot_config.dma_channel = 2;
	esp_vfs_fat_sdmmc_mount_config_t mount_config = {
		.format_if_mount_failed = false,
		.max_files = 5,
		.allocation_unit_size = 16 * 1024,
	};
	ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
	if (ret != ESP_OK)
		printf("esp_vfs_fat_sdmmc_mount() error.\n");

	// Card has been initialized, print its properties
	sdmmc_card_print_info(stdout, card);
	
	printf("Console initializing...\n");
	console_init();
	printf("Initialization completed. Starting main loop.\n");
	for (;;) {
		console_update();
		paint(spi);
		if (pressed == BUTTON_0 && duty > 0)
			--duty;
		else if (pressed == BUTTON_2 && duty < 7)
			++duty;
		pressed = 0;
		ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, duty);
		ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
		vTaskDelay(1 / portTICK_PERIOD_MS);
	}
}
