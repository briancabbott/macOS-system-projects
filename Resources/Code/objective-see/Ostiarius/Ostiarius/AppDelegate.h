//
//  AppDelegate.h
//  Ostiarius
//
//  Created by Patrick Wardle on 8/27/14.
//  Copyright (c) 2015 Objective-See. All rights reserved.
//
#import <Cocoa/Cocoa.h>

#import "ErrorWindowController.h"
#import "ConfigureWindowController.h"

@interface AppDelegate : NSObject <NSApplicationDelegate, NSMenuDelegate, NSWindowDelegate>
{

}

/* PROPERTIES */

//configure window controller
@property(nonatomic, retain)ConfigureWindowController* configureWindowController;

//error window controller
@property(nonatomic, retain)ErrorWindowController* errorWindowController;


/* METHODS */

//display configuration window w/ 'install' || 'uninstall' button
-(void)displayConfigureWindow:(BOOL)isInstalled;

//display error window
-(void)displayErrorWindow:(NSDictionary*)errorInfo;

@end
