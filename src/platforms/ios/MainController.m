#import "AppDelegate.h"
#import "KeyboardView.h"
#import "MainController.h"
#import "MainView.h"
#include "../../platform.h"
#include "../../key.h"
#include "../../str.h"
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
	NSString* str = [[NSString alloc] initWithBytes:ptr length:len encoding:NSASCIIStringEncoding];
	[[UIPasteboard generalPasteboard] setString:str];
}

int platform_paste(char* ptr, int len) {
	NSString* str = [[UIPasteboard generalPasteboard] string];
	if (str == nil)
		return 0;
	const char* buf = [str UTF8String];
	size_t size = strlen(buf);
	if (size > INT_MAX)
		size = INT_MAX;
	return copy_allowed_chars(ptr, len, buf, (int)size);
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
	uint32_t size;
	void* f = platform_open(STATE_PATH, &size);
	if (f) {
		NSLog(@"Loading serialized state.");
		console_deserialize_init(f);
		platform_close(f);
		NSFileManager *fileManager = [[NSFileManager alloc] init];
		NSString *path = get_filepath(STATE_PATH);
		if (![fileManager removeItemAtPath:path error:nil]) {
			NSLog(@"Remove state file failed.");
		}
	}
	else
		console_init();
	CADisplayLink *displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(update:)];
	[displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

-(void)serializeConsole {
	NSLog(@"Serializing console state.");
	void *f = platform_create(STATE_PATH);
	if (f) {
		console_serialize(f);
		platform_close(f);
		NSLog(@"State serialization succeeded.");
	}
	else
		NSLog(@"State serialization failed.");
}

-(void)update:(CADisplayLink *)displayLink {
	btn_standard_update();
	console_update();
	enum key key = [keyboardView extractDelayedReleaseKey];
	if (key != kc_none)
		key_release(key);
	[mainView setNeedsDisplay];
}

-(void)resetMouseLocation {
	struct io* io = console_getio();
	io->mousex = num_kint(-1);
	io->mousey = num_kint(-1);
}

-(void)updateMouseLocation:(UITouch *)touch {
	CGPoint point = [mainView translateMouseLocation:[touch locationInView:mainView]];
	struct io* io = console_getio();
	io->mousex = num_kint((int)point.x);
	io->mousey = num_kint((int)point.y);
}

-(void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
	[self updateMouseLocation:[touches anyObject]];
	key_press(kc_mleft);
}

-(void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
	[self updateMouseLocation:[touches anyObject]];
}

-(void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
	[self updateMouseLocation:[touches anyObject]];
	key_release(kc_mleft);
}

-(void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
	[self updateMouseLocation:[touches anyObject]];
	key_release(kc_mleft);
}

@end
