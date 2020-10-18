#include "arr.h"
#include "menu.h"
#include "str.h"
#include "tab.h"

#include "hal/bluetooth.h"
#include "hal/wifi.h"

#include <string.h>

struct menuprovider {
	const char* name;
	int (*available)();
	void (*resetmenu)();
	void (*getmenu)(struct cpu* cpu, struct arrobj* arr);
	int (*menuop)(struct cpu* cpu, int sp, int nargs);
};

static struct menuprovider g_menu_providers[] = {
	{ "Bluetooth", hal_bt_available, hal_bt_resetmenu, hal_bt_getmenu, hal_bt_menuop },
	{ "Wifi", hal_wifi_available, hal_wifi_resetmenu, hal_wifi_getmenu, hal_wifi_menuop },
	{ NULL, NULL, NULL, NULL },
};

static enum {
	menu_close,
	menu_top,
	menu_submenu,
} g_menustate;
int g_menuprovider_id;

void menuitem_action(struct cpu* cpu, struct arrobj* arr, int id, const char* text) {
	struct tabobj* tab = tab_new(cpu);
	tab_set(cpu, tab, str_intern(cpu, "id", 2), value_num(num_kint(id)));
	tab_set(cpu, tab, str_intern(cpu, "text", 4), value_str(str_intern(cpu, text, strlen(text))));
	arr_push(cpu, arr, value_tab(tab));
}

void menuitem_text(struct cpu* cpu, struct arrobj* arr, const char* text) {
	struct tabobj* tab = tab_new(cpu);
	tab_set(cpu, tab, str_intern(cpu, "text", 4), value_str(str_intern(cpu, text, strlen(text))));
	arr_push(cpu, arr, value_tab(tab));
}

void menuitem_input(struct cpu* cpu, struct arrobj* arr, int id, const char* text, const char* prompt, const char* input) {
	struct tabobj* tab = tab_new(cpu);
	tab_set(cpu, tab, str_intern(cpu, "id", 2), value_num(num_kint(id)));
	tab_set(cpu, tab, str_intern(cpu, "text", 4), value_str(str_intern(cpu, text, strlen(text))));
	tab_set(cpu, tab, str_intern(cpu, "prompt", 6), value_str(str_intern(cpu, prompt, strlen(prompt))));
	tab_set(cpu, tab, str_intern(cpu, "input", 5), value_str(str_intern(cpu, input, strlen(input))));
	arr_push(cpu, arr, value_tab(tab));
}

void menuitem_password(struct cpu* cpu, struct arrobj* arr, int id, const char* text, const char* prompt, const char* password) {
	struct tabobj* tab = tab_new(cpu);
	tab_set(cpu, tab, str_intern(cpu, "id", 2), value_num(num_kint(id)));
	tab_set(cpu, tab, str_intern(cpu, "text", 4), value_str(str_intern(cpu, text, strlen(text))));
	tab_set(cpu, tab, str_intern(cpu, "prompt", 6), value_str(str_intern(cpu, prompt, strlen(prompt))));
	tab_set(cpu, tab, str_intern(cpu, "password", 8), value_str(str_intern(cpu, password, strlen(password))));
	arr_push(cpu, arr, value_tab(tab));
}

struct arrobj* menu_getmenu(struct cpu* cpu) {
	struct arrobj* arr = arr_new(cpu);
	switch (g_menustate) {
	case menu_close: break;
	case menu_top: {
		for (int i = 0;; i++) {
			struct menuprovider* provider = &g_menu_providers[i];
			if (!provider->name)
				break;
			if (!provider->available())
				continue;
			menuitem_action(cpu, arr, i, provider->name);
		}
		menuitem_action(cpu, arr, -1, "Back");
		break;
	}
	case menu_submenu: {
		struct menuprovider* provider = &g_menu_providers[g_menuprovider_id];
		provider->getmenu(cpu, arr);
		break;
	}
	}
	return arr;
}

void menu_menuop(struct cpu* cpu, int sp, int nargs) {
	if (nargs == 0) {
		g_menustate = menu_top;
		return;
	}
	switch (g_menustate) {
	case menu_close: break;
	case menu_top: {
		if (nargs != 1)
			argument_error(cpu);
		value_t id_value = ARG(0);
		if (!value_is_num(id_value))
			return;
		int id = num_int(value_get_num(id_value));
		if (id == -1)
			g_menustate = menu_close;
		else {
			g_menustate = menu_submenu;
			g_menuprovider_id = id;
		}
		break;
	}
	case menu_submenu: {
		struct menuprovider* provider = &g_menu_providers[g_menuprovider_id];
		if (!provider->menuop(cpu, sp, nargs)) {
			provider->resetmenu();
			g_menustate = menu_top;
		}
		break;
	}
	}
}
