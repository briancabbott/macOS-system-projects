//
//  XServer.m
//
//  This class handles the interaction between the Cocoa front-end
//  and the Darwin X server thread.
//
//  Created by Andreas Monitzer on January 6, 2001.
//
/*
 * Copyright (c) 2001 Andreas Monitzer. All Rights Reserved.
 * Copyright (c) 2002-2003 Torrey T. Lyons. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz/XServer.m,v 1.8 2003/01/23 00:34:26 torrey Exp $ */

#include "quartzCommon.h"

#define BOOL xBOOL
#include "X.h"
#include "Xproto.h"
#include "os.h"
#include "darwin.h"
#undef BOOL

#import "XServer.h"
#import "Preferences.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/syslimits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <signal.h>
#include <fcntl.h>

// For power management notifications
#import <mach/mach_port.h>
#import <mach/mach_interface.h>
#import <mach/mach_init.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <IOKit/IOMessage.h>

#define ENQUEUE(xe)                                                         \
{                                                                           \
    char byte = 0;                                                          \
    DarwinEQEnqueue(xe);                                                    \
    /* signal there is an event ready to handle */                          \
    write(eventWriteFD, &byte, 1);                                          \
}

// Types of shells
enum {
    shell_Unknown,
    shell_Bourne,
    shell_C
};

typedef struct {
    char *name;
    int type;
} shellList_t;

static shellList_t const shellList[] = {
    { "csh",    shell_C },		// standard C shell
    { "tcsh",   shell_C },		// ... needs no introduction
    { "sh",     shell_Bourne },		// standard Bourne shell
    { "zsh",    shell_Bourne },		// Z shell
    { "bash",   shell_Bourne },		// GNU Bourne again shell
    { NULL,	shell_Unknown }
};

extern int argcGlobal;
extern char **argvGlobal;
extern char **envpGlobal;
extern int main(int argc, char *argv[], char *envp[]);
extern void HideMenuBar(void);
extern void ShowMenuBar(void);
extern void QuartzReallySetCursor();
static void childDone(int sig);
static void powerDidChange(void *x, io_service_t y, natural_t messageType,
                           void *messageArgument);

static NSPort *signalPort;
static NSPort *returnPort;
static NSPortMessage *signalMessage;
static pid_t clientPID;
static XServer *oneXServer;
static NSRect aquaMenuBarBox;
static io_connect_t root_port;


@implementation XServer

- (id)init
{
    self = [super init];
    oneXServer = self;

    serverState = server_NotStarted;
    serverLock = [[NSRecursiveLock alloc] init];
    clientPID = 0;
    sendServerEvents = NO;
    serverVisible = NO;
    rootlessMenuBarVisible = YES;
    queueShowServer = YES;
    quartzServerQuitting = NO;
    mouseState = 0;
    eventWriteFD = quartzEventWriteFD;
    windowClass = [NSWindow class];

    // set up a port to safely send messages to main thread from server thread
    signalPort = [[NSPort port] retain];
    returnPort = [[NSPort port] retain];
    signalMessage = [[NSPortMessage alloc] initWithSendPort:signalPort
                    receivePort:returnPort components:nil];

    // set up receiving end
    [signalPort setDelegate:self];
    [[NSRunLoop currentRunLoop] addPort:signalPort
                                forMode:NSDefaultRunLoopMode];
    [[NSRunLoop currentRunLoop] addPort:signalPort
                                forMode:NSModalPanelRunLoopMode];

    return self;
}

- (NSApplicationTerminateReply)
        applicationShouldTerminate:(NSApplication *)sender
{
    // Quit if the X server is not running
    if ([serverLock tryLock]) {
        quartzServerQuitting = YES;
        serverState = server_Done;
        if (clientPID != 0)
            kill(clientPID, SIGINT);
        return NSTerminateNow;
    }

    // Hide the X server and stop sending it events
    [self showServer:NO];
    sendServerEvents = NO;

    if (clientPID != 0 || !quartzStartClients) {
        int but;

        but = NSRunAlertPanel(NSLocalizedString(@"Quit X server?",@""),
                              NSLocalizedString(@"Quitting the X server will terminate any running X Window System programs.",@""),
                              NSLocalizedString(@"Quit",@""),
                              NSLocalizedString(@"Cancel",@""),
                              nil);

        switch (but) {
            case NSAlertDefaultReturn:		// quit
                break;
            case NSAlertAlternateReturn:	// cancel
                if (serverState == server_Running)
                    sendServerEvents = YES;
                return NSTerminateCancel;
        }
    }

    quartzServerQuitting = YES;
    if (clientPID != 0)
        kill(clientPID, SIGINT);

    // At this point the X server is either running or starting.
    if (serverState == server_Starting) {
        // Quit will be queued later when server is running
        return NSTerminateLater;
    } else if (serverState == server_Running) {
        [self quitServer];
    }

    return NSTerminateNow;
}

