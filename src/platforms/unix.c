#include "../platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <X11/Xlib.h>

char key_get_standard_input();
extern uint32_t palette[16];
int gfx_getpixel(struct gfx* gfx, int x, int y);

NORETURN void platform_error(const char* msg) {
	printf("%s\n", msg);
	platform_exit(1);
}

NORETURN void platform_exit(int code) {
	exit(code);
}

void* platform_malloc(int size) {
	return malloc(size);
}

void platform_free(void* ptr) {
	free(ptr);
}

void platform_copy(const char* ptr, int len) {
}

int platform_paste(char* ptr, int len) {
	return 0;
}

void* platform_open(const char* filename, uint32_t* filesize) {
	return fopen(filename, "rb");
}

void* platform_create(const char* filename) {
	return fopen(filename, "rw");
}

int platform_read(void* file, char* data, int len) {
	return fread(data, 1, len, (FILE*)file);
}

int platform_write(void* file, const char* data, int len) {
	return fwrite(data, 1, len, (FILE*)file);
}

void platform_close(void* file) {
	fclose((FILE*)file);
}

char key_get_input() {
	return key_get_standard_input();
}

int main(int argc, char** argv) {
	Display* display = XOpenDisplay(NULL);
	if (display == NULL)
		platform_error("XOpenDisplay() failed.");
	int blackColor = BlackPixel(display, DefaultScreen(display));
	int whiteColor = WhitePixel(display, DefaultScreen(display));
	Window w = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 400, 400, 0, blackColor, blackColor);
	XSelectInput(display, w, StructureNotifyMask);
	XMapWindow(display, w);
	GC gc = XCreateGC(display, w, 0, NULL);
	XSetForeground(display, gc, whiteColor);
	for (;;) {
		XEvent e;
		XNextEvent(display, &e);
		switch (e.type) {
		case Expose:
			if (e.xexpose.count > 0)
				break;
			XDrawLine(display, w, gc, 10, 60, 180, 20);
			XFlush(display);
			break;
		}
	}
	return 0;
}
