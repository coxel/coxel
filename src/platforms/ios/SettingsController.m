#import "MainController.h"
#import "SettingsController.h"
#include "../../platform.h"

@implementation SettingsController

-(void)viewDidLoad {
	[super viewDidLoad];
		
	self.title = @"Settings";
    UIBarButtonItem *menuItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(backSelected:)];
	[self.navigationItem setRightBarButtonItem:menuItem];
}

-(void)backSelected:(UIButton *)button {
	[self dismissViewControllerAnimated:YES completion:nil];
}

-(NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
	return 1;
}

-(NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
	return 2;
}

-(UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    static NSString *CellIdentifier = @"cell";
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];

    if (cell == nil) {
        cell = [[UITableViewCell alloc]initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:CellIdentifier];
    }
	cell.detailTextLabel.font = [UIFont fontWithName:@"Helvetica" size:16];
	if (indexPath.row == 0)
		cell.detailTextLabel.text = @"Reboot with stock firmware";
	else if (indexPath.row == 1)
		cell.detailTextLabel.text = @"Create firmware file for editing";
	return cell;
}

-(void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	if (indexPath.row == 0) {
		UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Confirm" message:@"This will reboot the console. All unsaved changes will be LOST, continue?" preferredStyle:UIAlertControllerStyleAlert];
		UIAlertAction *yesAction = [UIAlertAction actionWithTitle:@"Yes" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
			console_destroy();
			console_factory_init();
			[self dismissViewControllerAnimated:YES completion:nil];
			return;
		}];
		UIAlertAction *noAction = [UIAlertAction actionWithTitle:@"No" style:UIAlertActionStyleCancel handler:nil];
		[alert addAction:yesAction];
		[alert addAction:noAction];
		[self presentViewController:alert animated:YES completion:nil];
	}
	else if (indexPath.row == 1) {
		NSString *confirmMsg;
		NSString *path = get_filepath(FIRMWARE_PATH);
		NSFileManager *fileManager = [[NSFileManager alloc] init];
		if ([fileManager fileExistsAtPath:path])
			confirmMsg = [NSString stringWithFormat:@"The firmware file \"%s\" already exists, overwrite?",FIRMWARE_PATH];
		else
			confirmMsg = [NSString stringWithFormat:@"Create firmware file at \"%s\"?",FIRMWARE_PATH];
		
		UIAlertController *verifyAlert = [UIAlertController alertControllerWithTitle:@"Confirm" message:confirmMsg preferredStyle:UIAlertControllerStyleAlert];
		UIAlertAction *yesAction = [UIAlertAction actionWithTitle:@"Yes" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
			// Create any missing directories
			void *fstock = NULL;
			void *fcreate = NULL;
			void *buf = NULL;
			bool ok = [fileManager createDirectoryAtPath:path.stringByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:nil];
			if (!ok)
				goto cleanup;
			// Read stock firmware file
			uint32_t fsize;
			fstock = platform_open(NULL, &fsize);
			if (fstock == NULL) {
				ok = NO;
				goto cleanup;
			}
			fcreate = platform_create(FIRMWARE_PATH);
			if (fcreate == NULL) {
				ok = NO;
				goto cleanup;
			}
			buf = platform_malloc(fsize);
			if (buf == NULL) {
				ok = NO;
				goto cleanup;
			}
			if (platform_read(fstock, buf, fsize) != fsize) {
				ok = NO;
				goto cleanup;
			}
			if (platform_write(fcreate, buf, fsize) != fsize) {
				ok = NO;
				goto cleanup;
			}
			
		cleanup:
			if (fstock != NULL)
				platform_close(fstock);
			if (fcreate != NULL)
				platform_close(fcreate);
			if (buf != NULL)
				platform_free(buf);
			NSString *infoMsg;
			if (ok)
				infoMsg = [NSString stringWithFormat:@"Firmware file created at \"%s\".",FIRMWARE_PATH];
			else
				infoMsg = @"Create firmware file failed.";
			
			UIAlertController *infoAlert = [UIAlertController alertControllerWithTitle:@"Information" message:infoMsg preferredStyle:UIAlertControllerStyleAlert];
			UIAlertAction *okAction = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil];
			[infoAlert addAction:okAction];
			[self presentViewController:infoAlert animated:YES completion:nil];
		}];
		UIAlertAction *noAction = [UIAlertAction actionWithTitle:@"No" style:UIAlertActionStyleCancel handler:nil];
		[verifyAlert addAction:yesAction];
		[verifyAlert addAction:noAction];
		[self presentViewController:verifyAlert animated:YES completion:nil];
	}
}

@end
