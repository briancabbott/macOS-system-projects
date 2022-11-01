/*
 * KerberosErrorAlert.m
 *
 * $Header$
 *
 * Copyright 2004 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#import "KerberosErrorAlert.h"
#include <Kerberos/Kerberos.h>

#define ErrorAlertString(key) NSLocalizedStringFromTable (key, @"KerberosErrorAlert", NULL)

@implementation KerberosErrorAlert

static KLBoolean gFloatAtSpecialLevel = FALSE;
static NSInteger gFloatLevel = 0;

// ---------------------------------------------------------------------------

+ (void) setFloatAtLevel: (NSInteger) level
{
    gFloatAtSpecialLevel = TRUE;
    gFloatLevel = level;
}



// ---------------------------------------------------------------------------

+ (void) alertForError: (int) error
                action: (KerberosAction) action
{
    KerberosErrorAlert *alert = [[KerberosErrorAlert alloc] init];
    
    [alert setAlertStyle: NSWarningAlertStyle];
    [alert addButtonWithTitle: ErrorAlertString (@"ErrorAlertStringOK")];
    [alert setMessageText: [alert messageTextForAction: action]];
    [alert setInformativeText: [alert informativeTextForErrorCode: error]];
    
    [alert runModal];
    
    [alert release];
}

// ---------------------------------------------------------------------------

+ (void) alertForError: (int) error
                action: (KerberosAction) action
        modalForWindow: (NSWindow *) parentWindow
{
    KerberosErrorAlert *alert = [[KerberosErrorAlert alloc] init];

    [alert setAlertStyle: NSWarningAlertStyle];
    [alert addButtonWithTitle: ErrorAlertString (@"ErrorAlertStringOK")];
    [alert setMessageText: [alert messageTextForAction: action]];
    [alert setInformativeText: [alert informativeTextForErrorCode: error]];
    
    [alert beginSheetModalForWindow: parentWindow 
                      modalDelegate: alert 
                     didEndSelector: @selector (errorSheetDidEnd:returnCode:contextInfo:) 
                        contextInfo: NULL];
}

// ---------------------------------------------------------------------------

+ (void) alertForError: (int) error 
                action: (KerberosAction) action 
                sender: (id) sender;
{
    NSWindow *parentWindow = NULL;
    
    // Attempt to dynamically figure out the window of the sender:
    if ([sender isKindOfClass: [NSWindowController class]]) {
        parentWindow = [sender window];
    } else if ([sender isKindOfClass: [NSWindow class]]) {
        parentWindow = sender;
    }
    
    if (parentWindow) {
        [KerberosErrorAlert alertForError: error 
                                   action: action 
                           modalForWindow: parentWindow];
    } else {
        [KerberosErrorAlert alertForError: error 
                                   action: action];
    }
}

#pragma mark -

// ---------------------------------------------------------------------------

+ (BOOL) alertForYNQuestion: (NSString *) question
                     action: (KerberosAction) action
{
    KerberosErrorAlert *alert = [[KerberosErrorAlert alloc] init];
    
    [alert setAlertStyle: NSWarningAlertStyle];
    [alert addButtonWithTitle: ErrorAlertString (@"ErrorAlertStringYes")];
    [alert addButtonWithTitle: ErrorAlertString (@"ErrorAlertStringNo")];
    [alert setMessageText: [alert messageTextForAction: action]];
    [alert setInformativeText: question];
    
    int returnCode = [alert runModal];
    
    [alert release];
    
    return (returnCode == NSAlertFirstButtonReturn) ? YES : NO;
}

// ---------------------------------------------------------------------------

+ (BOOL) alertForYNQuestion: (NSString *) question
                     action: (KerberosAction) action 
             modalForWindow: (NSWindow *) parentWindow
{
    KerberosErrorAlert *alert = [[KerberosErrorAlert alloc] init];
    
    [alert setAlertStyle: NSWarningAlertStyle];
    [alert addButtonWithTitle: ErrorAlertString (@"ErrorAlertStringYes")];
    [alert addButtonWithTitle: ErrorAlertString (@"ErrorAlertStringNo")];
    [alert setMessageText: [alert messageTextForAction: action]];
    [alert setInformativeText: question];
    
    [alert beginSheetModalForWindow: parentWindow 
                      modalDelegate: alert 
                     didEndSelector: @selector (errorSheetDidEnd:returnCode:contextInfo:) 
                        contextInfo: NULL];
    int returnCode = [NSApp runModalForWindow: [alert window]];
    
    return (returnCode == NSAlertFirstButtonReturn) ? YES : NO;
}

// ---------------------------------------------------------------------------

+ (BOOL) alertForYNQuestion: (NSString *) question
                     action: (KerberosAction) action 
                     sender: (id) sender
{
    NSWindow *parentWindow = NULL;
    
    // Attempt to dynamically figure out the window of the sender:
    if ([sender isKindOfClass: [NSWindowController class]]) {
        parentWindow = [sender window];
    } else if ([sender isKindOfClass: [NSWindow class]]) {
        parentWindow = sender;
    }
    
    if (parentWindow) {
        return [KerberosErrorAlert alertForYNQuestion: question 
                                               action: action 
                                       modalForWindow: parentWindow];
    } else {
        return [KerberosErrorAlert alertForYNQuestion: question 
                                               action: action];
    }
}

#pragma mark -

// ---------------------------------------------------------------------------

+ (BOOL) alertForYNQuestion: (NSString *) question
{
    KerberosErrorAlert *alert = [[KerberosErrorAlert alloc] init];
    
    [alert setAlertStyle: NSWarningAlertStyle];
    [alert addButtonWithTitle: ErrorAlertString (@"ErrorAlertStringYes")];
    [alert addButtonWithTitle: ErrorAlertString (@"ErrorAlertStringNo")];
    [alert setMessageText: question];
    
    int returnCode = [alert runModal];
    
    [alert release];
    
    return (returnCode == NSAlertFirstButtonReturn) ? YES : NO;
}

// ---------------------------------------------------------------------------

+ (BOOL) alertForYNQuestion: (NSString *) question
             modalForWindow: (NSWindow *) parentWindow
{
    KerberosErrorAlert *alert = [[KerberosErrorAlert alloc] init];
    
    [alert setAlertStyle: NSWarningAlertStyle];
    [alert addButtonWithTitle: ErrorAlertString (@"ErrorAlertStringYes")];
    [alert addButtonWithTitle: ErrorAlertString (@"ErrorAlertStringNo")];
    [alert setMessageText: question];
    
    [alert beginSheetModalForWindow: parentWindow 
                      modalDelegate: alert 
                     didEndSelector: @selector (errorSheetDidEnd:returnCode:contextInfo:) 
                        contextInfo: NULL];
    int returnCode = [NSApp runModalForWindow: [alert window]];
        
    return (returnCode == NSAlertFirstButtonReturn) ? YES : NO;
}

// ---------------------------------------------------------------------------

+ (BOOL) alertForYNQuestion: (NSString *) question
                     sender: (id) sender
{
    NSWindow *parentWindow = NULL;
    
    // Attempt to dynamically figure out the window of the sender:
    if ([sender isKindOfClass: [NSWindowController class]]) {
        parentWindow = [sender window];
    } else if ([sender isKindOfClass: [NSWindow class]]) {
        parentWindow = sender;
    }
    
    if (parentWindow) {
        return [KerberosErrorAlert alertForYNQuestion: question 
                                       modalForWindow: parentWindow];
    } else {
        return [KerberosErrorAlert alertForYNQuestion: question];
    }
}

#pragma mark -

// ---------------------------------------------------------------------------

+ (void) alertForMessage: (NSString *) message
             description: (NSString *) description 
{
    KerberosErrorAlert *alert = [[KerberosErrorAlert alloc] init];
    
    [alert setAlertStyle: NSWarningAlertStyle];
    [alert addButtonWithTitle: ErrorAlertString (@"ErrorAlertStringOK")];
    [alert setMessageText: message];
    [alert setInformativeText: description];
    
    [alert runModal];
    
    [alert release];
}

// ---------------------------------------------------------------------------

+ (void) alertForMessage: (NSString *) message
             description: (NSString *) description 
             modalForWindow: (NSWindow *) parentWindow
{
    KerberosErrorAlert *alert = [[KerberosErrorAlert alloc] init];
    
    [alert setAlertStyle: NSWarningAlertStyle];
    [alert addButtonWithTitle: ErrorAlertString (@"ErrorAlertStringOK")];
    [alert setMessageText: message];
    [alert setInformativeText: description];
    
    [alert beginSheetModalForWindow: parentWindow 
                      modalDelegate: alert 
                     didEndSelector: @selector (errorSheetDidEnd:returnCode:contextInfo:) 
                        contextInfo: NULL];
}

// ---------------------------------------------------------------------------

+ (void) alertForMessage: (NSString *) message
             description: (NSString *) description 
                  sender: (id) sender
{
    NSWindow *parentWindow = NULL;
    
    // Attempt to dynamically figure out the window of the sender:
    if ([sender isKindOfClass: [NSWindowController class]]) {
        parentWindow = [sender window];
    } else if ([sender isKindOfClass: [NSWindow class]]) {
        parentWindow = sender;
    }
    
    if (parentWindow) {
        [KerberosErrorAlert alertForMessage: message 
                                description: description 
                             modalForWindow: parentWindow];
    } else {
        [KerberosErrorAlert alertForMessage: message 
                                description: description];
    }
}

#pragma mark -

// ---------------------------------------------------------------------------

- (NSInteger) runModal
{
    if (gFloatAtSpecialLevel) {
        [[self window] setLevel: gFloatLevel];    
    }
    return [super runModal];
}

// ---------------------------------------------------------------------------

- (void) beginSheetModalForWindow: (NSWindow *) window 
                    modalDelegate: (id) modalDelegate 
                   didEndSelector: (SEL) alertDidEndSelector 
                      contextInfo: (void *) contextInfo
{
    if (gFloatAtSpecialLevel) {
        [[self window] setLevel: gFloatLevel];    
    }
    [super beginSheetModalForWindow: window
                      modalDelegate: modalDelegate 
                     didEndSelector: alertDidEndSelector 
                        contextInfo: contextInfo];
}

// ---------------------------------------------------------------------------

- (NSString *) messageTextForAction: (KerberosAction) action
{
    NSString *key = NULL;
    
    switch (action) {
        case KerberosGetTicketsAction:
            key = @"ErrorAlertStringGetTicketsError";
            break;
        case KerberosRenewTicketsAction:
            key = @"ErrorAlertStringRenewTicketsError";
            break;
        case KerberosValidateTicketsAction:
            key = @"ErrorAlertStringValidateTicketsError";
            break;
        case KerberosDestroyTicketsAction:
            key = @"ErrorAlertStringDestroyTicketsError";
            break;
        case KerberosChangePasswordAction:
            key = @"ErrorAlertStringChangePasswordError";
            break;
        case KerberosChangeActiveUserAction:
            key = @"ErrorAlertStringChangeActiveUserError";
            break;
        default:
            key = @"ErrorAlertStringGenericError";
            break;
    }
    
    return ErrorAlertString (key);
}

// ---------------------------------------------------------------------------

- (NSString *) informativeTextForErrorCode: (int) error
{
    NSString *string = NULL;
    char *errorText;
    
    // Use a special error when the caps lock key is down and the password was incorrect
    // if the caps lock key is down accidentally, it will still be down now
    switch (error) {
        case KRB5KRB_AP_ERR_BAD_INTEGRITY:
        case KRB5KDC_ERR_PREAUTH_FAILED:
        case klBadPasswordErr: {
            unsigned int modifiers = [[NSApp currentEvent] modifierFlags];
            if (modifiers & NSAlphaShiftKeyMask) {
                error = klCapsLockErr;
            }
        }
    }

    if (KLGetErrorString (error, &errorText) == klNoErr) { 
        string = [NSString stringWithUTF8String: errorText]; 
        KLDisposeString (errorText);
    } else {
        NSString *format = ErrorAlertString (@"ErrorAlertStringUnknownErrorFormat");
        string = [NSString stringWithFormat: format, error];
    }
    
    return string;
}

// ---------------------------------------------------------------------------

- (void) errorSheetDidEnd: (NSAlert *) alert returnCode: (int) returnCode contextInfo: (void *) contextInfo 
{
    [NSApp endSheet: [alert window] returnCode: returnCode];
    [alert release];
}

@end
