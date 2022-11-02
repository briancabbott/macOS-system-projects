//
//  file: PrefsWindowController.h
//  project: lulu (main app)
//  description: preferences window controller (header)
//
//  created by Patrick Wardle
//  copyright (c) 2017 Objective-See. All rights reserved.
//

#import "consts.h"
#import "Update.h"
#import "utilities.h"
#import "AppDelegate.h"
#import "PrefsWindowController.h"
#import "UpdateWindowController.h"

/* GLOBALS */

//log handle
extern os_log_t logHandle;

//xpc for daemon comms
extern XPCDaemonClient* xpcDaemonClient;

@implementation PrefsWindowController

@synthesize toolbar;
@synthesize modesView;
@synthesize rulesView;
@synthesize updateView;
@synthesize updateWindowController;

//'allow apple' button
#define BUTTON_ALLOW_APPLE 1

//'allow installed' button
#define BUTTON_ALLOW_INSTALLED 2

//'allow iOS simulator apps' mode button
#define BUTTON_ALLOW_SIMULATOR 3

//'use blocklist' button
#define BUTTON_USE_BLOCK_LIST 4

//'passive mode' button
#define BUTTON_PASSIVE_MODE 5

//'block mode' button
#define BUTTON_BLOCK_MODE 6

//'no-icon mode' button
#define BUTTON_NO_ICON_MODE 7

//'update mode' button
#define BUTTON_NO_UPDATE_MODE 8

//init 'general' view
// add it, and make it selected
-(void)awakeFromNib
{
    //get prefs
    self.preferences = [xpcDaemonClient getPreferences];
    
    //set rules prefs as default
    [self toolbarButtonHandler:nil];
    
    //set rules prefs as default
    [self.toolbar setSelectedItemIdentifier:TOOLBAR_RULES_ID];
    
    return;
}

//toolbar view handler
// toggle view based on user selection
-(IBAction)toolbarButtonHandler:(id)sender
{
    //view
    NSView* view = nil;
    
    //when we've prev added a view
    // remove the prev view cuz adding a new one
    if(nil != sender)
    {
        //remove
        [[[self.window.contentView subviews] lastObject] removeFromSuperview];
    }
    
    //assign view
    switch(((NSToolbarItem*)sender).tag)
    {
        //rules
        case TOOLBAR_RULES:
            
            //set view
            view = self.rulesView;
            
            //set 'apple allowed' button state
            ((NSButton*)[view viewWithTag:BUTTON_ALLOW_APPLE]).state = [self.preferences[PREF_ALLOW_APPLE] boolValue];
            
            //set 'installed allowed' button state
            ((NSButton*)[view viewWithTag:BUTTON_ALLOW_INSTALLED]).state = [self.preferences[PREF_ALLOW_INSTALLED] boolValue];
            
            //set 'allow simulator apps' button
            ((NSButton*)[view viewWithTag:BUTTON_ALLOW_SIMULATOR]).state = [self.preferences[PREF_ALLOW_SIMULATOR] boolValue];

            //set 'block list' button state
            ((NSButton*)[view viewWithTag:BUTTON_USE_BLOCK_LIST]).state = [self.preferences[PREF_USE_BLOCK_LIST] boolValue];
            
            //is there a block list? ...set!
            if(0 != [self.preferences[PREF_BLOCK_LIST] length])
            {
                //set
                self.blockList.stringValue = self.preferences[PREF_BLOCK_LIST];
            }
            
            //set 'browse' button state
            self.selectBlockListButton.enabled = [self.preferences[PREF_USE_BLOCK_LIST] boolValue];
            
            //set block list input state
            self.blockList.enabled = [self.preferences[PREF_USE_BLOCK_LIST] boolValue];
            
            break;
            
        //modes
        case TOOLBAR_MODES:
            
            //set view
            view = self.modesView;
            
            //set 'passive mode' button state
            ((NSButton*)[view viewWithTag:BUTTON_PASSIVE_MODE]).state = [self.preferences[PREF_PASSIVE_MODE] boolValue];
            
            //set 'block mode' button
            ((NSButton*)[view viewWithTag:BUTTON_BLOCK_MODE]).state = [self.preferences[PREF_BLOCK_MODE] boolValue];
            
            //set 'no icon' button state
            ((NSButton*)[view viewWithTag:BUTTON_NO_ICON_MODE]).state = [self.preferences[PREF_NO_ICON_MODE] boolValue];
            
            break;
            
        //update
        case TOOLBAR_UPDATE:
            
            //set view
            view = self.updateView;
    
            //set 'update' button state
            ((NSButton*)[view viewWithTag:BUTTON_NO_UPDATE_MODE]).state = [self.preferences[PREF_NO_UPDATE_MODE] boolValue];
            
            break;
            
        default:
            
            //bail
            goto bail;
    }
    
    //set frame rect
    view.frame = CGRectMake(0, 75, self.window.contentView.frame.size.width, self.window.contentView.frame.size.height-75);
    
    //add to window
    [self.window.contentView addSubview:view];
    
bail:
    
    return;
}