// Ensure that everything has quit cleanly
- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    // Make sure the client process has finished
    if (clientPID != 0) {
        NSLog(@"Waiting on client process...");
        sleep(2);

        // If the client process hasn't finished yet, kill it off
        if (clientPID != 0) {
            int clientStatus;
            NSLog(@"Killing client process...");
            killpg(clientPID, SIGKILL);
            waitpid(clientPID, &clientStatus, 0);
        }
    }

    // Wait until the X server thread quits
    [serverLock lock];
}

// returns YES when event was handled
- (BOOL)translateEvent:(NSEvent *)anEvent
{
    xEvent xe;
    static BOOL mouse1Pressed = NO;
    NSEventType type;
    unsigned int flags;

    if (!sendServerEvents) {
        return NO;
    }

    type  = [anEvent type];
    flags = [anEvent modifierFlags];

    if (!quartzRootless) {
        // Check for switch keypress
        if ((type == NSKeyDown) && (![anEvent isARepeat]) &&
            ([anEvent keyCode] == [Preferences keyCode]))
        {
            unsigned int switchFlags = [Preferences modifiers];

            // Switch if all the switch modifiers are pressed, while none are
            // pressed that should not be, except for caps lock.
            if (((flags & switchFlags) == switchFlags) &&
                ((flags & ~(switchFlags | NSAlphaShiftKeyMask)) == 0))
            {
                [self toggle];
                return YES;
            }
        }

        if (!serverVisible)
            return NO;
    }

    memset(&xe, 0, sizeof(xe));

    switch (type) {
        case NSLeftMouseUp:
            [self getMousePosition:&xe fromEvent:anEvent];
            if (quartzRootless && !mouse1Pressed) {
                // MouseUp after MouseDown in menu - ignore
                return NO;
            }
            mouse1Pressed = NO;
            xe.u.u.type = ButtonRelease;
            xe.u.u.detail = 1;
            break;
        case NSLeftMouseDown:
            [self getMousePosition:&xe fromEvent:anEvent];
            if (quartzRootless &&
                ! ([anEvent window] &&
                   [[anEvent window] isKindOfClass:windowClass])) {
                // Click in non X window - ignore
                return NO;
            }
            mouse1Pressed = YES;
            xe.u.u.type = ButtonPress;
            xe.u.u.detail = 1;
            break;
        case NSMouseMoved:
        case NSLeftMouseDragged:
        case NSRightMouseDragged:
        case NSOtherMouseDragged:
            [self getMousePosition:&xe fromEvent:anEvent];
            xe.u.u.type = MotionNotify;
            break;
        case NSSystemDefined:
        {
            long hwButtons = [anEvent data2];

            if (![anEvent subtype]==7)
                return NO; // we only use multibutton mouse events
            if (mouseState == hwButtons)
                return NO; // ignore double events
            mouseState = hwButtons;

            [self getMousePosition:&xe fromEvent:anEvent];
            xe.u.u.type = kXDarwinUpdateButtons;
            xe.u.clientMessage.u.l.longs0 = [anEvent data1];
            xe.u.clientMessage.u.l.longs1 =[anEvent data2];
            break;
        }
        case NSScrollWheel:
            [self getMousePosition:&xe fromEvent:anEvent];
            xe.u.u.type = kXDarwinScrollWheel;
            xe.u.clientMessage.u.s.shorts0 = [anEvent deltaY];
            break;
        case NSKeyDown:
        case NSKeyUp:
            // If the mouse is not on the valid X display area,
            // we don't send the X server key events.
            if (![self getMousePosition:&xe fromEvent:nil])
                return NO;
            if (type == NSKeyDown)
                xe.u.u.type = KeyPress;
            else
                xe.u.u.type = KeyRelease;
            xe.u.u.detail = [anEvent keyCode];
            break;
        case NSFlagsChanged:
            [self getMousePosition:&xe fromEvent:nil];
            xe.u.u.type = kXDarwinUpdateModifiers;
            xe.u.clientMessage.u.l.longs0 = flags;
            break;
        case NSOtherMouseDown:          // undocumented MouseDown
        case NSOtherMouseUp:            // undocumented MouseUp
            // Hide these from AppKit to avoid its log messages
            return YES;
        default:
            return NO;
    }

    [self sendXEvent:&xe];

    // Rootless: Send first NSLeftMouseDown to windows and views so window
    // ordering can be suppressed.
    // Don't pass further events - they (incorrectly?) bring the window
    // forward no matter what.
    if (quartzRootless  &&
        (type == NSLeftMouseDown || type == NSLeftMouseUp) &&
        [anEvent clickCount] == 1 &&
        [[anEvent window] isKindOfClass:windowClass])
    {
        return NO;
    }

    return YES;
}

