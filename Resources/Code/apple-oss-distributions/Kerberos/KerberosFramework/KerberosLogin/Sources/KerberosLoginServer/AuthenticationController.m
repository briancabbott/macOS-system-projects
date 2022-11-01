/*
 * AuthenticationController.m
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

#import "KerberosAgentController.h"
#import "AuthenticationController.h"
#import "ChangePasswordController.h"
#import "PrompterController.h"
#import "KerberosLifetimeSlider.h"
#import "KerberosLifetimeFormatter.h"
#import "KerberosErrorAlert.h"

#define AuthControllerString(key) NSLocalizedStringFromTable (key, @"AuthenticationController", NULL)

@implementation AuthenticationController

// ---------------------------------------------------------------------------

- (id)initWithWindowNibName:(NSString *)windowNibName
{
    if ((self = [super initWithWindowNibName:windowNibName])) {
        dprintf ("AuthenticationController initializing");
        
        // Set these to NULL so we don't free random memory on release
        result = klNoErr;
        preferences = NULL;
        state.doesMinimize = NO;
        state.isMinimized = NO;
        state.callerProvidedPrincipal = NULL;
        state.serviceNameString = NULL;
        state.callerNameString = NULL;
        state.callerIconImage = NULL;
        acquiredPrincipal = NULL;
        acquiredCacheName = NULL;
        changePasswordController = NULL;
        _keychainPasswordFromKeychain = NULL;
		_foundKCItem = NULL;
		
        preferences = [[KerberosPreferences sharedPreferences] retain];
        if (preferences == NULL) {
            [self release];
            return NULL;
        }
        
        state.lifetime          = [preferences defaultLifetime];
        state.startTime         = 0;
        state.forwardable       = [preferences defaultForwardable];
        state.proxiable         = [preferences defaultProxiable];
        state.addressless       = [preferences defaultAddressless];
        state.renewable         = [preferences defaultRenewable];
        state.renewableLifetime = [preferences defaultRenewableLifetime];

        acquiredCacheName = [[NSMutableString alloc] init];
        if (acquiredCacheName == NULL) {
            [self release];
            return NULL;
        }        
    }
    
    return self;
}

- (id) init
{
	return [self initWithWindowNibName: @"AuthenticationController"];
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    dprintf ("AuthenticationController deallocating");
    if (preferences                   != NULL) { [preferences release]; }
    if (state.callerProvidedPrincipal != NULL) { [state.callerProvidedPrincipal release]; }
    if (state.serviceNameString       != NULL) { [state.serviceNameString release]; }
    if (state.callerNameString        != NULL) { [state.callerNameString release]; }
    if (state.callerIconImage         != NULL) { [state.callerIconImage release]; }
    if (acquiredPrincipal             != NULL) { [acquiredPrincipal release]; }
    if (acquiredCacheName             != NULL) { [acquiredCacheName release]; }
    if (_keychainPasswordFromKeychain != NULL) { [_keychainPasswordFromKeychain release]; }
    if (_foundKCItem				  != NULL) { CFRelease(_foundKCItem); }
    [super dealloc];
}

#pragma mark -- Loading --

// ---------------------------------------------------------------------------

- (void) awakeFromNib 
{
    // Fill in the version field in the about dialog
    NSString *name      = [[NSBundle mainBundle] objectForInfoDictionaryKey: @"KfMDisplayName"];
    NSString *version   = [[NSBundle mainBundle] objectForInfoDictionaryKey: @"KfMDisplayVersion"];
    NSString *copyright = [[NSBundle mainBundle] objectForInfoDictionaryKey: @"KfMDisplayCopyright"];
    
    [aboutVersionTextField setStringValue: [NSString stringWithFormat: @"%@ %@", 
                                               (name != NULL) ? name : @"", (version != NULL) ? version : @""]];    
    [aboutCopyrightTextField setStringValue: (copyright != NULL) ? copyright : @""];
    
    // Hide the "Remember password" checkbox if preferences dictate
    CFPropertyListRef savePasswordDisabled = CFPreferencesCopyAppValue(CFSTR("SavePasswordDisabled"), kCFPreferencesCurrentApplication);
    if (savePasswordDisabled != NULL) {
        if (CFGetTypeID(savePasswordDisabled) == CFBooleanGetTypeID() && CFBooleanGetValue(savePasswordDisabled)) {
            [rememberPasswordInKeychainCheckBox setHidden:YES];
            // Shrink the window by the height of the now-hidden checkbox
            NSRect frame = [[self window] frame];
            frame.origin.y += 18;
            frame.size.height -= 18;
            [[self window] setFrame:frame display:NO animate:NO];
        }
        CFRelease(savePasswordDisabled);
    }
}

// ---------------------------------------------------------------------------

- (void) windowDidLoad
{
    dprintf ("Entering AuthenticationController's windowDidLoad");
    
    // Message text
    if (state.callerNameString != NULL) {
        [headerTextField setStringValue: 
            [NSString stringWithFormat: AuthControllerString (@"AuthControllerApplicationRequest"), state.callerNameString]];
    } else {
        [headerTextField setStringValue: AuthControllerString (@"AuthControllerRequest")];
    }
    
    // Set up the dialog icon:
    NSImage *applicationIconImage = [NSImage imageNamed: @"NSApplicationIcon"];
    [kerberosIconImageView setImage: applicationIconImage];
    if (state.callerIconImage != NULL) {
        [kerberosIconImageView setBadgeImage: state.callerIconImage];
    }

    // remember the size of the window frame size with and without the options visible
    maximizedFrameHeight = [[[self window] contentView] frame].size.height;
    minimizedFrameHeight = [bannerBox frame].size.height;
        
    // Default KerberosPrincipal
    BOOL callerProvidedPrincipal = (state.callerProvidedPrincipal != NULL);
    if ( callerProvidedNameTextField )
    {
		[callerProvidedNameTextField  setEnabled: callerProvidedPrincipal];
		[callerProvidedNameTextField  setHidden: !callerProvidedPrincipal];
    }
	if ( callerProvidedNameTextField )
    {
		[nameTextField                setEnabled: !callerProvidedPrincipal];
		[callerProvidedRealmTextField setHidden: !callerProvidedPrincipal];
    }
	if ( nameTextField )
		[nameTextField                setHidden: callerProvidedPrincipal];
    if ( realmComboBox )
    {
		[realmComboBox                setEnabled: !callerProvidedPrincipal];
		[realmComboBox                setHidden: callerProvidedPrincipal];
    }
    if (callerProvidedPrincipal) {
#warning "No support for '\@' in realm names"
        NSString *principalString = [state.callerProvidedPrincipal displayString];
        NSRange separator = [principalString rangeOfString: @"@" options: (NSLiteralSearch | NSBackwardsSearch)];
        
        [callerProvidedNameTextField  setStringValue: [principalString substringToIndex: separator.location]];
        [callerProvidedRealmTextField setStringValue: [principalString substringFromIndex: separator.location + separator.length]];
    } else {
        [nameTextField setObjectValue: [preferences defaultName]];
        
        NSArray *realmsArray = [preferences realms];
        [realmComboBox removeAllItems];
        unsigned int i;
        for (i = 0; i < [realmsArray count]; i++) {
            NSString *realm = [realmsArray objectAtIndex: i];
            [realmComboBox addItemWithObjectValue: realm];
        }
        [realmComboBox selectItemWithObjectValue: [preferences defaultRealm]];
        [realmComboBox setObjectValue: [realmComboBox numberOfItems] > 0 ? [realmComboBox objectValueOfSelectedItem] : @""];
        [realmComboBox setNumberOfVisibleItems: [realmsArray count]];
        [realmComboBox setCompletes: YES];

		[self autoFillPasswordWithKeychainPasswordForRealm:[preferences defaultRealm]];
   }   

    // If we have no username and the username field is editable, put the cursor there;
    // otherwise put in in the password text field.
    if (!callerProvidedPrincipal && ([[nameTextField stringValue] length] == 0)) {
        [[self window] setInitialFirstResponder: nameTextField];
    } else {
        [[self window] setInitialFirstResponder: passwordSecureTextField];
    }
}

#pragma mark -- Notifications --

// ---------------------------------------------------------------------------

- (void)controlTextDidEndEditing: (NSNotification *) notification
{
	id obj = [notification object];
	if ((obj == realmComboBox || obj == nameTextField ) && _didChangeText )
	{
		_didChangeText = NO;
		NSInteger numberOfItems = [realmComboBox numberOfItems];
		NSString *realm = NULL;
		realm = [realmComboBox stringValue];
		[self autoFillPasswordWithKeychainPasswordForRealm:realm];
	}
}


- (void) controlTextDidChange: (NSNotification *) notification
{
    [self updateOKButtonState];
	id obj = [notification object];
	if (obj == realmComboBox || obj == nameTextField )
	{
		_didChangeText = YES;
	}
}

// ---------------------------------------------------------------------------

- (void) comboBoxSelectionDidChange: (NSNotification *) notification
{
    if ([notification object] == realmComboBox) {
        // For some reason the string value doesn't immediately get set for us but 
        // we need it for updateOKButton to do the right thing
        [realmComboBox setStringValue: [realmComboBox objectValueOfSelectedItem]];
        [self updateOKButtonState];
		NSInteger numberOfItems = [realmComboBox numberOfItems];
		NSString *realm = NULL;
		realm = [realmComboBox stringValue];
		[self autoFillPasswordWithKeychainPasswordForRealm:realm];
    }
}

// ---------------------------------------------------------------------------

- (void) comboBoxWillDismiss: (NSNotification *) notification
{
    [self updateOKButtonState];
}

// ---------------------------------------------------------------------------

- (void) windowDidBecomeKey: (NSNotification *) notification
{
    if (state.doesMinimize && ([notification object] == [self window])) {
        [self maximizeWindow];
    }
}

// ---------------------------------------------------------------------------

- (void) windowDidResignKey: (NSNotification *) notification
{
    // Only minimize if none of our windows are key anymore since we could be in the about box
    if (state.doesMinimize && ([notification object] == [self window]) && ([NSApp keyWindow] == NULL)) {
        [self minimizeWindow];
    }
}

#pragma mark -- Actions --

// ---------------------------------------------------------------------------

- (IBAction) changePassword: (id) sender
{
    KerberosPrincipal *dialogPrincipal = [self principal];
    if (dialogPrincipal != NULL) {
        changePasswordController = [[ChangePasswordController alloc] initWithPrincipal: dialogPrincipal];
        if (changePasswordController != NULL) {
            [changePasswordController beginSheetModalForWindow: [self window]
                                                 modalDelegate: self
                                                didEndSelector: @selector(changePasswordSheetDidEnd:returnCode:contextInfo:)
                                                   contextInfo: [NSNumber numberWithBool: FALSE]]; // Don't get tickets
        }
    } else {
        [KerberosErrorAlert alertForError: klBadPrincipalErr
                           action: KerberosChangePasswordAction
                   modalForWindow: [self window]];        
    }
}

// ---------------------------------------------------------------------------

- (void) changePasswordSheetDidEnd: (NSWindow *) sheet 
                        returnCode: (int) returnCode
                       contextInfo: (id) contextInfo
{
    if (changePasswordController != NULL) { 
        if (!returnCode && contextInfo && [contextInfo isKindOfClass: [NSNumber class]] && [contextInfo boolValue]) {
            KLStatus err = [self getTicketsWithPassword: [changePasswordController newPassword]];
            
            if (err == klNoErr) {
                [self stopWithCode: err];
            } else if (err != klUserCanceledErr) {
                [KerberosErrorAlert alertForError: err
                                   action: KerberosGetTicketsAction
                           modalForWindow: [self window]];
            }            
        }
        
        [changePasswordController release]; 
        changePasswordController = NULL;
    }
}


// ---------------------------------------------------------------------------

- (IBAction) showOptions: (id) sender
{
    if (state.isMinimized) {
        NSLog (@"Warning! Calling toggleOptions while window is minimized!");
    }
    
    SetupLifetimeSlider (lifetimeSlider, lifetimeTextField,
                         [preferences lifetimeMaximum], [preferences lifetimeMinimum], 
                         state.lifetime);
    
    [forwardableCheckbox setState: state.forwardable ? NSOnState : NSOffState];
    [addresslessCheckbox setState: state.addressless ? NSOnState : NSOffState];
    [renewableCheckbox   setState: state.renewable   ? NSOnState : NSOffState];
    
    SetupLifetimeSlider (renewableSlider, renewableTextField,
                         [preferences renewableLifetimeMaximum], [preferences renewableLifetimeMinimum], 
                         state.renewableLifetime);
    
    [self renewableCheckboxWasHit: self];

    [NSApp beginSheet: optionsSheet 
       modalForWindow: [self window] 
        modalDelegate: self
       didEndSelector: NULL
          contextInfo: NULL];
}

// ---------------------------------------------------------------------------

- (IBAction) renewableCheckboxWasHit: (id) sender
{
    BOOL enabled = ([renewableCheckbox state] == NSOnState);
    NSColor *enabledColor = enabled ? [NSColor blackColor] : [NSColor grayColor];
    
    if ([renewableSlider maxValue] > [renewableSlider minValue]) {
        [renewableTextField  setTextColor: enabledColor];
        [renewableSlider     setEnabled: enabled];
    }
}

// ---------------------------------------------------------------------------

- (IBAction) optionsOK: (id) sender
{
    // Save dialog state to controller
    [self setForwardable: ([forwardableCheckbox state] == NSOnState)];
    [self setAddressless: ([addresslessCheckbox state] == NSOnState)];
    [self setRenewable:   ([renewableCheckbox   state] == NSOnState)];
    
    KerberosLifetimeFormatter *lifetimeFormatter = [lifetimeTextField formatter];
    [self setLifetime: [lifetimeFormatter lifetimeForControl: lifetimeSlider]];
    
    KerberosLifetimeFormatter *renewableFormatter =  [renewableTextField formatter];
    [self setRenewableLifetime: [renewableFormatter lifetimeForControl: renewableSlider]];

    [NSApp endSheet: optionsSheet returnCode: klNoErr];
    [optionsSheet orderOut: self];
}

// ---------------------------------------------------------------------------

- (IBAction) optionsCancel: (id) sender
{
    // Toss dialog state
    [NSApp endSheet: optionsSheet returnCode: klUserCanceledErr];
    [optionsSheet orderOut: self];
}

// ---------------------------------------------------------------------------

- (IBAction) showAboutBox: (id) sender
{
    [aboutWindow center];
    [aboutWindow makeKeyAndOrderFront: self];
    [NSApp mainMenu];
}

// ---------------------------------------------------------------------------

- (IBAction) ok: (id) sender
{
    // Get tickets
    KLStatus err = [self getTicketsWithPassword: [passwordSecureTextField stringValue]];
    
    if (!err) {
        [self stopWithCode: err];
    } else {
        if (err == KRB5KDC_ERR_KEY_EXP) {
            // Ask the user if s/he wants to change the password
            NSString *question = AuthControllerString (@"AuthControllerStringPasswordExpired");
            BOOL changePassword = [KerberosErrorAlert alertForYNQuestion: question
                                                          action: KerberosGetTicketsAction
                                                  modalForWindow: [self window]];
            if (changePassword) {
                // Change the password
                changePasswordController = [[ChangePasswordController alloc] initWithPrincipal: acquiredPrincipal];
                if (changePasswordController != NULL) {
                    [changePasswordController beginSheetModalForWindow: [self window]
                                                         modalDelegate: self
                                                        didEndSelector: @selector(changePasswordSheetDidEnd:returnCode:contextInfo:)
                                                           contextInfo: [NSNumber numberWithBool: TRUE]]; // Get tickets on success
                }
            } else {
                // User doesn't want to change password
                err = klUserCanceledErr;
            }
        }
        
        if (err != klUserCanceledErr) {
            [KerberosErrorAlert alertForError: err
                               action: KerberosGetTicketsAction
                       modalForWindow: [self window]];
        }
    }
}

// ---------------------------------------------------------------------------

- (IBAction) cancel: (id) sender
{
    [self stopWithCode: klUserCanceledErr];
}

#pragma mark -- Settings and Options --

// ---------------------------------------------------------------------------

- (void) setDoesMinimize: (BOOL) doesMinimize
{
    state.doesMinimize = doesMinimize;
}

// ---------------------------------------------------------------------------

- (void) setCallerNameString: (NSString *) callerNameString
{
    if (state.callerNameString != NULL) { [state.callerNameString release]; }
    state.callerNameString = [callerNameString retain];
}

// ---------------------------------------------------------------------------

- (void) setCallerIcon: (NSImage *) callerIconImage
{
    if (state.callerIconImage != NULL) { [state.callerIconImage release]; }
    state.callerIconImage = [callerIconImage retain];
}
// ---------------------------------------------------------------------------

- (void) setCallerProvidedPrincipal: (KerberosPrincipal *) callerProvidedPrincipal
{
    if (state.callerProvidedPrincipal != NULL) { [state.callerProvidedPrincipal release]; }
    state.callerProvidedPrincipal = [callerProvidedPrincipal retain];
}

// ---------------------------------------------------------------------------

- (void) setServiceName: (NSString *) serviceNameString
{
    if (state.serviceNameString != NULL) { [state.serviceNameString release]; }
    state.serviceNameString = [serviceNameString retain];
}

// ---------------------------------------------------------------------------

- (void) setStartTime: (time_t) startTime
{
    state.startTime = startTime;
}

// ---------------------------------------------------------------------------

- (void) setLifetime: (time_t) lifetime
{
    state.lifetime = lifetime;
}

// ---------------------------------------------------------------------------

- (void) setForwardable: (BOOL) forwardable
{
    state.forwardable = forwardable;
}

// ---------------------------------------------------------------------------

- (void) setProxiable: (BOOL) proxiable
{
    state.proxiable = proxiable;
}

// ---------------------------------------------------------------------------

- (void) setAddressless: (BOOL) addressless
{
    state.addressless = addressless;
}

// ---------------------------------------------------------------------------

- (void) setRenewable: (BOOL) renewable
{
    state.renewable = renewable;
}

// ---------------------------------------------------------------------------

- (void) setRenewableLifetime: (time_t) renewableLifetime
{
    state.renewableLifetime = renewableLifetime;
}

#pragma mark -- Miscellaneous --

// ---------------------------------------------------------------------------
// Note: may return NULL

- (KerberosPrincipal *) principal
{
    if (state.callerProvidedPrincipal != NULL) {
        return state.callerProvidedPrincipal;
    } else {
        NSString *principalString = [NSString stringWithFormat: @"%@@%@", [nameTextField stringValue], [realmComboBox stringValue]];
        return [[[KerberosPrincipal alloc] initWithString: principalString] autorelease];
    }
}

// ---------------------------------------------------------------------------

- (void) updateOKButtonState
{
    BOOL havePrincipal = NO;
    
    if ([nameTextField isHidden]) {
        havePrincipal = ([[passwordSecureTextField stringValue] length] > 0);
    } else {
        if ( nameTextField != NULL && realmComboBox != NULL )
			havePrincipal = (([[nameTextField           stringValue] length] > 0) && 
							 ([[realmComboBox           stringValue] length] > 0) &&
							 ([[passwordSecureTextField stringValue] length] > 0));
        else
			havePrincipal = ([[passwordSecureTextField stringValue] length] > 0);
    }
    
    [okButton setEnabled: havePrincipal];
}

// ---------------------------------------------------------------------------

- (BOOL) validateMenuItem: (id <NSMenuItem>) menuItem
{
    if (menuItem == changePasswordMenuItem) {
        return ([nameTextField isHidden] || 
                (([[nameTextField stringValue] length] > 0) && 
                 ([[realmComboBox stringValue] length] > 0)));
    } else {
        return YES;
    }
}

// ---------------------------------------------------------------------------

- (void) setWindowContentHeight: (float) newHeight
{
    NSRect contentRect = [[self window] contentRectForFrameRect: [[self window] frame]];
    float currentHeight = contentRect.size.height;
    
    if (newHeight != currentHeight) {
        contentRect.size.height = newHeight;
        
        NSRect frameRect = [[self window] frameRectForContentRect: contentRect];
        if (newHeight > currentHeight) {
            frameRect.origin.y -= (newHeight - currentHeight); // move down
        } else {
            frameRect.origin.y += (currentHeight - newHeight); // move up
        }
        [[self window] setFrame: frameRect display: YES animate: YES]; 
    }
}

// ---------------------------------------------------------------------------

- (void) minimizeWindow
{
    if (state.doesMinimize && !state.isMinimized) {
        // minimize view
        [okButton        setAutoresizingMask: NSViewMinYMargin];
        [cancelButton    setAutoresizingMask: NSViewMinYMargin];
        [gearPopupButton setAutoresizingMask: NSViewMinYMargin];
		[rememberPasswordInKeychainCheckBox setAutoresizingMask: NSViewMinYMargin];
		   
        [self setWindowContentHeight: minimizedFrameHeight];
        
        state.isMinimized = YES;
    }
}

// ---------------------------------------------------------------------------

- (void) maximizeWindow
{
    if (state.isMinimized) {
        // maximize view
        [self setWindowContentHeight: maximizedFrameHeight];
        
        [okButton        setAutoresizingMask: NSViewMinXMargin | NSViewMaxYMargin];
        [cancelButton    setAutoresizingMask: NSViewMinXMargin | NSViewMaxYMargin];
        [gearPopupButton setAutoresizingMask: NSViewMinXMargin | NSViewMaxYMargin];
        [rememberPasswordInKeychainCheckBox setAutoresizingMask: NSViewMinXMargin | NSViewMaxYMargin];
                
        state.isMinimized = NO;
    }
}

// ---------------------------------------------------------------------------

- (void) loginOptionsToPreferences
{
    if ([preferences rememberPrincipalFromLastLogin]) {
        if (state.callerProvidedPrincipal != NULL) {
            [preferences setDefaultName:  [callerProvidedNameTextField stringValue]];
            [preferences setDefaultRealm: [callerProvidedRealmTextField stringValue]];
        } else {
            [preferences setDefaultName:  [nameTextField stringValue]];
            [preferences setDefaultRealm: [realmComboBox stringValue]];
        }
    }
    
    if ([preferences rememberOptionsFromLastLogin]) {
        [preferences setDefaultLifetime:          state.lifetime];
        [preferences setDefaultForwardable:       state.forwardable];
        [preferences setDefaultAddressless:       state.addressless];
        [preferences setDefaultRenewable:         state.renewable];
        [preferences setDefaultRenewableLifetime: state.renewableLifetime];
    }
}

// ---------------------------------------------------------------------------

- (int) getTicketsWithPassword: (NSString *) password
{
    KLStatus err = klNoErr;
    KLLoginOptions loginOptions = NULL;
    
    // Get Tickets:
    acquiredPrincipal = [[self principal] retain];
    if (acquiredPrincipal == NULL) { err = klBadPrincipalErr; }
    
    if (err == klNoErr) {
        err = KLCreateLoginOptions (&loginOptions);
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetTicketLifetime (loginOptions, state.lifetime);
    }
    
    if (err == klNoErr) {
        if (state.renewable) {
            err = KLLoginOptionsSetRenewableLifetime (loginOptions, state.renewableLifetime);
        } else {
            err = KLLoginOptionsSetRenewableLifetime (loginOptions, 0L);
        }
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetTicketStartTime (loginOptions, state.startTime);
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetServiceName (loginOptions, ((state.serviceNameString == NULL) ? 
                                                           NULL : [state.serviceNameString UTF8String]));
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetForwardable (loginOptions, state.forwardable);
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetProxiable (loginOptions, state.proxiable);
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetAddressless (loginOptions, state.addressless);
    }
    
    if (err == klNoErr) {
        __KLSetApplicationPrompter (GraphicalKerberosPrompter);
		NSString *pwdToUse = password ? password : @" "; // use @" " for password when authenticating through pkinit 
        err = [acquiredPrincipal getTicketsWithPassword: pwdToUse
                                           loginOptions: loginOptions
                                              cacheName: acquiredCacheName];
	}

    if (err == klNoErr) {
        // If we got tickets, save dialog state to preferences
        [self loginOptionsToPreferences];
    }
    
    if (loginOptions != NULL) { KLDisposeLoginOptions (loginOptions); }
    
    return err;
}

// ---------------------------------------------------------------------------

- (KerberosPrincipal *) acquiredPrincipal
{
    return acquiredPrincipal;
}

// ---------------------------------------------------------------------------

- (NSString *) acquiredCacheName
{
    return acquiredCacheName;
}

// ---------------------------------------------------------------------------

- (int) runWindow
{
    [[NSApp delegate] addActiveWindow: [self window]];
    
    // Popup or not:
    if (state.doesMinimize) {
        [self minimizeWindow];
        
        NSPoint frameOriginPoint;
        NSRect sFrameRect = [[NSScreen mainScreen] frame];
        NSSize wFrameSize = [[self window] frame].size;
        frameOriginPoint.x = ((sFrameRect.origin.x + sFrameRect.size.width)  - wFrameSize.width  - 100);
        frameOriginPoint.y = ((sFrameRect.origin.y + sFrameRect.size.height) - wFrameSize.height - 100);
        
        [[self window] setFrameOrigin: frameOriginPoint];
        
    } else {
        [self maximizeWindow];
        [[self window] center];
    }

    [self showWindow: self];
    [NSApp run];
    [self close];
    
    [[NSApp delegate] removeActiveWindow: [self window]];
    
    return result;
}

// ---------------------------------------------------------------------------

- (void) stopWithCode: (int) returnCode
{
    result = returnCode;
    [NSApp stop: self];
}

-(NSString*)enteredPassword
{
	return [passwordSecureTextField stringValue];
}

-(SecKeychainItemRef)foundKCItem
{
	return _foundKCItem;
}

-(BOOL)findKeychainItemWithUser:(NSString*)user serverName:(NSString*)serverName password:(NSString**)password
{
	OSStatus findResult = noErr;
	UInt32 pwdLen = 0;
	void *outPassword;
	if ( _foundKCItem )
	{
		CFRelease(_foundKCItem);
		_foundKCItem = NULL;
	}
	findResult = SecKeychainFindGenericPassword(nil, [serverName length], [serverName UTF8String],
					[user length], [user UTF8String],
					&pwdLen, &outPassword, &_foundKCItem);
	if ( findResult == noErr )
	{
		if ( password )
			*password = [[[NSString alloc] initWithBytes:outPassword length:pwdLen encoding:NSUTF8StringEncoding] autorelease];
		if ( _keychainPasswordFromKeychain )
		{
			[_keychainPasswordFromKeychain release];
			_keychainPasswordFromKeychain = NULL;
		}
		_keychainPasswordFromKeychain = [[NSString alloc] initWithBytes:outPassword length:pwdLen encoding:NSUTF8StringEncoding];
		SecKeychainItemFreeAttributesAndData(NULL, outPassword);
		return YES;
	}
	return NO;
}

-(NSString*)keychainPasswordFromKeychain
{
	return _keychainPasswordFromKeychain;
}

-(void)autoFillPasswordWithKeychainPasswordForRealm:(NSString*)realm
{
	// Auto-fill keychain password for the selected realm (if the kc item exists)
	//
	NSString *userName = [nameTextField stringValue];
	if ( [userName length] != 0 && [realm length] != 0 )
	{
		NSString *keychainPassword = NULL;
		if ( [self findKeychainItemWithUser:userName serverName:realm password:&keychainPassword] == YES )
		{
			[passwordSecureTextField setStringValue:keychainPassword];
			[self updateOKButtonState];
		}
	}
}

-(BOOL)rememberPasswordInKeychain
{
	return [rememberPasswordInKeychainCheckBox intValue];
}

@end
