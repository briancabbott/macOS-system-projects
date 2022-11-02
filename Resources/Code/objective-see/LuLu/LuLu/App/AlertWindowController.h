//
//  file: AlertWindowController.h
//  project: lulu (login item)
//  description: window controller for main firewall alert (header)
//
//  created by Patrick Wardle
//  copyright (c) 2017 Objective-See. All rights reserved.
//

@import Cocoa;
@import OSLog;

#import "ParentsWindowController.h"
#import "VirusTotalViewController.h"
#import "SigningInfoViewController.h"

@interface AlertWindowController : NSWindowController <NSTouchBarProvider, NSTouchBarDelegate>

/* PROPERTIES */

//alert info
@property(nonatomic, retain)NSDictionary* alert;

//touch bar
@property(nonatomic, retain)NSTouchBar* touchBar;

//reply
@property (nonatomic, copy)void(^reply)(NSDictionary*);


/* TOP */

//process icon
@property (weak) IBOutlet NSImageView *processIcon;

//process name
@property (weak) IBOutlet NSTextField *processName;

//general alert message
@property (weak) IBOutlet NSTextField *alertMessage;

//signing info button
@property (weak) IBOutlet NSButton *signingInfoButton;

//signing info: popover
@property (strong) IBOutlet NSPopover *signingInfoPopover;


//virus total: button
@property (weak) IBOutlet NSButton *virusTotalButton;

//virus total: popover
@property (strong) IBOutlet NSPopover *virusTotalPopover;

//view controller for ancestry view/popover
@property (weak) IBOutlet ParentsWindowController *ancestryViewController;

//ancestry button
@property (weak) IBOutlet NSButton *ancestryButton;

//popover for ancestry
@property (strong) IBOutlet NSPopover *ancestryPopover;

//process ancestry
@property (nonatomic, retain)NSMutableArray* processHierarchy;


/* BOTTOM */

//process id
@property (weak) IBOutlet NSTextField *processID;

//process args
@property (weak) IBOutlet NSTextField *processArgs;

//process path
@property (weak) IBOutlet NSTextField *processPath;

//ip address
@property (weak) IBOutlet NSTextField *ipAddress;

//port/protocol
@property (weak) IBOutlet NSTextField *portProto;

//reverse dns name
@property (weak) IBOutlet NSTextField *reverseDNS;

//ancestry view
@property (strong) IBOutlet NSView *ancestryView;

//outline view in ancestry popover
@property (weak) IBOutlet NSOutlineView *ancestryOutline;

//text cell for ancestry popover
@property (weak) IBOutlet NSTextFieldCell *ancestryTextCell;

//time stamp
@property (weak) IBOutlet NSTextField *timeStamp;

//action (rule) scope
@property (weak) IBOutlet NSPopUpButton *actionScope;

//check box for temp rule
@property (weak) IBOutlet NSButton *tempRule;

/* METHODS */

//automatically invoked when user clicks process ancestry button
// ->depending on state, show/populate the popup, or close it
-(IBAction)vtButtonHandler:(id)sender;

//invoked when user clicks process ancestry button
// ->depending on state, show/populate the popup, or close it
-(IBAction)ancestryButtonHandler:(id)sender;

//button handler
// ->block/allow, and then close
-(IBAction)handleUserResponse:(id)sender;

@end
