#ifndef _TAB_H
#define _TAB_H

#include "cpu.h"

struct tabobj* tab_new(struct cpu* cpu);
void tab_destroy(struct cpu* cpu, struct tabobj* tab);
struct value tab_get(struct cpu* cpu, struct tabobj* tab, struct strobj* key);
int tab_in(struct cpu* cpu, struct tabobj* tab, struct strobj* key);
void tab_set(struct cpu* cpu, struct tabobj* tab, struct strobj* key, struct value value);

#endif
