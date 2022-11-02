//
//  ConfigureWindowController.h
//  Ostiarius
//
//  Created by Patrick Wardle on 11/23/14.
//  Copyright (c) 2016 Objective-See. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface ConfigureWindowController : NSWindowController <NSWindowDelegate>
{
    
}

/* PROPERTIES */

//title for window
//@property (nonatomic, retain)NSString* windowTitle;

//action
//@property NSUInteger action;

@property (weak) IBOutlet NSProgressIndicator *activityIndicator;
@property (weak) IBOutlet NSTextField *statusMsg;
@property (weak) IBOutlet NSButton *installButton;
@property (weak) IBOutlet NSButton *uninstallButton;
@property (weak) IBOutlet NSButton *moreInfoButton;


/* METHODS */

//install/uninstall button handler
-(IBAction)buttonHandler:(id)sender;

//(more) info button handler
-(IBAction)info:(id)sender;

//configure window/buttons
// ->also brings to front
-(void)configure:(BOOL)isInstalled;

//display (show) window
-(void)display;


@end