// Return mouse coordinates, inverting y coordinate.
// The coordinates are extracted from an event or the current mouse position.
// For rootless mode, the menu bar is treated as not part of the usable
// X display area and the cursor position is adjusted accordingly.
// Returns YES if the cursor is not in the menu bar.
- (BOOL)getMousePosition:(xEvent *)xe fromEvent:(NSEvent *)anEvent
{
    NSPoint pt;

    if (anEvent) {
        NSWindow *eventWindow = [anEvent window];

        if (eventWindow) {
            pt = [anEvent locationInWindow];
            pt.x += [eventWindow frame].origin.x;
            pt.y += [eventWindow frame].origin.y;
        } else {
            pt = [NSEvent mouseLocation];
        }
    } else {
        pt = [NSEvent mouseLocation];
    }

    xe->u.keyButtonPointer.rootX = (int)(pt.x);

    if (quartzRootless && NSMouseInRect(pt, aquaMenuBarBox, NO)) {
        // mouse in menu bar - tell X11 that it's just below instead
        xe->u.keyButtonPointer.rootY = aquaMenuBarHeight;
        return NO;
    } else {
        xe->u.keyButtonPointer.rootY =
            NSHeight([[NSScreen mainScreen] frame]) - (int)(pt.y);
        return YES;
    }
}

// Append a string to the given enviroment variable
+ (void)append:(NSString*)value toEnv:(NSString*)name
{
    setenv([name cString],
        [[[NSString stringWithCString:getenv([name cString])]
            stringByAppendingString:value] cString],1);
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Block SIGPIPE
    // SIGPIPE repeatably killed the (rootless) server when closing a
    // dozen xterms in rapid succession. Those SIGPIPEs should have been
    // sent to the X server thread, which ignores them, but somehow they
    // ended up in this thread instead.
    {
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGPIPE);
        // pthread_sigmask not implemented yet
        // pthread_sigmask(SIG_BLOCK, &set, NULL);
        sigprocmask(SIG_BLOCK, &set, NULL);
    }

    if (quartzRootless == -1) {
        // The display mode was not set from the command line.
        // Show mode pick panel?
        if ([Preferences modeWindow]) {
            if ([Preferences rootless])
                [startRootlessButton setKeyEquivalent:@"\r"];
            else
                [startFullScreenButton setKeyEquivalent:@"\r"];
            [modeWindow makeKeyAndOrderFront:nil];
        } else {
            // Otherwise use default mode
            quartzRootless = [Preferences rootless];
            [self startX];
        }
    } else {
        [self startX];
    }
}

