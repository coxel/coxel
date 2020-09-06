#include "cfunc.h"

#define X(name) extern value_t name(struct cpu* cpu, int sp, int nargs);
CFUNC_DEFS(X)
#undef X

static cfunc g_cfuncs[] = {
#define X(name) name,
	CFUNC_DEFS(X)
#undef X
};

cfunc cfunc_get(enum cfuncname name) {
	return g_cfuncs[name];
}
