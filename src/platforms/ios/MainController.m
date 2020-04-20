#import "AppDelegate.h"
#import "KeyboardView.h"
#import "MainController.h"
#import "MainView.h"
#include "../../platform.h"
#include "../../key.h"
#include <sys/stat.h>

NORETURN void platform_error(const char *msg) {
	AppDelegate *delegate = (AppDelegate *)[[UIApplication sharedApplication] delegate];
	MainController* controller = [delegate controller];
	//controller present
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

uint32_t platform_seed() {
	uint32_t ret;
	int status = SecRandomCopyBytes(kSecRandomDefault, sizeof(ret), &ret);
	if (status != errSecSuccess)
		platform_error("SecRandomCopyBytes() failed.");
	return ret;
}

NSString *get_filepath(const char *filename) {
	NSString *docDir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)[0];
	return [NSString stringWithFormat:@"%@/%@", docDir, [NSString stringWithUTF8String:filename]];
}

void* platform_open(const char *filename, uint32_t *filesize) {
	NSString *path;
	if (filename == NULL)
		path = [[NSBundle mainBundle] pathForResource:@"firmware" ofType:@"cox"];
	else
		path = get_filepath(filename);
	FILE *file = fopen([path UTF8String], "rb");
	if (file == NULL)
		return NULL;
	struct stat stat;
	fstat(fileno(file), &stat);
	if (stat.st_size > UINT32_MAX) {
		fclose(file);
		return NULL;
	}
	*filesize = (uint32_t)stat.st_size;
	return file;
}

void* platform_create(const char* filename) {
	NSString *path = get_filepath(filename);
	FILE *file = fopen([path UTF8String], "wb");
	return file;
}

int platform_read(void* file, char* data, int len) {
	return (int)fread(data, 1, len, (FILE*)file);
}

int platform_write(void* file, const char* data, int len) {
	return (int)fwrite(data, 1, len, (FILE*)file);
}

void platform_close(void* file) {
	fclose((FILE*)file);
}

@implementation MainController
KeyboardView *keyboardView;
MainView *mainView;

-(void)loadView {
	UIStackView *view = [[UIStackView alloc] init];
	view.axis = UILayoutConstraintAxisVertical;

	mainView = [[MainView alloc] init];
	[view addArrangedSubview:mainView];

	keyboardView = [[KeyboardView alloc] init];
	[view addArrangedSubview:keyboardView];

	self.view = view;
}

-(void)viewDidLoad {
	[super viewDidLoad];
}

-(void)didReceiveMemoryWarning {
	[super didReceiveMemoryWarning];
}

-(void)initConsole {
	console_init();
	CADisplayLink *displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(update:)];
	[displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

-(void)update:(CADisplayLink *)displayLink {
	btn_standard_update();
	console_update();
	[mainView setNeedsDisplay];
}

@end