// Start the X server thread and the client process
- (void)startX
{
    NSDictionary *appDictionary;
    NSString *appVersion;

    [modeWindow close];

    // Calculate the height of the menu bar so rootless mode can avoid it
    if (quartzRootless) {
        aquaMenuBarHeight = NSHeight([[NSScreen mainScreen] frame]) -
                            NSMaxY([[NSScreen mainScreen] visibleFrame]) - 1;
        aquaMenuBarBox =
            NSMakeRect(0, NSMaxY([[NSScreen mainScreen] visibleFrame]) + 1,
                       NSWidth([[NSScreen mainScreen] frame]),
                       aquaMenuBarHeight);
    }

    // Write the XDarwin version to the console log
    appDictionary = [[NSBundle mainBundle] infoDictionary];
    appVersion = [appDictionary objectForKey:@"CFBundleShortVersionString"];
    if (appVersion)
        NSLog(@"\n%@", appVersion);
    else
        NSLog(@"No version");

    // Start the X server thread
    serverState = server_Starting;
    [NSThread detachNewThreadSelector:@selector(run) toTarget:self
              withObject:nil];

    // Start the X clients if started from GUI
    if (quartzStartClients) {
        [self startXClients];
    }

    if (quartzRootless) {
        // There is no help window for rootless; just start
        [helpWindow close];
        helpWindow = nil;
    } else {
        IONotificationPortRef notify;
        io_object_t anIterator;

        // Register for system power notifications
        root_port = IORegisterForSystemPower(0, &notify, powerDidChange,
                                             &anIterator);
        if (root_port) {
            CFRunLoopAddSource([[NSRunLoop currentRunLoop] getCFRunLoop],
                               IONotificationPortGetRunLoopSource(notify),
                               kCFRunLoopDefaultMode);
        } else {
            NSLog(@"Failed to register for system power notifications.");
        }
        
        // Show the X switch window if not using dock icon switching
        if (![Preferences dockSwitch])
            [switchWindow orderFront:nil];

        if ([Preferences startupHelp]) {
            // display the full screen mode help
            [helpWindow makeKeyAndOrderFront:nil];
            queueShowServer = NO;
        } else {
            // start running full screen and make sure X is visible
            ShowMenuBar();
            [self closeHelpAndShow:nil];
        }
    }
}

// Finish starting the X server thread
// This includes anything that must be done after the X server is
// ready to process events.
- (void)finishStartX
{
    sendServerEvents = YES;
    serverState = server_Running;

    if (quartzRootless) {
        [self forceShowServer:[NSApp isActive]];
    } else {
        [self forceShowServer:queueShowServer];
    }

    if (quartzServerQuitting) {
        [self quitServer];
        [NSApp replyToApplicationShouldTerminate:YES];
    }
}

