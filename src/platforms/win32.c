#include "../gfx.h"
#include "../key.h"
#include "../platform.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>
#include <NTSecAPI.h>
#include <ShlObj.h>

#pragma comment(linker,"/entry:main")
#pragma comment(lib,"imm32")
#ifdef _DEBUG
#pragma comment(lib,"libucrtd")
#pragma comment(lib,"libvcruntimed")
#else
#pragma comment(lib,"libvcruntime")
#endif

static enum key keymap[256] = {
	[VK_UP] = kc_up,
	[VK_DOWN] = kc_down,
	[VK_LEFT] = kc_left,
	[VK_RIGHT] = kc_right,
	['A'] = kc_a,
	['B'] = kc_b,
	['C'] = kc_c,
	['D'] = kc_d,
	['E'] = kc_e,
	['F'] = kc_f,
	['G'] = kc_g,
	['H'] = kc_h,
	['I'] = kc_i,
	['J'] = kc_j,
	['K'] = kc_k,
	['L'] = kc_l,
	['M'] = kc_m,
	['N'] = kc_n,
	['O'] = kc_o,
	['P'] = kc_p,
	['Q'] = kc_q,
	['R'] = kc_r,
	['S'] = kc_s,
	['T'] = kc_t,
	['U'] = kc_u,
	['V'] = kc_v,
	['W'] = kc_w,
	['X'] = kc_x,
	['Y'] = kc_y,
	['Z'] = kc_z,
	['1'] = kc_1,
	['2'] = kc_2,
	['3'] = kc_3,
	['4'] = kc_4,
	['5'] = kc_5,
	['6'] = kc_6,
	['7'] = kc_7,
	['8'] = kc_8,
	['9'] = kc_9,
	['0'] = kc_0,
	[VK_ESCAPE] = kc_esc,
	[VK_RETURN] = kc_return,
	[VK_OEM_3] = kc_backtick,
	[VK_OEM_MINUS] = kc_dash,
	[VK_OEM_PLUS] = kc_equal,
	[VK_TAB] = kc_tab,
	[VK_BACK] = kc_backspace,
	[VK_SPACE] = kc_space,
	[VK_OEM_4] = kc_lbracket,
	[VK_OEM_6] = kc_rbracket,
	[VK_OEM_2] = kc_lslash,
	[VK_OEM_5] = kc_rslash,
	[VK_OEM_1] = kc_semicolon,
	[VK_OEM_7] = kc_quote,
	[VK_OEM_COMMA] = kc_comma,
	[VK_OEM_PERIOD] = kc_period,
	[VK_F1] = kc_f1,
	[VK_F2] = kc_f2,
	[VK_F3] = kc_f3,
	[VK_F4] = kc_f4,
	[VK_F5] = kc_f5,
	[VK_F6] = kc_f6,
	[VK_F7] = kc_f7,
	[VK_F8] = kc_f8,
	[VK_F9] = kc_f9,
	[VK_F10] = kc_f10,
	[VK_F11] = kc_f11,
	[VK_F12] = kc_f12,
	[VK_INSERT] = kc_insert,
	[VK_DELETE] = kc_delete,
	[VK_HOME] = kc_home,
	[VK_END] = kc_end,
	[VK_PRIOR] = kc_pgup,
	[VK_NEXT] = kc_pgdn,
	[VK_CONTROL] = kc_ctrl,
	[VK_MENU] = kc_alt,
	[VK_SHIFT] = kc_shift,
};

static HWND g_hwnd;
static HDC memdc;

NORETURN void platform_error(const char *msg) {
	MessageBoxA(g_hwnd, msg, "Critical error", MB_OK | MB_ICONERROR);
	platform_exit(1);
}

NORETURN void platform_exit(int code) {
	ExitProcess(code);
}

