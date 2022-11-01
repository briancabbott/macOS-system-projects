/*
 * FloatingWindow.m
 *
 * $Header$
 *
 * Copyright 2003 Massachusetts Institute of Technology.
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

// This class is used to make windows and panels that float at the screensaver level.
// We could just call setLevel: on a regular NSWindow, but runModalForWindow:
// would just set it back to the regular modal dialog window level before display.
// Hence, we cheat with this subclass.

#import "FloatingWindow.h"

// We need to float over the loginwindow and SecurityAgent so use its hardcoded level.
#define KLSSecurityAgentWindowLevel 2003

@implementation FloatingWindow

- (id) initWithContentRect: (NSRect) contentRect 
           styleMask: (unsigned int) styleMask 
           backing: (NSBackingStoreType) backingType 
           defer: (BOOL) flag
{
    id retval = [super initWithContentRect: contentRect styleMask: styleMask backing: backingType defer: flag];
    [self setLevel: KLSSecurityAgentWindowLevel];
    return retval;
}

- (void) setLevel: (int) newLevel
{
    if ([super level] != KLSSecurityAgentWindowLevel) {
        //dprintf ("%ld setting window level from %ld to %ld", 
        //    [self windowNumber], [super level], NSScreenSaverWindowLevel);
        [super setLevel: KLSSecurityAgentWindowLevel];
    }
}

@end
