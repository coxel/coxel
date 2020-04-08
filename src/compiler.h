#ifndef _COMPILER_H
#define _COMPILER_H

#include "cpu.h"

struct compile_err {
	int linenum;
	const char* msg;
};
struct compile_err compile(struct cpu* cpu, const char* code, int codelen);

#endif
