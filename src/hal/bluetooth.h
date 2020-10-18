#ifndef _BLUETOOTH_H
#define _BLUETOOTH_H

#include "../cpu.h"

#define HAL_BT_MAX_DISCOVERED_PEERS		32
#define HAL_BT_ADDR_LEN		6
#define HAL_BT_MAXNAME		31
struct hal_bt_addr {
	uint8_t addr[HAL_BT_ADDR_LEN];
};

struct hal_bt_peerinfo {
	struct hal_bt_addr addr;
	char name[HAL_BT_MAXNAME + 1];
	uint32_t cod;
	int connectable;
	int rssi;
};

struct hal_bt_callbacks {
	void (*start_discovery)();
	void (*stop_discovery)();
	void (*connect)(struct hal_bt_addr* addr, uint32_t cod);
	int (*get_bonded_devices)(struct hal_bt_peerinfo* devices, int bufsize);
	void (*remove_bonded_device)(struct hal_bt_addr* addr);
};

void hal_bt_register(struct hal_bt_callbacks* callbacks);
int hal_bt_available();
void hal_bt_resetmenu();
void hal_bt_getmenu(struct cpu* cpu, struct arrobj* arr);
int hal_bt_menuop(struct cpu* cpu, int sp, int nargs);
void hal_bt_addr_to_str(const struct hal_bt_addr* addr, char* str);
int hal_bt_get_cod_major(uint32_t cod);
int hal_bt_get_cod_minor(uint32_t cod);
int hal_bt_get_cod_format(uint32_t cod);
int hal_bt_has_keyboard(uint32_t cod);
int hal_bt_has_mouse(uint32_t cod);
void hal_bt_discover_peer(const struct hal_bt_peerinfo* peer);

#endif