// Start the first X clients in a separate process
- (BOOL)startXClients
{
    struct passwd *passwdUser;
    NSString *shellPath, *dashShellName, *commandStr, *startXPath;
    NSMutableString *safeStartXPath;
    NSRange aRange;
    NSBundle *thisBundle;
    const char *shellPathStr, *newargv[3], *shellNameStr;
    int fd[2], outFD, length, shellType, i;

    // Register to catch the signal when the client processs finishes
    signal(SIGCHLD, childDone);

    // Get user's password database entry
    passwdUser = getpwuid(getuid());

    // Find the shell to use
    if ([Preferences useDefaultShell])
        shellPath = [NSString stringWithCString:passwdUser->pw_shell];
    else
        shellPath = [Preferences shellString];

    dashShellName = [NSString stringWithFormat:@"-%@",
                            [shellPath lastPathComponent]];
    shellPathStr = [shellPath cString];
    shellNameStr = [[shellPath lastPathComponent] cString];

    if (access(shellPathStr, X_OK)) {
        NSLog(@"Shell %s is not valid!", shellPathStr);
        return NO;
    }

    // Find the type of shell
    for (i = 0; shellList[i].name; i++) {
        if (!strcmp(shellNameStr, shellList[i].name))
            break;
    }
    shellType = shellList[i].type;

    newargv[0] = [dashShellName cString];
    if (shellType == shell_Bourne) {
        // Bourne shells need to be told they are interactive to make
        // sure they read all their initialization files.
        newargv[1] = "-i";
        newargv[2] = NULL;
    } else {
        newargv[1] = NULL;
    }

    // Create a pipe to communicate with the X client process
    NSAssert(pipe(fd) == 0, @"Could not create new pipe.");

    // Open a file descriptor for writing to stdout and stderr
    outFD = open("/dev/console", O_WRONLY, 0);
    if (outFD == -1) {
        outFD = open("/dev/null", O_WRONLY, 0);
        NSAssert(outFD != -1, @"Could not open shell output.");
    }

    // Fork process to start X clients in user's default shell
    // Sadly we can't use NSTask because we need to start a login shell.
    // Login shells are started by passing "-" as the first character of
    // argument 0. NSTask forces argument 0 to be the shell's name.
    clientPID = vfork();
    if (clientPID == 0) {

        // Inside the new process:
        if (fd[0] != STDIN_FILENO) {
            dup2(fd[0], STDIN_FILENO);	// Take stdin from pipe
            close(fd[0]);
        }
        close(fd[1]);			// Close write end of pipe
        if (outFD == STDOUT_FILENO) {	// Setup stdout and stderr
            dup2(outFD, STDERR_FILENO);
        } else if (outFD == STDERR_FILENO) {
            dup2(outFD, STDOUT_FILENO);
        } else {
            dup2(outFD, STDERR_FILENO);
            dup2(outFD, STDOUT_FILENO);
            close(outFD);
        }

        // Setup environment
        setenv("HOME", passwdUser->pw_dir, 1);
        setenv("SHELL", shellPathStr, 1);
        setenv("LOGNAME", passwdUser->pw_name, 1);
        setenv("USER", passwdUser->pw_name, 1);
        setenv("TERM", "unknown", 1);
        if (chdir(passwdUser->pw_dir))	// Change to user's home dir
            NSLog(@"Could not change to user's home directory.");

        execv(shellPathStr, (char * const *)newargv);	// Start user's shell

        NSLog(@"Could not start X client process with errno = %i.", errno);
        _exit(127);
    }

    // In parent process:
    close(fd[0]);	// Close read end of pipe
    close(outFD);	// Close output file descriptor

    thisBundle = [NSBundle bundleForClass:[self class]];
    startXPath = [thisBundle pathForResource:@"startXClients" ofType:nil];
    if (!startXPath) {
        NSLog(@"Could not find startXClients in application bundle!");
        return NO;
    }

    // We will run the startXClients script with the path in single quotes
    // in case there are problematic characters in the path. We still have
    // to worry about there being single quotes in the path. So, replace
    // all instances of the ' character in startXPath with '\''.
    safeStartXPath = [NSMutableString stringWithString:startXPath];
    aRange = NSMakeRange(0, [safeStartXPath length]);
    while (aRange.length) {
        aRange = [safeStartXPath rangeOfString:@"'" options:0 range:aRange];
        if (!aRange.length)
            break;
        [safeStartXPath replaceCharactersInRange:aRange
                        withString:@"\'\\'\'"];
        aRange.location += 4;
        aRange.length = [safeStartXPath length] - aRange.location;
    }

    if ([Preferences addToPath]) {
        commandStr = [NSString stringWithFormat:@"'%@' :%d %@\n",
                        safeStartXPath, [Preferences display],
                        [Preferences addToPathString]];
    } else {
        commandStr = [NSString stringWithFormat:@"'%@' :%d\n",
                        safeStartXPath, [Preferences display]];
    }

    length = [commandStr cStringLength];
    if (write(fd[1], [commandStr cString], length) != length) {
        NSLog(@"Write to X client process failed.");
        return NO;
    }

    // Close the pipe so that shell will terminate when xinit quits
    close(fd[1]);

    return YES;
}

// Run the X server thread
- (void)run
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    [serverLock lock];
    main(argcGlobal, argvGlobal, envpGlobal);
    serverVisible = NO;
    [pool release];
    [serverLock unlock];
    QuartzMessageMainThread(kQuartzServerDied, nil, 0);
}

// Full screen mode was picked in the mode pick panel
- (IBAction)startFullScreen:(id)sender
{
    [Preferences setModeWindow:[startupModeButton intValue]];
    [Preferences saveToDisk];
    quartzRootless = FALSE;
    [self startX];
}

// Rootless mode was picked in the mode pick panel
- (IBAction)startRootless:(id)sender
{
    [Preferences setModeWindow:[startupModeButton intValue]];
    [Preferences saveToDisk];
    quartzRootless = TRUE;
    [self startX];
}

// Close the help splash screen and show the X server
- (IBAction)closeHelpAndShow:(id)sender
{
    if (sender) {
        int helpVal = [startupHelpButton intValue];
        [Preferences setStartupHelp:helpVal];
        [Preferences saveToDisk];
    }
    [helpWindow close];
    helpWindow = nil;

    [self forceShowServer:YES];
    [NSApp activateIgnoringOtherApps:YES];
}

// Show the X server when sent message from GUI
- (IBAction)showAction:(id)sender
{
    [self forceShowServer:YES];
}

