#import "AppDelegate.h"

@implementation AppDelegate

-(BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(nullable NSDictionary<UIApplicationLaunchOptionsKey,id> *)launchOptions {
	self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
	self.controller = [[MainController alloc] init];
	SettingsController *settingsController = [[SettingsController alloc] init];
	self.settingsNavigationController = [[UINavigationController alloc] init];
	[self.settingsNavigationController pushViewController:settingsController animated:NO];
	[self.window setRootViewController:self.controller];
	//[self.window setRootViewController:self.settingsNavigationController];
	[self.window makeKeyAndVisible];
	[self.controller initConsole];
	return YES;
}

-(BOOL)application:(UIApplication *)application shouldSaveSecureApplicationState:(NSCoder *)coder {
	[self.controller serializeConsole];
	return YES;
}

-(BOOL)application:(UIApplication *)application shouldRestoreSecureApplicationState:(NSCoder *)coder {
	return YES;
}

-(void)applicationWillResignActive:(UIApplication *)application {
}

-(void)applicationDidEnterBackground:(UIApplication *)application {
}

-(void)applicationWillEnterForeground:(UIApplication *)application {
}

-(void)applicationDidBecomeActive:(UIApplication *)application {
}

-(void)applicationWillTerminate:(UIApplication *)application {
}

@end