//invoked when user toggles button
// update preferences for that button
-(IBAction)togglePreference:(id)sender
{
    //preferences
    NSMutableDictionary* updatedPreferences = nil;
    
    //button state
    NSNumber* state = nil;
    
    //init
    updatedPreferences = [NSMutableDictionary dictionary];
    
    //get button state
    state = [NSNumber numberWithBool:((NSButton*)sender).state];
    
    //set appropriate preference
    switch(((NSButton*)sender).tag)
    {
        //allow apple
        case BUTTON_ALLOW_APPLE:
            updatedPreferences[PREF_ALLOW_APPLE] = state;
            break;
            
        //allow installed
        case BUTTON_ALLOW_INSTALLED:
            updatedPreferences[PREF_ALLOW_INSTALLED] = state;
            break;
            
        //allow simulator apps
        case BUTTON_ALLOW_SIMULATOR:
            updatedPreferences[PREF_ALLOW_SIMULATOR] = state;
            break;
            
        //use block list
        case BUTTON_USE_BLOCK_LIST:
            updatedPreferences[PREF_USE_BLOCK_LIST] = state;
            
            //set 'browse' button state
            self.selectBlockListButton.enabled = (NSControlStateValueOn == state.longValue);
            
            //set block list input state
            self.blockList.enabled = (NSControlStateValueOn == state.longValue);
            
            break;
            
        //passive mode
        case BUTTON_PASSIVE_MODE:
            updatedPreferences[PREF_PASSIVE_MODE] = state;
            break;
            
        //block mode
        case BUTTON_BLOCK_MODE:
            
            //enable?
            // show alert
            if(NSControlStateValueOn == state.longValue)
            {
                //show alert
                showAlert(@"Outgoing traffic will now be blocked.", @"Note however:\r\n▪ Existing connections will not be impacted.\r\n▪ OS traffic (not routed thru LuLu) will not be blocked.");
            }
                
            updatedPreferences[PREF_BLOCK_MODE] = state;
            break;
            
        //no icon mode
        case BUTTON_NO_ICON_MODE:
            updatedPreferences[PREF_NO_ICON_MODE] = state;
            break;
            
        //no update mode
        case BUTTON_NO_UPDATE_MODE:
            updatedPreferences[PREF_NO_UPDATE_MODE] = state;
            break;
            
        default:
            break;
    }
    
    //send XPC msg to daemon to update prefs
    // returns (all/latest) prefs, which is what we want
    self.preferences = [xpcDaemonClient updatePreferences:updatedPreferences];

    //call back into app to process
    // e.g. show/hide status bar icon, etc.
    [((AppDelegate*)[[NSApplication sharedApplication] delegate]) preferencesChanged:self.preferences];
    
    return;
}

//browse for select list
-(IBAction)selectBlockList:(id)sender
{
    //'browse' panel
    NSOpenPanel *panel = nil;
        
    //init panel
    panel = [NSOpenPanel openPanel];
        
    //allow files
    panel.canChooseFiles = YES;
    
    //start ...at desktop
    panel.directoryURL = [NSURL fileURLWithPath:[NSSearchPathForDirectoriesInDomains (NSDesktopDirectory, NSUserDomainMask, YES) firstObject]];
        
    //disable multiple selections
    panel.allowsMultipleSelection = NO;
        
    //show it
    // and wait for response
    if(NSModalResponseOK == [panel runModal])
    {
        //update ui
        self.blockList.stringValue = panel.URL.path;
        
        //dbg msg
        os_log_debug(logHandle, "user selected block list: %{public}@", self.blockList.stringValue);
        
        //send XPC msg to daemon to update prefs
        // returns (all/latest) prefs, which is what we want
        self.preferences = [xpcDaemonClient updatePreferences:@{PREF_BLOCK_LIST:panel.URL.path}];
    }
    
    return;
}


