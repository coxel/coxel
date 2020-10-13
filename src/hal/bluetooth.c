#include "bluetooth.h"
#include "../arith.h"
#include "../menu.h"
#include "../platform.h"

#include <stdlib.h>
#include <string.h>

#define MAX_DISCOVERED_PEERS		32

static struct hal_bt_callbacks g_bt_callbacks;
static enum {
	bt_menu_top,
	bt_menu_discover,
} g_bt_menustate;
static struct hal_bt_peerinfo* g_discovered_peers;
static int g_discovered_peer_count;

void hal_bt_register(struct hal_bt_callbacks* callbacks) {
	memcpy(&g_bt_callbacks, callbacks, sizeof(struct hal_bt_callbacks));
	hal_bt_resetmenu();
	g_discovered_peers = (struct hal_bt_peerinfo*)platform_malloc_fast(sizeof(struct hal_bt_peerinfo) * MAX_DISCOVERED_PEERS);
}

int hal_bt_available() {
	return g_bt_callbacks.start_discovery != NULL;
}

void hal_bt_resetmenu() {
	g_bt_menustate = bt_menu_top;
}

void hal_bt_getmenu(struct cpu* cpu, struct arrobj* arr) {
	switch (g_bt_menustate) {
	case bt_menu_top: {
		menuitem_action(cpu, arr, -2, "Add new device");
		menuitem_action(cpu, arr, -1, "Back");
		menuitem_text(cpu, arr, "Bonded devices");
		// TODO: Sorting
		int bonded_count = g_bt_callbacks.get_bonded_devices(g_discovered_peers, MAX_DISCOVERED_PEERS);
		for (int i = 0; i < bonded_count; i++) {
			struct hal_bt_peerinfo* peer = &g_discovered_peers[i];
			if (peer->name[0])
				menuitem_action(cpu, arr, i, peer->name);
			else {
				char str[3 * HAL_BT_ADDR_LEN];
				hal_bt_addr_to_str(&peer->addr, str);
				menuitem_action(cpu, arr, i, str);
			}
		}
		break;
	}
	case bt_menu_discover: {
		menuitem_action(cpu, arr, -1, "Back");
		// TODO: Sorting
		for (int i = 0; i < g_discovered_peer_count; i++) {
			struct hal_bt_peerinfo* peer = &g_discovered_peers[i];
			if (peer->name[0])
				menuitem_action(cpu, arr, i, peer->name);
			else {
				char str[3 * HAL_BT_ADDR_LEN];
				hal_bt_addr_to_str(&peer->addr, str);
				menuitem_action(cpu, arr, i, str);
			}
		}
		break;
	}
	}
}

int hal_bt_menuop(struct cpu* cpu, int sp, int nargs) {
	value_t id_value = ARG(0);
	if (!value_is_num(id_value))
		return 1;
	int id = num_int(value_get_num(id_value));
	switch (g_bt_menustate) {
	case bt_menu_top: {
		if (id == -1)
			return 0;
		else if (id == -2) {
			g_bt_callbacks.start_discovery();
			g_discovered_peer_count = 0;
			g_bt_menustate = bt_menu_discover;
		}
	}
	case bt_menu_discover: {
		if (id == -1) {
			g_bt_callbacks.stop_discovery();
			g_bt_menustate = bt_menu_top;
		}
		else if (id >= 0 && id < g_discovered_peer_count) {
			struct hal_bt_peerinfo* peer = &g_discovered_peers[id];
			if (peer->connectable) {
				g_bt_callbacks.stop_discovery();
				g_bt_callbacks.connect(&peer->addr, peer->cod);
			}
		}
		break;
	}
	}
	return 1;
}

void hal_bt_addr_to_str(const struct hal_bt_addr* addr, char* str) {
	for (int i = 0; i < HAL_BT_ADDR_LEN; i++) {
		uint8_t byte = addr->addr[i];
		str[3 * i + 0] = to_hex(byte >> 4);
		str[3 * i + 1] = to_hex(byte & 0xF);
		str[3 * i + 2] = ':';
	}
	str[3 * HAL_BT_ADDR_LEN - 1] = 0;
}

int hal_bt_get_cod_major(uint32_t cod) {
	return (cod >> 8) & 0x1F;
}

int hal_bt_get_cod_minor(uint32_t cod) {
	return (cod >> 2) & 0x3F;
}

int hal_bt_get_cod_format(uint32_t cod) {
	return cod & 3;
}

int hal_bt_has_keyboard(uint32_t cod) {
	return hal_bt_get_cod_format(cod) == 0
		&& hal_bt_get_cod_major(cod) == 5
		&& (hal_bt_get_cod_minor(cod) & 0x10);
}

int hal_bt_has_mouse(uint32_t cod) {
	return hal_bt_get_cod_format(cod) == 0
		&& hal_bt_get_cod_major(cod) == 5
		&& (hal_bt_get_cod_minor(cod) & 0x20);
}

void hal_bt_discover_peer(const struct hal_bt_peerinfo* peer) {
	for (int i = 0; i < g_discovered_peer_count; i++) {
		struct hal_bt_peerinfo* saved_peer = &g_discovered_peers[i];
		if (!memcmp(saved_peer->addr.addr, &peer->addr.addr, HAL_BT_ADDR_LEN)) {
			if (peer->name[0])
				memcpy(saved_peer->name, peer->name, sizeof(peer->name));
			if (peer->cod) {
				saved_peer->cod = peer->cod;
				saved_peer->connectable = peer->connectable;
			}
			if (peer->rssi)
				saved_peer->rssi = peer->rssi;
			return;
		}
	}
	if (g_discovered_peer_count + 1 < MAX_DISCOVERED_PEERS) {
		g_discovered_peers[g_discovered_peer_count] = *peer;
		g_discovered_peer_count++;
	}
}
