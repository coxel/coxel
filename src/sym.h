#ifndef _SYM_H
#define _SYM_H

#include <stdint.h>

struct sym {
	uint8_t reg;
	uint8_t upval_used;
	uint8_t len;
	char key[];
};

struct sym_level {
	uint16_t size;
};

struct sym_table {
	int level;
	uint8_t data[8192];
	uint8_t* top;
	uint8_t* last_top;
};

void sym_init(struct sym_table* sym_table);
void sym_push(struct sym_table* sym_table);
void sym_pop(struct sym_table* sym_table);
int sym_level_needclose(struct sym_table* sym_table);
struct sym* sym_find(struct sym_table* sym_table, int min_level, const char* key, uint8_t len, int* out_level);
int sym_emplace(struct sym_table* sym_table, int min_level, const char* key, uint8_t len, struct sym** out_sym);

#endif
