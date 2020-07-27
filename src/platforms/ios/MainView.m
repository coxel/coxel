#import "MainView.h"
#include "../../gfx.h"
#include "../../platform.h"

@implementation MainView
char pixels[HEIGHT * WIDTH * 3];

-(id)initWithFrame:(CGRect)frame {
	self = [super initWithFrame:frame];
	self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
	return self;
}

-(CGPoint)translateMouseLocation:(CGPoint)point {
	struct io* io = console_getio();
	CGFloat view_width = self.frame.size.width;
	CGFloat view_height = self.frame.size.height;
	CGFloat scale = MIN(view_width / WIDTH, view_height / HEIGHT);
	CGFloat width = WIDTH * scale, height = HEIGHT * scale;
	CGFloat x1 = (view_width - width) / 2;
	CGFloat y1 = (view_height - height) / 2;
	return CGPointMake((point.x - x1) / scale, (point.y - y1) / scale);
}

-(void)drawRect:(CGRect)rect {
	for (int y = 0; y < HEIGHT; y++) {
		for (int x = 0; x < WIDTH; x++) {
			int c = palette[gfx_getpixel(console_getgfx(), x, y)];
			int i = ((HEIGHT - y - 1) * WIDTH + x) * 3;
			pixels[i + 0] = c >> 16;
			pixels[i + 1] = (c & 0xFF00) >> 8;
			pixels[i + 2] = c & 0x00FF;
		}
	}
	CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, pixels, HEIGHT * WIDTH * 4, NULL);
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	CGBitmapInfo bitmapInfo = kCGBitmapByteOrderDefault | kCGImageAlphaNone;
	CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
	CGImageRef imageRef = CGImageCreate(WIDTH, HEIGHT, 8, 24, 3 * WIDTH, colorSpace, bitmapInfo, provider, NULL, NO, renderingIntent);
	CGContextRef ctx = UIGraphicsGetCurrentContext();
	CGContextSetInterpolationQuality(ctx, kCGInterpolationNone);
	CGFloat view_width = self.frame.size.width;
	CGFloat view_height = self.frame.size.height;
	CGFloat scale = MIN(view_width / WIDTH, view_height / HEIGHT);
	CGFloat width = WIDTH * scale, height = HEIGHT * scale;
	CGContextDrawImage(ctx, CGRectMake((view_width - width) / 2, (view_height - height) / 2, width, height), imageRef);
	CGImageRelease(imageRef);
	CGColorSpaceRelease(colorSpace);
	CGDataProviderRelease(provider);
}

@end
