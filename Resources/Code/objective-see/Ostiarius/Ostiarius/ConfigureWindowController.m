//
//  Utilities.m
//  Ostiarius
//
//  Created by Patrick Wardle on 1/2/16.
//  Copyright (c) 2016 Objective-See. All rights reserved.
//

#import "Consts.h"
#import "Logging.h"
#import "Configure.h"
#import "Utilities.h"
#import "ConfigureWindowController.h"

@implementation ConfigureWindowController

@synthesize statusMsg;
@synthesize moreInfoButton;

//automatically called when nib is loaded
// ->just center window
-(void)awakeFromNib
{
    //center
    [self.window center];
    
    return;
}

//configure window/buttons
// ->also brings window to front
-(void)configure:(BOOL)isInstalled
{
    //set window title
    [self window].title = [NSString stringWithFormat:@"version %@", getAppVersion()];
    
    //dbg msg
    logMsg(LOG_DEBUG, @"configuring install/uninstall window");
    
    //init status msg
    [self.statusMsg setStringValue:@"blocks unsigned internet binaries"];
    
    //enable 'uninstall' button when app is installed already
    if(YES == isInstalled)
    {
        //enable
        self.uninstallButton.enabled = YES;
    }
    //otherwise disable
    else
    {
        //disable
        self.uninstallButton.enabled = NO;
    }
    
    //make 'install' have focus
    // ->more likely they'll be upgrading
    [self.window makeFirstResponder:self.installButton];
    
    //set delegate
    [self.window setDelegate:self];
    
    return;
}

//display (show) window
// ->center, make front, set bg to white, etc
-(void)display
{
    //center window
    [[self window] center];
    
    //show (now configured) windows
    [self showWindow:self];
    
    //make it key window
    [self.window makeKeyAndOrderFront:self];
    
    //make window front
    [NSApp activateIgnoringOtherApps:YES];
    
    //make white
    [self.window setBackgroundColor: NSColor.whiteColor];
    
    return;
}

//button handler for uninstall/install
-(IBAction)buttonHandler:(id)sender
{
    //button title
    NSString* buttonTitle = nil;
    
    //extact button title
    buttonTitle = ((NSButton*)sender).title;
    
    //action
    NSUInteger action = 0;
    
    //dbg msg
    logMsg(LOG_DEBUG, [NSString stringWithFormat:@"handling action click: %@", buttonTitle]);
    
    //hide 'get more info' button
    self.moreInfoButton.hidden = YES;
    
    //set action
    // ->install daemon
    if(YES == [buttonTitle isEqualToString:ACTION_INSTALL])
    {
        //set
        action = ACTION_INSTALL_FLAG;
    }
    //set action
    // ->uninstall daemon
    else
    {
        //set
        action = ACTION_UNINSTALL_FLAG;
    }
    
    //disable 'x' button
    // ->don't want user killing app during install/upgrade
    [[self.window standardWindowButton:NSWindowCloseButton] setEnabled:NO];
    
    //clear status msg
    [self.statusMsg setStringValue:@""];
    
    //force redraw of status msg
    // ->sometime doesn't refresh (e.g. slow VM)
    [self.statusMsg setNeedsDisplay:YES];
    
    //dbg msg
    logMsg(LOG_DEBUG, [NSString stringWithFormat:@"%@'ing Ostiarius", buttonTitle]);
    
    //invoke logic to install/uninstall
    // ->do in background so UI doesn't block
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
    ^{
        //install/uninstall
        [self lifeCycleEvent:action];

        //dbg msg
        logMsg(LOG_DEBUG, [NSString stringWithFormat:@"done %@'ing Ostiarius", buttonTitle]);
    });
    
//bail
bail:
    
    return;
}

//button handler for '?' button (on an error)
// ->load objective-see's documentation for error(s) in default browser
-(IBAction)info:(id)sender
{
    //url
    NSURL *helpURL = nil;
    
    //build help URL
    helpURL = [NSURL URLWithString:FATAL_ERROR_URL];
    
    //open URL
    // ->invokes user's default browser
    [[NSWorkspace sharedWorkspace] openURL:helpURL];
    
    return;
}

//perform install | uninstall via Control obj
// ->invoked on background thread so that UI doesn't block
-(void)lifeCycleEvent:(NSUInteger)event
{
    //status var
    BOOL status = NO;
    
    //configure object
    Configure* configureObj = nil;
    
    //dbg msg
    logMsg(LOG_DEBUG, [NSString stringWithFormat:@"handling life cycle event, %lu", (unsigned long)event]);
    
    //alloc control object
    configureObj = [[Configure alloc] init];
    
    //begin event
    // ->updates ui on main thread
    dispatch_sync(dispatch_get_main_queue(),
    ^{
        //complete
        [self beginEvent:event];
    });
    
    //sleep
    // ->allow 'install' || 'uninstall' msg to show up
    sleep(1);
    
    //perform action (install | uninstall)
    // ->perform background actions
    if(YES == [configureObj configure:event])
    {
        //set flag
        status = YES;
    }
    
    //error occurred
    else
    {
        //err msg
        logMsg(LOG_ERR, @"ERROR: failed to perform life cycle event");
        
        //set flag
        status = NO;
    }
    
    //complet event
    // ->updates ui on main thread
    dispatch_async(dispatch_get_main_queue(),
    ^{
        //complete
        [self completeEvent:status event:event];
    });
    
    return;
}

