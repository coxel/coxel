#import <UIKit/UIKit.h>

@interface MainController : UIViewController

-(void)initConsole;
-(void)serializeConsole;

@end

NSString *get_filepath(const char *filename);
