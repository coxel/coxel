#import "AppDelegate.h"
#import "KeyboardView.h"
#include "../../key.h"

#define KB_ROWS		6
static const int kbBtnCount[KB_ROWS] = {
	8, /* Extra functional keys */
	10, /* Numerical keys */
	10, /* First alphabet row */
	9, /* Second alphabet row */
	10, /* Third alphabet row */
	6, /* Functional keys */
};

struct KeyTitle {
	enum key key;
	NSString *title;
};

static const enum key kc_symbol = kc_cnt;

static const enum key mainLayout[] = {
	kc_esc, kc_tab, kc_home, kc_end, kc_pgup, kc_pgdn, kc_delete, kc_return,
	kc_1, kc_2, kc_3, kc_4, kc_5, kc_6, kc_7, kc_8, kc_9, kc_0,
	kc_q, kc_w, kc_e, kc_r, kc_t, kc_y, kc_u, kc_i, kc_o, kc_p,
	kc_a, kc_s, kc_d, kc_f, kc_g, kc_h, kc_j, kc_k, kc_l,
	kc_shift, kc_z, kc_x, kc_c, kc_v, kc_b, kc_n, kc_m, kc_up, kc_backspace,
	kc_symbol, kc_ctrl, kc_space, kc_left, kc_down, kc_right,
};

static const struct KeyTitle keyTitles[] = {
	{ kc_esc, @"Esc" },
	{ kc_tab, @"Tab" },
	{ kc_home, @"Home" },
	{ kc_end, @"End" },
	{ kc_pgup, @"PgUp" },
	{ kc_pgdn, @"PgDn" },
	{ kc_delete, @"Del" },
	{ kc_backspace, @"⌫" },
	{ kc_shift, @"⇧" },
	{ kc_ctrl, @"Ctrl" },
	{ kc_symbol, @"#+=" },
	{ kc_up, @"▲" },
	{ kc_left, @"◀" },
	{ kc_down, @"▼" },
	{ kc_right, @"▶" },
	{ kc_return, @"⏎" },
	{ kc_none, nil },
};

static const char symbolLayout[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	'!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
	'`', '~', '-', '_', '=', '+', '[', ']', '{', '}',
	'|', '\\', '<', '>', '/', ';', ':', '\'', '"',
	0, ' ', ' ', ' ', ' ', '?', ',', '.', 0, 0,
	0, 0, ' ', 0, 0, 0,
};

@interface BackgroundTrackingView : UIView
@end

@implementation BackgroundTrackingView
-(id)initWithFrame:(CGRect)frame {
	self = [super initWithFrame:frame];
	self.backgroundColor = [UIColor systemGray4Color];
	self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
	return self;
}

-(bool)pointInside:(CGPoint)point withEvent:(UIEvent *)event {
	return NO;
}

