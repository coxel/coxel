#ifndef _MENU_H
#define _MENU_H

#include "cpu.h"

void menuitem_action(struct cpu* cpu, struct arrobj* arr, int id, const char* text);
void menuitem_text(struct cpu* cpu, struct arrobj* arr, const char* text);
void menuitem_input(struct cpu* cpu, struct arrobj* arr, int id, const char* text, const char* prompt, const char* input);
void menuitem_password(struct cpu* cpu, struct arrobj* arr, int id, const char* text, const char* prompt, const char* password);
struct arrobj* menu_getmenu(struct cpu* cpu);
void menu_menuop(struct cpu* cpu, int sp, int nargs);

#endif