//begin event
// ->basically just update UI
-(void)beginEvent:(NSUInteger)event
{
    //status msg frame
    CGRect statusMsgFrame = {0};
    
    //grab exiting frame
    statusMsgFrame = self.statusMsg.frame;
    
    //avoid activity indicator
    // ->shift frame shift delta
    statusMsgFrame.origin.x += FRAME_SHIFT;
    
    //update frame to align
    self.statusMsg.frame = statusMsgFrame;
    
    //align text left
    [self.statusMsg setAlignment:NSLeftTextAlignment];
    
    //install msg
    if(ACTION_INSTALL_FLAG == event)
    {
        //update status msg
        [self.statusMsg setStringValue:@"Installing..."];
    }
    //uninstall msg
    else
    {
        //update status msg
        [self.statusMsg setStringValue:@"Uninstalling..."];
    }
    
    //disable action button
    self.uninstallButton.enabled = NO;
    
    //disable cancel button
    self.installButton.enabled = NO;
    
    //show spinner
    [self.activityIndicator setHidden:NO];
    
    //start spinner
    [self.activityIndicator startAnimation:nil];
    
    return;
}

//complete event
// ->update UI after background event has finished
-(void)completeEvent:(BOOL)success event:(NSUInteger)event
{
    //status msg frame
    CGRect statusMsgFrame = {0};
    
    //action
    NSString* action = nil;
    
    //result msg
    NSString* resultMsg = nil;
    
    //msg font
    NSColor* resultMsgColor = nil;
    
    //generally want centered text
    [self.statusMsg setAlignment:NSCenterTextAlignment];
    
    //set action msg for install
    if(ACTION_INSTALL_FLAG == event)
    {
        //set msg
        action = @"install";
        
        //set result msg
        resultMsg = [NSString stringWithFormat:@"Ostiarius %@ed (note: on OS upgrade new version may be required)", action];

    }
    //set action msg for uninstall
    else
    {
        //set msg
        action = @"uninstall";
        
        //set result msg
        resultMsg = [NSString stringWithFormat:@"Ostiarius %@ed", action];
    }
    
    //success
    if(YES == success)
    {
        //set font to black
        resultMsgColor = [NSColor blackColor];
    }
    //failure
    else
    {
        //set result msg
        resultMsg = [NSString stringWithFormat:@"error: %@ failed", action];
        
        //set font to red
        resultMsgColor = [NSColor redColor];
        
        //show 'get more info' button
        self.moreInfoButton.hidden = NO;
    }
    
    //stop/hide spinner
    [self.activityIndicator stopAnimation:nil];
    
    //hide spinner
    [self.activityIndicator setHidden:YES];
    
    //grab exiting frame
    statusMsgFrame = self.statusMsg.frame;
    
    //shift back since activity indicator is gone
    statusMsgFrame.origin.x -= FRAME_SHIFT;
    
    //update frame to align
    self.statusMsg.frame = statusMsgFrame;
    
    //set font to bold
    [self.statusMsg setFont:[NSFont fontWithName:@"Menlo-Bold" size:13]];
    
    //set msg color
    [self.statusMsg setTextColor:resultMsgColor];
    
    //set status msg
    [self.statusMsg setStringValue:resultMsg];
    
    //toggle buttons
    // ->after install turn on 'uninstall' and off 'install'
    if(ACTION_INSTALL_FLAG == event)
    {
        //enable uninstall
        self.uninstallButton.enabled = YES;
        
        //disable install
        self.installButton.enabled = NO;
    }
    //toggle buttons
    // ->after uninstall turn off 'uninstall' and on 'install'
    else
    {
        //disable
        self.uninstallButton.enabled = NO;
        
        //enable close button
        self.installButton.enabled = YES;
    }
    
    //ok to re-enable 'x' button
    [[self.window standardWindowButton:NSWindowCloseButton] setEnabled:YES];
    
    //(re)make window window key
    [self.window makeKeyAndOrderFront:self];
    
    //(re)make window front
    [NSApp activateIgnoringOtherApps:YES];
    
    return;
}

//automatically invoked when window is closing
// ->just exit application
-(void)windowWillClose:(NSNotification *)notification
{
    //exit
    [NSApp terminate:self];
    
    return;
}

@end