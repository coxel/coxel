#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../gfx.h"
#include "../platform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/ledc.h"
#include "bootloader_random.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_hidh.h"
#include "esp_vfs_fat.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdmmc_cmd.h"

#define MCP23017_SCL	15
#define MCP23017_SDA	13
#define MCP23017_ADDR	0x20

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
	printf("Critical error: %s\n", msg);
	vTaskDelay(5000 / portTICK_RATE_MS);
	esp_restart();
}

NORETURN void platform_exit(int code) {
	printf("Called platform_exit(%d)\n", code);
	vTaskDelay(5000 / portTICK_RATE_MS);
	esp_restart();
}

void* platform_malloc(int size) {
	void *addr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
	printf("Memory allocation: %p size: 0x%x\n", addr, size);
	return addr;
}

void platform_free(void* ptr) {
	printf("Memory free: %p\n", ptr);
	heap_caps_free(ptr);
}

void* platform_malloc_fast(int size) {
	void* addr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	printf("Fast memory allocation: %p size: 0x%x\n", addr, size);
	return addr;
}

void platform_free_fast(void* ptr) {
	printf("Fast memory free: %p\n", ptr);
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

extern const uint8_t firmware_start[] asm("_binary_firmware_cox_start");
extern const uint8_t firmware_end[] asm("_binary_firmware_cox_end");

void* platform_open(const char* filename, uint32_t* filesize) {
	char buf[300] = "/sdcard/";
	if (filename == NULL) {
		*filesize = (uint32_t)(firmware_end - firmware_start);
		return fmemopen(firmware_start, firmware_end - firmware_start, "rb");
	}
	else {
		if (strlen(filename) > 256)
			return 0;
		strcpy(buf + 8, filename);
	}
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

static uint8_t gfx_screen[WIDTH * HEIGHT / 2];

static FORCEINLINE uint16_t rgb888_to_rgb565(uint32_t color) {
	uint16_t color16 = ((color & 0xF80000) >> 8) | ((color & 0xFC00) >> 5) | ((color & 0xF8) >> 3);
	return (color16 >> 8) | (color16 << 8);
}

static void paint(spi_device_handle_t spi) {
	esp_err_t err;
	err = spi_device_acquire_bus(spi, portMAX_DELAY);
	assert(err == ESP_OK);
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
		err = spi_device_polling_transmit(spi, &init_trans[i]);
		assert(err == ESP_OK);
	}
	static uint32_t line[2][PARALLEL_LINES * WIDTH / 2];
	int bufno = 0;
	for (int y = 0; y < HEIGHT; y += PARALLEL_LINES) {
		for (int lineno = 0; lineno < PARALLEL_LINES; lineno++) {
			for (int i = 0; i < WIDTH / 2; i++) {
				uint8_t color = gfx_screen[(y + lineno) * WIDTH / 2 + i];
				int low = color & 0x0F;
				int high = color >> 4;
				line[bufno][lineno * WIDTH / 2 + i] = rgb888_to_rgb565(palette[low]) + (rgb888_to_rgb565(palette[high]) << 16);
			}
		}
		if (y) {
			err = spi_device_polling_end(spi, portMAX_DELAY);
			if (err != ESP_OK) {
				printf("spi_device_polling_end() failed.\n");
				assert(err == ESP_OK);
			}
		}
		spi_transaction_t line_trans = spi_trans_data(line[bufno], PARALLEL_LINES * WIDTH * 2);
		err = spi_device_polling_start(spi, &line_trans, portMAX_DELAY);
		if (err != ESP_OK) {
			printf("spi_device_polling_start() failed.\n");
			assert(err == ESP_OK);
		}
		bufno = !bufno;
	}
	err = spi_device_polling_end(spi, portMAX_DELAY);
	if (err != ESP_OK) {
		printf("spi_device_polling_end() failed.\n");
		assert(err == ESP_OK);
	}
	spi_device_release_bus(spi);
}

static SemaphoreHandle_t console_sem;
static SemaphoreHandle_t paint_sem;

static void console_task_main(void *pvParameters) {
	printf("Console initializing...\n");
	printf("Built-in firmware size: %d\n", firmware_end - firmware_start);
	console_init();
	printf("Initialization completed. Starting main loop.\n");
	for (;;) {
		console_update();
		xSemaphoreGive(paint_sem);
		xSemaphoreTake(console_sem, portMAX_DELAY);
		vTaskDelay(1 / portTICK_PERIOD_MS);
	}
}

static char* bda2str(esp_bd_addr_t bda, char* str, size_t size) {
	if (bda == NULL || str == NULL || size < 18) {
		return NULL;
	}
	uint8_t* p = bda;
	sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
		p[0], p[1], p[2], p[3], p[4], p[5]);
	return str;
}

