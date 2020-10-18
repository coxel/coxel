#ifndef _WIFI_H
#define _WIFI_H

#include "../cpu.h"

#define HAL_WIFI_MAX_DISCOVERED_APS		32
#define HAL_WIFI_SSID_MAXLEN			32
#define HAL_WIFI_PASSWORD_MAXLEN		63
#define HAL_WIFI_ADDR_LEN				6

struct hal_wifi_apinfo {
	char ssid[HAL_WIFI_SSID_MAXLEN + 1];
	uint8_t addr[HAL_WIFI_ADDR_LEN];
	int connectable;
};

struct hal_wifi_callbacks {
	int (*enabled)();
	void (*set_enable)(int enabled);
	void (*start_discovery)();
	void (*stop_discovery)();
	void (*connect)(const char ssid[HAL_WIFI_SSID_MAXLEN + 1], const char password[HAL_WIFI_PASSWORD_MAXLEN + 1]);
};

void hal_wifi_register(struct hal_wifi_callbacks* callbacks);
int hal_wifi_available();
void hal_wifi_resetmenu();
void hal_wifi_getmenu(struct cpu* cpu, struct arrobj* arr);
int hal_wifi_menuop(struct cpu* cpu, int sp, int nargs);
void hal_wifi_discover_ap(struct hal_wifi_apinfo* apinfo);

#endif