// Show or hide the X server or menu bar in rootless mode
- (void)toggle
{
    if (quartzRootless) {
#if 0
        // FIXME: Remove or add option to not dodge menubar
        if (rootlessMenuBarVisible)
            HideMenuBar();
        else
            ShowMenuBar();
        rootlessMenuBarVisible = !rootlessMenuBarVisible;
#endif
    } else {
        [self showServer:!serverVisible];
    }
}

// Show or hide the X server on screen
- (void)showServer:(BOOL)show
{
    // Do not show or hide multiple times in a row
    if (serverVisible == show)
        return;

    if (sendServerEvents) {
        [self sendShowHide:show];
    } else if (serverState == server_Starting) {
        queueShowServer = show;
    }
}

// Show or hide the X server irregardless of the current state
- (void)forceShowServer:(BOOL)show
{
    serverVisible = !show;
    [self showServer:show];
}

// Tell the X server to show or hide itself.
// This ignores the current X server visible state.
//
// In full screen mode, the order we do things is important and must be
// preserved between the threads. X drawing operations have to be performed
// in the X server thread. It appears that we have the additional
// constraint that we must hide and show the menu bar in the main thread.
//
// To show the X server:
//   1. Capture the displays. (Main thread)
//   2. Hide the menu bar. (Must be in main thread)
//   3. Send event to X server thread to redraw X screen.
//   4. Redraw the X screen. (Must be in X server thread)
//
// To hide the X server:
//   1. Send event to X server thread to stop drawing.
//   2. Stop drawing to the X screen. (Must be in X server thread)
//   3. Message main thread that drawing is stopped.
//   4. If main thread still wants X server hidden:
//     a. Release the displays. (Main thread)
//     b. Unhide the menu bar. (Must be in main thread)
//   Otherwise we have already queued an event to start drawing again.
//
- (void)sendShowHide:(BOOL)show
{
    xEvent xe;

    [self getMousePosition:&xe fromEvent:nil];

    if (show) {
        if (!quartzRootless) {
            QuartzFSCapture();
            HideMenuBar();
        }
        xe.u.u.type = kXDarwinShow;
        [self sendXEvent:&xe];

        // the mouse location will have moved; track it
        xe.u.u.type = MotionNotify;
        [self sendXEvent:&xe];

        // inform the X server of the current modifier state
        xe.u.u.type = kXDarwinUpdateModifiers;
        xe.u.clientMessage.u.l.longs0 = [[NSApp currentEvent] modifierFlags];
        [self sendXEvent:&xe];

        // put the pasteboard into the X cut buffer
        [self readPasteboard];
    } else {
        // put the X cut buffer on the pasteboard
        [self writePasteboard];

        xe.u.u.type = kXDarwinHide;
        [self sendXEvent:&xe];
    }

    serverVisible = show;
}

// Enable or disable rendering to the X screen
- (void)setRootClip:(BOOL)enable
{
    xEvent xe;

    xe.u.u.type = kXDarwinSetRootClip;
    xe.u.clientMessage.u.l.longs0 = enable;
    [self sendXEvent:&xe];
}

// Tell the X server to read from the pasteboard into the X cut buffer
- (void)readPasteboard
{
    xEvent xe;

    xe.u.u.type = kXDarwinReadPasteboard;
    [self sendXEvent:&xe];
}

// Tell the X server to write the X cut buffer into the pasteboard
- (void)writePasteboard
{
    xEvent xe;

    xe.u.u.type = kXDarwinWritePasteboard;
    [self sendXEvent:&xe];
}

- (void)quitServer
{
    xEvent xe;

    xe.u.u.type = kXDarwinQuit;
    [self sendXEvent:&xe];

    // Revert to the Mac OS X arrow cursor. The main thread sets the cursor
    // and it won't be responding to future requests to change it.
    [[NSCursor arrowCursor] set];

    serverState = server_Quitting;
}

- (void)sendXEvent:(xEvent *)xe
{
    // This field should be filled in for every event
    xe->u.keyButtonPointer.time = GetTimeInMillis();

#if 0
    // FIXME: Really?
    if (quartzRootless  &&
        (ev->type == NSLeftMouseDown  ||  ev->type == NSLeftMouseUp  ||
        (ev->type == NSSystemDefined && ev->data.compound.subType == 7)))
    {
        // mouse button event - send mouseMoved to this position too
        // X gets confused if it gets a click that isn't at the last
        // reported mouse position.
        xEvent moveEvent = *ev;
        xe.u.u.type = NSMouseMoved;
        [self sendXEvent:&moveEvent];
    }
#endif

    ENQUEUE(xe);
}