static int discovery_finished;
static int discovered_peer;
static esp_bd_addr_t discovered_bda;

void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
	switch (event) {
	case ESP_BT_GAP_DISC_RES_EVT: {
		char bda_str[18];
		printf("Device found: %s\n", bda2str(param->disc_res.bda, bda_str, 18));
		for (int i = 0; i < param->disc_res.num_prop; i++) {
			esp_bt_gap_dev_prop_t* prop = &param->disc_res.prop[i];
			if (prop->type == ESP_BT_GAP_DEV_PROP_BDNAME) {
				const char* name = (const char*)prop->val;
				printf("Device name: %s\n", name);
			}
			else if (prop->type == ESP_BT_GAP_DEV_PROP_COD) {
				uint32_t cod = *(uint32_t*)prop->val;
				printf("Class of device: %u\n", cod);
				if (!esp_bt_gap_is_valid_cod(cod))
					printf("Invalid COD!\n");
				else {
					uint32_t major = esp_bt_gap_get_cod_major_dev(cod);
					uint32_t minor = esp_bt_gap_get_cod_minor_dev(cod);
					uint32_t format = esp_bt_gap_get_cod_format_type(cod);
					printf("Major device class: %u\n", major);
					printf("Minor device class: %u\n", minor);
					printf("Format type ID: %u\n", format);
					if (major == ESP_BT_COD_MAJOR_DEV_PERIPHERAL) {
						printf("Peripheral device.\n");
						if (minor & 16)
							printf("Keyboard.\n");
						if (minor & 32)
							printf("Mouse.\n");
					}
					if (major == ESP_BT_COD_MAJOR_DEV_PERIPHERAL && (minor & 48)) { // keyboard / mouse
						esp_bt_gap_cancel_discovery();
						discovered_peer = 1;
						memcpy(&discovered_bda, &param->disc_res.bda, sizeof(esp_bd_addr_t));
					}
				}
			}
			else if (prop->type == ESP_BT_GAP_DEV_PROP_RSSI) {
				int rssi = *(int8_t*)prop->val;
				printf("RSSI: %d\n", rssi);
			}
			else if (prop->type == ESP_BT_GAP_DEV_PROP_EIR) {
				uint8_t len = 0;
				uint8_t* data = NULL;
				data = esp_bt_gap_resolve_eir_data((uint8_t*)prop->val, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);
				if (data == NULL)
					data = esp_bt_gap_resolve_eir_data((uint8_t*)prop->val, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &len);
				if (data != NULL) {
					printf("Name: ");
					for (int i = 0; i < len; i++)
						printf("%c", data[i]);
					printf("\n");
				}
			}
		}
		break;
	}
	case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
		if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED)
			printf("Bluetooth discovery started.\n");
		else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
			printf("Bluetooth discovery stopped.\n");
			discovery_finished = 1;
		}
		break;
	}
	default:
		printf("Event got: %d\n", event);
		break;
	}
}

void hidh_callback(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
	esp_hidh_event_t event = (esp_hidh_event_t)id;
	esp_hidh_event_data_t* param = (esp_hidh_event_data_t*)event_data;

	switch (event) {
	case ESP_HIDH_OPEN_EVENT: {
		const uint8_t* bda = esp_hidh_dev_bda_get(param->open.dev);
		printf("OPEN: %s\n", esp_hidh_dev_name_get(param->open.dev));
		esp_hidh_dev_dump(param->open.dev, stdout);
		break;
	}
	case ESP_HIDH_BATTERY_EVENT: {
		const uint8_t* bda = esp_hidh_dev_bda_get(param->battery.dev);
		printf("BATTERY: %d%%\n", param->battery.level);
		break;
	}
	case ESP_HIDH_INPUT_EVENT: {
		const uint8_t* bda = esp_hidh_dev_bda_get(param->input.dev);
		printf("INPUT: %8s, MAP: %2u, ID: %3u, Len: %d, Data:\n", esp_hid_usage_str(param->input.usage), param->input.map_index, param->input.report_id, param->input.length);
		break;
	}
	case ESP_HIDH_FEATURE_EVENT: {
		const uint8_t* bda = esp_hidh_dev_bda_get(param->feature.dev);
		printf("FEATURE: %8s, MAP: %2u, ID: %3u, Len: %d\n", esp_hid_usage_str(param->feature.usage), param->feature.map_index, param->feature.report_id, param->feature.length);
		break;
	}
	case ESP_HIDH_CLOSE_EVENT: {
		const uint8_t* bda = esp_hidh_dev_bda_get(param->close.dev);
		printf("CLOSE: '%s' %s\n", esp_hidh_dev_name_get(param->close.dev), esp_hid_disconnect_reason_str(esp_hidh_dev_transport_get(param->close.dev), param->close.reason));
		esp_hidh_dev_free(param->close.dev);
		break;
	}
	default:
		printf("EVENT: %d\n", event);
		break;
	}
	int bonded_num = esp_bt_gap_get_bond_device_num();
	printf("Bonded num: %d\n", bonded_num);
}

