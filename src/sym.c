#include "str.h"
#include "sym.h"

#include <string.h>

void sym_init(struct sym_table* sym_table) {
	sym_table->level = 0;
	sym_table->top = sym_table->data + sizeof(sym_table->data);
	sym_table->last_top = sym_table->top;
}

void sym_push(struct sym_table* sym_table) {
	sym_table->top -= sizeof(struct sym_level);
	struct sym_level* sym_level = (struct sym_level*)sym_table->top;
	sym_level->size = sym_table->last_top - sym_table->top;
	sym_table->last_top = sym_table->top;
	++sym_table->level;
}

void sym_pop(struct sym_table* sym_table) {
	struct sym_level* sym_level = (struct sym_level*)sym_table->last_top;
	sym_table->top = sym_table->last_top + sizeof(struct sym_level);
	sym_table->last_top += sym_level->size;
	--sym_table->level;
}

int sym_level_needclose(struct sym_table* sym_table) {
	for (uint8_t* top = sym_table->top; top < sym_table->last_top;) {
		struct sym* sym = (struct sym*)top;
		if (sym->upval_used)
			return 1;
		top += sizeof(struct sym) + sym->len;
	}
	return 0;
}

struct sym* sym_find(struct sym_table* sym_table, int min_level, const char* key, uint8_t len, int* out_level) {
	uint8_t* top = sym_table->top;
	uint8_t* last_top = sym_table->last_top;
	for (int level = sym_table->level; level >= min_level; level--) {
		/* search in this level */
		while (top < last_top) {
			struct sym* sym = (struct sym*)top;
			if (str_equal(key, len, sym->key, sym->len)) {
				*out_level = level;
				return sym;
			}
			top += sizeof(struct sym) + sym->len;
		}
		/* go up one level */
		struct sym_level* sym_level = (struct sym_level*)last_top;
		top += sizeof(struct sym_level);
		last_top += sym_level->size;
	}
	return NULL;
}

int sym_emplace(struct sym_table* sym_table, int min_level, const char* key, uint8_t len, struct sym** out_sym) {
	int level;
	struct sym* sym = sym_find(sym_table, min_level, key, len, &level);
	if (sym) {
		*out_sym = sym;
		return 0;
	}
	/* not found, create new one */
	const uint16_t size = sizeof(struct sym) + len;
	// TODO: Detect out of memory
	sym_table->top -= size;
	sym = (struct sym*)sym_table->top;
	sym->upval_used = 0;
	sym->len = len;
	memcpy(sym->key, key, len);
	*out_sym = sym;
	return 1;
}
