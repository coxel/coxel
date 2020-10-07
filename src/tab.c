#include "alloc.h"
#include "gc.h"
#include "tab.h"

struct tabobj* tab_new(struct cpu* cpu) {
	struct tabobj* tab = (struct tabobj*)gc_alloc(cpu, t_tab, sizeof(struct tabobj));
	tab->entry_cnt = 0;
	tab->entry = writeptr_nullable(NULL);
	tab->freelist = TAB_NULL;
	tab->bucket_cnt = 0;
	tab->bucket = writeptr_nullable(NULL);
	return tab;
}

void tab_destroy(struct cpu* cpu, struct tabobj* tab) {
	mem_dealloc(&cpu->alloc, readptr_nullable(tab->entry));
	mem_dealloc(&cpu->alloc, readptr_nullable(tab->bucket));
	mem_dealloc(&cpu->alloc, tab);
}

static void tab_grow(struct cpu* cpu, struct tabobj* tab) {
	/* grow freelist */
	int new_entry_cnt = tab->entry_cnt == 0 ? 4 : tab->entry_cnt * 2;
	struct tabent* entries = (struct tabent*)mem_realloc(&cpu->alloc, readptr_nullable(tab->entry), new_entry_cnt * sizeof(struct tabent));
	tab->entry = writeptr(entries);
	tab->freelist = tab->entry_cnt;
	for (uint16_t i = tab->entry_cnt; i < new_entry_cnt; i++)
		entries[i].next = i + 1;
	entries[new_entry_cnt - 1].next = TAB_NULL;
	if (new_entry_cnt * 4 > tab->bucket_cnt * 3) {
		/* grow hash table */
		int new_bucket_cnt = tab->bucket_cnt == 0 ? 16 : tab->bucket_cnt * 2;
		uint16_t* buckets = (uint16_t*)mem_realloc(&cpu->alloc, readptr_nullable(tab->bucket), new_bucket_cnt * sizeof(uint16_t));
		tab->bucket = writeptr(buckets);
		for (uint16_t i = 0; i < new_bucket_cnt; i++)
			buckets[i] = TAB_NULL;
		for (uint16_t i = 0; i < tab->entry_cnt; i++) {
			struct tabent* ent = &entries[i];
			uint16_t bucket = ((struct strobj*)readptr(ent->key))->hash % new_bucket_cnt;
			ent->next = buckets[bucket];
			buckets[bucket] = i;
		}
		tab->bucket_cnt = new_bucket_cnt;
	}
	tab->entry_cnt = new_entry_cnt;
}

static uint16_t tab_find(struct cpu* cpu, struct tabobj* tab, struct strobj* key, uint16_t* out_prev) {
	if (!tab->bucket_cnt)
		return TAB_NULL;
	uint16_t bucket = key->hash % tab->bucket_cnt;
	uint16_t p = ((uint16_t*)readptr(tab->bucket))[bucket];
	uint16_t prev = TAB_NULL;
	while (p != TAB_NULL) {
		struct tabent* ent = &((struct tabent*)readptr(tab->entry))[p];
		if (ent->key == writeptr(key)) {
			if (out_prev)
				*out_prev = prev;
			return p;
		}
		prev = p;
		p = ent->next;
	}
	return TAB_NULL;
}

value_t tab_get(struct cpu* cpu, struct tabobj* tab, struct strobj* key) {
	uint16_t p = tab_find(cpu, tab, key, NULL);
	if (p == TAB_NULL)
		return value_undef();
	return ((struct tabent*)readptr(tab->entry))[p].value;
}

int tab_in(struct cpu* cpu, struct tabobj* tab, struct strobj* key) {
	return tab_find(cpu, tab, key, NULL) != TAB_NULL;
}

void tab_set(struct cpu* cpu, struct tabobj* tab, struct strobj* key, value_t value) {
	gc_barrier_kv(cpu, tab, key, value);
	uint16_t p = tab_find(cpu, tab, key, NULL);
	if (p != TAB_NULL) {
		((struct tabent*)readptr(tab->entry))[p].value = value;
		return;
	}
	if (tab->freelist == TAB_NULL)
		tab_grow(cpu, tab);
	uint16_t bucket = key->hash % tab->bucket_cnt;
	p = tab->freelist;
	struct tabent* ent = &((struct tabent*)readptr(tab->entry))[p];
	tab->freelist = ent->next;
	ent->key = writeptr(key);
	ent->value = value;
	uint16_t* buckets = readptr(tab->bucket);
	ent->next = buckets[bucket];
	buckets[bucket] = p;
}
