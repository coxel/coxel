#include "wifi.h"
#include "../menu.h"
#include "../platform.h"

#include <string.h>

static struct hal_wifi_callbacks g_wifi_callbacks;
static enum {
	wifi_menu_top,
} g_wifi_menustate;
static int g_wifi_scan_started;
static struct hal_wifi_apinfo* g_discovered_aps;
static int g_discovered_ap_count;

void hal_wifi_register(struct hal_wifi_callbacks* callbacks) {
	g_wifi_callbacks = *callbacks;
	g_discovered_aps = (struct hal_wifi_apinfo*)platform_malloc_fast(sizeof(struct hal_wifi_apinfo) * HAL_WIFI_MAX_DISCOVERED_APS);
	hal_wifi_resetmenu();
}

int hal_wifi_available() {
	return g_wifi_callbacks.enabled != NULL;
}

void hal_wifi_resetmenu() {
	g_wifi_menustate = wifi_menu_top;
	g_wifi_scan_started = 0;
	g_discovered_ap_count = 0;
	g_wifi_callbacks.stop_discovery();
}

void hal_wifi_getmenu(struct cpu* cpu, struct arrobj* arr) {
	switch (g_wifi_menustate) {
	case wifi_menu_top:
		menuitem_action(cpu, arr, -1, "Back");
		for (int i = 0; i < g_discovered_ap_count; i++) {
			struct hal_wifi_apinfo* apinfo = &g_discovered_aps[i];
			if (apinfo->connectable)
				menuitem_password(cpu, arr, i, apinfo->ssid, "Password:", "");
			else
				menuitem_text(cpu, arr, apinfo->ssid);
		}
		if (!g_wifi_scan_started) {
			g_wifi_callbacks.start_discovery();
			g_wifi_scan_started = 1;
		}
		break;
	}
}

int hal_wifi_menuop(struct cpu* cpu, int sp, int nargs) {
	if (nargs < 1)
		return 1;
	value_t id_value = ARG(0);
	if (!value_is_num(id_value))
		return 1;
	int id = num_int(value_get_num(id_value));
	switch (g_wifi_menustate) {
	case wifi_menu_top:
		if (id == -1)
			return 0;
		if (nargs < 2)
			return 1;
		value_t text_value = ARG(1);
		if (value_get_type(text_value) != t_str)
			return 1;
		struct strobj* text = (struct strobj*)value_get_object(text_value);
		char password[HAL_WIFI_PASSWORD_MAXLEN + 1] = { 0 };
		int len = HAL_WIFI_PASSWORD_MAXLEN;
		if (text->len < len)
			len = text->len;
		memcpy(password, text->data, len);
		struct hal_wifi_apinfo* ap = &g_discovered_aps[id];
		g_wifi_callbacks.connect(ap->ssid, password);
		break;
	}
	return 1;
}

void hal_wifi_discover_ap(struct hal_wifi_apinfo* ap) {
	if (g_wifi_scan_started && g_discovered_ap_count + 1 < HAL_WIFI_MAX_DISCOVERED_APS) {
		g_discovered_aps[g_discovered_ap_count++] = *ap;
	}
}