-(UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event {
	return nil;
}
@end

@implementation KeyboardView
bool shiftHold = NO;
bool shiftToggled = NO;
bool ctrlHold = NO;
bool ctrlToggled = NO;
bool symbolHold = NO;
bool symbolToggled = NO;
bool keyPressedDuringHold = NO;
NSMutableArray *btnArray = nil;
UIButton *spaceBtn = nil;

-(id)initWithFrame:(CGRect)frame {
	const CGFloat horizontalSpacing = 6;
	self = [super initWithFrame:frame];
	BackgroundTrackingView *backgroundView = [[BackgroundTrackingView alloc] initWithFrame:frame];
	[self addSubview:backgroundView];
	self.backgroundColor = [UIColor colorWithRed:0 green:0 blue:0 alpha:0];
	self.axis = UILayoutConstraintAxisVertical;
	self.spacing = 6;
	self.translatesAutoresizingMaskIntoConstraints = NO;
	self.layoutMargins = UIEdgeInsetsMake(3, 2, 3, 2);
	[self setLayoutMarginsRelativeArrangement:YES];
	btnArray = [[NSMutableArray alloc] init];
	for (int row = 0; row < KB_ROWS; row++) {
		UIView *rowView = [[UIView alloc] init];
		rowView.translatesAutoresizingMaskIntoConstraints = NO;
		// Fixed height
		[rowView.heightAnchor constraintEqualToConstant:36].active = YES;
		UIButton *prevButton = nil;
		for (int col = 0; col < kbBtnCount[row]; col++) {
			UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
			if (row == 0)
				button.titleLabel.font = [UIFont systemFontOfSize:11];
			button.layer.cornerRadius = 5.0;
			button.layer.shadowColor = [[UIColor systemGrayColor] CGColor];
			button.layer.shadowOffset = CGSizeMake(0, 1.5f);
			button.layer.shadowOpacity = 1.0f;
			button.layer.shadowRadius = 0.0f;
			button.layer.masksToBounds = NO;
			button.translatesAutoresizingMaskIntoConstraints = NO;
			[button setTitleColor:[UIColor labelColor] forState:UIControlStateNormal];
			[button addTarget:self action:@selector(keyPress:) forControlEvents:UIControlEventTouchDown];
			[button addTarget:self action:@selector(keyRelease:) forControlEvents:UIControlEventTouchUpInside];
			[button addTarget:self action:@selector(keyRelease:) forControlEvents:UIControlEventTouchUpOutside];
			[rowView addSubview:button];
			// Add layout constraints
			int keyId = (int)[btnArray count];
			// Top/bottom alignment
			[button.topAnchor constraintEqualToAnchor:rowView.topAnchor].active = YES;
			[button.bottomAnchor constraintEqualToAnchor:rowView.bottomAnchor].active = YES;
			// Leading and trailing alignment
			if (mainLayout[keyId] == kc_a) {
				NSLayoutConstraint *constraint = [NSLayoutConstraint constraintWithItem:rowView attribute:NSLayoutAttributeTrailing relatedBy:NSLayoutRelationEqual toItem:button attribute:NSLayoutAttributeLeading multiplier:20 constant:9*horizontalSpacing];
				[rowView addConstraint:constraint];
			}
			else if (col == 0)
				[button.leadingAnchor constraintEqualToAnchor:rowView.leadingAnchor].active = YES;
			else if (row != 3 && col + 1 == kbBtnCount[row])
				[button.trailingAnchor constraintEqualToAnchor:rowView.trailingAnchor].active = YES;
			if (col > 0) {
				// Spacing
				[button.leadingAnchor constraintEqualToAnchor:prevButton.trailingAnchor constant:horizontalSpacing].active = YES;
			}
			// Width
			if (mainLayout[keyId] == kc_ctrl || mainLayout[keyId] == kc_symbol) {
				[rowView.widthAnchor constraintEqualToAnchor:button.widthAnchor multiplier:7].active = YES;
			}
			else if (mainLayout[keyId] != kc_space && mainLayout[keyId] != kc_return) {
				CGFloat multiplier = 10;
				CGFloat spacingMultiplier = 9;
				if (row == 0) {
					multiplier = 9;
					spacingMultiplier = 7;
				}
				[rowView.widthAnchor constraintEqualToAnchor:button.widthAnchor multiplier:multiplier constant:spacingMultiplier*horizontalSpacing].active = YES;
			}
			if (mainLayout[keyId] == kc_space)
				spaceBtn = button;
			prevButton = button;
			[btnArray addObject:button];
		}
		[self addArrangedSubview:rowView];
	}
	[self updateButtonTitles];
	return self;
}

-(UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event {
	if (!CGRectContainsPoint(self.bounds, point))
		return nil;
	// Find best row view
	UIView *rowView = nil;
	CGFloat bestDistance = CGFLOAT_MAX;
	int rowId = -1;
	NSArray<UIView *> *rowViews = [self arrangedSubviews];
	for (int i = 0; i < rowViews.count; i++) {
		UIView *view = rowViews[i];
		CGFloat distance = ABS(point.y - CGRectGetMinY(view.frame));
		distance = MIN(distance, ABS(point.y - CGRectGetMaxY(view.frame)));
		if (distance < bestDistance) {
			rowView = view;
			bestDistance = distance;
			rowId = i;
		}
	}
	if (rowView == nil)
		return nil;
	// Find best column view
	UIView *hitView = nil;
	bestDistance = CGFLOAT_MAX;
	for (UIView *view in [rowView subviews]) {
		CGFloat distance = ABS(point.x - CGRectGetMinX(view.frame));
		distance = MIN(distance, ABS(point.x - CGRectGetMaxX(view.frame)));
		if (distance < bestDistance) {
			hitView = view;
			bestDistance = distance;
		}
	}
	if (rowId + 1 == rowViews.count && event.type == UIEventTypeTouches && hitView != spaceBtn) {
		[self keyPress:(UIButton *)hitView];
	}
	return hitView;
}

-(void)updateButtonTitles {
	[UIView performWithoutAnimation:^{
		for (int i = 0; i < btnArray.count; i++) {
			UIButton *button = btnArray[i];
			NSString *title = nil;
			char ch = 0;
			enum key key = mainLayout[i];
			if (symbolLayout[i] != 0
				|| (key == kc_shift && (shiftHold || shiftToggled))
				|| (key == kc_ctrl && (ctrlHold || ctrlToggled))
				|| (key == kc_symbol && (symbolHold || symbolToggled)))
				button.backgroundColor = [UIColor systemBackgroundColor];
			else
				button.backgroundColor = [UIColor systemGray2Color];
			if (symbolHold || symbolToggled)
				ch = symbolLayout[i];
			if (ch == 0 && key < kc_cnt) {
				if (shiftHold || shiftToggled)
					ch = key_get_shift_char(key);
				else
					ch = key_get_char(key);
			}
			if (ch == 0) {
				for (int j = 0; keyTitles[j].title != nil; j++) {
					if (keyTitles[j].key == key) {
						title = keyTitles[j].title;
						break;
					}
				}
			}
			else
				title = [NSString stringWithFormat:@"%c",ch];
			if (symbolToggled && key == kc_symbol)
				title = @"ABC";
			else if (ctrlToggled && key == kc_return)
				title = @"Settings";
			if (title != nil) {
				[button setTitle:title forState:UIControlStateNormal];
				[button layoutIfNeeded];
			}
		}
	}];
}

-(int)getButtonId:(UIButton *)button {
	for (int i = 0; i < btnArray.count; i++) {
		if (btnArray[i] == button)
			return i;
	}
	return -1;
}

-(void)keyPress:(UIButton *)button {
	int btnId = [self getButtonId:button];
	if (btnId == -1)
		return;
	if (symbolHold || symbolToggled) {
		char ch = symbolLayout[btnId];
		if (ch != 0) {
			key_input(ch);
			symbolToggled = NO;
			if (symbolHold)
				keyPressedDuringHold = YES;
			[self updateButtonTitles];
			return;
		}
	}
	enum key key = mainLayout[btnId];
	if (ctrlToggled && key == kc_return) {
		AppDelegate *delegate = (AppDelegate *)[[UIApplication sharedApplication] delegate];
		delegate.settingsNavigationController.modalPresentationStyle = UIModalPresentationFullScreen;
		[delegate.controller presentViewController:delegate.settingsNavigationController animated:YES completion:nil];
		ctrlToggled = NO;
		[self updateButtonTitles];
		return;
	}
	if (key < kc_cnt) {
		key_press(key);
		key_input(key_get_standard_input(key));
	}
	if (key == kc_shift) {
		shiftHold = YES;
		keyPressedDuringHold = NO;
		[self updateButtonTitles];
	}
	else if (shiftToggled) {
		key_release(kc_shift);
		shiftToggled = NO;
		[self updateButtonTitles];
	}
	if (key == kc_ctrl) {
		ctrlHold = YES;
		keyPressedDuringHold = NO;
		[self updateButtonTitles];
	}
	else if (ctrlToggled) {
		key_release(kc_ctrl);
		ctrlToggled = NO;
		[self updateButtonTitles];
	}
	if (key == kc_symbol) {
		symbolHold = YES;
		keyPressedDuringHold = NO;
		[self updateButtonTitles];
	}
	if (!key_is_modifier(key) && key != kc_symbol)
		keyPressedDuringHold = YES;
}

-(void)keyRelease:(UIButton *)button {
	int btnId = [self getButtonId:button];
	if (btnId == -1)
		return;
	if (symbolHold || symbolToggled) {
		char ch = symbolLayout[btnId];
		if (ch != 0)
			return;
	}
	enum key key = mainLayout[btnId];
	if (key == kc_shift) {
		shiftHold = NO;
		if (keyPressedDuringHold) {
			shiftToggled = NO;
			keyPressedDuringHold = NO;
			[self updateButtonTitles];
		}
		else {
			shiftToggled = !shiftToggled;
			[self updateButtonTitles];
			if (shiftToggled)
				return;
		}
	}
	else if (key == kc_ctrl) {
		ctrlHold = NO;
		if (keyPressedDuringHold) {
			ctrlToggled = NO;
			keyPressedDuringHold = NO;
			[self updateButtonTitles];
		}
		else {
			ctrlToggled = !ctrlToggled;
			[self updateButtonTitles];
			if (ctrlToggled)
				return;
		}
	}
	else if (key == kc_symbol) {
		symbolHold = NO;
		if (keyPressedDuringHold) {
			symbolToggled = NO;
			keyPressedDuringHold = NO;
			[self updateButtonTitles];
		}
		else {
			symbolToggled = !symbolToggled;
			[self updateButtonTitles];
			return;
		}
	}
	if (key < kc_cnt)
		key_release(key);
}

@end
