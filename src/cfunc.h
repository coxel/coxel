#ifndef _CFUNC_H
#define _CFUNC_H

struct cpu;
typedef struct value(*cfunc)(struct cpu* cpu, int sp, int nargs);

#define CFUNC_DEFS(X) \
	X(lib_btn) \
	X(lib_btnp) \
	X(lib_cls) \
	X(lib_camera) \
	X(lib_pal) \
	X(lib_palt) \
	X(lib_pget) \
	X(lib_pset) \
	X(lib_line) \
	X(lib_rect) \
	X(lib_fillRect) \
	X(lib_spr) \
	X(lib_sspr) \
	X(lib_print) \
	X(lib_abs) \
	X(lib_max) \
	X(lib_min) \
	X(lib_ceil) \
	X(lib_floor) \
	X(lib_srand) \
	X(lib_rand) \
	X(lib_statCpu) \
	X(lib_statMem) \
	X(devlib_key) \
	X(devlib_keyp) \
	X(devlib_mpos) \
	X(devlib_mwheel) \
	X(devlib_input) \
	X(devlib_copy) \
	X(devlib_paste) \
	X(devlib_newbuf) \
	X(devlib_fastParse) \
	X(devlib_run) \
	X(devlib_save) \
	X(devlib_load) \
	X(libarr_pop) \
	X(libarr_push) \
	X(libarr_slice) \
	X(libstr_indexOf) \
	X(libstr_lastIndexOf) \
	X(libstr_substr)

enum cfuncname {
#define X(name) cf_##name,
	CFUNC_DEFS(X)
#undef X
};

cfunc cfunc_get(enum cfuncname name);

#endif