#define ACK_CHECK_EN	0x1

static void mcp23017_write_register(uint8_t reg, uint8_t value) {
	esp_err_t err;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	err = i2c_master_start(cmd);
	if (err != ESP_OK)
		printf("i2c_master_start() error.\n");
	err = i2c_master_write_byte(cmd, I2C_MASTER_WRITE | (MCP23017_ADDR << 1), ACK_CHECK_EN);
	if (err != ESP_OK)
		printf("i2c_master_write_byte() error.\n");
	err = i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
	if (err != ESP_OK)
		printf("i2c_master_write_byte() error.\n");
	err = i2c_master_write_byte(cmd, value, ACK_CHECK_EN);
	if (err != ESP_OK)
		printf("i2c_master_write_byte() error.\n");
	err = i2c_master_stop(cmd);
	if (err != ESP_OK)
		printf("i2c_master_stop() error.\n");
	err = i2c_master_cmd_begin(I2C_NUM_0, cmd, portMAX_DELAY);
	if (err != ESP_OK)
		printf("i2c_master_cmd_begin() error: %s.\n", esp_err_to_name(err));
	i2c_cmd_link_delete(cmd);
}

static int mcp23017_read_gpios() {
	uint8_t lo, hi;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, I2C_MASTER_WRITE | (MCP23017_ADDR << 1), 1);
	i2c_master_write_byte(cmd, 0x12, 1);
	//i2c_master_cmd_begin(I2C_NUM_0, cmd, 0);
	//i2c_cmd_link_delete(cmd);

	//cmd = i2c_cmd_link_create();
	//i2c_master_start(cmd);
	i2c_master_write_byte(cmd, I2C_MASTER_READ | (MCP23017_ADDR << 1), 1);
	i2c_master_read_byte(cmd, &lo, 1);
	i2c_master_read_byte(cmd, &hi, 1);
	i2c_master_cmd_begin(I2C_NUM_0, cmd, portMAX_DELAY);
	i2c_master_stop(cmd);
	i2c_cmd_link_delete(cmd);
	return (int)lo + ((int)hi << 8);
}

void app_main() {
	esp_err_t ret;
	/* Initialize NVS - it is used to store PHY calibration data */
	printf("Initializing NVS...\n");
	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		if (nvs_flash_erase() != 0)
			printf("nvs_flash_erase() error.\n");
		ret = nvs_flash_init();
	}
	if (ret != ESP_OK)
		printf("nvs_flash_init() error.\n");

#if 0
	
	printf("Initializing bluetooth...\n");
	/* Release BLE stack */
	ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
	if (ret != 0)
		printf("esp_bt_controller_mem_release() error.\n");
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	bt_cfg.mode = ESP_BT_MODE_CLASSIC_BT;
	bt_cfg.bt_max_acl_conn = 3;
	bt_cfg.bt_max_sync_conn = 3;
	ret = esp_bt_controller_init(&bt_cfg);
	if (ret != 0)
		printf("esp_bt_controller_init() error.\n");
	ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
	if (ret != 0)
		printf("esp_bt_controller_enable() error.\n");
	ret = esp_bluedroid_init();
	if (ret != 0)
		printf("esp_bluedroid_init() error.\n");
	ret = esp_bluedroid_enable();
	if (ret != 0)
		printf("esp_bluedroid_enable() error.\n");

	{
		const uint8_t* bda = esp_bt_dev_get_address();
		char str[18];
		bda2str(bda, str, 18);
		printf("Local bda address: %s\n", str);
	}

	printf("Retrieving bonded devices...\n");
	int bonded_num = esp_bt_gap_get_bond_device_num();
	printf("Bonded num: %d\n", bonded_num);
	esp_bd_addr_t* dev_list = (esp_bd_addr_t*)malloc(sizeof(esp_bd_addr_t) * bonded_num);
	ret = esp_bt_gap_get_bond_device_list(&bonded_num, dev_list);
	if (ret != 0)
		printf("esp_bt_gap_get_bond_device_list() failed.\n");
	else {
		for (int i = 0; i < bonded_num; i++) {
			char bda_str[18];
			bda2str(dev_list[i], bda_str, 18);
			printf("#%d: %s\n", bonded_num, bda_str);
		}
	}

	printf("Initializing gap...\n");
	esp_bt_dev_set_device_name("Coxel ESP32");

	esp_bt_cod_t cod;
	cod.major = ESP_BT_COD_MAJOR_DEV_COMPUTER;
	cod.minor = 0b000101;
	cod.service = 0;
	esp_bt_gap_set_cod(cod, ESP_BT_INIT_COD);

	/* Allow devices to connect back to us */
	esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

	/* Set default parameters for Secure Simple Pairing */
	esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
	esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_OUT;
	esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

	/*
	 * Set default parameters for Legacy Pairing
	 * Use fixed pin code
	 */
	esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
	esp_bt_pin_code_t pin_code;
	pin_code[0] = '1';
	pin_code[1] = '2';
	pin_code[2] = '3';
	pin_code[3] = '4';
	esp_bt_gap_set_pin(pin_type, 4, pin_code);
	//esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, 0);

	esp_bt_gap_register_callback(bt_app_gap_cb);

	esp_hidh_config_t config = {
		.callback = hidh_callback,
	};
	ret = esp_hidh_init(&config);
	if (ret != 0)
		printf("esp_hidh_init() error.\n");

	esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
	while (!discovery_finished)
		vTaskDelay(16 / portTICK_PERIOD_MS);
	if (discovered_peer)
		esp_hidh_dev_open(discovered_bda, ESP_HID_TRANSPORT_BT, BLE_ADDR_TYPE_PUBLIC);
	return;