//invoked when block list path is (manually entered)
-(IBAction)updateBlockList:(id)sender
{
    //dbg msg
    os_log_debug(logHandle, "got 'update block list event' (value: %{public}@)", self.blockList.stringValue);
    
    //send XPC msg to daemon to update prefs
    // returns (all/latest) prefs, which is what we want
    self.preferences = [xpcDaemonClient updatePreferences:@{PREF_BLOCK_LIST:self.blockList.stringValue}];
    
    return;
}

//'view rules' button handler
// call helper method to show rule's window
-(IBAction)viewRules:(id)sender
{
    //call into app delegate to show app rules
    [((AppDelegate*)[[NSApplication sharedApplication] delegate]) showRules:nil];
    
    return;
}

//'check for update' button handler
-(IBAction)check4Update:(id)sender
{
    //update obj
    Update* update = nil;
    
    //disable button
    self.updateButton.enabled = NO;
    
    //reset
    self.updateLabel.stringValue = @"";
    
    //show/start spinner
    [self.updateIndicator startAnimation:self];
    
    //init update obj
    update = [[Update alloc] init];
    
    //check for update
    // 'updateResponse newVersion:' method will be called when check is done
    [update checkForUpdate:^(NSUInteger result, NSString* newVersion) {
        
        //process response
        [self updateResponse:result newVersion:newVersion];
        
    }];
    
    return;
}

//process update response
// error, no update, update/new version
-(void)updateResponse:(NSInteger)result newVersion:(NSString*)newVersion
{
    //re-enable button
    self.updateButton.enabled = YES;
    
    //stop/hide spinner
    [self.updateIndicator stopAnimation:self];
    
    switch(result)
    {
        //error
        case -1:
            
            //set label
            self.updateLabel.stringValue = @"error: update check failed";
            break;
            
        //no updates
        case 0:
            
            //dbg msg
            os_log_debug(logHandle, "no updates available");
            
            //set label
            self.updateLabel.stringValue = [NSString stringWithFormat:@"Installed version (%@),\r\nis the latest.", getAppVersion()];
           
            break;
         
            
        //new version
        case 1:
            
            //dbg msg
            os_log_debug(logHandle, "a new version (%@) is available", newVersion);
            
            //alloc update window
            updateWindowController = [[UpdateWindowController alloc] initWithWindowNibName:@"UpdateWindow"];
            
            //configure
            [self.updateWindowController configure:[NSString stringWithFormat:@"a new version (%@) is available!", newVersion]];
            
            //center window
            [[self.updateWindowController window] center];
            
            //show it
            [self.updateWindowController showWindow:self];
            
            //invoke function in background that will make window modal
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                
                //make modal
                makeModal(self.updateWindowController);
                
            });
            
            break;
    }
    
    
    return;
}

//button handler
// open LuLu home page/docs
-(IBAction)openHomePage:(id)sender {
    
    //open
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:PRODUCT_URL]];
    
    return;
}

//on window close
// update prefs/set activation policy
-(void)windowWillClose:(NSNotification *)notification
{
    //blank block list?
    // uncheck 'enabled' and update prefs
    if(0 == self.blockList.stringValue.length)
    {
        //uncheck 'blocklist' radio button
        ((NSButton*)[self.rulesView viewWithTag:BUTTON_USE_BLOCK_LIST]).state = NSControlStateValueOff;
        
        //disable 'browse' button
        self.selectBlockListButton.enabled = NSControlStateValueOff;
        
        //disable block list input
        self.blockList.enabled = NSControlStateValueOff;
        
        //send XPC msg to daemon to update prefs
        // returns (all/latest) prefs, which is what we want
        self.preferences = [xpcDaemonClient updatePreferences:@{PREF_USE_BLOCK_LIST:@0}];
    }
        
    //block list changed? capture!
    // this logic is needed, as window can be closed when text field still has focus and 'end edit' won't have fired
    if(YES != [self.preferences[PREF_BLOCK_LIST] isEqualToString:self.blockList.stringValue])
    {
        //send XPC msg to daemon to update prefs
        // returns (all/latest) prefs, which is what we want
        self.preferences = [xpcDaemonClient updatePreferences:@{PREF_BLOCK_LIST:self.blockList.stringValue}];
    }
     
    //wait a bit, then set activation policy
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
    ^{
         //on main thread
         dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
             
             //set activation policy
             [((AppDelegate*)[[NSApplication sharedApplication] delegate]) setActivationPolicy];
             
         });
    });
    
    return;
}
@end
