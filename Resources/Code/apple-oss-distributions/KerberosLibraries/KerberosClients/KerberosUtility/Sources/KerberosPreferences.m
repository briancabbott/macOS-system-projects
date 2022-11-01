/*
 * KerberosPreferences.m
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

#import "KerberosPreferences.h"

@implementation KerberosPreferences

#pragma mark -- KerberosApp --

// ---------------------------------------------------------------------------

+ (KerberosPreferences *) sharedPreferences
{
    static KerberosPreferences *sSharedPreferences = NULL;
    
    if (!sSharedPreferences) {
        sSharedPreferences = [[KerberosPreferences alloc] init];
    }
    
    return sSharedPreferences;
}


// ---------------------------------------------------------------------------

- (BOOL) autoRenewTickets
{
    BOOL autoRenewTickets = YES;
    CFPropertyListRef value = CFPreferencesCopyAppValue (CFSTR("KAAutoRenewTickets"), 
                                                         kCFPreferencesCurrentApplication);
    if ((value) && (CFGetTypeID (value) == CFBooleanGetTypeID ())) {
        autoRenewTickets = CFBooleanGetValue (value);
    }
    if (value) { CFRelease (value); }
    return autoRenewTickets;
}

// ---------------------------------------------------------------------------

- (void) setAutoRenewTickets: (BOOL) autoRenewTickets
{
    CFBooleanRef value = autoRenewTickets ? kCFBooleanTrue : kCFBooleanFalse;
    CFPreferencesSetAppValue (CFSTR("KAAutoRenewTickets"), value, kCFPreferencesCurrentApplication);
    CFPreferencesAppSynchronize (kCFPreferencesCurrentApplication);
}

// ---------------------------------------------------------------------------

- (BOOL) showTimeInDockIcon
{
    BOOL showTimeInDockIcon = YES;
    CFPropertyListRef value = CFPreferencesCopyAppValue (CFSTR("KADisplayTimeInDock"), 
                                                         kCFPreferencesCurrentApplication);
    if (value && CFGetTypeID (value) == CFBooleanGetTypeID ()) {
        showTimeInDockIcon = CFBooleanGetValue (value);
    }
    if (value) { CFRelease (value); }
    return showTimeInDockIcon;
}

// ---------------------------------------------------------------------------

- (void) setShowTimeInDockIcon: (BOOL) showTimeInDockIcon
{
    CFBooleanRef value = showTimeInDockIcon ? kCFBooleanTrue : kCFBooleanFalse;
    CFPreferencesSetAppValue (CFSTR("KADisplayTimeInDock"), value, kCFPreferencesCurrentApplication);
    CFPreferencesAppSynchronize (kCFPreferencesCurrentApplication);
}

// ---------------------------------------------------------------------------

- (LaunchAction) launchAction
{
    LaunchAction launchAction = 1;
    CFPropertyListRef value = CFPreferencesCopyAppValue (CFSTR("KATicketListStartupDisplayOption"), 
                                                         kCFPreferencesCurrentApplication);
    if (value && CFGetTypeID (value) == CFNumberGetTypeID ()) {
        SInt32 number;
        if (CFNumberGetValue (value, kCFNumberSInt32Type, &number)) {
            launchAction = number;
        }
    }
    if (value) { CFRelease (value); }
    return launchAction;
}

// ---------------------------------------------------------------------------

- (void) setLaunchAction: (LaunchAction) launchAction
{
    SInt32 number = launchAction;
    CFNumberRef value = CFNumberCreate (kCFAllocatorDefault, kCFNumberSInt32Type, &number);
    if (value) {
        CFPreferencesSetAppValue (CFSTR("KATicketListStartupDisplayOption"), value, kCFPreferencesCurrentApplication);
        CFPreferencesAppSynchronize (kCFPreferencesCurrentApplication);        
    }
}

// ---------------------------------------------------------------------------

- (BOOL) ticketWindowLastOpen
{
    BOOL ticketWindowLastOpen = YES;
    CFPropertyListRef value = CFPreferencesCopyAppValue (CFSTR("KATicketListLastOpen"), 
                                                         kCFPreferencesCurrentApplication);
    if (value && CFGetTypeID (value) == CFBooleanGetTypeID ()) {
        ticketWindowLastOpen = CFBooleanGetValue (value);
    }
    if (value) { CFRelease (value); }
    return ticketWindowLastOpen;
}

// ---------------------------------------------------------------------------

- (void) setTicketWindowLastOpen: (BOOL) ticketWindowLastOpen
{
    CFBooleanRef value = ticketWindowLastOpen ? kCFBooleanTrue : kCFBooleanFalse;
    CFPreferencesSetAppValue (CFSTR("KATicketListLastOpen"), value, kCFPreferencesCurrentApplication);
    CFPreferencesAppSynchronize (kCFPreferencesCurrentApplication);
}

// ---------------------------------------------------------------------------

- (BOOL) ticketWindowDefaultPosition
{
    BOOL ticketWindowDefaultPosition = YES;
    CFPropertyListRef value = CFPreferencesCopyAppValue (CFSTR("KADefaultPositionAndSizeMainWindow"), 
                                                         kCFPreferencesCurrentApplication);
    if (value && CFGetTypeID (value) == CFBooleanGetTypeID ()) {
        ticketWindowDefaultPosition = CFBooleanGetValue (value);
    }
    if (value) { CFRelease (value); }
    return ticketWindowDefaultPosition;
}

// ---------------------------------------------------------------------------

- (void) setTicketWindowDefaultPosition: (BOOL) ticketWindowDefaultPosition
{
    CFBooleanRef value = ticketWindowDefaultPosition ? kCFBooleanTrue : kCFBooleanFalse;
    CFPreferencesSetAppValue (CFSTR("KADefaultPositionAndSizeMainWindow"), value, kCFPreferencesCurrentApplication);
    CFPreferencesAppSynchronize (kCFPreferencesCurrentApplication);
}

#pragma mark -- Default KerberosPrincipal --

// ---------------------------------------------------------------------------

- (BOOL) rememberPrincipalFromLastLogin
{
    KLBoolean value;
    KLSize size = sizeof (value);
    KLStatus err = KLGetDefaultLoginOption (loginOption_RememberPrincipal, &value, &size);

    if (err) {
        dprintf ("rememberPrincipalFromLastLogin failed with error %d (%s)", err, error_message (err));        
    }
    
    return !err ? (value != 0) : YES;
}

// ---------------------------------------------------------------------------

- (void) setRememberPrincipalFromLastLogin: (BOOL) rememberPrincipalFromLastLogin
{
    KLBoolean value = (rememberPrincipalFromLastLogin == YES);
    KLStatus err = KLSetDefaultLoginOption (loginOption_RememberPrincipal, &value, sizeof (value));
    if (err) {
        dprintf ("setRememberPrincipalFromLastLogin: failed with error %d (%s)", err, error_message (err));        
    }
}

// ---------------------------------------------------------------------------

- (NSString *) defaultName
{
    KLSize size = 0;
    char *value = NULL;
    NSString *string = @"";
    KLStatus err = KLGetDefaultLoginOption (loginOption_LoginName, NULL, &size);
    
    if (!err) {
        value = (char *) malloc (sizeof (char) * (size + 1));
        if (!value) { err = klMemFullErr; }
    }
    
    if (!err) {
        err = KLGetDefaultLoginOption (loginOption_LoginName, value, &size);
    }
    
    if (!err) {
        value[size] = '\0';  // nul-terminate the value buffer
        string = [NSString stringWithUTF8String: value];
    } else {
        dprintf ("defaultName failed with error %d (%s)", err, error_message (err));        
    }

    if (value) { free (value); }

    return string;
}

// ---------------------------------------------------------------------------

- (void) setDefaultName: (NSString *) defaultName
{
    KLStatus err = KLSetDefaultLoginOption (loginOption_LoginName, [defaultName UTF8String], [defaultName length]);
    if (err) {
        dprintf ("setDefaultName: failed with error %d (%s)", err, error_message (err));        
    }
}

// ---------------------------------------------------------------------------

- (NSString *) defaultRealm
{
    char *value = NULL;
    NSString *string = @"";
    KLStatus err = KLGetKerberosDefaultRealmByName (&value);

    if (!err) {
        string = [NSString stringWithUTF8String: value];
    } else {
        dprintf ("defaultRealm failed with error %d (%s)", err, error_message (err));        
    }
    
    if (value) { KLDisposeString (value); }
    
    return string;
}

// ---------------------------------------------------------------------------

- (void) setDefaultRealm: (NSString *) defaultRealm
{
    KLIndex realmIndex;
    KLStatus err = KLFindKerberosRealmByName ([defaultRealm UTF8String], &realmIndex);
    if (err) {
        err = KLInsertKerberosRealm (realmList_End, [defaultRealm UTF8String]);
    }
    if (!err) {
        err = KLSetKerberosDefaultRealmByName ([defaultRealm UTF8String]);
        if (err) {
            dprintf ("KLSetDefaultLoginOption return err %d (%s)", err, error_message (err));
        }
    }
}

- (NSArray *) realms
{
    NSMutableArray *realmsArray = [[[NSMutableArray alloc] init] autorelease];
    KLIndex realmCount = KLCountKerberosRealms ();
    KLIndex i;
        
    for (i = 0; i < realmCount; i++) {
        char *realm;
        if (KLGetKerberosRealm (i, &realm) == klNoErr) {
            [realmsArray addObject: [NSString stringWithUTF8String: realm]];
            KLDisposeString (realm);
        }
    }
    
    return [NSArray arrayWithArray: realmsArray];    
}

#pragma mark -- Default Ticket Options --

// ---------------------------------------------------------------------------

- (BOOL) rememberOptionsFromLastLogin
{
    KLBoolean value;
    KLSize size = sizeof (value);
    KLStatus err = KLGetDefaultLoginOption (loginOption_RememberExtras, &value, &size);
    
    if (err) {
        dprintf ("rememberOptionsFromLastLogin failed with error %d (%s)", err, error_message (err));        
    }
    
    return (!err) ? (value != 0) : YES;
}

// ---------------------------------------------------------------------------

- (void) setRememberOptionsFromLastLogin: (BOOL) rememberOptionsFromLastLogin
{
    KLBoolean value = (rememberOptionsFromLastLogin == YES);
    KLStatus err = KLSetDefaultLoginOption (loginOption_RememberExtras, &value, sizeof (value));
    if (err) {
        dprintf ("setRememberOptionsFromLastLogin: failed with error %d (%s)", err, error_message (err));        
    }
}

// ---------------------------------------------------------------------------

- (time_t) defaultLifetime
{
    KLLifetime value;
    KLSize size = sizeof (value);
    KLStatus err = KLGetDefaultLoginOption (loginOption_DefaultTicketLifetime, &value, &size);
    
    if (err) {
        dprintf ("defaultLifetime failed with error %d (%s)", err, error_message (err));        
    }
    
    return (!err) ? value : 21*60*60;    
}

// ---------------------------------------------------------------------------

- (void) setDefaultLifetime: (time_t) defaultLifetime
{
    KLLifetime value = defaultLifetime;
    KLStatus err = KLSetDefaultLoginOption (loginOption_DefaultTicketLifetime, &value, sizeof (value));
    if (err) {
        dprintf ("setDefaultLifetime: failed with error %d (%s)", err, error_message (err));        
    }
}

// ---------------------------------------------------------------------------

- (BOOL) defaultForwardable
{
    KLBoolean value;
    KLSize size = sizeof (value);
    KLStatus err = KLGetDefaultLoginOption (loginOption_DefaultForwardableTicket, &value, &size);
    
    if (err) {
        dprintf ("defaultForwardable failed with error %d (%s)", err, error_message (err));        
    }
    
    return !err ? (value != 0) : YES;
}

// ---------------------------------------------------------------------------

- (void) setDefaultForwardable: (BOOL) defaultForwardable
{
    KLBoolean value = (defaultForwardable == YES);
    KLStatus err = KLSetDefaultLoginOption (loginOption_DefaultForwardableTicket, &value, sizeof (value));
    if (err) {
        dprintf ("setDefaultForwardable: failed with error %d (%s)", err, error_message (err));        
    }
}

// ---------------------------------------------------------------------------

- (BOOL) defaultProxiable
{
    KLBoolean value;
    KLSize size = sizeof (value);
    KLStatus err = KLGetDefaultLoginOption (loginOption_DefaultProxiableTicket, &value, &size);
    
    if (err) {
        dprintf ("defaultProxiable failed with error %d (%s)", err, error_message (err));        
    }
    
    return !err ? (value != 0) : YES;
}

// ---------------------------------------------------------------------------

- (void) setDefaultProxiable: (BOOL) defaultProxiable
{
    KLBoolean value = (defaultProxiable == YES);
    KLStatus err = KLSetDefaultLoginOption (loginOption_DefaultProxiableTicket, &value, sizeof (value));
    if (err) {
        dprintf ("setDefaultProxiable: failed with error %d (%s)", err, error_message (err));        
    }
}

// ---------------------------------------------------------------------------

- (BOOL) defaultAddressless
{
    KLBoolean value;
    KLSize size = sizeof (value);
    KLStatus err = KLGetDefaultLoginOption (loginOption_DefaultAddresslessTicket, &value, &size);
    
    if (err) {
        dprintf ("defaultAddressless failed with error %d (%s)", err, error_message (err));        
    }
    
    return !err ? (value != 0) : YES;
}

// ---------------------------------------------------------------------------

- (void) setDefaultAddressless: (BOOL) defaultAddressless
{
    KLBoolean value = (defaultAddressless == YES);
    KLStatus err = KLSetDefaultLoginOption (loginOption_DefaultAddresslessTicket, &value, sizeof (value));
    if (err) {
        dprintf ("setDefaultAddressless: failed with error %d (%s)", err, error_message (err));        
    }
}

// ---------------------------------------------------------------------------

- (BOOL) defaultRenewable
{
    KLBoolean value;
    KLSize size = sizeof (value);
    KLStatus err = KLGetDefaultLoginOption (loginOption_DefaultRenewableTicket, &value, &size);
    
    if (err) {
        dprintf ("defaultRenewable failed with error %d (%s)", err, error_message (err));        
    }
    
    return !err ? (value != 0) : YES;
}

// ---------------------------------------------------------------------------

- (void) setDefaultRenewable: (BOOL) defaultRenewable
{
    KLBoolean value = (defaultRenewable == YES);
    KLStatus err = KLSetDefaultLoginOption (loginOption_DefaultRenewableTicket, &value, sizeof (value));
    if (err) {
        dprintf ("setDefaultRenewable: failed with error %d (%s)", err, error_message (err));        
    }
}

// ---------------------------------------------------------------------------

- (time_t) defaultRenewableLifetime
{
    KLLifetime value;
    KLSize size = sizeof (value);
    KLStatus err = KLGetDefaultLoginOption (loginOption_DefaultRenewableLifetime, &value, &size);
    
    if (err) {
        dprintf ("defaultRenewableLifetime failed with error %d (%s)", err, error_message (err));        
    }
    
    return !err ? value : 21*60*60;    
}

// ---------------------------------------------------------------------------

- (void) setDefaultRenewableLifetime: (time_t) defaultRenewableLifetime
{
    KLLifetime value = defaultRenewableLifetime;
    KLStatus err = KLSetDefaultLoginOption (loginOption_DefaultRenewableLifetime, &value, sizeof (value));
    if (err) {
        dprintf ("setDefaultRenewableLifetime: failed with error %d (%s)", err, error_message (err));        
    }
}

#pragma mark -- Time Ranges --
// ---------------------------------------------------------------------------

- (time_t) lifetimeMaximum
{
    KLLifetime value;
    KLSize size = sizeof (value);
    KLStatus err = KLGetDefaultLoginOption (loginOption_MaximalTicketLifetime, &value, &size);
    
    if (err) {
        dprintf ("lifetimeMaximum failed with error %d (%s)", err, error_message (err));        
    }
    
    return !err ? value : 21*60*60;    
}

// ---------------------------------------------------------------------------

- (void) setLifetimeMaximum: (time_t) lifetimeMaximum
{
    KLLifetime value = lifetimeMaximum;
    KLStatus err = KLSetDefaultLoginOption (loginOption_MaximalTicketLifetime, &value, sizeof (value));
    if (err) {
        dprintf ("setLifetimeMaximum: failed with error %d (%s)", err, error_message (err));        
    }
}

// ---------------------------------------------------------------------------

- (time_t) lifetimeMinimum
{
    KLLifetime value;
    KLSize size = sizeof (value);
    KLStatus err = KLGetDefaultLoginOption (loginOption_MinimalTicketLifetime, &value, &size);
    
    if (err) {
        dprintf ("lifetimeMinimum failed with error %d (%s)", err, error_message (err));        
    }
    
    return !err ? value : 10;    
}

// ---------------------------------------------------------------------------

- (void) setLifetimeMinimum: (time_t) lifetimeMinimum
{
    KLLifetime value = lifetimeMinimum;
    KLStatus err = KLSetDefaultLoginOption (loginOption_MinimalTicketLifetime, &value, sizeof (value));
    if (err) {
        dprintf ("setLifetimeMinimum: failed with error %d (%s)", err, error_message (err));        
    }
}

// ---------------------------------------------------------------------------

- (time_t) renewableLifetimeMaximum
{
    KLLifetime value;
    KLSize size = sizeof (value);
    KLStatus err = KLGetDefaultLoginOption (loginOption_MaximalRenewableLifetime, &value, &size);
    
    if (err) {
        dprintf ("renewableLifetimeMaximum failed with error %d (%s)", err, error_message (err));        
    }
    
    return !err ? value : 7*24*60*60;    
}

// ---------------------------------------------------------------------------

- (void) setRenewableLifetimeMaximum: (time_t) renewableLifetimeMaximum
{
    KLLifetime value = renewableLifetimeMaximum;
    KLStatus err = KLSetDefaultLoginOption (loginOption_MaximalRenewableLifetime, &value, sizeof (value));
    if (err) {
        dprintf ("setRenewableLifetimeMaximum: failed with error %d (%s)", err, error_message (err));        
    }
}

// ---------------------------------------------------------------------------

- (time_t) renewableLifetimeMinimum
{
    KLLifetime value;
    KLSize size = sizeof (value);
    KLStatus err = KLGetDefaultLoginOption (loginOption_MinimalRenewableLifetime, &value, &size);
    
    if (err) {
        dprintf ("renewableMinimum failed with error %d (%s)", err, error_message (err));        
    }
    
    return !err ? value : 10;    
}

// ---------------------------------------------------------------------------

- (void) setRenewableLifetimeMinimum: (time_t) renewableLifetimeMinimum
{
    KLLifetime value = renewableLifetimeMinimum;
    KLStatus err = KLSetDefaultLoginOption (loginOption_MinimalRenewableLifetime, &value, sizeof (value));
    if (err) {
        dprintf ("setRenewableLifetimeMinimum: failed with error %d (%s)", err, error_message (err));        
    }
}

// ---------------------------------------------------------------------------

@end