#endif

#if 0
	printf("Initializing MCP23017...\n");
	i2c_config_t i2c_cfg;
	i2c_cfg.mode = I2C_MODE_MASTER;
	i2c_cfg.sda_io_num = MCP23017_SDA;
	i2c_cfg.scl_io_num = MCP23017_SCL;
	i2c_cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
	i2c_cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
	i2c_cfg.master.clk_speed = 100000;
	ret = i2c_param_config(I2C_NUM_0, &i2c_cfg);
	if (ret != ESP_OK)
		printf("i2c_param_config() error.\n");
	ret = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
	if (ret != ESP_OK)
		printf("i2c_driver_install() error.\n");
	/* Set all IO direction to input */
	mcp23017_write_register(0x00, 0xFF);
	/* Set all IO to have pull-up resistor */
	mcp23017_write_register(0x0C, 0xFF);
	for (;;) {
		printf("%d\n", mcp23017_read_gpios());
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
#endif

	printf("Initializing bootloader random number generator...\n");
	bootloader_random_enable();

	printf("Initializing button input...\n");
	{
		gpio_config_t io_conf;
		io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
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
		.clock_speed_hz = 26 * 1000 * 1000,
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

	printf("Creating synchronization semaphores...\n");
	console_sem = xSemaphoreCreateBinary();
	if (console_sem == NULL)
		printf("Creating console semaphore failed.\n");
	paint_sem = xSemaphoreCreateBinary();
	if (paint_sem == NULL)
		printf("Creating paint semaphore failed.\n");

	printf("Starting console task...\n");
	TaskHandle_t console_task;
	if (xTaskCreatePinnedToCore(console_task_main, "console", 16384, NULL, 1, &console_task, 1) != pdPASS)
		printf("xTaskCreatePinnedToCore() error.\n");
	
	printf("Starting main loop...\n");
	uint64_t wakeup_time = esp_timer_get_time();
	for (;;) {
		uint64_t start_time = esp_timer_get_time();
		xSemaphoreTake(paint_sem, portMAX_DELAY);
		memcpy(gfx_screen, console_getgfx()->screen, WIDTH * HEIGHT / 2);
		xSemaphoreGive(console_sem);
		uint64_t end_time = esp_timer_get_time();
		paint(spi);
		uint64_t end_time2 = esp_timer_get_time();
		//printf("Elapsed time: %llu %llu\n", end_time - start_time, end_time2 - end_time);
		if (pressed == BUTTON_0 && duty > 0)
			--duty;
		else if (pressed == BUTTON_1 && duty < 7)
			++duty;
		else if (pressed == BUTTON_2) {
			uint64_t time = esp_timer_get_time();
			if (time - wakeup_time > 1000000) { /* 1s */
				printf("pressed.\n");
				ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
				ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
				gpio_wakeup_enable(BUTTON_2, GPIO_INTR_LOW_LEVEL);
				esp_sleep_enable_gpio_wakeup();
				esp_light_sleep_start();
				wakeup_time = esp_timer_get_time();
			}
		}
		pressed = 0;
		ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, duty);
		ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
		vTaskDelay(1 / portTICK_PERIOD_MS);
	}
}
