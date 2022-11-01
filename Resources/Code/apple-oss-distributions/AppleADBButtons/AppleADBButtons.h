#ifndef _APPLEADBBUTTONS_H
#define _APPLEADBBUTTONS_H

/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/adb/IOADBDevice.h>

#define kVolume_up			0x06
#define kVolume_down			0x07
#define kMute				0x08
#define kVolume_up_AV			0x03  //Apple ADB AV monitors have different button codes
#define kVolume_down_AV			0x02
#define kMute_AV			0x01
#define kBrightness_up			0x09
#define kBrightness_down		0x0a
#define kEject				0x0b
#define kVideoMirror			0x0c
#define kIllumination_toggle		0x0d
#define kIllumination_down		0x0e
#define kIllumination_up		0x0f
#define kNum_lock_on_laptops		0x7f

#define kMax_registrations 		10
#define	kMax_keycode			0x0a
#define kNullKey			0xFF

typedef void (*button_handler)(void * );

class AppleADBButtons :  public IOHIKeyboard
{
    OSDeclareDefaultStructors(AppleADBButtons)

private:

    unsigned int	keycodes[kMax_registrations];
    thread_call_t	downHandlerThreadCalls[kMax_registrations];

    void dispatchButtonEvent (unsigned int, bool );
    UInt32		_initial_handler_id;
    const OSSymbol 	*_register_for_button;
    UInt32              _cachedKeyboardFlags;
    UInt32		_eject_delay;
    thread_call_t	_peject_timer;
    bool		_eject_released;
	IONotifier			*_publishNotify;
	IONotifier			*_terminateNotify;
	IOHIKeyboard		*_pADBKeyboard;
    
    static bool _publishNotificationHandler( void *target, 
                                void *ref, IOService *newService );
    
    static bool _terminateNotificationHandler( void *target, 
								void *ref, IOService *service );

public:

    const unsigned char * defaultKeymapOfLength (UInt32 * length );
    UInt32 interfaceID();
    UInt32 deviceType();
    UInt64 getGUID();
    void _check_eject_held( void ) ;

public:

    IOADBDevice *	adbDevice;

    bool init(OSDictionary * properties);
    bool start ( IOService * theNub );
    void free();
    IOReturn packet (UInt8 * data, IOByteCount length, UInt8 adbCommand );
    IOReturn registerForButton ( unsigned int, IOService *, button_handler, bool );
    bool doesKeyLock( unsigned key );
    void setDeviceFlags(unsigned flags); // Set device event flags

    IOReturn setParamProperties(OSDictionary *dict);
    virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
                                        void *param1, void *param2,
                                        void *param3, void *param4);

};

#endif /* _APPLEADBBUTTONS_H */