void* platform_malloc(int size) {
	return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void platform_free(void* ptr) {
	VirtualFree(ptr, 0, MEM_RELEASE);
}

void platform_copy(const char* ptr, int len) {
	if (!OpenClipboard(g_hwnd))
		return;
	if (!EmptyClipboard())
		return;
	HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
	if (mem == NULL) {
		CloseClipboard();
		return;
	}
	char* buf = GlobalLock(mem);
	memcpy(buf, ptr, len);
	buf[len] = 0;
	GlobalUnlock(mem);

	SetClipboardData(CF_TEXT, mem);

	CloseClipboard();
}

int platform_paste(char* ptr, int len) {
	if (!IsClipboardFormatAvailable(CF_TEXT))
		return 0;
	if (!OpenClipboard(g_hwnd))
		return 0;
	int r = 0;
	HGLOBAL mem = GetClipboardData(CF_TEXT);
	if (mem != NULL) {
		char* buf = (char*)GlobalLock(mem);
		if (buf != NULL) {
			size_t size = GlobalSize(mem);
			if (size > 0 && buf[size] == 0)
				--size;
			if (size > len)
				r = -1;
			else {
				r = (int)size;
				memcpy(ptr, buf, r);
			}
			GlobalUnlock(mem);
		}
	}
	CloseClipboard();
	return r;
}

uint32_t platform_seed() {
	uint32_t ret;
	RtlGenRandom(&ret, sizeof(ret));
	return ret;
}

void* platform_open(const char* filename, uint32_t *filesize) {
	HANDLE handle;
	if (filename == NULL) {
		/* Set current dir to exe path/data */
		WCHAR* path = platform_malloc(65536 + 64);
		DWORD size = GetModuleFileNameW(NULL, path, 32768);
		if (size == 0 || GetLastError() != ERROR_SUCCESS)
			platform_error("GetModuleFileNameW() failed.");
		/* Remove executable name */
		while (path[size] != '/' && path[size] != '\\')
			size--;
		if (path[size] != '/')
			path[size++] = '/';
		path[size++] = L'f';
		path[size++] = L'i';
		path[size++] = L'r';
		path[size++] = L'm';
		path[size++] = L'w';
		path[size++] = L'a';
		path[size++] = L'r';
		path[size++] = L'e';
		path[size++] = L'.';
		path[size++] = L'c';
		path[size++] = L'o';
		path[size++] = L'x';
		path[size] = 0;
		handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	else
		handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
		return NULL;
	DWORD size_high;
	DWORD size = GetFileSize(handle, &size_high);
	if (size_high) {
		CloseHandle(handle);
		return NULL;
	}
	*filesize = size;
	return (void*)handle;
}

void* platform_create(const char* filename) {
	HANDLE handle = CreateFileA(filename, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
		return NULL;
	return (void*)handle;
}

int platform_read(void* file, char* data, int len) {
	DWORD bytes_read;
	if (!ReadFile((HANDLE)file, data, len, &bytes_read, NULL))
		return 0;
	return bytes_read;
}

int platform_write(void* file, const char* data, int len) {
	DWORD bytes_written;
	if (!WriteFile((HANDLE)file, data, len, &bytes_written, NULL))
		return 0;
	return bytes_written;
}

void platform_close(void* file) {
	CloseHandle((HANDLE)file);
}

static void fill(HDC dc, int left, int top, int right, int bottom, HBRUSH brush) {
	RECT rect;
	rect.left = left;
	rect.top = top;
	rect.right = right;
	rect.bottom = bottom;
	FillRect(dc, &rect, brush);
}

static void paint(HWND hwnd, HDC dc) {
	RECT rect;
	GetClientRect(g_hwnd, &rect);
	int win_width = rect.right - rect.left;
	int win_height = rect.bottom - rect.top;
	int scale = min(win_width / WIDTH, win_height / HEIGHT);
	int width = WIDTH * scale, height = HEIGHT * scale;
	/* fill background */
	int x1 = (win_width - width) / 2, x2 = x1 + width;
	int y1 = (win_height - height) / 2, y2 = y1 + height;
	HBRUSH brush = (HBRUSH)GetStockObject(BLACK_BRUSH);
	fill(dc, 0, 0, win_width, y1, brush);
	fill(dc, 0, y2, win_width, win_height, brush);
	fill(dc, 0, y1, x1, y2, brush);
	fill(dc, x2, y1, win_width, y2, brush);
	StretchBlt(dc, x1, y1, width, height, memdc, 0, HEIGHT - 1, WIDTH, -HEIGHT, SRCCOPY);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	enum captured_button {
		cb_none,
		cb_left,
		cb_right,
		cb_middle,
	};
	static enum captured_button captured_button = cb_none;
	switch (msg) {
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(hwnd, &ps);
		paint(hwnd, dc);
		EndPaint(hwnd, &ps);
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_SIZING:
		InvalidateRect(hwnd, NULL, FALSE);
		PostMessage(hwnd, WM_PAINT, 0, 0);
		break;
	case WM_KEYDOWN:
		key_press(keymap[wparam]);
		key_input(key_get_standard_input(keymap[wparam]));
		break;
	case WM_KEYUP:
		key_release(keymap[wparam]);
		break;
	case WM_LBUTTONDOWN:
		if (captured_button == cb_none) {
			captured_button = cb_left;
			SetCapture(g_hwnd);
		}
		key_press(kc_mleft);
		break;
	case WM_LBUTTONUP:
		if (captured_button == cb_left) {
			captured_button = cb_none;
			ReleaseCapture();
		}
		key_release(kc_mleft);
		break;
	case WM_RBUTTONDOWN:
		if (captured_button == cb_none) {
			captured_button = cb_right;
			SetCapture(g_hwnd);
		}
		key_press(kc_mright);
		break;
	case WM_RBUTTONUP:
		if (captured_button == cb_right) {
			captured_button = cb_none;
			ReleaseCapture();
		}
		key_release(kc_mright);
		break;
	case WM_MBUTTONDOWN:
		if (captured_button == cb_none) {
			captured_button = cb_middle;
			SetCapture(g_hwnd);
		}
		key_press(kc_mmiddle);
		break;
	case WM_MBUTTONUP:
		if (captured_button == cb_middle) {
			captured_button = cb_none;
			ReleaseCapture();
		}
		key_release(kc_mmiddle);
		break;
	case WM_MOUSEMOVE: {
		int x = GET_X_LPARAM(lparam);
		int y = GET_Y_LPARAM(lparam);
		struct io* io = console_getio();

		RECT rect;
		GetClientRect(g_hwnd, &rect);
		int win_width = rect.right - rect.left;
		int win_height = rect.bottom - rect.top;
		int scale = min(win_width / WIDTH, win_height / HEIGHT);
		int width = WIDTH * scale, height = HEIGHT * scale;
		int x1 = (win_width - width) / 2, x2 = x1 + width;
		int y1 = (win_height - height) / 2, y2 = y1 + height;

		io->mousex = num_kint((x - x1) / scale);
		io->mousey = num_kint((y - y1) / scale);
		break;
	}
	case WM_MOUSEWHEEL: {
		int delta = GET_WHEEL_DELTA_WPARAM(wparam);
		console_getio()->mousewheel -= delta / WHEEL_DELTA;
		break;
	}
	}
	return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static void modifier_update() {
	key_setstate(kc_ctrl, GetKeyState(VK_CONTROL) & 0x8000);
	key_setstate(kc_shift, GetKeyState(VK_SHIFT) & 0x8000);
	key_setstate(kc_alt, GetKeyState(VK_MENU) & 0x8000);
}

void main() {
	/* Set current dir to My Documents/Coxel */
	WCHAR* path = platform_malloc(65536);
	SHGetFolderPathW(NULL, CSIDL_MYDOCUMENTS | CSIDL_FLAG_CREATE, NULL, 0, path);
	if (!SetCurrentDirectoryW(path))
		platform_error("SetCurrentDirectoryW() failed.");
	CreateDirectoryW(L"Coxel", NULL);
	if (!SetCurrentDirectoryW(L"Coxel"))
		platform_error("SetCurrentDirectoryW() failed.");

	g_hwnd = NULL;
#ifdef RELATIVE_ADDRESSING
	uint32_t size;
	void* f = platform_open(STATE_PATH, &size);
	if (f) {
		console_deserialize_init(f);
		platform_close(f);
		DeleteFileA(STATE_PATH);
	}
	else
#endif
		console_init();
	ImmDisableIME(-1);

	HINSTANCE instance = GetModuleHandleW(NULL);
	WNDCLASSW wc = { 0 };
	wc.style = CS_OWNDC;
	wc.lpszClassName = L"Window";
	wc.hInstance = instance;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpfnWndProc = window_proc;
	wc.hCursor = LoadCursor(0, IDC_ARROW);

	ATOM atom = RegisterClassW(&wc);
	if (atom == 0)
		platform_error("Register window class failed.");
	g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"Coxel", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 500, 500, NULL, NULL, instance, NULL);
	if (g_hwnd == NULL)
		platform_error("Create window failed.");
	MSG msg;
	HDC dc = GetDC(g_hwnd);
	BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biWidth = WIDTH;
	bmi.bmiHeader.biHeight = HEIGHT;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biClrUsed = 0;
	bmi.bmiHeader.biPlanes = 1;
	char* bits;
	memdc = CreateCompatibleDC(dc);
	if (memdc == NULL)
		platform_error("CreateCompatibleDC() failed.");
	HBITMAP bitmap = CreateDIBSection(memdc, &bmi, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
	if (bitmap == NULL || bits == NULL)
		platform_error("CreateDIBSection() failed.");
	HANDLE buf_bitmap = NULL;

	HBITMAP oldbitmap = (HBITMAP)SelectObject(memdc, bitmap);

	LARGE_INTEGER start, freq;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);
	double last_time = 0;
	for (;;) {
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				goto end;
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		modifier_update();
		btn_standard_update();
		console_update();
		for (int y = 0; y < HEIGHT; y++) {
			for (int x = 0; x < WIDTH; x++) {
				int c = palette[gfx_getpixel(console_getgfx(), x, y)];
				int i = (y * WIDTH + x) * 3;
				bits[i + 0] = c & 0xFF;
				bits[i + 1] = (c & 0xFF00) >> 8;
				bits[i + 2] = c >> 16;
			}
		}
		paint(g_hwnd, dc);
		LARGE_INTEGER current;
		QueryPerformanceCounter(&current);
		last_time += 1.0 / 60;
		double cur_time = (double)(current.QuadPart - start.QuadPart) / freq.QuadPart;
		if (cur_time < last_time)
			Sleep((DWORD)((last_time - cur_time) * 1000));
		else if (cur_time > last_time + 0.1) /* too far behind, give up */
			last_time = cur_time;
	}

end:
#ifdef RELATIVE_ADDRESSING
	f = platform_create(STATE_PATH);
	if (f) {
		console_serialize(f);
		platform_close(f);
	}
#endif

	SelectObject(memdc, oldbitmap);
	DeleteObject(bitmap);
	DeleteDC(memdc);
	DestroyWindow(g_hwnd);
	ExitProcess(0);
}
