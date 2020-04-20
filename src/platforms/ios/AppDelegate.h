#import <UIKit/UIKit.h>
#import "MainController.h"
#import "SettingsController.h"

@interface AppDelegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow *window;
@property (strong, nonatomic) MainController *controller;
@property (strong, nonatomic) UINavigationController *settingsNavigationController;

@end