// Handle messages from the X server thread
- (void)handlePortMessage:(NSPortMessage *)portMessage
{
    unsigned msg = [portMessage msgid];

    switch(msg) {
        case kQuartzServerHidden:
            // Make sure the X server wasn't queued to be shown again while
            // the hide was pending.
            if (!quartzRootless && !serverVisible) {
                QuartzFSRelease();
                ShowMenuBar();
            }
            break;

        case kQuartzServerStarted:
            [self finishStartX];
            break;

        case kQuartzServerDied:
            sendServerEvents = NO;
            serverState = server_Done;
            if (!quartzServerQuitting) {
                [NSApp terminate:nil];	// quit if we aren't already
            }
            break;

        case kQuartzCursorUpdate:
            QuartzReallySetCursor();
            break;

        case kQuartzPostEvent:
        {
            const xEvent *xe = [[[portMessage components] lastObject] bytes];
            ENQUEUE(xe);
            break;
        }

        default:
            NSLog(@"Unknown message from server thread.");
    }
}

// Quit the X server when the X client process finishes
- (void)clientProcessDone:(int)clientStatus
{
    if (WIFEXITED(clientStatus)) {
        int exitStatus = WEXITSTATUS(clientStatus);
        if (exitStatus != 0)
            NSLog(@"X client process terminated with status %i.", exitStatus);
    } else {
        NSLog(@"X client process terminated abnormally.");
    }

    if (!quartzServerQuitting) {
        [NSApp terminate:nil];	// quit if we aren't already
    }
}

// Called when the user clicks the application icon,
// but not when Cmd-Tab is used.
// Rootless: Don't switch until applicationWillBecomeActive.
- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication
            hasVisibleWindows:(BOOL)flag
{
    if ([Preferences dockSwitch] && !quartzRootless) {
        [self showServer:YES];
    }
    return NO;
}

- (void)applicationWillResignActive:(NSNotification *)aNotification
{
    [self showServer:NO];
}

- (void)applicationWillBecomeActive:(NSNotification *)aNotification
{
    if (quartzRootless)
        [self showServer:YES];
}

@end


// Send a message to the main thread, which calls handlePortMessage in
// response. Must only be called from the X server thread because
// NSPort is not thread safe.
void QuartzMessageMainThread(unsigned msg, void *data, unsigned length)
{
    if (msg == kQuartzPostEvent) {
        NSData *eventData = [NSData dataWithBytes:data length:length];
        NSArray *eventArray = [NSArray arrayWithObject:eventData];
        NSPortMessage *newMessage =
                [[NSPortMessage alloc]
                        initWithSendPort:signalPort
                        receivePort:returnPort components:eventArray];
        [newMessage setMsgid:msg];
        [newMessage sendBeforeDate:[NSDate distantPast]];
        [newMessage release];
    } else {
        [signalMessage setMsgid:msg];
        [signalMessage sendBeforeDate:[NSDate distantPast]];
    }
}

// Handle SIGCHLD signals
static void childDone(int sig)
{
    int clientStatus;

    if (clientPID == 0)
        return;

    // Make sure it was the client task that finished
    if (waitpid(clientPID, &clientStatus, WNOHANG) == clientPID) {
        if (WIFSTOPPED(clientStatus))
            return;
        clientPID = 0;
        [oneXServer clientProcessDone:clientStatus];
    }
}

static void powerDidChange(
    void *x,
    io_service_t y,
    natural_t messageType,
    void *messageArgument)
{
    switch (messageType) {
        case kIOMessageSystemWillSleep:
            if (!quartzRootless) {
                [oneXServer setRootClip:FALSE];
            }
            IOAllowPowerChange(root_port, (long)messageArgument);
            break;
        case kIOMessageCanSystemSleep:
            IOAllowPowerChange(root_port, (long)messageArgument);
            break;
        case kIOMessageSystemHasPoweredOn:
            if (!quartzRootless) {
                [oneXServer setRootClip:TRUE];
            }
            break;
    }
    
}
