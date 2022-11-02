//
//  file: StatusBarMenu.h
//  project: BlockBlock (login item)
//  description: menu handler for status bar icon (header)
//
//  created by Patrick Wardle
//  copyright (c) 2017 Objective-See. All rights reserved.
//


@import Cocoa;

@interface StatusBarItem : NSObject <NSPopoverDelegate>
{

}

//status item
@property(nonatomic, strong, readwrite)NSStatusItem *statusItem;

//popover
@property(retain, nonatomic)NSPopover *popover;

//disabled flag
@property BOOL isDisabled;

/* METHODS */

//init
-(id)init:(NSMenu*)menu preferences:(NSDictionary*)preferences;

//remove status item
-(void)removeStatusItem;

@end
