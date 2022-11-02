//
//  AppDelegate.h
//  TaskExplorer
//
//  Created by Patrick Wardle
//  Copyright (c) 2015 Objective-See. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import "Task.h"
#import "Filter.h"
#import "Binary.h"
#import "ItemBase.h"
#import "VirusTotal.h"
#import "TaskEnumerator.h"
#import "CustomTextField.h"
#import "TaskTableController.h"
#import "AboutWindowController.h"
#import "PrefsWindowController.h"
#import "SearchWindowController.h"
#import "UpdateWindowController.h"
#import "ResultsWindowController.h"
#import "FlaggedItemWindowController.h"
#import "RequestRootWindowController.h"

/* GLOBALS */

//shared enumerator
extern TaskEnumerator* taskEnumerator;

//shared virustotal object
extern VirusTotal* virusTotal;

//network connected flag
extern BOOL isConnected;

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate, NSTableViewDataSource, NSTableViewDelegate, NSMenuDelegate>
{

}

//friends
@property (weak) IBOutlet NSWindow *friends;

//'filter task' search box
// ->top pane
@property (weak) IBOutlet NSSearchField *filterTasksBox;

//(current) bottom view controller
@property(nonatomic, retain)TaskTableController *bottomViewController;

//top view/pane
@property (weak) IBOutlet NSView *topPane;

//task table controller object
@property (nonatomic, retain)TaskTableController *taskTableController;

//current task view format
// ->flat or tree
@property NSUInteger taskViewFormat;

//drop-down view selector
@property (weak) IBOutlet NSPopUpButton *viewSelector;

//segmented button for button pane
// ->select to view dylib, files, network, etc
@property (weak) IBOutlet NSSegmentedControl *bottomPaneBtn;

//action when segmented button is clicked
-(IBAction)selectBottomPaneContent:(id)sender;

//bottom pane view
@property (weak) IBOutlet NSView *bottomPane;

//'filter items' search box (bottom pane)
@property (weak) IBOutlet NSSearchField *filterItemsBox;

//(main) window
@property (assign) IBOutlet NSWindow *window;

//logo button
@property (weak) IBOutlet NSButton *logoButton;

//spinner
@property (weak) IBOutlet NSProgressIndicator *progressIndicator;

//filter object
@property(nonatomic, retain)Filter* filterObj;

//array for all virus total threads
@property(nonatomic, retain)NSMutableArray* vtThreads;

//request root window controller
@property(nonatomic, retain)RequestRootWindowController* requestRootWindowController;

//preferences window controller
@property(nonatomic, retain)PrefsWindowController* prefsWindowController;

//about window controller
@property(nonatomic, retain)AboutWindowController* aboutWindowController;

//search window controller
@property(nonatomic, retain)SearchWindowController* searchWindowController;

//results window controller
@property(nonatomic, retain)ResultsWindowController* resultsWindowController;

//flagged items window controller
@property(nonatomic, retain)FlaggedItems* flagItemsWindowController;

//currently selected task
@property(nonatomic, retain)Task* currentTask;

//activity indicator for bottom pane
@property (weak) IBOutlet NSProgressIndicator *bottomPaneSpinner;

//'no items' found label for bottom pane
@property (weak) IBOutlet NSTextField *noItemsLabel;

//refresh button
@property (weak) IBOutlet NSButton *refreshButton;

//search button
@property (weak) IBOutlet NSButton *searchButton;

//save button
@property (weak) IBOutlet NSButton *saveButton;

//flagged items button
@property (weak) IBOutlet NSButton *flaggedButton;

//top constraint
@property(nonatomic, retain)NSLayoutConstraint* topConstraint;

//bottom constraint
@property(nonatomic, retain)NSLayoutConstraint* bottomConstraint;

//top constraint
@property(nonatomic, retain)NSLayoutConstraint* leadingConstraint;

//top constraint
@property(nonatomic, retain)NSLayoutConstraint* trailingConstraint;

//flag for filter field (autocomplete)
@property BOOL completePosting;

//flag for filter field (autocomplete)
@property BOOL commandHandling;

//custom search field for tasks
@property(nonatomic, retain)CustomTextField* customTasksFilter;

//custom search field for items
@property(nonatomic, retain)CustomTextField* customItemsFilter;

//overlay view for filter
@property (weak) IBOutlet NSView *filteringOverlay;

//activity indicator for filtering
@property (weak) IBOutlet NSProgressIndicator *filteringIndicator;

//message for filtering
@property (weak) IBOutlet NSTextField *filteringMessage;

//update window controller
@property(nonatomic, retain)UpdateWindowController* updateWindowController;

/* METHODS */

//complete a few inits
// ->then invoke helper method to start enum'ing task (in bg thread)
-(void)go;

//switch between flat/tree view
-(IBAction)switchView:(id)sender;

//init tracking areas for buttons
// ->provide mouse over effects
-(void)initTrackingAreas;

//action for 'refresh' button
// ->query OS to refresh/reload all tasks
-(IBAction)refreshTasks:(id)sender;

//button handler for logo
-(IBAction)logoButtonHandler:(id)sender;

//action for 'about' in menu/logo in UI
-(IBAction)about:(id)sender;

//reload (to re-draw) a specific row in table
-(void)reloadRow:(id)item;

//reload task table
-(void)reloadTaskTable;

//smartly, reload bottom pane
// ->checks if task & item type (e.g. files) are both selected
-(void)reloadBottomPane:(Task*)task itemView:(NSUInteger)itemView;

//begin task enumeration
-(void)exploreTasks;

//sort tasks
// ->either name (flat view) or pid (tree view)
-(void)sortTasksForView:(OrderedDictionary*)tasks;

//VT callback to reload a binary
-(void)reloadBinary:(Binary*)binary;

//save button handler
-(IBAction)saveResults:(id)sender;

//search button handler
-(IBAction)search:(id)sender;

//constrain subview to parent view
-(void)constrainView:(NSView*)containerView subView:(NSView*)subView;

//display (in separate popup) all flagged items
-(IBAction)showFlaggedItems:(id)sender;

//callback for custom search fields
// ->handle auto-complete filterings
-(void)filterAutoComplete:(NSTextView*)textField;

//show the filter overlay, etc
-(void)prepUIForFiltering:(NSUInteger)pane;

//hide overlay, etc
-(void)unprepUIForFiltering;

//code to complete filtering/search
// ->reload table/scroll to top etc
-(void)finalizeFiltration:(NSUInteger)pane;

@end
