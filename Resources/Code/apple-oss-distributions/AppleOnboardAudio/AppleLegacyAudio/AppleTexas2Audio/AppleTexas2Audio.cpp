/*
 *  AppleTexas2Audio.cpp (definition)
 *  Project : Apple02Audio
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 * 
 *  The contents of this file constitute Original Code as defined in and
 *  are subject to the Apple Public Source License Version 1.1 (the
 *  "License").  You may not use this file except in compliance with the
 *  License.  Please obtain a copy of the License at
 *  http://www.apple.com/publicsource and read it before using this file.
 * 
 *  This Original Code and all software distributed under the License are
 *  distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 *  License for the specific language governing rights and limitations
 *  under the License.
 * 
 *  @APPLE_LICENSE_HEADER_END@
 *
 *  Hardware independent (relatively) code for the Texas Insruments Texas2 Codec
 *  NEW-WORLD MACHINE ONLY!!!
 */

#include "AppleTexas2Audio.h"
#include "Texas2_hw.h"

#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOCommandGate.h>
#include <UserNotification/KUNCUserNotifications.h>
#include <IOKit/IOTimerEventSource.h>
#include "AudioI2SHardwareConstants.h"

#include "AppleTexas2EQPrefs.cpp"

//#define durationMillisecond 1000	// number of microseconds in a millisecond
#ifdef DEBUG
	#define kInterruptSettleTime		5000000000ULL
#else
	#define kInterruptSettleTime		10000000ULL
#endif

#define super Apple02Audio

OSDefineMetaClassAndStructors(AppleTexas2Audio, Apple02Audio)

EQPrefsPtr		gEQPrefs = &theEQPrefs;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#pragma mark +UNIX LIKE FUNCTIONS

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// init, free, probe are the "Classical Unix driver functions" that you'll like as not find
// in other device drivers.	 Note that there are no start and stop methods.	 The code for start 
// effectively moves to the sndHWInitialize routine.  Also note that the initHardware method 
// effectively does very little other than calling the inherited method, which in turn calls
// sndHWInitialize, so all of the init code should be in the sndHWInitialize routine.

// ::init()
// call into superclass and initialize.
bool AppleTexas2Audio::init(OSDictionary *properties)
{
	debugIOLog (3, "+ AppleTexas2Audio::init");
	if (!super::init(properties))
		return false;

	gVolLeft = 0;
	gVolRight = 0;
	gVolMuteActive = false;
	gModemSoundActive = false;
	gInputNoneAlias = kSndHWInputNone;	//	default assumption is that no unused input is available to alias kSndHWInputNone
	powerStateChangeInProcess = false;	//	[3234624]

	debugIOLog (3, "- AppleTexas2Audio::init");
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ::free
// call inherited free
void AppleTexas2Audio::free()
{
	IOWorkLoop				*workLoop;

	debugIOLog (3, "+ AppleTexas2Audio::free");

	CLEAN_RELEASE(hdpnMuteRegMem);
	CLEAN_RELEASE(ampMuteRegMem);
	CLEAN_RELEASE(lineOutMuteGpioMem);
	CLEAN_RELEASE(masterMuteGpioMem);			//	[2933090]
	CLEAN_RELEASE(hwResetRegMem);
	CLEAN_RELEASE(headphoneExtIntGpioMem);
	CLEAN_RELEASE(lineOutExtIntGpioMem);
	CLEAN_RELEASE(dallasExtIntGpioMem);
	CLEAN_RELEASE(audioI2SControl);
 	if (NULL != ioBaseAddressMemory) {
		ioBaseAddressMemory->release();
	}
	
	if (NULL != ioClockBaseAddressMemory) {
		ioClockBaseAddressMemory->release();
	}

	workLoop = getWorkLoop();
	if (NULL != workLoop) {
		if (NULL != headphoneIntEventSource && NULL != headphoneIntProvider)
			workLoop->removeEventSource (headphoneIntEventSource);
		if (NULL != lineOutIntEventSource && NULL != lineOutIntProvider)		//	[2878119]
			workLoop->removeEventSource (lineOutIntEventSource);				//	[2878119]
		if (NULL != dallasIntEventSource && NULL != dallasIntProvider)
			workLoop->removeEventSource (dallasIntEventSource);
		if (NULL != dallasHandlerTimer)
			workLoop->removeEventSource (dallasHandlerTimer);
		if (NULL != notifierHandlerTimer)
			workLoop->removeEventSource (notifierHandlerTimer);
	}

	publishResource (gAppleAudioVideoJackStateKey, NULL);
	CLEAN_RELEASE(headphoneIntEventSource);
	CLEAN_RELEASE(dallasIntEventSource);

	super::free();
	debugIOLog (3, "- AppleTexas2Audio::free");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ::probe
// called at load time, to see if this driver really does match with a device.	In our
// case we check the registry to ensure we are loading on the appropriate hardware.
IOService* AppleTexas2Audio::probe(IOService *provider, SInt32 *score)
{
	// Finds the possible candidate for sound, to be used in
	// reading the caracteristics of this hardware:
	IORegistryEntry		*sound = 0;
	IOService			*result = 0;
	
	debugIOLog (3, "+ AppleTexas2Audio::probe");

	super::probe(provider, score);
	*score = kIODefaultProbeScore;
	sound = provider->childFromPath("sound", gIODTPlane);
	// we are on a new world : the registry is assumed to be fixed
	if(sound) {
		OSData *tmpData;

		tmpData = OSDynamicCast(OSData, sound->getProperty(kModelPropName));
		if(tmpData) {
			if(tmpData->isEqualTo(kTexas2ModelName, sizeof(kTexas2ModelName) -1)) {
				*score = *score+1;
				debugIOLog (3, "++ AppleTexas2Audio::probe increasing score");
				result = (IOService*)this;
			}
		}
		if (!result) {
			sound->release ();
		}
	}

	debugIOLog (3, "- AppleTexas2Audio::probe returns %d", (unsigned int)result );
	return (result);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ::initHardware
// Don't do a whole lot in here, but do call the inherited inithardware method.
// in turn this is going to call sndHWInitialize to perform initialization.	 All
// of the code to initialize and start the device needs to be in that routine, this 
// is kept as simple as possible.
bool AppleTexas2Audio::initHardware(IOService *provider)
{
	bool myreturn = true;

	debugIOLog (3, "+ AppleTexas2Audio::initHardware");
	
	// calling the superclasses initHarware will indirectly call our
	// sndHWInitialize() method.  So we don't need to do a whole lot 
	// in this function.
	super::initHardware(provider);
	
	debugIOLog (3, "- AppleTexas2Audio::initHardware");
	return myreturn;
}

// --------------------------------------------------------------------------
// Method: timerCallback
//
// Purpose:
//		  This is a static method that gets called from a timer task at regular intervals
//		  Generally we do not want to do a lot of work here, we simply turn around and call
//		  the appropriate method to perform out periodic tasks.

void AppleTexas2Audio::timerCallback(OSObject *target, IOAudioDevice *device)
{
// probably don't want this on since we will get called a lot...
//	  debugIOLog (3, "+ AppleTexas2Audio::timerCallback");
	AppleTexas2Audio *templateDriver;
	
	templateDriver = OSDynamicCast(AppleTexas2Audio, target);

	if (templateDriver) {
		templateDriver->checkStatus(false);
	}
// probably don't want this on since we will get called a lot...
//	  debugIOLog (3, "- AppleTexas2Audio::timerCallback");
	return;
}



// --------------------------------------------------------------------------
// Method: checkStatus
//
// Purpose:
//		 poll the detects, note this should prolly be done with interrupts rather
//		 than by polling if interrupts are supported

void AppleTexas2Audio::checkStatus(bool force)
{
// probably don't want this on since we will get called a lot...
//	  debugIOLog (3, "+ AppleTexas2Audio::checkStatus");

// probably don't want this on since we will get called a lot...
//	  debugIOLog (3, "- AppleTexas2Audio::checkStatus");
}

/*************************** sndHWXXXX functions******************************/
/*	 These functions should be common to all Apple hardware drivers and be	 */
/*	 declared as virtual in the superclass. This is the only place where we	 */
/*	 should manipulate the hardware. Order is given by the UI policy system	 */
/*	 The set of functions should be enough to implement the policy			 */
/*****************************************************************************/


#pragma mark +HARDWARE REGISTER MANIPULATION
/************************** Hardware Register Manipulation ********************/
// Hardware specific functions : These are all virtual functions and we have to 
// implement these in the driver class

// --------------------------------------------------------------------------
// ::sndHWInitialize
// hardware specific initialization needs to be in here, together with the code
// required to start audio on the device.
//
//	There are three sections of memory mapped I/O that are directly accessed by the Apple02Audio.  These
//	include the GPIOs, I2S DMA Channel Registers and I2S control registers.  They fall within the memory map 
//	as follows:
//	~                              ~
//	|______________________________|
//	|                              |
//	|         I2S Control          |
//	|______________________________|	<-	soundConfigSpace = ioBase + i2s0BaseOffset ...OR... ioBase + i2s1BaseOffset
//	|                              |
//	~                              ~
//	~                              ~
//	|______________________________|
//	|                              |
//	|       I2S DMA Channel        |
//	|______________________________|	<-	i2sDMA = ioBase + i2s0_DMA ...OR... ioBase + i2s1_DMA
//	|                              |
//	~                              ~
//	~                              ~
//	|______________________________|
//	|            FCRs              |
//	|            GPIO              |	<-	gpio = ioBase + gpioOffsetAddress
//	|         ExtIntGPIO           |	<-	fcr = ioBase + fcrOffsetAddress
//	|______________________________|	<-	ioConfigurationBaseAddress
//	|                              |
//	~                              ~
//
//	The I2S DMA Channel is mapped in by the Apple02DBDMAAudioDMAEngine.  Only the I2S control registers are 
//	mapped in by the AudioI2SControl.  The Apple I/O Configuration Space (i.e. FCRs, GPIOs and ExtIntGPIOs)
//	are mapped in by the subclass of Apple02Audio.  The FCRs must also be mapped in by the AudioI2SControl
//	object as the init method must enable the I2S I/O Module for which the AudioI2SControl object is
//	being instantiated for.
//
void	AppleTexas2Audio::sndHWInitialize(IOService *provider)
{
	IOReturn				err;
	IORegistryEntry			*i2s;
	IORegistryEntry			*macio;
	IORegistryEntry			*gpio;
	IORegistryEntry			*i2c;
	IORegistryEntry			*deq;
	IORegistryEntry			*intSource;
    IORegistryEntry			*headphoneMute;
    IORegistryEntry			*ampMute;
	IORegistryEntry			*lineOutMute;								//	[2855519]
	IORegistryEntry			*masterMute;								//	[2933090]
	IORegistryEntry			*codecReset;								//	[2855519]
	OSData					*tmpData;
	IOMemoryMap				*map;
	UInt32					*hdpnMuteGpioAddr;
	UInt32					*ampMuteGpioAddr;
	UInt32					*headphoneExtIntGpioAddr;
	UInt32					*lineOutExtIntGpioAddr;
	UInt32					*dallasExtIntGpioAddr;
	UInt32					*i2cAddrPtr;
	UInt32					*tmpPtr;
	UInt32					*lineOutMuteGpioAddr;						//	[2855519]
	UInt32					*masterMuteGpioAddr;						//	[2933090]
	UInt32					*hwResetGpioAddr;							//	[2855519]
	UInt32					loopCnt;
//	UInt32					myFrameRate;
	UInt8					data[kTexas2BIQwidth];						// space for biggest register size
	UInt8					curValue;

	debugIOLog (3, "+ AppleTexas2Audio::sndHWInitialize");

	i2s		= NULL;		//	audio sample transport layer
	macio	= NULL;		//	parent entry for gpio & I2C
	gpio	= NULL;		//	detects & mutes
	i2c		= NULL;		//	audio control transport layer
	savedNanos = 0;
#if 0
    // Sets the frame rate:
	myFrameRate = frameRate(0);				//	get the fixed point sample rate from the register but expressed as an int
#endif
	FailIf (!provider, Exit);
	ourProvider = provider;
    FailIf (!findAndAttachI2C (provider), Exit);

	i2s = ourProvider->getParentEntry (gIODTPlane);
	FailIf (!i2s, Exit);
	macio = i2s->getParentEntry (gIODTPlane);
	FailIf (!macio, Exit);
	gpio = macio->childFromPath (kGPIODTEntry, gIODTPlane);
	FailIf (!gpio, Exit);
	i2c = macio->childFromPath (kI2CDTEntry, gIODTPlane);
	setProperty (kSpeakerConnectError, speakerConnectFailed);

	hasANDedReset = HasANDedReset();			//	[2855519]

	//	Determine which systems to exclude from the default behavior of releasing the headphone
	//	mute after 200 milliseconds delay [2660341].  Typically this is done for any non-portable
	//	CPU.  Portable CPUs will achieve better battery life by leaving the mute asserted.  Desktop
	//	CPUs have a different amplifier configuration and only want the amplifier quiet during a
	//	detect transition.
	tmpData = OSDynamicCast (OSData, macio->getProperty (kDeviceIDPropName));
	FailIf (!tmpData, Exit);
	deviceID = (UInt32)tmpData->getBytesNoCopy ();

	// get the physical address of the i2c cell that the sound chip (Digital EQ, "deq") is connected to.
    deq = i2c->childFromPath (kDigitalEQDTEntry, gIODTPlane);
	FailIf (!deq, Exit);
	tmpData = OSDynamicCast (OSData, deq->getProperty (kI2CAddress));
	deq->release ();
	FailIf (!tmpData, Exit);
	i2cAddrPtr = (UInt32*)tmpData->getBytesNoCopy ();
	DEQAddress = *i2cAddrPtr;
	
	//
	//	Conventional I2C address nomenclature concatenates a 7 bit address to a 1 bit read/*write bit
	//	 ___ ___ ___ ___ ___ ___ ___ ___
	//	|   |   |   |   |   |   |   |   |
	//	| 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	//	|___|___|___|___|___|___|___|___|
	//	  |   |   |   |   |   |   |   |____	1 = Read, 0 = Write
	//	  |___|___|___|___|___|___|________	7 bit address
	//
	//	The conventional method of referring to the I2C address is to read the address in
	//	place without any shifting of the address to compensate for the Read/*Write bit.
	//	The I2C driver does not use this standardized method of referring to the address
	//	and instead, requires shifting the address field right 1 bit so that the Read/*Write
	//	bit is not passed to the I2C driver as part of the address field.
	//
	DEQAddress = DEQAddress >> 1;

	// get the physical address of the gpio pin for setting the headphone mute
	headphoneMute = FindEntryByProperty (gpio, kAudioGPIO, kHeadphoneAmpEntry);
	FailIf (!headphoneMute, Exit);
	tmpData = OSDynamicCast (OSData, headphoneMute->getProperty (kAAPLAddress));
	FailIf (!tmpData, Exit);
	hdpnMuteGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();
	tmpData = OSDynamicCast (OSData, headphoneMute->getProperty (kAudioGPIOActiveState));
	if (tmpData) {
		tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
		hdpnActiveState = *tmpPtr;
		debugIOLog (3, "hdpnActiveState = 0x%X", hdpnActiveState);
	}

	// take the hard coded memory address that's in the boot rom, and convert it a virtual address
	if (hdpnMuteGpioAddr) {
		debugIOLog (3, "hdpnMuteGpioAddr = 0x%X", (unsigned int)hdpnMuteGpioAddr);
		hdpnMuteRegMem = IODeviceMemory::withRange (*hdpnMuteGpioAddr, sizeof (UInt8));
		map = hdpnMuteRegMem->map (0);
		hdpnMuteGpio = (UInt8*)map->getVirtualAddress ();
	}
	
	//	[2855519]	begin {
	//	Locate a line output amplifier mute control if one exists and setup to manage the control...
	//	This is an optional control that may not exist on all CPU configurations so conditional
	//	execution is appropriate but don't FailIf out of the initialization process if the
	//	line output mute control does not exist.
	
	lineOutMute = FindEntryByProperty ( gpio, kAudioGPIO, kLineOutAmpEntry );
	if ( lineOutMute ) {
		tmpData = OSDynamicCast ( OSData, lineOutMute->getProperty ( kAAPLAddress ) );
		if ( tmpData ) {
			lineOutMuteGpioAddr = (UInt32*)tmpData->getBytesNoCopy();
            if ( lineOutMuteGpioAddr ) {
                debugIOLog (3, "lineOutMuteGpioAddr = 0x%X", (unsigned int)lineOutMuteGpioAddr);
                tmpData = OSDynamicCast ( OSData, lineOutMute->getProperty ( kAudioGPIOActiveState ) );
                if ( tmpData ) {
                    tmpPtr = (UInt32*)tmpData->getBytesNoCopy();
                    lineOutMuteActiveState = *tmpPtr;
                    debugIOLog (3, "lineOutMuteActiveState = %d", (unsigned int)lineOutMuteActiveState);
                    //	Take the hard coded memory address that's in the boot rom and convert it to a virtual address
                    lineOutMuteGpioMem = IODeviceMemory::withRange ( *lineOutMuteGpioAddr, sizeof ( UInt8 ) );
                    map = lineOutMuteGpioMem->map ( 0 );
                    lineOutMuteGpio = (UInt8*)map->getVirtualAddress();
                }
            }
		}
	}	
	
	//	[2855519]	} end
	
	//	[2933090]	begin {
	//	Locate a master output amplifier mute control if one exists and setup to manage the control...
	//	This is an optional control that may not exist on all CPU configurations so conditional
	//	execution is appropriate but don't FailIf out of the initialization process if the
	//	master mute control does not exist.
	
	masterMute = FindEntryByProperty ( gpio, kAudioGPIO, kMasterAmpEntry );
	if ( masterMute ) {
		tmpData = OSDynamicCast ( OSData, masterMute->getProperty ( kAAPLAddress ) );
		if ( tmpData ) {
			masterMuteGpioAddr = (UInt32*)tmpData->getBytesNoCopy();
            if ( masterMuteGpioAddr ) {
                debugIOLog (3, "masterMuteGpioAddr = 0x%X", (unsigned int)masterMuteGpioAddr);
                tmpData = OSDynamicCast ( OSData, masterMute->getProperty ( kAudioGPIOActiveState ) );
                if ( tmpData ) {
                    tmpPtr = (UInt32*)tmpData->getBytesNoCopy();
                    masterMuteActiveState = *tmpPtr;
                    debugIOLog (3, "masterMuteActiveState = %d", (unsigned int)masterMuteActiveState);
                    //	Take the hard coded memory address that's in the boot rom and convert it to a virtual address
                    masterMuteGpioMem = IODeviceMemory::withRange ( *masterMuteGpioAddr, sizeof ( UInt8 ) );
                    map = masterMuteGpioMem->map ( 0 );
                    masterMuteGpio = (UInt8*)map->getVirtualAddress();
                }
            }
		}
	}	
	
	//	[2933090]	} end
	
	//	[2855519]	begin {
	//	Determine audio Codec reset method "A".  Get the physical address of the GPIO pin for applying
	//	a reset to the audio Codec.  Don't FailIf out of this code segment as the Codec reset may be
	//	defined through another property so this object may need to run even if no Codec reset method
	//	is defined through an 'audio-gpio' / 'audio-gpio-active-state' property pair.
	codecReset = FindEntryByProperty ( gpio, kAudioGPIO, kHWResetEntry );
	if ( codecReset ) {
		tmpData = OSDynamicCast ( OSData, codecReset->getProperty ( kAAPLAddress ) );
		if ( tmpData ) {
			hwResetGpioAddr = (UInt32*)tmpData->getBytesNoCopy();
            if ( hwResetGpioAddr ) {
                debugIOLog (3, "hwResetGpioAddr = 0x%X", (unsigned int)hwResetGpioAddr);
                tmpData = OSDynamicCast ( OSData, codecReset->getProperty ( kAudioGPIOActiveState ) );
                if ( tmpData ) {
                    tmpPtr = (UInt32*)tmpData->getBytesNoCopy();
                    hwResetActiveState = *tmpPtr;
                    debugIOLog (3, "hwResetActiveState = %d", (unsigned int)hwResetActiveState);
                    //	Take the hard coded memory address that's in the boot rom and convert it to a virtual address
                    hwResetRegMem = IODeviceMemory::withRange ( *hwResetGpioAddr, sizeof ( UInt8 ) );
                    map = hwResetRegMem->map ( 0 );
                    hwResetGpio = (UInt8*)map->getVirtualAddress();
                }
            }
		}
	}
	//	[2855519]	} end

	intSource = 0;

	//	Get the interrupt provider for the Dallas speaker insertion interrupt.
	//	This can be located by searching for a 'one-wire-bus' property with a
	//	value of 'speaker-id'.  The ExtInt-gpio that is the parent of this
	//	property value is the interrupt provider for the detect.  When a Dallas 
	//	interface is detected by the perferred method, the active state for the 
	//	GPIO is implied as active '1' due to the open collector nature of the 
	//	Dallas interface.  Support the 'audio-gpio' property with a value of 'speaker-detect'.
	intSource = FindEntryByProperty (gpio, kOneWireBusPropName, kSpeakerIDPropValue);
	
	if (NULL != intSource) {
		dallasIntProvider = OSDynamicCast (IOService, intSource);
		FailIf (!dallasIntProvider, Exit);
		
		// get the active state of the dallas speaker inserted pin
		tmpData = OSDynamicCast (OSData, intSource->getProperty (kAudioGPIOActiveState));
		if (!tmpData) {
			dallasInsertedActiveState = 1;
		} else {
			tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
			dallasInsertedActiveState = *tmpPtr;
		}

		// get the physical address of the pin for detecting the dallas speaker insertion/removal
		tmpData = OSDynamicCast (OSData, intSource->getProperty (kAAPLAddress));
		FailIf (!tmpData, Exit);
		dallasExtIntGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();

		// take the hard coded memory address that's in the boot rom, and convert it a virtual address
		dallasExtIntGpioMem = IODeviceMemory::withRange (*dallasExtIntGpioAddr, sizeof (UInt8));
		map = dallasExtIntGpioMem->map (0);
		dallasExtIntGpio = (UInt8*)map->getVirtualAddress ();

		//	Setup Dallas Speaker ID detect for dual edge interrupt
		curValue = *dallasExtIntGpio;
		curValue = curValue | (dualEdge << intEdgeSEL);
		*dallasExtIntGpio = curValue;
	} else {
		debugIOLog (3, "!!!!Couldn't find a dallas speaker interrupt source!!!!");
	}

	intSource = 0;

	// get the interrupt provider for the headphone insertion interrupt
	intSource = FindEntryByProperty (gpio, kAudioGPIO, kHeadphoneDetectInt);
	if (!intSource)
		intSource = FindEntryByProperty (gpio, kCompatible, kKWHeadphoneDetectInt);

	FailIf (!intSource, Exit);
	headphoneIntProvider = OSDynamicCast (IOService, intSource);
	FailIf (!headphoneIntProvider, Exit);

	// We only want to publish the jack state if this hardware has video on its headphone connector
	tmpData = OSDynamicCast (OSData, intSource->getProperty (kVideoPropertyEntry));
	debugIOLog (3, "kVideoPropertyEntry = %p", tmpData);
	if (NULL != tmpData) {
		hasVideo = TRUE;
		gAppleAudioVideoJackStateKey = OSSymbol::withCStringNoCopy ("AppleAudioVideoJackState");
		debugIOLog (3, "has video in headphone");
	}

	// We only want to publish the jack state if this hardware has video on its headphone connector
	tmpData = OSDynamicCast (OSData, intSource->getProperty (kSerialPropertyEntry));
	debugIOLog (3, "kSerialPropertyEntry = %p", tmpData);
	if (NULL != tmpData) {
		hasSerial = TRUE;
		gAppleAudioSerialJackStateKey = OSSymbol::withCStringNoCopy ("AppleAudioSerialJackState");
		debugIOLog (3, "has serial in headphone");
	}

	// get the active state of the headphone inserted pin
	// This should really be gotten from the sound-objects property, but we're not parsing that yet.
	tmpData = OSDynamicCast (OSData, intSource->getProperty (kAudioGPIOActiveState));
	if (NULL == tmpData) {
		headphoneInsertedActiveState = 1;
	} else {
		tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
		headphoneInsertedActiveState = *tmpPtr;
	}
	debugIOLog (3, "headphoneInsertedActiveState = 0x%X", headphoneInsertedActiveState);

	// get the physical address of the pin for detecting the headphone insertion/removal
	tmpData = OSDynamicCast (OSData, intSource->getProperty (kAAPLAddress));
	FailIf (!tmpData, Exit);
	headphoneExtIntGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();
	debugIOLog (3, "headphoneExtIntGpioAddr = 0x%X", (unsigned int)headphoneExtIntGpioAddr);

	// take the hard coded memory address that's in the boot rom, and convert it a virtual address
	headphoneExtIntGpioMem = IODeviceMemory::withRange (*headphoneExtIntGpioAddr, sizeof (UInt8));
	map = headphoneExtIntGpioMem->map (0);
	headphoneExtIntGpio = (UInt8*)map->getVirtualAddress ();
	
	//	Set interrupt source for dual edge to support jack insertion & removal
	curValue = *headphoneExtIntGpio;
	curValue = curValue | (dualEdge << intEdgeSEL);
	*headphoneExtIntGpio = curValue;
	
	//	begin {	[2878119]
	intSource = 0;
	// get the interrupt provider for the line output jack insertion & removal interrupt
	intSource = FindEntryByProperty (gpio, kAudioGPIO, kLineOutDetectInt);
	if ( 0 != intSource ) {
		debugIOLog (3,  "##### Found LINE OUT DETECT!" );
		lineOutIntProvider = OSDynamicCast (IOService, intSource);
		if ( lineOutIntProvider ) {
			// get the active state of the line output inserted pin
			// This should really be gotten from the sound-objects property, but we're not parsing that yet.
			tmpData = OSDynamicCast (OSData, intSource->getProperty (kAudioGPIOActiveState));
			if (NULL != tmpData) {
				tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
				lineOutExtIntActiveState = *tmpPtr;
				debugIOLog (3, "lineOutInsertedActiveState = 0x%X", lineOutExtIntActiveState);
			
				// get the physical address of the pin for detecting the line output insertion/removal
				tmpData = OSDynamicCast (OSData, intSource->getProperty (kAAPLAddress));
				if ( tmpData ) {
					lineOutExtIntGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();
					debugIOLog (3, "lineOutExtIntGpioAddr = 0x%X", (unsigned int)lineOutExtIntGpioAddr);
				
					// take the hard coded memory address that's in the boot rom, and convert it a virtual address
					lineOutExtIntGpioMem = IODeviceMemory::withRange (*lineOutExtIntGpioAddr, sizeof (UInt8));
					map = lineOutExtIntGpioMem->map (0);
					lineOutExtIntGpio = (UInt8*)map->getVirtualAddress ();
					
					//	Set interrupt source for dual edge to support jack insertion & removal
					curValue = *lineOutExtIntGpio;
					curValue = curValue | (dualEdge << intEdgeSEL);
					*lineOutExtIntGpio = curValue;
				}
			}
		}
	}
	//	[2878119]	} end

	// get the physical address of the gpio pin for setting the amplifier mute
	ampMute = FindEntryByProperty (gpio, kAudioGPIO, kAmpEntry);
	FailIf (!ampMute, Exit);
	tmpData = OSDynamicCast (OSData, ampMute->getProperty (kAAPLAddress));
	FailIf (!tmpData, Exit);
	ampMuteGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();
	debugIOLog (3, "ampMuteGpioAddr = 0x%X", (unsigned int)ampMuteGpioAddr);
	tmpData = OSDynamicCast (OSData, ampMute->getProperty (kAudioGPIOActiveState));
	tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
	ampActiveState = *tmpPtr;
	debugIOLog (3, "ampActiveState = 0x%X", ampActiveState);

	// take the hard coded memory address that's in the boot rom, and convert it a virtual address
	ampMuteRegMem = IODeviceMemory::withRange (*ampMuteGpioAddr, sizeof (UInt8));
	map = ampMuteRegMem->map (0);
	ampMuteGpio = (UInt8*)map->getVirtualAddress ();

	layoutID = GetDeviceID ();
	debugIOLog (3, "layoutID = %ld", layoutID);
	
	i2sSerialFormat = ( kClockSource45MHz | ( 1 << kMClkDivisorShift ) | ( 1 << kSClkDivisorShift ) | kSClkMaster | kSerialFormat64x );

	drc.compressionRatioNumerator	= kDrcRatioNumerator;
	drc.compressionRatioDenominator	= kDrcRationDenominator;
	drc.threshold					= kDrcThresholdMax;
	drc.maximumVolume				= kDefaultMaximumVolume;
	drc.enable						= false;

	//	Initialize the Texas2 as follows:
	//		Mode:					normal
	//		SCLK:					64 fs
	//		input serial mode:		i2s
	//		output serial mode:		i2s
	//		serial word length:		16 bits
	//		Dynamic range control:	disabled
	//		Volume (left & right):	muted
	//		Treble / Bass:			unity
	//		Biquad filters:			unity
	//	Initialize the Texas2 registers the same as the Texas2 with the following additions:
	//		AnalogPowerDown:		normal
	//		
	data[0] = ( kNormalLoad << kFL ) | ( k64fs << kSC ) | TAS_I2S_MODE | ( TAS_WORD_LENGTH << kW0 );
	Texas2_WriteRegister( kTexas2MainCtrl1Reg, data );	//	default to normal load mode, 16 bit I2S

	data[DRC_AboveThreshold]	= kDisableDRC;
	data[DRC_BelowThreshold]	= kDRCBelowThreshold1to1;
	data[DRC_Threshold]			= kDRCUnityThreshold;
	data[DRC_Integration]		= kDRCIntegrationThreshold;
	data[DRC_Attack]			= kDRCAttachThreshold;
	data[DRC_Decay]				= kDRCDecayThreshold;
	Texas2_WriteRegister( kTexas2DynamicRangeCtrlReg, data );

	for( loopCnt = 0; loopCnt < kTexas2VOLwidth; loopCnt++ )				//	init to volume = muted
	    data[loopCnt] = 0;
	Texas2_WriteRegister( kTexas2VolumeCtrlReg, data );

	data[0] = 0x72;									//	treble = bass = unity 0.0 dB
	Texas2_WriteRegister( kTexas2TrebleCtrlReg, data );
	Texas2_WriteRegister( kTexas2BassCtrlReg, data );

	data[0] = 0x10;								//	output mixer output channel to unity = 0.0 dB
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;								//	output mixer call progress channel to mute = -70.0 dB
	data[4] = 0x00;
	data[5] = 0x00;
	data[6] = 0x00;								//	output mixer analog playthrough channel to mute = -70.0 dB
	data[7] = 0x00;
	data[8] = 0x00;
	Texas2_WriteRegister( kTexas2MixerLeftGainReg, data );	//	initialize left channel
	Texas2_WriteRegister( kTexas2MixerRightGainReg, data );	//	initialize right channel

	for( loopCnt = 1; loopCnt < kTexas2BIQwidth; loopCnt++ )				//	all biquads to unity gain all pass mode
		data[loopCnt] = 0x00;
	data[0] = 0x10;

	Texas2_WriteRegister( kTexas2LeftBiquad0CtrlReg, data );
	Texas2_WriteRegister( kTexas2LeftBiquad1CtrlReg, data );
	Texas2_WriteRegister( kTexas2LeftBiquad2CtrlReg, data );
	Texas2_WriteRegister( kTexas2LeftBiquad3CtrlReg, data );
	Texas2_WriteRegister( kTexas2LeftBiquad4CtrlReg, data );
	Texas2_WriteRegister( kTexas2LeftBiquad5CtrlReg, data );
	Texas2_WriteRegister( kTexas2LeftBiquad6CtrlReg, data );
	Texas2_WriteRegister( kTexas2LeftLoudnessBiquadReg, data );

	Texas2_WriteRegister( kTexas2RightBiquad0CtrlReg, data );
	Texas2_WriteRegister( kTexas2RightBiquad1CtrlReg, data );
	Texas2_WriteRegister( kTexas2RightBiquad2CtrlReg, data );
	Texas2_WriteRegister( kTexas2RightBiquad3CtrlReg, data );
	Texas2_WriteRegister( kTexas2RightBiquad4CtrlReg, data );
	Texas2_WriteRegister( kTexas2RightBiquad5CtrlReg, data );
	Texas2_WriteRegister( kTexas2RightBiquad6CtrlReg, data );
	Texas2_WriteRegister( kTexas2RightLoudnessBiquadReg, data );
	
	data[0] = 0x00;	//	loudness gain to mute
	Texas2_WriteRegister( kTexas2LeftLoudnessBiquadGainReg, data );
	Texas2_WriteRegister( kTexas2RightLoudnessBiquadGainReg, data );
	
	data[0] = ( kADMNormal << kADM ) | ( kDeEmphasisOFF << kADM ) | ( kPowerDownAnalog << kAPD );
	Texas2_WriteRegister( kTexas2AnalogControlReg, data );
	
	data[0] = ( kAllPassFilter << kAP ) | ( kNormalBassTreble << kDL );
	Texas2_WriteRegister( kTexas2MainCtrl2Reg, data );

	// All this config should go in a single method: 
	map = provider->mapDeviceMemoryWithIndex (Apple02DBDMAAudioDMAEngine::kDBDMADeviceIndex);
	FailIf (!map, Exit);
    // the i2s stuff is in a separate class.  make an instance of the class and store
    AudioI2SInfo tempInfo;
    tempInfo.map = map;
    tempInfo.i2sSerialFormat = kSerialFormat64x;					//	[3060321]	rbm	2 Oct 2002
    
    // create an object, this will get inited.
    audioI2SControl = AudioI2SControl::create(&tempInfo) ;
    FailIf (NULL == audioI2SControl, Exit);
    audioI2SControl->retain();

	err = Texas2_Initialize();			//	flush the shadow contents to the HW
	IOSleep (1);
	ToggleAnalogPowerDownWake();

	sndHWSetPowerState ( kIOAudioDeviceIdle );			//	initialize to the idle state
Exit:
	if (NULL != gpio)
		gpio->release ();
	if (NULL != i2c)
		i2c->release ();

	debugIOLog (3, "- AppleTexas2Audio::sndHWInitialize");
}

// --------------------------------------------------------------------------
void AppleTexas2Audio::sndHWPostDMAEngineInit (IOService *provider) {
	IOWorkLoop				*workLoop;

	debugIOLog (3, "+ AppleTexas2Audio::sndHWPostDMAEngineInit");

	if (NULL != driverDMAEngine) {
		driverDMAEngine->setSampleLatencies (kTexas2OutputSampleLatency, kTexas2InputSampleLatency);
		driverDMAEngine->setRightChanDelay(TRUE);		// [3134221] aml
	}
	
	if (layoutID == layoutQ16) {
		OSDictionary *		AOAprop;
		OSData *			volumeData;
		UInt32				volume;

		driverDMAEngine->setBalanceAdjust(TRUE);

		if (AOAprop = OSDynamicCast(OSDictionary, getProperty("AOAAttributes") )) {
			volumeData = (OSDynamicCast(OSData, AOAprop->getObject("leftBalanceAdjust")));			
			if (NULL != volumeData) {
				memcpy (&(volume), volumeData->getBytesNoCopy (), 4);
				driverDMAEngine->setLeftBalanceAdjust (volume);
			} else {
				driverDMAEngine->setLeftBalanceAdjust (NULL);
			}
			volumeData = (OSDynamicCast(OSData, AOAprop->getObject("rightBalanceAdjust")));
			if (NULL != volumeData) {
				memcpy (&(volume), volumeData->getBytesNoCopy (), 4);
				driverDMAEngine->setRightBalanceAdjust (volume);
			} else {
				driverDMAEngine->setRightBalanceAdjust (NULL);
			}
		}
	}
		
	dallasDriver = NULL;
	dallasDriverNotifier = addNotification (gIOPublishNotification, serviceMatching ("AppleDallasDriver"), (IOServiceNotificationHandler)&DallasDriverPublished, this);
	if (NULL != dallasDriver)
		dallasDriverNotifier->remove ();

	workLoop = getWorkLoop();
	FailIf (NULL == workLoop, Exit);

	if (NULL != headphoneIntProvider) {
		headphoneIntEventSource = IOInterruptEventSource::interruptEventSource ((OSObject *)this,
																			headphoneInterruptHandler,
																			headphoneIntProvider,
																			0);
		FailIf (NULL == headphoneIntEventSource, Exit);
		workLoop->addEventSource (headphoneIntEventSource);
	}
	
	//	begin {	[2878119]
	if ( NULL != lineOutIntProvider ) {
		lineOutIntEventSource = IOInterruptEventSource::interruptEventSource ((OSObject *)this,
																			lineOutInterruptHandler,
																			lineOutIntProvider,
																			0);
		FailIf (NULL == lineOutIntEventSource, Exit);
		workLoop->addEventSource (lineOutIntEventSource);

		if (NULL != outputSelector) {
			outputSelector->addAvailableSelection(kIOAudioOutputPortSubTypeLine, "LineOut");
		}
	}
	//	[2878119]	} end
	
	// Create a (primary) device interrupt source for dallas and attach it to the work loop
	if (NULL != dallasIntProvider) {
		// Create a timer event source
		dallasHandlerTimer = IOTimerEventSource::timerEventSource (this, DallasInterruptHandlerTimer);
		if (NULL != dallasHandlerTimer) {
			workLoop->addEventSource (dallasHandlerTimer);
		}

 		notifierHandlerTimer = IOTimerEventSource::timerEventSource (this, DisplaySpeakersNotFullyConnected);
		if (NULL != notifierHandlerTimer) {
			workLoop->addEventSource (notifierHandlerTimer);
		}

		dallasIntEventSource = IOFilterInterruptEventSource::filterInterruptEventSource (this,
																			dallasInterruptHandler, 
																			interruptFilter,
																			dallasIntProvider, 
																			0);
		FailIf (NULL == dallasIntEventSource, Exit);
		workLoop->addEventSource (dallasIntEventSource);

		if (NULL != outputSelector) {
			outputSelector->addAvailableSelection(kIOAudioOutputPortSubTypeExternalSpeaker, "ExtSpeakers");
		}
	}

	//	Timing for systems with a master mute [see 2949185] is as follows:
	//					           ____________________
	//	MASTER_MUTE:	__________|
	//					                     __________
	//	SPEAKER MUTE:	____________________|
	//
	//					                     __________
	//	HEADPHONE MUTE:	____________________|
	//
	//				  -->| 200 MS |<--
	//					       -->| 500 MS |<--
	//
	if ( NULL != masterMuteGpio ) {			//	[2949185] begin {
		if ( masterMuteActiveState == GpioRead( masterMuteGpio ) ) {					//	delay & release only if master is muted
			IOSleep ( 200 );
			SetAmplifierMuteState (kMASTER_AMP, NEGATE_GPIO (masterMuteActiveState));	//	unmute
			IOSleep ( 1000 );
		}
	}										//	} end [2949185]

	if (NULL != dallasDriver && TRUE == IsSpeakerConnected ()) {
		UInt8				bEEPROM[32];
		Boolean				result;

		debugIOLog (3, "sndHWPostDMAEngineInit: About to get the speaker ID");
		speakerID = 0;
		familyID = 0;
		// dallasIntEventSource isn't enabled yet, so we don't have to disable it before this call
		result = dallasDriver->getSpeakerID (bEEPROM);
		if (TRUE == result) {				//	[2965804]
			// The speakers have been successfully probed
			dallasSpeakersProbed = TRUE;
			familyID = bEEPROM[0];
			speakerID = bEEPROM[1];
			debugIOLog (3, "dallasDeviceFamily = %d, dallasDeviceType = %d, dallasDeviceSubType = %d", (unsigned int)bEEPROM[0], (unsigned int)bEEPROM[1], (unsigned int)bEEPROM[2]);
			SelectOutputAndLoadEQ();
		} else {
			debugIOLog (3, "speakerID unknown, probe failed");
		}
	}

	if ( kIOAudioDeviceActive == mCurrentPowerState ) {  //  [3731023]		begin   {   Do not perform interrupt dispatch when hardware is not awake
		if (FALSE == IsHeadphoneConnected ()) {
			SelectOutputAndLoadEQ();			//	[2878119]
			if (TRUE == hasVideo) {
				// Tell the video driver about the jack state change in case a video connector was plugged in
				publishResource (gAppleAudioVideoJackStateKey, kOSBooleanFalse);
			}
			if (TRUE == hasSerial) {
				// Tell the serial driver about the jack state change in case a serial connector was plugged in
				publishResource (gAppleAudioSerialJackStateKey, kOSBooleanFalse);
			}
			if (NULL != dallasIntProvider) {
				// Set the correct EQ
				DallasInterruptHandlerTimer (this, 0);
			} else {
				DeviceInterruptService ();
			}
		} else {
			if (NULL != headphoneIntProvider) {
				// Set amp mutes accordingly
				RealHeadphoneInterruptHandler (0, 0);
			}
		}
	}													//  }   end		[3731023]

	if (NULL == outVolRight && NULL != outVolLeft) {
		// If they are running mono at boot time, set the right channel's last value to an illegal value
		// so it will come up in stereo and center balanced if they plug in speakers or headphones later.
		lastRightVol = kOUT_OF_BOUNDS_VOLUME_VALUE;
		lastLeftVol = outVolLeft->getIntValue ();
	}

	if ( kIOAudioDeviceActive == mCurrentPowerState ) {  //  [3731023]		begin   {   Do not perform interrupt dispatch when hardware is not awake
		//	begin {	[2878119]
		if (FALSE == IsLineOutConnected ()) {
			SelectOutputAndLoadEQ();			//	[2878119]
		} else {
			if (NULL != lineOutIntProvider) {
				// Set amp mutes accordingly
				RealLineOutInterruptHandler (0, 0);
			}
		}
	}													//  }   end		[3731023]
	
	if (NULL != lineOutIntEventSource) {
		lineOutIntEventSource->enable ();
	}	//	[2878119]	} end

	if (NULL != headphoneIntEventSource) {
		headphoneIntEventSource->enable ();
	}
	
	if (NULL != dallasIntEventSource) {
		debugIOLog (3,  "... dallasIntEventSource->enable" );
		dallasIntEventSource->enable ();
	}

Exit:
	debugIOLog (3, "- AppleTexas2Audio::sndHWPostDMAEngineInit");
	return;
}


// --------------------------------------------------------------------------
IOReturn	AppleTexas2Audio::SetAnalogPowerDownMode( UInt8 mode )
{
	IOReturn	err;
	UInt8		dataBuffer[kTexas2ANALOGCTRLREGwidth];
	
	err = kIOReturnSuccess;
	if ( kPowerDownAnalog == mode || kPowerNormalAnalog == mode ) {
		err = Texas2_ReadRegister( kTexas2AnalogControlReg, dataBuffer );
		FailIf ( kIOReturnSuccess != err, Exit );
		if ( kIOReturnSuccess == err ) {
			dataBuffer[0] &= ~( kAPD_MASK << kAPD );
			dataBuffer[0] |= ( mode << kAPD );
			err = Texas2_WriteRegister( kTexas2AnalogControlReg, dataBuffer );
			FailIf ( kIOReturnSuccess != err, Exit );
		}
	}
Exit:
	return err;
}


// --------------------------------------------------------------------------
IOReturn	AppleTexas2Audio::ToggleAnalogPowerDownWake( void )
{
	IOReturn	err;
	
	err = SetAnalogPowerDownMode (kPowerDownAnalog);
	if (kIOReturnSuccess == err) {
		err = SetAnalogPowerDownMode (kPowerNormalAnalog);
	}
	return err;
}


// --------------------------------------------------------------------------
UInt32	AppleTexas2Audio::sndHWGetInSenseBits(
	void)
{
	debugIOLog (3, "+ AppleTexas2Audio::sndHWGetInSenseBits");
	debugIOLog (3, "- AppleTexas2Audio::sndHWGetInSenseBits");
	return 0;	  
}

// --------------------------------------------------------------------------
// we can't read the registers back, so return the value in the shadow reg.
UInt32	AppleTexas2Audio::sndHWGetRegister(
	UInt32 regNum)
{
	debugIOLog (3, "x AppleTexas2Audio::sndHWGetRegister");
	
	UInt32	returnValue = 0;
	
	 return returnValue;
}

// --------------------------------------------------------------------------
// set the reg over i2c and make sure the value is cached in the shadow reg so we can "get it back"
IOReturn  AppleTexas2Audio::sndHWSetRegister(
	UInt32 regNum, 
	UInt32 val)
{
	debugIOLog (3, "+ AppleTexas2Audio::sndHWSetRegister");
	IOReturn myReturn = kIOReturnSuccess;
	debugIOLog (3, "- AppleTexas2Audio::sndHWSetRegister");
	return(myReturn);
}

UInt32 AppleTexas2Audio::sndHWGetCurrentSampleFrame (void) {
	return audioI2SControl->GetFrameCountReg ();
}

void AppleTexas2Audio::sndHWSetCurrentSampleFrame (UInt32 value) {
	audioI2SControl->SetFrameCountReg (value);
}

#pragma mark +HARDWARE IO ACTIVATION
/************************** Manipulation of input and outputs ***********************/
/********(These functions are enough to implement the simple UI policy)**************/

// --------------------------------------------------------------------------
UInt32	AppleTexas2Audio::sndHWGetActiveOutputExclusive(
	void)
{
	debugIOLog (3, "+ AppleTexas2Audio::sndHWGetActiveOutputExclusive");
	debugIOLog (3, "- AppleTexas2Audio::sndHWGetActiveOutputExclusive");
	return 0;
}

// --------------------------------------------------------------------------
IOReturn   AppleTexas2Audio::sndHWSetActiveOutputExclusive(
	UInt32 outputPort )
{
	debugIOLog (3, "+ AppleTexas2Audio::sndHWSetActiveOutputExclusive");

	IOReturn myReturn = kIOReturnSuccess;

	debugIOLog (3, "- AppleTexas2Audio::sndHWSetActiveOutputExclusive");
	return(myReturn);
}

// --------------------------------------------------------------------------
UInt32	AppleTexas2Audio::sndHWGetActiveInputExclusive(
	void)
{
	debugIOLog (3, "+ AppleTexas2Audio::sndHWGetActiveInputExclusive");
	debugIOLog (3, "- AppleTexas2Audio::sndHWGetActiveInputExclusive");
	return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Set the hardware to select the desired input port after validating
//	that the target input port is available. 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	The Texas2 supports mutually exclusive selection of one of four analog
//	input signals.  There is no provision for selection of a 'none' input.
//	Mapping of input selections is as follows:
//		kSndHWInput1:		Texas2 analog input A, Stereo
//		kSndHWInput2:		Texas2 analog input B, Stereo
//		kSndHWInput3:		Texas2 analog input B, Mono sourced from left
//		kSndHWInput4:		Texas2 analog input B, Mono sourced from right
//	Only a subset of the available inputs are implemented on any given CPU
//	due to the multiple modes that analog input B can operate in.  The
//	'sound-objects' describes the resident hardware implemenation regarding
//	available inputs and the type of input.  It is possible to achieve an
//	equivalent selection of none by collecting the connections to the analog
//	ports.  If the Texas2 stereo input A is unused then kSndHWInput1 may
//	be aliased as kSndHWInputNone.  If neither of the Texas2 input B
//	ports are used then kSndHWInput2 may be aliased as kSndHWInputNone.  If
//	one of the Texas2 input B ports is used as a mono input port but the 
//	other input B mono input port remains unused then the unused mono input
//	port (i.e. kSndHWInput3 for the left channel or kSndHWInput4 for the
//	right channel) may be aliased as kSndHWInputNone.
IOReturn   AppleTexas2Audio::sndHWSetActiveInputExclusive( UInt32 input )
{
    UInt8		data[kTexas2MaximumRegisterWidth];
    IOReturn	result; 
    
    debugIOLog (3, "+ AppleTexas2Audio::sndHWSetActiveInputExclusive (%ld)", input);

	//	Mask off the current input selection and then OR in the new selections
	result = Texas2_ReadRegister (kTexas2AnalogControlReg, data);
	FailIf ( kIOReturnSuccess != result, Exit );

	data[0] &= ~((1 << kADM) | (1 << kLRB) | (1 << kINP));
    switch (input) {
        case kSndHWInput1:
			data[0] |= ((kADMNormal << kADM) | (kLeftInputForMonaural << kLRB) | (kAnalogInputA << kINP));			
			driverDMAEngine->setRightChanDelayInput(false);		// [3173869]
			break;
        case kSndHWInput2:
			data[0] |= ((kADMNormal << kADM) | (kLeftInputForMonaural << kLRB) | (kAnalogInputB << kINP));			
			driverDMAEngine->setRightChanDelayInput(false);		// [3173869]
			break;
        case kSndHWInput3:
			data[0] |= ((kADMBInputsMonaural << kADM) | (kLeftInputForMonaural << kLRB) | (kAnalogInputB << kINP));	
			driverDMAEngine->setRightChanDelayInput(true);		// [3173869]
			break;
        case kSndHWInput4:
			data[0] |= ((kADMBInputsMonaural << kADM) | (kRightInputForMonaural << kLRB) | (kAnalogInputB << kINP));	
			driverDMAEngine->setRightChanDelayInput(true);		// [3173869]
			break;
        default:			result = kIOReturnError;																					
			break;
    }
	FailIf ( kIOReturnSuccess != result, Exit );

	//	If the new input selection remains valid then flush the
	//	selection setting out to the hardware.
	result = Texas2_WriteRegister ( kTexas2AnalogControlReg, data );
	FailIf ( kIOReturnSuccess != result, Exit );

	//	Update history of current input selection for sndHWGetActiveInputExclusive
	mActiveInput = input;

Exit:
    debugIOLog (3, "- AppleTexas2Audio::sndHWSetActiveInputExclusive");
    return(result);
}

#pragma mark +CONTROL FUNCTIONS
// control function

// --------------------------------------------------------------------------
bool AppleTexas2Audio::sndHWGetSystemMute(void)
{
	debugIOLog (3, "+ AppleTexas2Audio::sndHWGetSystemMute");
	debugIOLog (3, "- AppleTexas2Audio::sndHWGetSystemMute");
	return (gVolMuteActive);
}

// --------------------------------------------------------------------------
IOReturn AppleTexas2Audio::sndHWSetSystemMute(bool mutestate)
{
	IOReturn						result;

	result = kIOReturnSuccess;

	debugIOLog (3, "+ AppleTexas2Audio::sndHWSetSystemMute");

	if (true == mutestate) {
		// mute the part
		gVolMuteActive = mutestate ;
		result = SetVolumeCoefficients (0, 0);
	} else {
		// unmute the part
		gVolMuteActive = mutestate ;
		result = SetVolumeCoefficients (volumeTable[(UInt32)gVolLeft], volumeTable[(UInt32)gVolRight]);
	}

	debugIOLog (3, "- AppleTexas2Audio::sndHWSetSystemMute");
	return (result);
}

// --------------------------------------------------------------------------
bool AppleTexas2Audio::sndHWSetSystemVolume(UInt32 leftVolume, UInt32 rightVolume)
{
	bool					result;

	result = TRUE;

	debugIOLog (3, "+ AppleTexas2Audio::sndHWSetSystemVolume (left: %ld, right %ld)", leftVolume, rightVolume);
	
	if ( ( gVolLeft != (SInt32)leftVolume ) || ( gVolRight != (SInt32)rightVolume ) ) {
		gVolLeft = leftVolume;
		gVolRight = rightVolume;
		result = ( kIOReturnSuccess == SetVolumeCoefficients ( volumeTable[(UInt32)gVolLeft], volumeTable[(UInt32)gVolRight] ) );
	}

	debugIOLog (3, "- AppleTexas2Audio::sndHWSetSystemVolume");
	return result;
}

// --------------------------------------------------------------------------
IOReturn AppleTexas2Audio::sndHWSetSystemVolume(UInt32 value)
{
	debugIOLog (3, "+ AppleTexas2Audio::sndHWSetSystemVolume (vol: %ld)", value);
	
	IOReturn myReturn = kIOReturnError;
		
	// just call the default function in this class with the same val for left and right.
	if ( true == sndHWSetSystemVolume ( value, value ) ) {
		myReturn = kIOReturnSuccess;
	}

	debugIOLog (3, "- AppleTexas2Audio::sndHWSetSystemVolume");
	return(myReturn);
}

// --------------------------------------------------------------------------
IOReturn AppleTexas2Audio::sndHWSetPlayThrough(bool playthroughstate)
{
	UInt8		leftMixerGain[kTexas2MIXERGAINwidth];
	UInt8		rightMixerGain[kTexas2MIXERGAINwidth];
	IOReturn	err;
	
	debugIOLog (3, "+ AppleTexas2Audio::sndHWSetPlayThrough");
	err = Texas2_ReadRegister ( kTexas2MixerLeftGainReg, leftMixerGain );
	FailIf ( kIOReturnSuccess != err, Exit );

	err = Texas2_ReadRegister ( kTexas2MixerRightGainReg, rightMixerGain );
	FailIf ( kIOReturnSuccess != err, Exit );

	if ( usesDigitalPlaythrough() ) {
		if ( playthroughstate ) {
			leftMixerGain[3] = 0x10;
			rightMixerGain[3] = 0x10;
		} else {
			leftMixerGain[3] = 0x00;
			rightMixerGain[3] = 0x00;
		}
	} else {
		if ( playthroughstate ) {
			leftMixerGain[6] = 0x10;
			rightMixerGain[6] = 0x10;
		} else {
			leftMixerGain[6] = 0x00;
			rightMixerGain[6] = 0x00;
		}
	}
	err = Texas2_WriteRegister ( kTexas2MixerLeftGainReg, leftMixerGain );
	FailIf ( kIOReturnSuccess != err, Exit );

	err = Texas2_WriteRegister ( kTexas2MixerRightGainReg, rightMixerGain );
	FailIf ( kIOReturnSuccess != err, Exit );

Exit:
	debugIOLog (3, "- AppleTexas2Audio::sndHWSetPlayThrough");
	return err;
}

// --------------------------------------------------------------------------
IOReturn AppleTexas2Audio::sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain) 
{
	debugIOLog (3, "+ AppleTexas2Audio::sndHWSetPlayThrough");

	IOReturn myReturn = kIOReturnSuccess;

	debugIOLog (3, "- AppleTexas2Audio::sndHWSetPlayThrough");
	return(myReturn);
}


// --------------------------------------------------------------------------
// You either have only a master volume control, or you have both volume controls.
IOReturn AppleTexas2Audio::AdjustControls (void) {
	IOFixed							mindBVol;
	IOFixed							maxdBVol;
	Boolean							mustUpdate;

	debugIOLog ( 6, "+ AppleTexas2Audio::AdjustControls ()" );
	
	FailIf (NULL == driverDMAEngine, Exit);
	mustUpdate = FALSE;

	mindBVol = volumedBTable[minVolume];
	maxdBVol = volumedBTable[maxVolume];

	//	Must update if any of the following conditions exist:
	//	1.	No master volume control exists AND the master volume is the target
	//	2.	The master volume control exists AND the master volume is not the target
	//	3.	the minimum or maximum dB volume setting for the left volume control changes
	//	4.	the minimum or maximum dB volume setting for the right volume control changes
	
	if ((NULL == outVolMaster && TRUE == useMasterVolumeControl) ||
		(NULL != outVolMaster && FALSE == useMasterVolumeControl) ||
		(NULL != outVolLeft && outVolLeft->getMinValue () != minVolume) ||
		(NULL != outVolLeft && outVolLeft->getMaxValue () != maxVolume) ||
		(NULL != outVolRight && outVolRight->getMinValue () != minVolume) ||
		(NULL != outVolRight && outVolRight->getMaxValue () != maxVolume)) {
		mustUpdate = TRUE;
	}

	if (TRUE == mustUpdate) {
		debugIOLog (3, "AdjustControls: mindBVol = %lx, maxdBVol = %lx", mindBVol, maxdBVol);
	
		driverDMAEngine->pauseAudioEngine ();
		driverDMAEngine->beginConfigurationChange ();
	
		if (TRUE == useMasterVolumeControl) {
			// We have only the master volume control (possibly not created yet) and have to remove the other volume controls (possibly don't exist)
			if (NULL == outVolMaster) {
				// remove the existing left and right volume controls
				if (NULL != outVolLeft) {
					lastLeftVol = outVolLeft->getIntValue ();
					driverDMAEngine->removeDefaultAudioControl (outVolLeft);
					outVolLeft = NULL;
				}
		
				if (NULL != outVolRight) {
					lastRightVol = outVolRight->getIntValue ();
					driverDMAEngine->removeDefaultAudioControl (outVolRight);
					outVolRight = NULL;
				}
	
				// Create the master control
				debugIOLog ( 6, "  outVolMaster: lastMasterVol = 0x%lX, minVolume = %d, maxVolume = %d, mindBVol = 0x%lX, maxdBVol = 0x%lX", (lastLeftVol + lastRightVol) / 2, minVolume, maxVolume, mindBVol, maxdBVol );
				outVolMaster = IOAudioLevelControl::createVolumeControl((lastLeftVol + lastRightVol) / 2, minVolume, maxVolume, mindBVol, maxdBVol,
													kIOAudioControlChannelIDAll,
													kIOAudioControlChannelNameAll,
													kOutVolMaster, 
													kIOAudioControlUsageOutput);
	
				if (NULL != outVolMaster) {
					driverDMAEngine->addDefaultAudioControl(outVolMaster);
					outVolMaster->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
					outVolMaster->flushValue ();
				}
			}
		} else {
			// or we have both controls (possibly not created yet) and we have to remove the master volume control (possibly doesn't exist)
			if (NULL == outVolLeft) {
				// Have to create the control again...
				if (lastLeftVol > kMAXIMUM_LEGAL_VOLUME_VALUE && NULL != outVolMaster) {
					lastLeftVol = outVolMaster->getIntValue ();
				}
				debugIOLog ( 6, "  outVolLeft: lastLeftVol = 0x%lX, minVolume = %d, maxVolume = %d, mindBVol = 0x%lX, maxdBVol = 0x%lX", lastLeftVol, kMinimumVolume, kMaximumVolume, mindBVol, maxdBVol );
				outVolLeft = IOAudioLevelControl::createVolumeControl (lastLeftVol, kMinimumVolume, kMaximumVolume, mindBVol, maxdBVol,
													kIOAudioControlChannelIDDefaultLeft,
													kIOAudioControlChannelNameLeft,
													kOutVolLeft,
													kIOAudioControlUsageOutput);
				if (NULL != outVolLeft) {
					driverDMAEngine->addDefaultAudioControl (outVolLeft);
					outVolLeft->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
				}
			}
			
			if (NULL == outVolRight) {
				// Have to create the control again...
				if (lastRightVol > kMAXIMUM_LEGAL_VOLUME_VALUE && NULL != outVolMaster) {
					lastRightVol = outVolMaster->getIntValue ();
				}
				debugIOLog ( 6, "  outVolRight: lastRightVol = 0x%lX, minVolume = %d, maxVolume = %d, mindBVol = 0x%lX, maxdBVol = 0x%lX", lastRightVol, kMinimumVolume, kMaximumVolume, mindBVol, maxdBVol );
				outVolRight = IOAudioLevelControl::createVolumeControl (lastRightVol, kMinimumVolume, kMaximumVolume, mindBVol, maxdBVol,
													kIOAudioControlChannelIDDefaultRight,
													kIOAudioControlChannelNameRight,
													kOutVolRight,
													kIOAudioControlUsageOutput);
				if (NULL != outVolRight) {
					driverDMAEngine->addDefaultAudioControl (outVolRight);
					outVolRight->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
				}
			}
	
			if (NULL != outVolMaster) {
				driverDMAEngine->removeDefaultAudioControl (outVolMaster);
				outVolMaster = NULL;
			}
		}
	
		if (NULL != outVolMaster) {
			outVolMaster->setMinValue (minVolume);
			outVolMaster->setMinDB (mindBVol);
			outVolMaster->setMaxValue (maxVolume);
			outVolMaster->setMaxDB (maxdBVol);
			if (outVolMaster->getIntValue () > maxVolume) {
				outVolMaster->setValue (maxVolume);
			}
			outVolMaster->flushValue ();
		}
	
		if (NULL != outVolLeft) {
			outVolLeft->setMinValue (minVolume);
			outVolLeft->setMinDB (mindBVol);
			outVolLeft->setMaxValue (maxVolume);
			outVolLeft->setMaxDB (maxdBVol);
			if (outVolLeft->getIntValue () > maxVolume) {
				outVolLeft->setValue (maxVolume);
			}
			outVolLeft->flushValue ();
		}
	
		if (NULL != outVolRight) {
			outVolRight->setMinValue (minVolume);
			outVolRight->setMinDB (mindBVol);
			outVolRight->setMaxValue (maxVolume);
			outVolRight->setMaxDB (maxdBVol);
			if (outVolRight->getIntValue () > maxVolume) {
				outVolRight->setValue (maxVolume);
			}
			outVolRight->flushValue ();
		}
	
		driverDMAEngine->completeConfigurationChange ();
		driverDMAEngine->resumeAudioEngine ();
	}

Exit:
	debugIOLog ( 6, "- AppleTexas2Audio::AdjustControls () returns 0x%lX", kIOReturnSuccess );
	return kIOReturnSuccess;
}

#pragma mark +INDENTIFICATION

// --------------------------------------------------------------------------
// ::sndHWGetType
UInt32 AppleTexas2Audio::sndHWGetType(void )
{
	UInt32				returnValue;

	debugIOLog (3, "+ AppleTexas2Audio::sndHWGetType");
 
	// in AudioHardwareConstants.h need to set up a constant for the hardware type.
	returnValue = kSndHWTypeTexas2;

	debugIOLog (3, "- AppleTexas2Audio::sndHWGetType");
	return returnValue ;
}

// --------------------------------------------------------------------------
// ::sndHWGetManufactuer
// return the detected part's manufacturer.	 I think Daca is a single sourced part
// from Micronas Intermetall.  Always return just that.
UInt32 AppleTexas2Audio::sndHWGetManufacturer(void )
{
	UInt32				returnValue;

	debugIOLog (3, "+ AppleTexas2Audio::sndHWGetManufacturer");

	// in AudioHardwareConstants.h need to set up a constant for the part manufacturer.
	returnValue = kSndHWManfTI ;
	
	debugIOLog (3, "- AppleTexas2Audio::sndHWGetManufacturer");
	return returnValue ;
}

#pragma mark +DETECT ACTIVATION & DEACTIVATION
// --------------------------------------------------------------------------
// ::setDeviceDetectionActive
// turn on detection, TODO move to superclass?? 
void AppleTexas2Audio::setDeviceDetectionActive(void)
{
	debugIOLog (3, "+ AppleTexas2Audio::setDeviceDetectionActive");
	mCanPollStatus = true ;
	
	debugIOLog (3, "- AppleTexas2Audio::setDeviceDetectionActive");
	return ;
}

// --------------------------------------------------------------------------
// ::setDeviceDetectionInActive
// turn off detection, TODO move to superclass?? 
void AppleTexas2Audio::setDeviceDetectionInActive(void)
{
	debugIOLog (3, "+ AppleTexas2Audio::setDeviceDetectionInActive");
	mCanPollStatus = false ;
	
	debugIOLog (3, "- AppleTexas2Audio::setDeviceDetectionInActive");
	return ;
}

#pragma mark +POWER MANAGEMENT
// Power Management

// --------------------------------------------------------------------------
/*
	Update AudioProj14PowerObject::setHardwarePowerOn() to set *microsecondsUntilComplete = 2000000
	when waking from sleep.
*/
IOReturn AppleTexas2Audio::sndHWSetPowerState(IOAudioDevicePowerState theState)
{
	IOReturn							result;

	debugIOLog (3, "+ AppleTexas2Audio::sndHWSetPowerState (%d)", theState);

	result = kIOReturnError;
	switch (theState) {
		case kIOAudioDeviceActive:
			result = performDeviceWake ();
			break;
		case kIOAudioDeviceIdle:
			result = performDeviceIdleSleep ();
			break;
		case kIOAudioDeviceSleep:
			result = performDeviceSleep ();
			break;
	}
	if ( kIOReturnSuccess == result ) {
		mCurrentPowerState = theState;
	}
	debugIOLog (3, "- AppleTexas2Audio::sndHWSetPowerState");
	return result;
}

// --------------------------------------------------------------------------
//	Set the audio hardware to sleep mode by placing the Texas2 into
//	analog power down mode after muting the amplifiers.  The I2S clocks
//	must also be stopped after these two tasks in order to achieve
//	a fully low power state.  Some CPU implemenations implement a
//	Codec RESET by ANDing the mute states of the internal speaker
//	amplifier and the headphone amplifier.  The Codec must be put 
//	into analog power down state prior to asserting the both amplifier
//	mutes.  This is only invoked when the 'has-anded-reset' property
//	exists with a value of 'true'.  For all other cases, the original
//	signal manipulation order exists.
IOReturn AppleTexas2Audio::performDeviceSleep () {
    IOService *	keyLargo;

	debugIOLog (3, "+ AppleTexas2Audio::performDeviceSleep");

	keyLargo = NULL;

	if ( kIOAudioDeviceIdle == gPowerState ) {
		//	If the hardware is already idle sleeping then all that need to be done is to 
		//	assert RESET so that the GPIO is not driving against an input protection
		//	diode when the power is removed from the TAS3004.
		Texas2_Reset_ASSERT();
	} else {
		Texas2_Quiesce ( kIOAudioDeviceSleep );
	}

	gPowerState = kIOAudioDeviceSleep;
	debugIOLog (3, "- AppleTexas2Audio::performDeviceSleep");
	return kIOReturnSuccess;
}
	
// --------------------------------------------------------------------------
//	Set the audio hardware to sleep mode by placing the Texas2 into
//	analog power down mode after muting the amplifiers.  The I2S clocks
//	must also be stopped after these two tasks in order to achieve
//	a fully low power state.  Some CPU implemenations implement a
//	Codec RESET by ANDing the mute states of the internal speaker
//	amplifier and the headphone amplifier.  The Codec must be put 
//	into analog power down state prior to asserting the both amplifier
//	mutes.  This is only invoked when the 'has-anded-reset' property
//	exists with a value of 'true'.  For all other cases, the original
//	signal manipulation order exists.
IOReturn AppleTexas2Audio::performDeviceIdleSleep () {
	debugIOLog (3, "+ AppleTexas2Audio::performDeviceIdleSleep");
	
	Texas2_Quiesce ( kIOAudioDeviceIdle );
	gPowerState = kIOAudioDeviceIdle;
	
	debugIOLog (3, "- AppleTexas2Audio::performDeviceIdleSleep");
	return kIOReturnSuccess;
}
	
// --------------------------------------------------------------------------
//	The I2S clocks must have been started before performing this method.
//	This method sets the Texas2 analog control register to normal operating
//	mode and unmutes the amplifers.  [2855519]  When waking the device it
//	is necessary to release at least one of the headphone or speaker amplifier
//	mute signals to release the Codec RESET on systems that implement the
//	Codec RESET by ANDing the headphone and speaker amplifier mute active states.
//	This must be done AFTER waking the I2S clocks!
IOReturn AppleTexas2Audio::performDeviceWake () {
    IOService *				keyLargo;
	IOReturn				err;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	debugIOLog (3, "+ AppleTexas2Audio::performDeviceWake");

	err = kIOReturnSuccess;
	// *** NOTE: In the new model, this code needs to go in the format control object, not the KeyLargoPlatform object
	keyLargo = NULL;
    keyLargo = IOService::waitForService (IOService::serviceMatching ("KeyLargo"));
    
    if (NULL != keyLargo) {
		funcSymbolName = OSSymbol::withCString ( "keyLargo_powerI2S" );						//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		// Turn on the i2s clocks...
		switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:	keyLargo->callPlatformFunction (funcSymbolName, false, (void *)true, (void *)0, 0, 0);	break;
			case kUseI2SCell1:	keyLargo->callPlatformFunction (funcSymbolName, false, (void *)true, (void *)1, 0, 0);	break;
		}
		funcSymbolName->release ();															//	[3323977]
	}
	// *** END NOTE

	//	Set the Texas2 analog control register to analog power up mode
	SetAnalogPowerDownMode (kPowerNormalAnalog);
    
	// ...then bring everything back up the way it should be.
	err = Texas2_Initialize ();			//	reset the TAS3001C and flush the shadow contents to the HW

	if (NULL != dallasDriver && FALSE == dallasSpeakersProbed && TRUE == IsSpeakerConnected ()) {
		UInt8				bEEPROM[32];
		Boolean				result;

		speakerID = 0;						//	assume that rom will not be read successfully
		familyID = 0;
		dallasIntEventSource->disable ();
		result = dallasDriver->getSpeakerID (bEEPROM);
		dallasIntEventSource->enable ();
		if (TRUE == result) {				//	[2965804]
			// The speakers have been successfully probed
			dallasSpeakersProbed = TRUE;
			familyID = bEEPROM[0];
			speakerID = bEEPROM[1];
			debugIOLog (3, "dallasDeviceFamily = %d, dallasDeviceType = %d, dallasDeviceSubType = %d", (unsigned int)bEEPROM[0], (unsigned int)bEEPROM[1], (unsigned int)bEEPROM[2]);
		} else {
			debugIOLog (3, "speakerID unknown, probe *** failed ***");
		}
	}

	//	Mute the amplifiers as needed

	//	Timing for systems with a master mute [see 2949185] is as follows:
	//					           ____________________
	//	MASTER_MUTE:	__________|
	//					                     __________
	//	SPEAKER MUTE:	____________________|
	//
	//					                     __________
	//	HEADPHONE MUTE:	____________________|
	//
	//				  -->| 200 MS |<--
	//					       -->| 500 MS |<--
	//
	if ( NULL != masterMuteGpio ) {			//	[2949185] begin {
		if ( masterMuteActiveState == GpioRead( masterMuteGpio ) ) {					//	delay & release only if master is muted
			IOSleep ( 200 );
			SetAmplifierMuteState (kMASTER_AMP, NEGATE_GPIO (masterMuteActiveState));	//	unmute
			IOSleep ( 1000 );
		}
	}										//	} end [2949185]

	if ( FALSE == IsLineOutConnected () )
	{
		if (FALSE == IsHeadphoneConnected ()) {
			// [2931666] Not needed
			SelectOutputAndLoadEQ();										//	[2878119]
			if ( TRUE == hasVideo && kSndHWCPUHeadphone != GetDeviceMatch() ) {
				// Tell the video driver about the jack state change in case a video connector was plugged in
				publishResource (gAppleAudioVideoJackStateKey, kOSBooleanFalse);
			}
			if ( TRUE == hasSerial && kSndHWCPUHeadphone != GetDeviceMatch() ) {
				// Tell the serial driver about the jack state change in case a serial connector was plugged in
				publishResource (gAppleAudioSerialJackStateKey, kOSBooleanFalse);
			}
			if (NULL != dallasIntProvider) {
				// Set the correct EQ
				DallasInterruptHandlerTimer (this, 0);
			} else {
				DeviceInterruptService ();
			}
		} else {
			if (NULL != headphoneIntProvider) {
				// Set amp mutes accordingly
				RealHeadphoneInterruptHandler (0, 0);
			}
		}
	}
	else
	{
		if (NULL != lineOutIntProvider) {
			// Set amp mutes accordingly
			RealLineOutInterruptHandler (0, 0);
		}
	}

	gPowerState = kIOAudioDeviceActive;
Exit:
	debugIOLog (3, "- AppleTexas2Audio::performDeviceWake");
	return err;
}

// --------------------------------------------------------------------------
// ::sndHWGetConnectedDevices
// TODO: Move to superclass
UInt32 AppleTexas2Audio::sndHWGetConnectedDevices(
	void)
{
	debugIOLog (3, "+ AppleTexas2Audio::sndHWGetConnectedDevices");
   UInt32	returnValue = currentDevices;

	debugIOLog (3, "- AppleTexas2Audio::sndHWGetConnectedDevices");
	return returnValue ;
}

// --------------------------------------------------------------------------
UInt32 AppleTexas2Audio::sndHWGetProgOutput(
	void)
{
	debugIOLog (3, "+ AppleTexas2Audio::sndHWGetProgOutput");
	debugIOLog (3, "- AppleTexas2Audio::sndHWGetProgOutput");
	return 0;
}

// --------------------------------------------------------------------------
IOReturn AppleTexas2Audio::sndHWSetProgOutput(
	UInt32 outputBits)
{
	debugIOLog (3, "+ AppleTexas2Audio::sndHWSetProgOutput");

	IOReturn myReturn = kIOReturnSuccess;

	debugIOLog (3, "- AppleTexas2Audio::sndHWSetProgOutput");
	return(myReturn);
}

#pragma mark +INTERRUPT HANDLERS
// --------------------------------------------------------------------------
//	Returns 'TRUE' if the external speaker jack is inserted as determined by
//	direct query of the ExtInt-GPIO.
Boolean AppleTexas2Audio::IsSpeakerConnected (void) {
	UInt8						dallasSenseContents;
	Boolean						connection;

	connection = FALSE;
	dallasSenseContents = 0;
	if (NULL != dallasIntProvider) {
		dallasSenseContents = *(dallasExtIntGpio);

		if ((dallasSenseContents & (gpioBIT_MASK << gpioPIN_RO)) == (dallasInsertedActiveState << gpioPIN_RO)) {
			connection = TRUE;
			detectCollection |= kSndHWCPUExternalSpeaker;
		} else {
			connection = FALSE;
			detectCollection &= ~kSndHWCPUExternalSpeaker;
		}
	}
	debugIOLog (3,  "AppleTexas2Audio::IsSpeakerConnected detectCollection %X", (unsigned int)detectCollection );
	return connection;
}

// --------------------------------------------------------------------------
void AppleTexas2Audio::DallasInterruptHandlerTimer (OSObject *owner, IOTimerEventSource *sender) {
    AppleTexas2Audio *			device;
    IOCommandGate *				cg;
	AbsoluteTime				currTime;


	debugIOLog (3, "+ DallasInterruptHandlerTimer");

	device = OSDynamicCast (AppleTexas2Audio, owner);
	FailIf (NULL == device, Exit);

	device->dallasSpeakersConnected = device->IsSpeakerConnected ();

	device->SetOutputSelectorCurrentSelection ();

	if (TRUE == device->dallasSpeakersProbed && FALSE == device->dallasSpeakersConnected) {
		// They've unplugged the dallas speakers, so we'll need to check them out if they get plugged back in.
		device->dallasSpeakersProbed = FALSE;
		device->speakerID = 0;
		device->familyID = 0;
	}

	if ( kSndHWCPUExternalSpeaker == device->GetDeviceMatch() || kSndHWInternalSpeaker == device->GetDeviceMatch() ) {	//	[2878119]	GetDeviceMatch implies call to IsSpeakerConnected()
		// Set the proper EQ
		cg = device->getCommandGate ();
		if (NULL != cg) {
			cg->runAction (DeviceInterruptServiceAction);
		}

		clock_get_uptime (&currTime);
		absolutetime_to_nanoseconds (currTime, &device->savedNanos);
	}

	if (NULL != device->dallasIntEventSource) {
		debugIOLog (3,  "... dallasIntEventSource->enable" );
		device->dallasIntEventSource->enable();
	}

Exit:
	debugIOLog (3, "- DallasInterruptHandlerTimer");
	return;
}

// --------------------------------------------------------------------------
void AppleTexas2Audio::SetOutputSelectorCurrentSelection (void) {
	OSNumber *					activeOutput;

	switch (GetDeviceMatch ()) {
		case kSndHWCPUHeadphone:		activeOutput = OSNumber::withNumber (kIOAudioOutputPortSubTypeHeadphones, 32);			break;
		case kSndHWInternalSpeaker:		activeOutput = OSNumber::withNumber (kIOAudioOutputPortSubTypeInternalSpeaker, 32);		break;
		case kSndHWCPUExternalSpeaker:	activeOutput = OSNumber::withNumber (kIOAudioOutputPortSubTypeExternalSpeaker, 32);		break;
		case kSndHWLineOutput:			activeOutput = OSNumber::withNumber (kIOAudioOutputPortSubTypeLine, 32);				break;
		default:						activeOutput = NULL;																	break;
	}

	if (NULL != activeOutput) {
		debugIOLog (3,  "... outputSelector->hardwareValueChanged ( %d )", (unsigned int)activeOutput );	//	[3066930]
		outputSelector->hardwareValueChanged (activeOutput);
		activeOutput->release ();
	}
}

// --------------------------------------------------------------------------
// Set a flag to say if the dallas speakers are plugged in or not so we know which EQ to use.
void AppleTexas2Audio::RealDallasInterruptHandler (IOInterruptEventSource *source, int count) {
    AbsoluteTime				fireTime;
    UInt64						nanos;

	// call DallasInterruptHandlerTimer in a bit to check for the dallas rom (and complete speaker insertion).
	if (NULL != dallasHandlerTimer) {
		clock_get_uptime (&fireTime);
		absolutetime_to_nanoseconds (fireTime, &nanos);
		nanos += kInsertionDelayNanos;	// Schedule 250ms in the future...

		nanoseconds_to_absolutetime (nanos, &fireTime);
		dallasHandlerTimer->wakeAtTime (fireTime);
	}

	return;
}

// --------------------------------------------------------------------------
// Initial creation of this routine duplicates dallasInterruptHandler to encourage changes 
// to be added to dallasInterruptHandler - not here. 
void AppleTexas2Audio::dallasInterruptHandler(OSObject *owner, IOInterruptEventSource *source, int count) {
	AbsoluteTime 			currTime;
	UInt64 					currNanos;
	AppleTexas2Audio *		appleTexas2Audio;
    IOCommandGate *			cg;

	appleTexas2Audio = OSDynamicCast (AppleTexas2Audio, owner);
	FailIf (!appleTexas2Audio, Exit);

	// Need this disable for when we call dallasInterruptHandler instead of going through the interrupt filter
	appleTexas2Audio->dallasIntEventSource->disable ();

	clock_get_uptime (&currTime);
	absolutetime_to_nanoseconds (currTime, &currNanos);

	if ((currNanos - appleTexas2Audio->savedNanos) > kInterruptSettleTime) {
		//	It is only necessary to call the Dallas driver when the speaker is inserted.
		//	On jack removed events, no delay is necessary and the EQ and DRC coefficients
		//	should be loaded without delay.	[2875924]
		if (TRUE == appleTexas2Audio->IsSpeakerConnected ()) {
			appleTexas2Audio->RealDallasInterruptHandler (source, count);
		} else {
			//	On jack removal, just go load the EQ...
			appleTexas2Audio->dallasSpeakersConnected = FALSE;
			appleTexas2Audio->dallasSpeakersProbed = FALSE;
			appleTexas2Audio->speakerID = 0;
			cg = appleTexas2Audio->getCommandGate ();
			if (NULL != cg) {
				cg->runAction (appleTexas2Audio->DeviceInterruptServiceAction);
			}
			appleTexas2Audio->dallasIntEventSource->enable ();
			appleTexas2Audio->SetOutputSelectorCurrentSelection ();
		}
	} else { 
		appleTexas2Audio->dallasIntEventSource->enable ();
	}

Exit:
	return;
}

// --------------------------------------------------------------------------
// static "action" function to connect to our object
// return TRUE if you want the handler function to be called, or FALSE if you don't want it to be called.
bool AppleTexas2Audio::interruptFilter(OSObject *owner, IOFilterInterruptEventSource *src)
{
	AppleTexas2Audio* self = (AppleTexas2Audio*) owner;

	self->dallasIntEventSource->disable();
	return (true);
}

// --------------------------------------------------------------------------
// This is called to tell the user that they may not have plugged their speakers in all the way.
void AppleTexas2Audio::DisplaySpeakersNotFullyConnected (OSObject *owner, IOTimerEventSource *sender) {
	AppleTexas2Audio *		appleTexas2Audio;
    IOCommandGate *			cg;
	AbsoluteTime			currTime;
	UInt32					deviceID;
	UInt8					bEEPROM[32];
	Boolean					result;
	IOReturn				error;

	debugIOLog (3, "+ DisplaySpeakersNotFullyConnected");

	appleTexas2Audio = OSDynamicCast (AppleTexas2Audio, owner);
	FailIf (!appleTexas2Audio, Exit);

	deviceID = appleTexas2Audio->GetDeviceMatch ();
	if (kExternalSpeakersActive == deviceID) {
		if (NULL != appleTexas2Audio->dallasDriver) {
			appleTexas2Audio->dallasIntEventSource->disable ();
			result = appleTexas2Audio->dallasDriver->getSpeakerID (bEEPROM);
			appleTexas2Audio->dallasIntEventSource->enable ();
			clock_get_uptime (&currTime);
			absolutetime_to_nanoseconds (currTime, &appleTexas2Audio->savedNanos);

			if (FALSE == result) {	// FALSE == failure for DallasDriver
				error = KUNCUserNotificationDisplayNotice (
				0,		// Timeout in seconds
				0,		// Flags (for later usage)
				"",		// iconPath (not supported yet)
				"",		// soundPath (not supported yet)
				"/System/Library/Extensions/Apple02Audio.kext",		// localizationPath
				"HeaderOfDallasPartialInsert",		// the header
				"StringOfDallasPartialInsert",
				"ButtonOfDallasPartialInsert"); 

				if (kIOReturnSuccess != error) {
					appleTexas2Audio->notifierHandlerTimer->setTimeoutMS (kNotifyTimerDelay);
				} else {
					IOLog ("The device plugged into the Apple speaker mini-jack cannot be recognized.");
					IOLog ("Remove the plug from the jack. Then plug it back in and make sure it is fully inserted.");
				}
			} else {
				// Speakers are fully plugged in now, so load the proper EQ for them
				appleTexas2Audio->dallasIntEventSource->disable ();
				cg = appleTexas2Audio->getCommandGate ();
				if (NULL != cg) {
					cg->runAction (DeviceInterruptServiceAction);
				}
				appleTexas2Audio->dallasIntEventSource->enable ();
				clock_get_uptime (&currTime);
				absolutetime_to_nanoseconds (currTime, &appleTexas2Audio->savedNanos);
			}
		}
	}

Exit:
	debugIOLog (3, "- DisplaySpeakersNotFullyConnected");
    return;
}


// --------------------------------------------------------------------------
//	Returns 'TRUE' if the headphone jack is inserted as determined by
//	direct query of the ExtInt-GPIO.
Boolean AppleTexas2Audio::IsHeadphoneConnected (void) {
	UInt8				headphoneSenseContents;
	Boolean				connection;

	connection = FALSE;
	headphoneSenseContents = 0;
	if (NULL != headphoneIntEventSource) {
		headphoneSenseContents = *headphoneExtIntGpio;

		if ((headphoneSenseContents & (gpioBIT_MASK << gpioPIN_RO)) == (headphoneInsertedActiveState << gpioPIN_RO)) {
			connection = TRUE;
			detectCollection |= kSndHWCPUHeadphone;
		} else {
			connection = FALSE;
			detectCollection &= ~kSndHWCPUHeadphone;
		}
	}
	debugIOLog (3,  "AppleTexas2Audio::IsHeadphoneConnected detectCollection %X", (unsigned int)detectCollection );
	return connection;
}

// --------------------------------------------------------------------------
//	Returns 'TRUE' if the line output jack is inserted as determined by
//	direct query of the ExtInt-GPIO.
//	begin {	[2878119]
Boolean AppleTexas2Audio::IsLineOutConnected (void) {
	UInt8				lineOutSenseContents;
	Boolean				connection;

	connection = FALSE;
	lineOutSenseContents = 0;
	if (NULL != lineOutIntEventSource) {
		lineOutSenseContents = *lineOutExtIntGpio;

		if ((lineOutSenseContents & (gpioBIT_MASK << gpioPIN_RO)) == (lineOutExtIntActiveState << gpioPIN_RO)) {
			connection = TRUE;
			detectCollection |= kSndHWLineOutput;
		} else {
			connection = FALSE;
			detectCollection &= ~kSndHWLineOutput;
		}
	}
	debugIOLog (3,  "AppleTexas2Audio::IsLineOutConnected detectCollection %X", (unsigned int)detectCollection );
	return connection;
}
//	[2878119]	} end

// --------------------------------------------------------------------------
void AppleTexas2Audio::RealHeadphoneInterruptHandler (IOInterruptEventSource *source, int count) {
	IOCommandGate *		cg;
	
	debugIOLog (3,  "+ RealHeadphoneInterruptHandler( 0x%X, %d )", (unsigned int)source, (unsigned int)count );
	
	cg = getCommandGate ();
	if ( NULL != cg ) {
		cg->runAction ( DeviceInterruptServiceAction );
	}

	headphonesConnected = IsHeadphoneConnected ();

	//	[3234624]	begin {
	if ( ( kIOAudioDeviceIdle == gPowerState ) && hasANDedReset && !powerStateChangeInProcess ) {
		//	Go back to active on jack state changes if on a system with ANDed RESET.
		//	Note that invoking setHardwarePowerOn will cause a nested execution of
		//	this (i.e. 'RealHeadphoneInterruptHandler') routine so a semaphore is
		//	used to block redundant calls to setHardwarePowerOn so that the nested
		//	calls do not go more than two layers.
		powerStateChangeInProcess = true;
		theAudioPowerObject->setHardwarePowerOn ();
		powerStateChangeInProcess = false;
	}
	//	[3234624]	} end

	SetOutputSelectorCurrentSelection ();

	if (TRUE == hasVideo) {
		// Tell the video driver about the jack state change in case a video connector was plugged in
		publishResource (gAppleAudioVideoJackStateKey, headphonesConnected ? kOSBooleanTrue : kOSBooleanFalse);
	}
	if (TRUE == hasSerial) {
		// Tell the serial driver about the jack state change in case a serial connector was plugged in
		publishResource (gAppleAudioSerialJackStateKey, headphonesConnected ? kOSBooleanTrue : kOSBooleanFalse);
	}
	debugIOLog (3,  "- RealHeadphoneInterruptHandler" );
}

// --------------------------------------------------------------------------
void AppleTexas2Audio::headphoneInterruptHandler(OSObject *owner, IOInterruptEventSource *source, int count)
{
	debugIOLog (3,  "+ headphoneInterruptHandler( 0x%X, %d )", (unsigned int)source, (unsigned int)count );
	
	AppleTexas2Audio *appleTexas2Audio = (AppleTexas2Audio *)owner;
	FailIf (!appleTexas2Audio, Exit);

	appleTexas2Audio->RealHeadphoneInterruptHandler (source, count);

Exit:
	debugIOLog (3,  "- headphoneInterruptHandler" );
	
	return;
}

// --------------------------------------------------------------------------
//	begin {	[2878119]
void AppleTexas2Audio::RealLineOutInterruptHandler (IOInterruptEventSource *source, int count) {
	IOCommandGate *		cg;
	
	debugIOLog (3,  "+ RealLineOutInterruptHandler( 0x%X, %d )", (unsigned int)source, count );

	cg = getCommandGate ();
	if ( NULL != cg ) {
		cg->runAction ( DeviceInterruptServiceAction );
	}

	lineOutConnected = IsLineOutConnected ();

	SetOutputSelectorCurrentSelection ();

	// appears to be redundant since DeviceInterruptService already calls this function
//	SelectOutputAndLoadEQ();
	
	debugIOLog (3,  "- RealLineOutInterruptHandler" );
}

// --------------------------------------------------------------------------
void AppleTexas2Audio::lineOutInterruptHandler(OSObject *owner, IOInterruptEventSource *source, int count)
{
	debugIOLog (3,  "+ lineOutInterruptHandler( 0x%X, 0x%X, %d )", (unsigned int)owner, (unsigned int)source, count );
	
	AppleTexas2Audio *appleTexas2Audio = (AppleTexas2Audio *)owner;
	FailIf (!appleTexas2Audio, Exit);

	appleTexas2Audio->RealLineOutInterruptHandler (source, count);

Exit:
	debugIOLog (3,  "- lineOutInterruptHandler" );
	return;
}

// --------------------------------------------------------------------------
//	Returns a target output selection for the current detect states as
//	follows:
//
//						Line Out	Headphone	External
//						Detect		Detect		Speaker
//												Detect
//						--------	---------	--------
//	Internal Speaker	out			out			out
//	External Speaker	out			out			in
//	Headphone			out			in			out
//	Headphone			out			in			in
//	Line Output			in			out			out
//	Line Output			in			out			in
//	Line Output			in			in			out
//	Line Output			in			in			in
//
UInt32 AppleTexas2Audio::ParseDetectCollection ( void ) {
	UInt32		result;
	
	debugIOLog (3,  "+ ParseDetectCollection ... detectCollection 0x%X", (unsigned int)detectCollection );
	result = kSndHWOutput2;				//	Assume internal speaker
	if ( NULL == masterMuteGpio ) {
		if ( detectCollection & kSndHWLineOutput ) {
			result = kSndHWOutput4;		//	line output
		} else if ( detectCollection & kSndHWCPUHeadphone ) {
			result = kSndHWOutput1;		//	headphone
			} else {
			result = kSndHWOutput3;		//	external speaker
		}
	} else {
		//	Systems that have a master mute have a different behavior.
		//	The master amplifier is always on.  The headphone may be
		//	active while the line output jack is occupied but the
		//	speaker amplifier may not be active while the line output
		//	jack is occupied.	[2933090]
		if ( detectCollection & kSndHWCPUHeadphone ) {
			result = kSndHWOutput1;								// for headphone or ( headphone & line out)
		} else if ( detectCollection & kSndHWLineOutput ) {
			result = kSndHWOutput4;								// line out
		} else {
			result = kSndHWOutput3;								//	for internal or external speaker
		}
	}
	debugIOLog (3,  "- ParseDetectCollection returns %d", (unsigned int)result );
	return result;
}


//	[2878119]	} end

#pragma mark +DIRECT HARDWARE MANIPULATION
// --------------------------------------------------------------------------
//	User Client Support
UInt8 *	AppleTexas2Audio::getGPIOAddress (UInt32 gpioSelector) {
	UInt8 *				gpioAddress;

	gpioAddress = NULL;
	switch (gpioSelector) {
		case kHeadphoneMuteSel:			gpioAddress = hdpnMuteGpio;						break;
		case kHeadphoneDetecteSel:		gpioAddress = headphoneExtIntGpio;				break;
		case kAmplifierMuteSel:			gpioAddress = ampMuteGpio;						break;
		case kSpeakerIDSel:				gpioAddress = dallasExtIntGpio;					break;
		case kCodecResetSel:			gpioAddress = hwResetGpio;						break;
		case kLineInDetectSel:			gpioAddress = lineInExtIntGpio;					break;
		case kLineOutDetectSel:			gpioAddress = lineOutExtIntGpio;				break;
		case kLineOutMuteSel:			gpioAddress = lineOutMuteGpio;					break;
		case kMasterMuteSel:			gpioAddress = masterMuteGpio;					break;		//	[2933090]
	}
	if ( NULL == gpioAddress ) {
		debugIOLog (3,  "AppleTexas2Audio::getGPIOAddress ( %d ) returns NULL", (unsigned int)gpioSelector );
	}
	return gpioAddress;
}

// --------------------------------------------------------------------------
//	User Client Support
Boolean	AppleTexas2Audio::getGPIOActiveState (UInt32 gpioSelector) {
	Boolean				activeState;

	activeState = NULL;
	switch (gpioSelector) {
		case kHeadphoneMuteSel:			activeState = hdpnActiveState;					break;
		case kHeadphoneDetecteSel:		activeState = headphoneInsertedActiveState;		break;
		case kAmplifierMuteSel:			activeState = ampActiveState;					break;
		case kSpeakerIDSel:				activeState = dallasInsertedActiveState;		break;
		case kCodecResetSel:			activeState = hwResetActiveState;				break;
		case kLineInDetectSel:			activeState = lineInExtIntActiveState;			break;
		case kLineOutDetectSel:			activeState = lineOutExtIntActiveState;			break;
		case kLineOutMuteSel:			activeState = lineOutMuteActiveState;			break;
		case kMasterMuteSel:			activeState = masterMuteActiveState;			break;		//	[2933090]
		default:
			debugIOLog (3,  "AppleTexas2Audio::getGPIOActiveState ( %d ) UNKNOWN", (unsigned int)gpioSelector );
			break;
	}

	return activeState;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
void	AppleTexas2Audio::setGPIOActiveState ( UInt32 selector, UInt8 gpioActiveState ) {

	gpioActiveState = 0 == gpioActiveState ? FALSE : TRUE;
	switch ( selector ) {
		case kHeadphoneMuteSel:			hdpnActiveState = gpioActiveState;					break;
		case kHeadphoneDetecteSel:		headphoneInsertedActiveState = gpioActiveState;		break;
		case kAmplifierMuteSel:			ampActiveState = gpioActiveState;					break;
		case kSpeakerIDSel:				dallasInsertedActiveState = gpioActiveState;		break;
		case kCodecResetSel:			hwResetActiveState = gpioActiveState;				break;
		case kLineInDetectSel:			lineInExtIntActiveState = gpioActiveState;			break;
		case kLineOutDetectSel:			lineOutExtIntActiveState = gpioActiveState;			break;
		case kLineOutMuteSel:			lineOutMuteActiveState = gpioActiveState;			break;
		case kMasterMuteSel:			masterMuteActiveState = gpioActiveState;			break;
		default:
			debugIOLog (3,  "  AppleTexas2Audio::setGPIOActiveState ( %d, %d ) UNKNOWN", (unsigned int)selector, gpioActiveState );
			break;
	}
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
Boolean	AppleTexas2Audio::checkGpioAvailable ( UInt32 selector ) {
	Boolean			result = FALSE;
	switch ( selector ) {
		case kHeadphoneMuteSel:			if ( NULL != hdpnMuteGpio ) { result = TRUE; }				break;
		case kHeadphoneDetecteSel:		if ( NULL != headphoneExtIntGpio ) { result = TRUE; }		break;
		case kAmplifierMuteSel:			if ( NULL != ampMuteGpio ) { result = TRUE; }				break;
		case kSpeakerIDSel:				if ( NULL != dallasExtIntGpio ) { result = TRUE; }			break;
		case kCodecResetSel:			if ( NULL != hwResetGpio ) { result = TRUE; }				break;
		case kLineInDetectSel:			if ( NULL != lineInExtIntGpio ) { result = TRUE; }			break;
		case kLineOutDetectSel:			if ( NULL != lineOutExtIntGpio ) { result = TRUE; }			break;
		case kLineOutMuteSel:			if ( NULL != lineOutMuteGpio ) { result = TRUE; }			break;
		case kMasterMuteSel:			if ( NULL != masterMuteGpio ) { result = TRUE; }			break;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3060321]	Fixed kI2sDataWordFormatRegisterSelector to reference the
//				data word format register.  
IOReturn	AppleTexas2Audio::readHWReg32 ( UInt32 selector, UInt32 * registerData ) {
	IOReturn	result = kIOReturnError;
	if ( NULL != registerData ) {
		result = kIOReturnSuccess;
		switch ( selector ) {
			case kI2sSerialFormatRegisterSelector:		*registerData = audioI2SControl->GetSerialFormatReg();	break;
			case kI2sDataWordFormatRegisterSelector:	*registerData = audioI2SControl->GetDataWordSizesReg();	break;
			case kFeatureControlRegister1Selector:		*registerData = audioI2SControl->FCR1GetReg();			break;
			case kFeatureControlRegister3Selector:		*registerData = audioI2SControl->FCR3GetReg();			break;
			case kI2s1SerialFormatRegisterSelector:		*registerData = audioI2SControl->GetSerialFormatReg();	break;
			case kI2s1DataWordFormatRegisterSelector:	*registerData = audioI2SControl->GetDataWordSizesReg();	break;
			default:									result = kIOReturnError;								break;
		}
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3060321]	Fixed kI2sDataWordFormatRegisterSelector to reference the
//				data word format register.  
IOReturn	AppleTexas2Audio::writeHWReg32 ( UInt32 selector, UInt32 registerData ) {
	IOReturn	result = kIOReturnError;
	result = kIOReturnSuccess;
	switch ( selector ) {
		case kI2sSerialFormatRegisterSelector:		audioI2SControl->SetSerialFormatReg( registerData );		break;
		case kI2sDataWordFormatRegisterSelector:	audioI2SControl->SetDataWordSizesReg( registerData );		break;
		case kFeatureControlRegister1Selector:		audioI2SControl->Fcr1SetReg( registerData );				break;
		case kFeatureControlRegister3Selector:		audioI2SControl->Fcr3SetReg( registerData );				break;
		case kI2s1SerialFormatRegisterSelector:		audioI2SControl->SetSerialFormatReg( registerData );		break;
		case kI2s1DataWordFormatRegisterSelector:	audioI2SControl->SetDataWordSizesReg( registerData );		break;
		default:									result = kIOReturnError;									break;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
IOReturn	AppleTexas2Audio::readCodecReg ( UInt32 selector, void * registerData,  UInt32 * registerDataSize ) {
	UInt32			codecCacheSize;
	IOReturn		err = kIOReturnError;
	
	codecCacheSize = sizeof ( Texas2_ShadowReg );
	if ( NULL != registerDataSize && NULL != registerData ) {
		if ( codecCacheSize <= *registerDataSize && 0 != codecCacheSize && 0 == selector ) {
			for ( UInt32 index = 0; index < codecCacheSize; index++ ) {
				((UInt8*)registerData)[index] = ((UInt8*)&shadowTexas2Regs)[index];
			}
			err = kIOReturnSuccess;
			if ( kIOReturnSuccess == err ) {
				*registerDataSize = codecCacheSize;
			}
		}
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
IOReturn	AppleTexas2Audio::writeCodecReg ( UInt32 selector, void * registerData ) {
	UInt32			codecRegSize;
	IOReturn		err = 0;
	err = getCodecRegSize ( selector, &codecRegSize );
	if ( kIOReturnSuccess == err ) {
		err = Texas2_WriteRegister( (UInt8)selector, (UInt8*)registerData );
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Returns information regarding the CPU ID, port ID and speaker ID.  The
//	speaker ID is a concatenation of the Dallas ROM 8 bit fields to form a
//	32 bit field where the family ID occupies the least significant byte of
//	the 32 bit field.  The remaining fields appear in ascending order.
//
//	speakerID = 0xCCFFTTSS where:	CC = Connected
//									FF = DallasID.deviceFamily
//									TT = DallasID.deviceType
//									SS = DallasID.deviceSubType
//
IOReturn	AppleTexas2Audio::readSpkrID ( UInt32 selector, UInt32 * speakerIDPtr ) {
	UInt8				bEEPROM[32];
	IOReturn			result = kIOReturnError;
	
	if ( NULL != speakerIDPtr ) {
		for ( UInt32 index = 0; index < 4; index++ ) {
			bEEPROM[index] = 0;
		}
		if ( selector ) {
			//	Force a ROM access here.  WARNING:  This may cause corruption the equalizer set
			//	and should only be used with caution for diagnostic purposes		[3053696]
			if ( IsSpeakerConnected () && NULL != dallasDriver ) {
				if ( dallasDriver->getSpeakerID (bEEPROM) ) {
					*speakerIDPtr = ( IsSpeakerConnected () << kSpeakerID_Connected ) | ( bEEPROM[0] << kSpeakerID_Family ) | ( bEEPROM[1] << kSpeakerID_Type ) | ( bEEPROM[2] << kSpeakerID_SubType );
				}
			}
		} else {
			//	Default method for accessing speaker ID information is to use the data that
			//	has been cached by the speaker detect interrupt handler.
			*speakerIDPtr = ( IsHeadphoneConnected () << kHeadphone_Connected ) | \
							( IsSpeakerConnected () << kSpeakerID_Connected ) | \
							( familyID << kSpeakerID_Family ) | \
							( speakerID << kSpeakerID_Type ) | \
							( 0 << kSpeakerID_SubType );
		}
		result = kIOReturnSuccess;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
IOReturn	AppleTexas2Audio::getCodecRegSize ( UInt32 selector, UInt32 * codecRegSizePtr )
{
	UInt8*			shadowPtr;
	UInt8			regSize;
	IOReturn		err = kIOReturnError;
	if ( NULL != codecRegSizePtr ) {
		*codecRegSizePtr = 0;
		err = GetShadowRegisterInfo ( (UInt8)selector, &shadowPtr, &regSize );
		if ( kIOReturnSuccess == err ) {
			*codecRegSizePtr = (UInt32)regSize;
		} else {
			debugIOLog (3,  "... GetShadowRegisterInfo ( %X, %X, %X ) returns %X", (unsigned int)selector, (unsigned int)shadowPtr, (unsigned int)regSize, (unsigned int)err );
		}
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
IOReturn	AppleTexas2Audio::getVolumePRAM ( UInt32 * pramDataPtr )
{
	UInt8					curPRAMVol;
	IODTPlatformExpert * 	platform;
	IOReturn				err = kIOReturnError;

	curPRAMVol = 0;
	if ( NULL != pramDataPtr ) {
		platform = OSDynamicCast(IODTPlatformExpert,getPlatform());
		if (platform) {
			platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
			*pramDataPtr = (UInt32)curPRAMVol;
			err = kIOReturnSuccess;
		}
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
IOReturn	AppleTexas2Audio::getDmaState ( UInt32 * dmaStatePtr )
{
	IOReturn				err = kIOReturnError;
	if ( NULL != dmaStatePtr && NULL != driverDMAEngine ) {
		*dmaStatePtr = (UInt32)driverDMAEngine->getDmaState();
		err = kIOReturnSuccess;
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
IOReturn	AppleTexas2Audio::getStreamFormat ( IOAudioStreamFormat * streamFormatPtr )
{
	IOReturn				err = kIOReturnError;
	if ( NULL != streamFormatPtr ) {
		err = driverDMAEngine->getAudioStreamFormat( streamFormatPtr );
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
IOReturn	AppleTexas2Audio::readPowerState ( UInt32 selector, IOAudioDevicePowerState * powerState )
{
	IOReturn				err = kIOReturnError;
	if ( NULL != powerState ) {
		*powerState = ourPowerState;
		err = kIOReturnSuccess;
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
IOReturn	AppleTexas2Audio::setPowerState ( UInt32 selector, IOAudioDevicePowerState powerState )
{
	UInt32			microsecondsUntilComplete;
	IOReturn		err = kIOReturnError;
	if ( NULL != powerState ) {
		err = performPowerStateChange ( ourPowerState, powerState, &microsecondsUntilComplete );
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
IOReturn	AppleTexas2Audio::setBiquadCoefficients ( UInt32 selector, void * biquadCoefficients, UInt32 coefficientSize )
{
	UInt8			mcr2Data[kTexas2MC2Rwidth];
	IOReturn		err = kIOReturnSuccess;

	if ( kMaxBiquadWidth >= coefficientSize && NULL != biquadCoefficients && 0 == selector ) {
		for ( UInt32 index = 0; index < ( kTexas2NumBiquads * kTexas2MaxStreamCnt ); index ++ ) {
			if ( kTexas2NumBiquads > index ) {
				err = SndHWSetOutputBiquad ( kStreamFrontLeft, index, (FourDotTwenty*)biquadCoefficients );
				FailIf ( kIOReturnSuccess != err, Exit );
			} else {
				err = SndHWSetOutputBiquad ( kStreamFrontRight, index - kTexas2NumBiquads, (FourDotTwenty*)biquadCoefficients );
				FailIf ( kIOReturnSuccess != err, Exit );
			}
			(( EQFilterCoefficients*)biquadCoefficients)++;
		}
		err = Texas2_ReadRegister( kTexas2MainCtrl2Reg, mcr2Data );					//	[TAS EQ support]	rbm	10 Oct 2002	begin {
		FailIf ( kIOReturnSuccess != err, Exit );
		
		mcr2Data[0] &= ~( kFilter_MASK << kAP );
		mcr2Data[0] |= ( kNormalFilter << kAP );
		
		err = Texas2_WriteRegister( kTexas2MainCtrl2Reg, mcr2Data );		//	[TAS EQ support]	rbm	10 Oct 2002	} end
		FailIf ( kIOReturnSuccess != err, Exit );
	}
Exit:
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
IOReturn	AppleTexas2Audio::getBiquadInformation ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr )
{
#pragma unused ( scalarArg1 )
	IOReturn		result = kIOReturnError;
	if ( NULL != outStructPtr && NULL != outStructSizePtr ) {
		if ( *outStructSizePtr >= ( sizeof ( BiquadInfoList ) + ( sizeof ( UInt32 ) * ( kTexas2CoefficientsPerBiquad - 1 ) ) ) ) {
			((BiquadInfoListPtr)outStructPtr)->numBiquad = kTexas2NumBiquads;
			((BiquadInfoListPtr)outStructPtr)->numCoefficientsPerBiquad = kTexas2CoefficientsPerBiquad;
			((BiquadInfoListPtr)outStructPtr)->biquadCoefficientBitWidth = kTexas2CoefficientBitWidth;
			((BiquadInfoListPtr)outStructPtr)->coefficientIntegerBitWidth = kTexas2CoefficientIntegerBitWidth;
			((BiquadInfoListPtr)outStructPtr)->coefficientFractionBitWidth = kTexas2CoefficientFractionBitWidth;
			((BiquadInfoListPtr)outStructPtr)->coefficientOrder[0] = 'b0  ';
			((BiquadInfoListPtr)outStructPtr)->coefficientOrder[1] = 'b1  ';
			((BiquadInfoListPtr)outStructPtr)->coefficientOrder[2] = 'b2  ';
			((BiquadInfoListPtr)outStructPtr)->coefficientOrder[3] = 'a1  ';
			((BiquadInfoListPtr)outStructPtr)->coefficientOrder[4] = 'a2  ';
			*outStructSizePtr = ( sizeof ( BiquadInfoList ) + ( sizeof ( UInt32 ) * ( kTexas2CoefficientsPerBiquad - 1 ) ) );
			result = kIOReturnSuccess;
		}
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
IOReturn	AppleTexas2Audio::getProcessingParameters ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr )
{
	UInt32			index;
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != outStructPtr && NULL != outStructSizePtr ) {
		if (  kMaxProcessingParamSize >= *outStructSizePtr ) {
			for ( index = 0; index < ( kMaxProcessingParamSize / sizeof(UInt32) ); index++ ) {
				((UInt32*)outStructPtr)[index] = mProcessingParams[index];
			}
			/*	STUB:	Insert code here (see Aram and/or Joe)		*/
			//driverDMAEngine->set_swEQ(&mProcessingParams);
			/*	END STUB											*/
			err = kIOReturnSuccess;
		}
	}
	return (err);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
//	If loading software EQ, the scalarArg1 will be set to 'true' to indicate
//	that hardware EQ should be disabled.  If re-enabling hardware EQ then
//	scalarArg1 will be set to 'false' and the proper EQ for the current
//	hardware environment (jack state, speaker model, etc.) must be applied.
IOReturn	AppleTexas2Audio::setProcessingParameters ( UInt32 scalarArg1, void * inStructPtr, UInt32 inStructSize )
{
	UInt32			index;
	IOReturn		err = kIOReturnNotReadable;
	
	if ( NULL != inStructPtr && kMaxProcessingParamSize >= inStructSize ) {
		for ( index = 0; index < ( kMaxProcessingParamSize / sizeof(UInt32) ); index++ ) {
			mProcessingParams[index] = ((UInt32*)inStructPtr)[index];
		}
		disableLoadingEQFromFile = (bool)scalarArg1;
		if ( disableLoadingEQFromFile ) {
			SetUnityGainAllPass();
		} else {
			SelectOutputAndLoadEQ();
		}
		/*	STUB:	Insert code here (see Aram and/or Joe)		*/
		
		//driverDMAEngine->set_swEQ(inStructPtr);
		
		/*	END STUB											*/
		err = kIOReturnSuccess;
	}
	return (err);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTexas2Audio::invokeInternalFunction ( UInt32 functionSelector, void * inData ) {
	IOReturn			result = kIOReturnSuccess;
	
	switch ( functionSelector ) {
		case kInvokeHeadphoneInterruptHandler:		RealHeadphoneInterruptHandler ( 0, 0 );			break;
		case kInvokeSpeakerInterruptHandler:		RealDallasInterruptHandler ( 0, 0 );			break;
		case kInvokePerformDeviceSleep:				performDeviceSleep ();							break;
		case kInvokePerformDeviceIdleSleep:			performDeviceIdleSleep ();						break;
		case kInvokePerformDeviceWake:				performDeviceWake ();							break;
	}
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Hardware Utility user client support
UInt8	AppleTexas2Audio::readGPIO (UInt32 selector) {
	UInt8 *				address;
	UInt8				gpioValue;

	gpioValue = NULL;
	address = getGPIOAddress (selector);
	if (NULL != address)
		gpioValue = GpioReadByte (address);

	return (gpioValue);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Hardware Utility user client support
void	AppleTexas2Audio::writeGPIO (UInt32 selector, UInt8 data) {
	UInt8 *				address;
	UInt32				gpioValue;

	gpioValue = NULL;
	address = getGPIOAddress (selector);
	if (NULL != address)
		GpioWriteByte (address, data);

	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Setup the pointer to the shadow register and the size of the shadow
//	register.  The Texas2 has a different register set and different
//	register sizes from the Texas2 so a shadow register set is maintained
//	for both parts but only the appropriate set is used.
IOReturn 	AppleTexas2Audio::GetShadowRegisterInfo( UInt8 regAddr, UInt8 ** shadowPtr, UInt8* registerSize ) {
	IOReturn		err;
	
	err = kIOReturnSuccess;
	FailWithAction( NULL == shadowPtr, err = -50, Exit );
	FailWithAction( NULL == registerSize, err = -50, Exit );
	
	switch( regAddr )
	{
		case kTexas2MainCtrl1Reg:					*shadowPtr = (UInt8*)shadowTexas2Regs.sMC1R;	*registerSize = kTexas2MC1Rwidth;					break;
		case kTexas2DynamicRangeCtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sDRC;		*registerSize = kTexas2DRCwidth;					break;
		case kTexas2VolumeCtrlReg:					*shadowPtr = (UInt8*)shadowTexas2Regs.sVOL;		*registerSize = kTexas2VOLwidth;					break;
		case kTexas2TrebleCtrlReg:					*shadowPtr = (UInt8*)shadowTexas2Regs.sTRE;		*registerSize = kTexas2TREwidth;					break;
		case kTexas2BassCtrlReg:					*shadowPtr = (UInt8*)shadowTexas2Regs.sBAS;		*registerSize = kTexas2BASwidth;					break;
		case kTexas2MixerLeftGainReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sMXL;		*registerSize = kTexas2MIXERGAINwidth;				break;
		case kTexas2MixerRightGainReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sMXR;		*registerSize = kTexas2MIXERGAINwidth;				break;
		case kTexas2LeftBiquad0CtrlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sLB0;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftBiquad1CtrlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sLB1;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftBiquad2CtrlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sLB2;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftBiquad3CtrlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sLB3;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftBiquad4CtrlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sLB4;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftBiquad5CtrlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sLB5;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftBiquad6CtrlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sLB6;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightBiquad0CtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRB0;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightBiquad1CtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRB1;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightBiquad2CtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRB2;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightBiquad3CtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRB3;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightBiquad4CtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRB4;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightBiquad5CtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRB5;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightBiquad6CtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRB6;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftLoudnessBiquadReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sLLB;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightLoudnessBiquadReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRLB;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftLoudnessBiquadGainReg:		*shadowPtr = (UInt8*)shadowTexas2Regs.sLLBG;	*registerSize = kTexas2LOUDNESSBIQUADGAINwidth;	break;
		case kTexas2RightLoudnessBiquadGainReg:		*shadowPtr = (UInt8*)shadowTexas2Regs.sRLBG;	*registerSize = kTexas2LOUDNESSBIQUADGAINwidth;	break;
		case kTexas2AnalogControlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sACR;		*registerSize = kTexas2ANALOGCTRLREGwidth;			break;
		case kTexas2MainCtrl2Reg:					*shadowPtr = (UInt8*)shadowTexas2Regs.sMC2R;	*registerSize = kTexas2MC2Rwidth;					break;
		default:									err = -201; /* notEnoughHardware  */																break;
	}
	
Exit:
    return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Return 'true' if the 'gpioDDR' bit is non-zero.
UInt8	AppleTexas2Audio::GpioGetDDR( UInt8* gpioAddress )
{
	UInt8			gpioData;
	Boolean			result;
	
	result = gpioDDR_INPUT;
	if( NULL != gpioAddress )
	{
		gpioData = *gpioAddress;
		if( 0 != ( gpioData & ( 1 << gpioDDR ) ) )
			result = gpioDDR_OUTPUT ;
#ifdef kDEBUG_GPIO
		debugIOLog (3,  "***** GPIO DDR RD 0x%8.0X = 0x%2.0X returns %d", (unsigned int)gpioAddress, gpioData, result );
#endif
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt8 AppleTexas2Audio::GpioReadByte( UInt8* gpioAddress )
{
	UInt8			gpioData;
	
	gpioData = 0;
	if( NULL != gpioAddress )
	{
		gpioData = *gpioAddress;
#ifdef kDEBUG_GPIO
		debugIOLog (3,  "GpioReadByte( 0x%8.0X ), *gpioAddress 0x%X", (unsigned int)gpioAddress, gpioData );
#endif
	}

	return gpioData;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTexas2Audio::GpioWriteByte( UInt8* gpioAddress, UInt8 data )
{
	if( NULL != gpioAddress )
	{
		*gpioAddress = data;
#ifdef kDEBUG_GPIO
		debugIOLog (3,  "GpioWrite( 0x%8.0X, 0x%2.0X )", (unsigned int)gpioAddress, data);
#endif
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Return 'true' if the 'gpioData' bit is non-zero.  This function does not
//	return the state of the pin.
Boolean AppleTexas2Audio::GpioRead( UInt8* gpioAddress )
{
	UInt8			gpioData;
	Boolean			result;
	
	result = 0;
	if( NULL != gpioAddress )
	{
		gpioData = *gpioAddress;
		if( 0 != ( gpioData & ( 1 << gpioDATA ) ) )
			result = 1;
#ifdef kDEBUG_GPIO
		debugIOLog (3,  "GpioRead( 0x%8.0X ) result %d, *gpioAddress 0x%2.0X", (unsigned int)gpioAddress, result, *gpioAddress );
#endif
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Sets the 'gpioDDR' to OUTPUT and sets the 'gpioDATA' to the 'data' state.
void	AppleTexas2Audio::GpioWrite( UInt8* gpioAddress, UInt8 data )
{
	UInt8		gpioData;
	
	if( NULL != gpioAddress ) {
		if( 0 == data )
			gpioData = ( gpioDDR_OUTPUT << gpioDDR ) | ( 0 << gpioDATA );
		else
			gpioData = ( gpioDDR_OUTPUT << gpioDDR ) | ( 1 << gpioDATA );
		*gpioAddress = gpioData;
		debugIOLog (7,  "GpioWrite( 0x%8.0X, 0x%2.0X ), *gpioAddress 0x%2.0X", (unsigned int)gpioAddress, gpioData, *gpioAddress );
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexas2Audio::InitEQSerialMode (UInt32 mode, Boolean restoreOnNormal)
{
	IOReturn		err;
	UInt8			initData;
	UInt8			previousData;
	
	debugIOLog (3, "+ InitEQSerialMode (%8lX, %d)", mode, restoreOnNormal);
	initData = (kNormalLoad << kFL);
	if (kSetFastLoadMode == mode)
		initData = (kFastLoad << kFL);
		
	err = Texas2_ReadRegister (kTexas2MainCtrl1Reg, &previousData);
	FailIf ( kIOReturnSuccess != err, Exit );

	initData |= ( previousData & ~( 1 << kFL ) );
	err = Texas2_WriteRegister (kTexas2MainCtrl1Reg, &initData);
	FailIf ( kIOReturnSuccess != err, Exit );

Exit:
	debugIOLog (3, "- InitEQSerialMode (%8lX, %d) err = %d", mode, restoreOnNormal, err);
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Updates the amplifier mute state and delays for amplifier settling if
//	the amplifier mute state is not the current mute state.
IOReturn AppleTexas2Audio::SetAmplifierMuteState( UInt32 ampID, Boolean muteState )
{
	IOReturn			err;
	Boolean				curMuteState;
	
	err = kIOReturnSuccess;
	switch( ampID )
	{
		case kHEADPHONE_AMP:
			if ( hdpnMuteGpio ) {
				curMuteState = GpioRead( hdpnMuteGpio );
				if( muteState != curMuteState )
				{
					GpioWrite( hdpnMuteGpio, muteState );
				}
			}
			break;
		case kSPEAKER_AMP:
			if ( ampMuteGpio ) {
				curMuteState = GpioRead( ampMuteGpio );
				if( muteState != curMuteState )
				{
					GpioWrite( ampMuteGpio, muteState );
				}
			}
			break;
		case kLINEOUT_AMP:
			if ( lineOutMuteGpio ) {
				curMuteState = GpioRead( lineOutMuteGpio );
				if( muteState != curMuteState )
				{
					GpioWrite( lineOutMuteGpio, muteState );
				}
			}
			break;
		case kMASTER_AMP:
			if ( masterMuteGpio ) {
				curMuteState = GpioRead( masterMuteGpio );
				if( muteState != curMuteState )
				{
					GpioWrite( masterMuteGpio, muteState );
				}
			}
			break;
		default:
			err = kIOReturnBadArgument;
			break;
	}
	if ( kIOReturnSuccess != err ) {
		debugIOLog (3,  "... SetAmplifierMuteState( ampID %d, %d ) RETURNS 0x%X", (unsigned int)ampID, (unsigned int)muteState, err );
	}
	return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexas2Audio::SetVolumeCoefficients( UInt32 left, UInt32 right )
{
	UInt8		volumeData[kTexas2VOLwidth];
	IOReturn	err;
	
	debugIOLog (3, "SetVolumeCoefficients: L=%ld R=%ld", left, right);
	
	volumeData[2] = left;														
	volumeData[1] = left >> 8;												
	volumeData[0] = left >> 16;												
	
	volumeData[5] = right;														
	volumeData[4] = right >> 8;												
	volumeData[3] = right >> 16;
	
	err = Texas2_WriteRegister( kTexas2VolumeCtrlReg, volumeData );
	FailIf ( kIOReturnSuccess != err, Exit );
Exit:
	return err;
}


// --------------------------------------------------------------------------
//	This utility function indicates if the CODEC is in reset.  No I2C transaction
//	will be attempted while the CODEC is in reset as reset initialization of
//	register contents takes precedence.
Boolean AppleTexas2Audio::IsCodecRESET( Boolean logMessage ) {
	Boolean		result;
	
	result = false;
	if ( hasANDedReset ) {
		if ( ( hdpnActiveState == GpioRead ( hdpnMuteGpio ) ) && ( ampActiveState == GpioRead ( ampMuteGpio ) ) ) {
			result = true;
		}
	} else if ( NULL != hwResetGpio ) {
		if ( hwResetActiveState == GpioRead ( hwResetGpio ) ) {
			result = true;
		}
	}
	if ( result && logMessage ) {
		debugIOLog (3,  "[AppleTexas2Audio] ***** IsCodecRESET() completed WITH TAS3004 IS RESET!!!!" );
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Sets the TAS3004 to either an idle or sleep state.  Both modes 
//	set the amplifiers and TAS3004 to a sleep state.  Idle leaves the
//	TAS3004 RESET negated while Sleep leaves the TAS3004 RESET asserted.
void	AppleTexas2Audio::Texas2_Quiesce ( UInt32 mode ) {
    IOService *				keyLargo;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	debugIOLog (3, "+ AppleTexas2Audio::Texas2_Quiesce");

	keyLargo = NULL;
	
	//	Mute all of the amplifiers
		
	//	[2855519]	begin {
	
	//	[3234624]	begin {
	//	Only set analog power down mode if running with the headphone jack
	//	inserted when on a system that has the ANDed reset or always for 
	//	systems having a dedicated GPIO reset.
	if ( hasANDedReset ) {
		if ( kIOAudioDeviceSleep == mode ) {
			SetAmplifierMuteState ( kHEADPHONE_AMP, ASSERT_GPIO ( hdpnActiveState ) );		//	mute
			SetAmplifierMuteState ( kSPEAKER_AMP, ASSERT_GPIO ( ampActiveState ) );			//	mute
			IOSleep (kAmpRecoveryMuteDuration);
			SetAnalogPowerDownMode (kPowerDownAnalog);
			Texas2_Reset_ASSERT();
		} else if ( kIOAudioDeviceIdle == mode ) {
			if ( codecResetFlag ) {
				Texas2_Reset_NEGATE();
			}
			if ( !IsHeadphoneConnected() ) {
				//	Headphones aren't connected so go to lowest power configuration
				//	by switching to the headphone amplifier (driving no transducer)
				//	and turn off the analog power section of the TAS3004.
				SetAmplifierMuteState ( kHEADPHONE_AMP, NEGATE_GPIO ( hdpnActiveState  ) );	//	unmute
				IOSleep (kAmpRecoveryMuteDuration);
				SetAmplifierMuteState ( kSPEAKER_AMP, ASSERT_GPIO ( ampActiveState ) );		//	mute
				//debugIOLog (3,  "CHECK 3" );
			} else {
				SetAnalogPowerDownMode (kPowerDownAnalog);
			}
		}
	} else {
		SetAmplifierMuteState ( kHEADPHONE_AMP, ASSERT_GPIO ( hdpnActiveState ) );			//	mute
		SetAmplifierMuteState ( kSPEAKER_AMP, ASSERT_GPIO ( ampActiveState ) );				//	mute
		IOSleep (kAmpRecoveryMuteDuration);
		SetAnalogPowerDownMode (kPowerDownAnalog);
		SetAmplifierMuteState( kLINEOUT_AMP, 1 == lineOutMuteActiveState ? 1 : 0 );
		IOSleep (kAmpRecoveryMuteDuration);
		
		if ( kIOAudioDeviceSleep == mode ) {
			debugIOLog (3,  "Texas2_Quiesce ( %d ) asserting RESET", (unsigned int)mode );
			Texas2_Reset_ASSERT();
		} else {
			debugIOLog (3,  "Texas2_Quiesce ( %d ) negating RESET", (unsigned int)mode );
			Texas2_Reset_NEGATE();
		}
	}
	//	[3234624]	} end		[2855519]	} end

	//	Timing for systems with a master mute [see 2949185] is as follows:
	//					____________________
	//	MASTER_MUTE:	                    |__________
	//					__________
	//	SPEAKER MUTE:	          |____________________
	//
	//					__________
	//	HEADPHONE MUTE:	          |____________________
	//
	//					       -->| 500 MS |<--
	//
	if ( NULL != masterMuteGpio && hasANDedReset ) {			//	[2949185] begin {
		IOSleep ( 500 );
		SetAmplifierMuteState (kMASTER_AMP, ASSERT_GPIO (masterMuteActiveState));	//	assert mute
	}										//	} end [2949185]

	keyLargo = IOService::waitForService (IOService::serviceMatching ("KeyLargo"));
	
	if (NULL != keyLargo) {
		funcSymbolName = OSSymbol::withCString ( "keyLargo_powerI2S" );						//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		// ...and turn off the i2s clocks...
		switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:	keyLargo->callPlatformFunction (funcSymbolName, false, (void *)false, (void *)0, 0, 0);	break;
			case kUseI2SCell1:	keyLargo->callPlatformFunction (funcSymbolName, false, (void *)false, (void *)1, 0, 0);	break;
		}
		funcSymbolName->release ();															//	[3323977]
	}
Exit:
	debugIOLog (3, "- AppleTexas2Audio::Texas2_Quiesce");
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[2855519]	Call down to lower level functions to implement the Codec
//	RESET assertion and negation where hardware dependencies exist...
void	AppleTexas2Audio::Texas2_Reset ( void ) {
	ClockSource				clockSource;
	UInt32					sclkDivisor;
	UInt32					mclkDivisor;
	UInt32					dataFormat;	//	[3060321]	rbm	2 Oct 2002
	UInt32					myFrameRate;
	dataFormat = ( ( 2 << kNumChannelsInShift ) | kDataIn16 | ( 2 << kNumChannelsOutShift ) | kDataOut16 );	//	[3060321]	rbm	2 Oct 2002

	myFrameRate = frameRate (0);

	FailIf (FALSE == audioI2SControl->setSampleParameters (myFrameRate, 256, &clockSource, &mclkDivisor, &sclkDivisor, kSndIOFormatI2S64x), Exit);
	//	[3060321]	The data word format register and serial format register require that the I2S clocks be stopped and
	//				restarted before the register value is applied to operation of the I2S IOM.  We now pass the data
	//				word format to setSerialFormatRegister as that method stops the clocks when applying the value
	//				to the serial format register.  That method now also sets the data word format register while
	//				the clocks are stopped.		rbm	2 Oct 2002
	audioI2SControl->setSerialFormatRegister (clockSource, mclkDivisor, sclkDivisor, kSndIOFormatI2S64x, dataFormat);

    if ( hwResetGpio || hasANDedReset ) {
		IOSleep ( kCodec_RESET_SETUP_TIME );	//	I2S clocks must be running prerequisite to RESET
        Texas2_Reset_ASSERT();
        IOSleep ( kCodec_RESET_HOLD_TIME );
        Texas2_Reset_NEGATE();
        IOSleep ( kCodec_RESET_RELEASE_TIME );	//	No I2C transactions for 
    }

Exit:
	return;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[2855519]	
void	AppleTexas2Audio::Texas2_Reset_ASSERT ( void ) {
    if ( hasANDedReset ) {
        SetAmplifierMuteState (kHEADPHONE_AMP, ASSERT_GPIO (hdpnActiveState));		//	mute
        SetAmplifierMuteState (kSPEAKER_AMP, ASSERT_GPIO (ampActiveState));			//	mute
    } else if ( hwResetGpio ) {
		GpioWrite( hwResetGpio, hwResetActiveState );
    } else {
		debugIOLog (3,  "*** HARDWARE RESET ASSERT METHOD NOT DETERMINED!!!" );
	}
	codecResetFlag = TRUE;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[2855519]	Negation of the Codec RESET for CPUs that implement the
//	reset with an AND of the speaker and headphone amplifier mute states
//	requires negating the target amplifier mute.  A 'Line Out' amplifier
//	cannot exist in this configuration.  All other CPUs implement the
//	Codec RESET with a GPIO where direct manipulation is implemented.
void	AppleTexas2Audio::Texas2_Reset_NEGATE ( void ) {
	UInt32		theDeviceMatch;
	
    if ( hasANDedReset ) {
		theDeviceMatch = GetDeviceMatch();
		if ( kSndHWCPUHeadphone == theDeviceMatch ) {
			SetAmplifierMuteState ( kHEADPHONE_AMP, NEGATE_GPIO ( hdpnActiveState ) );	//	unmute
		} else {
			SetAmplifierMuteState ( kSPEAKER_AMP, NEGATE_GPIO ( ampActiveState ) );		//	unmute
		}
    } else if ( hwResetGpio ) {
		GpioWrite( hwResetGpio, hwResetActiveState ? 0 : 1 );
    } else {
		debugIOLog (3,  "*** HARDWARE RESET NEGATE METHOD NOT DETERMINED!!!" );
	}
	codecResetFlag = FALSE;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This routine will perform a reset of the TAS3004C and then initialize
//	all registers within the TAS3004C to the values already held within 
//	the shadow registers.  The RESET sequence must not be performed until
//	the I2S clocks are running.	 The TAS3004C may hold the I2C bus signals
//	SDA and SCL low until the reset sequence (high->low->high) has been
//	completed.
IOReturn	AppleTexas2Audio::Texas2_Initialize() {
	IOReturn		err;
	UInt32		retryCount;
	UInt32		initMode;
	UInt8		oldMode;
	UInt8		*shadowPtr;
	UInt8		registerSize;
	Boolean		done;
	
	err = -227;	// siDeviceBusyErr
	done = false;
	oldMode = 0;
	initMode = kUPDATE_HW;
	retryCount = 0;
	if (!semaphores)
	{
		semaphores = 1;
		do{
			debugIOLog (6,  "[AppleTexas2Audio] ... RETRYING, retryCount %ld", retryCount );
            Texas2_Reset();
			if ( !IsCodecRESET( true ) ) {
				if( 0 == oldMode )
					Texas2_ReadRegister( kTexas2MainCtrl1Reg, &oldMode );					//	save previous load mode
	
				err = InitEQSerialMode( kSetFastLoadMode, kDontRestoreOnNormal );								//	set fast load mode for biquad initialization
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				GetShadowRegisterInfo( kTexas2LeftBiquad0CtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2LeftBiquad0CtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
		
				GetShadowRegisterInfo( kTexas2LeftBiquad1CtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2LeftBiquad1CtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
		
				GetShadowRegisterInfo( kTexas2LeftBiquad2CtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2LeftBiquad2CtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
		
				GetShadowRegisterInfo( kTexas2LeftBiquad3CtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2LeftBiquad3CtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
		
				GetShadowRegisterInfo( kTexas2LeftBiquad4CtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2LeftBiquad4CtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
		
				GetShadowRegisterInfo( kTexas2LeftBiquad5CtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2LeftBiquad5CtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				GetShadowRegisterInfo( kTexas2LeftBiquad6CtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2LeftBiquad6CtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				GetShadowRegisterInfo( kTexas2LeftLoudnessBiquadReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2LeftLoudnessBiquadReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				GetShadowRegisterInfo( kTexas2RightBiquad0CtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2RightBiquad0CtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
		
				GetShadowRegisterInfo( kTexas2RightBiquad1CtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2RightBiquad1CtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
		
				GetShadowRegisterInfo( kTexas2RightBiquad2CtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2RightBiquad2CtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
		
				GetShadowRegisterInfo( kTexas2RightBiquad3CtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2RightBiquad3CtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
		
				GetShadowRegisterInfo( kTexas2RightBiquad4CtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2RightBiquad4CtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
		
				GetShadowRegisterInfo( kTexas2RightBiquad5CtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2RightBiquad5CtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				GetShadowRegisterInfo( kTexas2RightBiquad6CtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2RightBiquad6CtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				GetShadowRegisterInfo( kTexas2RightLoudnessBiquadReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2RightLoudnessBiquadReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				err = InitEQSerialMode( kSetNormalLoadMode, kDontRestoreOnNormal );								//	set normal load mode for most register initialization
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				GetShadowRegisterInfo( kTexas2DynamicRangeCtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2DynamicRangeCtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
					
				GetShadowRegisterInfo( kTexas2VolumeCtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2VolumeCtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				GetShadowRegisterInfo( kTexas2TrebleCtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2TrebleCtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				GetShadowRegisterInfo( kTexas2BassCtrlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2BassCtrlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				GetShadowRegisterInfo( kTexas2MixerLeftGainReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2MixerLeftGainReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
		
				GetShadowRegisterInfo( kTexas2MixerRightGainReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2MixerRightGainReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
				GetShadowRegisterInfo( kTexas2LeftLoudnessBiquadGainReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2LeftLoudnessBiquadGainReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				GetShadowRegisterInfo( kTexas2RightLoudnessBiquadGainReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2RightLoudnessBiquadGainReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				GetShadowRegisterInfo( kTexas2AnalogControlReg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2AnalogControlReg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				GetShadowRegisterInfo( kTexas2MainCtrl2Reg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2MainCtrl2Reg, shadowPtr );
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
				GetShadowRegisterInfo( kTexas2MainCtrl1Reg, &shadowPtr, &registerSize );
				err = Texas2_WriteRegister( kTexas2MainCtrl1Reg, &oldMode );			//	restore previous load mode
				FailIf( kIOReturnSuccess != err, AttemptToRetry );
			}
AttemptToRetry:				
			if( kIOReturnSuccess == err )		//	terminate when successful
			{
				done = true;
			}
			retryCount++;
		} while ( !done && ( kTexas2_MAX_RETRY_COUNT != retryCount ) );
		semaphores = 0;
		if( kTexas2_MAX_RETRY_COUNT == retryCount )
			debugIOLog (3,  "\n\n\n\n          Texas2 IS DEAD: Check %s\n\n\n", "ChooseAudio in fcr1" );
	}
	if( kIOReturnSuccess != err )
		debugIOLog (3,  "[AppleTexas2Audio] Texas2_Initialize() err = %d", err );

    return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Reading registers with the Texas2 is not possible.  A shadow register
//	is maintained for each Texas2 hardware register.  Whenever a write
//	operation is performed on a hardware register, the data is written to 
//	the shadow register.  Read operations copy data from the shadow register
//	to the client register buffer.
IOReturn 	AppleTexas2Audio::Texas2_ReadRegister(UInt8 regAddr, UInt8* registerData) {
	UInt8			registerSize;
	UInt32			regByteIndex;
	UInt8			*shadowPtr;
	IOReturn		err;
	
	err = kIOReturnSuccess;
	if ( IsCodecRESET( false ) ) {
		debugIOLog (3,  "[AppleTexas2Audio] Texas2_ReadRegister( 0x%2.0X, 0x%8.0X ) WHILE TAS3004 IS RESET!!!!", regAddr, (unsigned int)registerData );
	}
	
	// quiet warnings caused by a complier that can't really figure out if something is going to be used uninitialized or not.
	registerSize = 0;
	shadowPtr = NULL;
	err = GetShadowRegisterInfo( regAddr, &shadowPtr, &registerSize );
	if( kIOReturnSuccess == err )
	{
		for( regByteIndex = 0; regByteIndex < registerSize; regByteIndex++ )
		{
			registerData[regByteIndex] = shadowPtr[regByteIndex];
		}
	}
	if( kIOReturnSuccess != err )
		debugIOLog (3,  "[AppleTexas2Audio] %d notEnoughHardware = Texas2_ReadRegister( 0x%2.0X, 0x%8.0X )", err, regAddr, (unsigned int)registerData );

    return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	All Texas2 write operations pass through this function so that a shadow
//	copy of the registers can be kept in global storage.  This function does enforce the data
//	size of the target register.  No partial register write operations are
//	supported.  IMPORTANT:  There is no enforcement regarding 'load' mode 
//	policy.  Client routines should properly maintain the 'load' mode by 
//	saving the contents of the master control register, set the appropriate 
//	load mode for the target register and then restore the previous 'load' 
//	mode.  All biquad registers should only be loaded while in 'fast load' 
//	mode.  All other registers should be loaded while in 'normal load' mode.
IOReturn 	AppleTexas2Audio::Texas2_WriteRegister ( UInt8 regAddr, UInt8* registerData ) {
	UInt8			registerSize;
	UInt32			regByteIndex;
	UInt8			*shadowPtr;
	IOReturn		result = kIOReturnSuccess;
	Boolean			success = FALSE;
	
	debugIOLog ( 6, "+ AppleTexas2Audio::Texas2_WriteRegister ( 0x%2X, %p )", regAddr, registerData );
	
	// quiet warnings caused by a complier that can't really figure out if something is going to be used uninitialized or not.
	registerSize = 0;
	shadowPtr = NULL;
	
	result = GetShadowRegisterInfo ( regAddr, &shadowPtr, &registerSize );
	if ( kIOReturnSuccess == result ) {
		//	Write through to the shadow register as a 'write through' cache would and
		//	then write the data to the hardware;
		for ( regByteIndex = 0; regByteIndex < registerSize; regByteIndex++ ) {
			shadowPtr[regByteIndex] = registerData[regByteIndex];
		}
		if ( NULL != audioI2SControl ) {
			if ( IsCodecRESET( false ) ) {
				debugIOLog (3,  "  TAS3004 in RESET!" );
				result = kIOReturnNotReady;
			} else {
				if ( openI2C() ) {
					success = interface->writeI2CBus ( DEQAddress, regAddr, registerData, registerSize );
					closeI2C();
					//	[3166905]	begin {
					if ( !success ) {
						result = Texas2_Initialize();
					}
					//	[3166905]	} end
				} else {
					debugIOLog (3, "  Could not OPEN I2C driver!");
					result = kIOReturnNotReady;
				}
			}
		}
	}

	debugIOLog ( 6, "- AppleTexas2Audio::Texas2_WriteRegister ( 0x%2X, %p ) returns 0x%lX", regAddr, registerData, result );
    return result;
}


#pragma mark +UTILITY FUNCTIONS
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTexas2Audio::DallasDriverPublished (AppleTexas2Audio * AppleTexas2Audio, void * refCon, IOService * newService) {
	bool						resultCode;

	resultCode = FALSE;

	FailIf (NULL == AppleTexas2Audio, Exit);
	FailIf (NULL == newService, Exit);

	AppleTexas2Audio->dallasDriver = (AppleDallasDriver *)newService;

	AppleTexas2Audio->attach (AppleTexas2Audio->dallasDriver);
	AppleTexas2Audio->dallasDriver->open (AppleTexas2Audio);

	if (NULL != AppleTexas2Audio->dallasDriverNotifier)
		AppleTexas2Audio->dallasDriverNotifier->remove ();

	resultCode = TRUE;

Exit:
	return resultCode;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexas2Audio::DeviceInterruptServiceAction ( OSObject * owner, void * arg1, void * arg2, void * arg3, void * arg4 ) {
	AppleTexas2Audio *		appleTexas2Audio;
	
	appleTexas2Audio = OSDynamicCast ( AppleTexas2Audio, owner );
	FailIf ( !appleTexas2Audio, Exit );
	appleTexas2Audio->DeviceInterruptService ();
Exit:
	return kIOReturnSuccess;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Set state equal to if external speakers are around. If active, restore
// preferred mute state. If inactive, mute in hardware.
// Has to be called on the IOAudioFamily workloop (because we do the config change of the controls)!!!
void AppleTexas2Audio::DeviceInterruptService (void) {
	IOReturn			err;
	UInt32				result;
	UInt8				bEEPROM[32];
	OSNumber *			headphoneState;			// For [2926907]

	debugIOLog (3, "+ OutputPorts::DeviceInterruptService");
	err = kIOReturnSuccess;

	// get the layoutID from the IORegistry for the machine we're running on
	// deviceMatch is set from sound objects, but we're hard coding it using a table at the moment
	result = FALSE;						// FALSE == failure from dallasDriver->getSpeakerID
	
	// for 2749470
	if (NULL != driverDMAEngine) {
		if (kSndHWInternalSpeaker == GetDeviceMatch ()) {
			// when it's just the internal speaker, figure out if we have to mute the right channel
			switch (layoutID) {
				case layoutQ26:					//	[3084945]	rbm		18 Nov 2002		Fall through to layoutP58
				case layoutP58:
					driverDMAEngine->setRightChanMixed (TRUE);
					useMasterVolumeControl = TRUE;
					break;
				default:
					driverDMAEngine->setRightChanMixed (FALSE);
					useMasterVolumeControl = FALSE;
			}
		} else {
			// If it's external speakers or headphones, don't mute the right channel and create it if we've already deleted it
			driverDMAEngine->setRightChanMixed (FALSE);
			useMasterVolumeControl = FALSE;
		}
	}
	
	if (NULL != dallasDriver && TRUE == dallasSpeakersConnected && FALSE == dallasSpeakersProbed) {
		speakerID = 0;						//	assume that rom will not be read successfully
		familyID = 0;
		// get the layoutID from the IORegistry for the machine we're running on (which is the machine's device-id)
		// deviceMatch is set from sound objects, but we're hard coding it using a table at the moment
		debugIOLog (3, "About to get the speaker ID");
		result = dallasDriver->getSpeakerID (bEEPROM);
		dallasSpeakersProbed = TRUE;
	
		debugIOLog (3, "DallasDriver result = %d speakerID = %d", (unsigned int)result, (unsigned int)speakerID);

		if (FALSE == result) {
			// If the Dallas speakers are misinserted, set registry up for our MacBuddy buddies no matter what the output device is
			speakerConnectFailed = TRUE;
		} else {
			familyID = bEEPROM[0];
			speakerID = bEEPROM[1];			//	only copy rom result if rom was successfully read...
			speakerConnectFailed = FALSE;
		}
		setProperty (kSpeakerConnectError, speakerConnectFailed);
	
		if ( dallasSpeakersConnected && FALSE == result) {		//	[2973537] rbm 25 Sept. 2002
			// Only put up our alert if the Dallas speakers are the output device
			DisplaySpeakersNotFullyConnected (this, NULL);
		}
	}

	SelectOutputAndLoadEQ();									//	[2878119]

	// Set the level controls to their (possibly) new min and max values
	minVolume = kMinimumVolume;
	maxVolume = kMaximumVolume + drc.maximumVolume;

	AdjustControls ();

	// For [3252100]
	if (detectCollection & kSndHWCPUHeadphone) {
		headphoneState = OSNumber::withNumber (1, 32);
	} else {
		headphoneState = OSNumber::withNumber ((long long unsigned int)0, 32);
	}

	debugIOLog (3,  "DeviceInterruptService headphoneConnection->hardwareValueChanged ( %d )", (unsigned int)headphoneState );	// [3066930]
	(void)headphoneConnection->hardwareValueChanged (headphoneState);	// write value out to registry
	headphoneState->release ();

	debugIOLog (3, "- OutputPorts::DeviceInterruptService");
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Boolean AppleTexas2Audio::usesDigitalPlaythrough ( void )
{
	Boolean			result = false;
	
	//	[3275982]	begin	{
	if ( layoutP72D <= layoutID ) {
		result = true;
	}
	//	[3275982]	}	end
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AppleTexas2Audio::GetDeviceMatch (void) {
	UInt32			theDeviceMatch;

	debugIOLog (3,  "+ GetDeviceMatch detectCollection 0x%X", (unsigned int)detectCollection );
	
	IsHeadphoneConnected();							//	update the detectCollection bit map with the headphone detect status
	IsLineOutConnected();							//	update the detectCollection bit map with the line output detect status
	IsSpeakerConnected();							//	update the detectCollection bit map with the external speaker detect status
	
	if ( NULL == masterMuteGpio ) {
		//	Returns a target output selection for the current detect states as
		//	follows:
		//
		//											Line Out	Headphone	External
		//											Detect		Detect		Speaker
		//																	Detect
		//											--------	---------	--------
		//	Internal Speaker						out			out			out
		//	External Speaker						out			out			in
		//	Headphone								out			in			out
		//	Headphone								out			in			in
		//	Line Output								in			out			out
		//	Line Output								in			out			in
		//	Line Output								in			in			out
		//	Line Output								in			in			in
		//
		if ( detectCollection & kSndHWLineOutput ) {
			theDeviceMatch = kSndHWLineOutput;
		} else if ( detectCollection & kSndHWCPUHeadphone ) {
			theDeviceMatch = kSndHWCPUHeadphone;
		} else if ( detectCollection & kSndHWCPUExternalSpeaker ) {
			theDeviceMatch = kSndHWCPUExternalSpeaker;
		} else {
			theDeviceMatch = kSndHWInternalSpeaker;
		}
	} else {
		//	Returns a target output selection for the current detect states as
		//	follows:
		//											Line Out	Headphone	External
		//											Detect		Detect		Speaker
		//																	Detect
		//											--------	---------	--------
		//	Internal Speaker						out			out			out
		//	External Speaker						out			out			in
		//	Headphone								out			in			out
		//	Headphone								out			in			in
		//	Line Output								in			out			out
		//	Line Output								in			out			in
		//	Line Output								in			in			out
		//	Line Output								in			in			in
		//
		if ( detectCollection & kSndHWLineOutput ) {
			theDeviceMatch = kSndHWLineOutput;
		} else if ( detectCollection & kSndHWCPUHeadphone ) {
			theDeviceMatch = kSndHWCPUHeadphone;
		} else if ( detectCollection & kSndHWCPUExternalSpeaker ) {
			theDeviceMatch = kSndHWCPUExternalSpeaker;
		} else {
			theDeviceMatch = kSndHWInternalSpeaker;
		}
	}
	
	switch ( theDeviceMatch ) {
		case kSndHWLineOutput:			debugIOLog (3,  "- GetDeviceMatch returns %d kSndHWLineOutput", (unsigned int)theDeviceMatch );				break;
		case kSndHWCPUHeadphone:		debugIOLog (3,  "- GetDeviceMatch returns %d kSndHWCPUHeadphone", (unsigned int)theDeviceMatch );			break;
		case kSndHWCPUExternalSpeaker:	debugIOLog (3,  "- GetDeviceMatch returns %d kSndHWCPUExternalSpeaker", (unsigned int)theDeviceMatch );		break;
		case kSndHWInternalSpeaker:		debugIOLog (3,  "- GetDeviceMatch returns %d kSndHWInternalSpeaker", (unsigned int)theDeviceMatch );		break;
	}
	
	return theDeviceMatch;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IORegistryEntry *AppleTexas2Audio::FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value) {
	OSIterator				*iterator;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSNumber				*tmpNumber;

	theEntry = NULL;
	iterator = NULL;
	FailIf (NULL == start, Exit);

	iterator = start->getChildIterator (gIOServicePlane);
	FailIf (NULL == iterator, Exit);

	while (NULL == theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, iterator->getNextObject ())) != NULL) {
		if (strcmp (tmpReg->getName (), name) == 0) {
			tmpNumber = OSDynamicCast (OSNumber, tmpReg->getProperty (key));
			if (NULL != tmpNumber && tmpNumber->unsigned32BitValue () == value) {
				theEntry = tmpReg;
								theEntry->retain();
			}
		}
	}

Exit:
	if (NULL != iterator) {
		iterator->release ();
	}
	return theEntry;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IORegistryEntry *AppleTexas2Audio::FindEntryByProperty (const IORegistryEntry * start, const char * key, const char * value) {
	OSIterator				*iterator;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSData					*tmpData;

	theEntry = NULL;
	iterator = start->getChildIterator (gIODTPlane);
	FailIf (NULL == iterator, Exit);

	while (NULL == theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, iterator->getNextObject ())) != NULL) {
		tmpData = OSDynamicCast (OSData, tmpReg->getProperty (key));
		if (NULL != tmpData && tmpData->isEqualTo (value, strlen (value))) {
			theEntry = tmpReg;
		}
	}

Exit:
	if (NULL != iterator) {
		iterator->release ();
	}
	return theEntry;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexas2Audio::GetCustomEQCoefficients (UInt32 layoutID, UInt32 deviceID, UInt32 speakerID, EQPrefsElementPtr *filterSettings) {
	IOReturn				err;
	Boolean					found;
	UInt32					index;
	EQPrefsElementPtr		eqElementPtr;

	debugIOLog (3, "+ GetCustomEQCoefficients (0x%lX, 0x%lX, 0x%lX, %p)", layoutID, deviceID, speakerID, filterSettings);
#ifdef kLOG_EQ_TABLE_TRAVERSE
	debugIOLog (3, "gEQPrefs %p", gEQPrefs);
#endif

	err = -50;
	FailIf (0 == layoutID, Exit);
	FailIf (NULL == filterSettings, Exit);
	FailIf (NULL == gEQPrefs, Exit);

	found = FALSE;
	eqElementPtr = NULL;
	*filterSettings = NULL;
	for (index = 0; index < gEQPrefs->eqCount && !found; index++) {
		eqElementPtr = &(gEQPrefs->eq[index]);
#ifdef kLOG_EQ_TABLE_TRAVERSE
		debugIOLog (3, "eqElementPtr %p, index %d, eqCount %d, layoutID 0x%X, deviceID 0x%X, speakerID 0x%X", eqElementPtr, index, gEQPrefs->eqCount, eqElementPtr->layoutID, eqElementPtr->deviceID, eqElementPtr->speakerID);
#endif
		if ((eqElementPtr->layoutID == layoutID) && (eqElementPtr->deviceID == deviceID) && (eqElementPtr->speakerID == speakerID)) {
			found = TRUE;
		}
	}

	if (TRUE == found) {
		*filterSettings = eqElementPtr;
		err = kIOReturnSuccess;
	}

Exit:
#ifdef kLOG_EQ_TABLE_TRAVERSE
	if (kIOReturnSuccess == err) {
		debugIOLog (3, "filterSettings %p", filterSettings);
	}
#endif
	debugIOLog (3, "- GetCustomEQCoefficients (0x%lX, 0x%lX, 0x%lX, %p) returns %d", layoutID, deviceID, speakerID, filterSettings, err);

	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AppleTexas2Audio::GetDeviceID (void) {
	IORegistryEntry			*sound;
	OSData					*tmpData;
	UInt32					*deviceID;
	UInt32					theDeviceID;

	debugIOLog (3,  "+ AppleTexas2Audio::GetDeviceID" );
	theDeviceID = 0;

	sound = ourProvider->childFromPath (kSoundEntry, gIODTPlane);
	FailIf (!sound, Exit);

	tmpData = OSDynamicCast (OSData, sound->getProperty (kDeviceID));
	FailIf (!tmpData, Exit);
	deviceID = (UInt32*)tmpData->getBytesNoCopy ();
	if (NULL != deviceID) {
		theDeviceID = *deviceID;
	}

Exit:
	debugIOLog (3,  "- AppleTexas2Audio::GetDeviceID returns %d", (unsigned int)theDeviceID );
	return theDeviceID;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[2855519]	begin {
//	Detect Codec reset method "B" where the logical AND of the speaker and
//	headphone mute in the active state (i.e. muted) generates the Codec
//	reset.  This is an optional implemenation that may not be present on
//	all CPUs.  No FailIf exit here since absence of the properties is not
//	indicative of a failure.
Boolean AppleTexas2Audio::HasANDedReset (void) {
	IORegistryEntry			*sound;
	OSData					*tmpData;
	UInt32					*hasANDedResetPtr;
	UInt32					hasANDedResetPropValue;

	debugIOLog (3,  "+ AppleTexas2Audio::HasANDedReset" );
	hasANDedResetPropValue = 0;

	sound = ourProvider->childFromPath ( kSoundEntry, gIODTPlane );
	FailIf (!sound, Exit);
	
	tmpData = OSDynamicCast ( OSData, sound->getProperty ( kHasANDedResetPropName ) );
	if ( tmpData ) {
		hasANDedResetPtr = (UInt32*)tmpData->getBytesNoCopy ();
		if ( NULL != hasANDedResetPtr ) {
			hasANDedResetPropValue = *hasANDedResetPtr;
		}
	}
Exit:
	debugIOLog (3,  "- AppleTexas2Audio::HasANDedReset returns %d", (unsigned int)hasANDedResetPropValue );
	return (Boolean)hasANDedResetPropValue;
}		//	[2855519]	} end

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Boolean AppleTexas2Audio::HasInput (void) {
	IORegistryEntry			*sound;
	OSData					*tmpData;
	UInt32					*numInputs = NULL;
	Boolean					hasInput;

	debugIOLog (3,  "+ AppleTexas2Audio::HasInput" );
	hasInput = false;

	sound = ourProvider->childFromPath ( kSoundEntry, gIODTPlane );
	FailIf (!sound, Exit);

	tmpData = OSDynamicCast (OSData, sound->getProperty (kNumInputs));
	FailIf (!tmpData, Exit);
	numInputs = (UInt32*)tmpData->getBytesNoCopy ();
	if (*numInputs > 1) {
		hasInput = true;
	}
Exit:
	debugIOLog (3,  "- AppleTexas2Audio::HasInput returns %d, numInputs %ld", hasInput, NULL == numInputs ? 0 : *numInputs );
	return hasInput;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Check if the detect states indicate that an output port change is
//	required.  If so, select the output while flushing the biquad
//	filters and DRC.  Then load and apply biquad filter coefficients
//	and DRC coefficients for the output that was just selected.
void AppleTexas2Audio::SelectOutputAndLoadEQ ( void ) {
	IOReturn			err;
	EQPrefsElementPtr	eqPrefs;
	UInt8				volumeData[kTexas2VOLwidth];
	UInt8				oldVolume[kTexas2VOLwidth];
	
	debugIOLog (3,  "+ SelectOutputAndLoadEQ" );

	for ( UInt32 index = 0; index < kTexas2VOLwidth; index++ ) {
		volumeData[index] = 0;					//	[2965804] prepare to mute volume
	}
	
	err = Texas2_ReadRegister ( kTexas2VolumeCtrlReg, oldVolume );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = Texas2_WriteRegister ( kTexas2VolumeCtrlReg, volumeData );								//	[2965804] mute volume without overwriting cache
	FailIf ( kIOReturnSuccess != err, Exit );
	
	IOSleep ( kMAX_VOLUME_RAMP_DELAY );														//	[2965804] delay to allow mute to occur (time = 2400 X fs)
	
	err = SetActiveOutput ( kSndHWOutputNone, kTouchBiquad);	//	[2965804]
	FailIf ( kIOReturnSuccess != err, Exit );

	if ( !disableLoadingEQFromFile ) {
		err = GetCustomEQCoefficients (layoutID, GetDeviceMatch (), speakerID, &eqPrefs);
	} else {
		err = kIOReturnSuccess;
	}
	
	if ( kIOReturnSuccess == err && NULL != eqPrefs ) {
		//	[2965804]	Changed order to load EQ prior to DRC.  Added muting of stream.
		err = SndHWSetOutputBiquadGroup (eqPrefs->filterCount, eqPrefs->filter[0].coefficient);
		FailIf ( kIOReturnSuccess != err, Exit );

		DRCInfo				localDRC;

		//	Set the dynamic range compressor coefficients.
		localDRC.compressionRatioNumerator		= eqPrefs->drcCompressionRatioNumerator;
		localDRC.compressionRatioDenominator	= eqPrefs->drcCompressionRatioDenominator;
		localDRC.threshold						= eqPrefs->drcThreshold;
		localDRC.maximumVolume					= eqPrefs->drcMaximumVolume;
		localDRC.enable							= (Boolean)((UInt32)(eqPrefs->drcEnable));

		err = SndHWSetDRC ((DRCInfoPtr)&localDRC);
		FailIf ( kIOReturnSuccess != err, Exit );
	}
	err = SetActiveOutput ( ParseDetectCollection(), kTouchBiquad);				//	[2965804] select target amplifier after EQ loading
	FailIf ( kIOReturnSuccess != err, Exit );

	err = Texas2_ReadRegister( kTexas2VolumeCtrlReg, volumeData );				//	[2965804] get cached volume setting
	FailIf ( kIOReturnSuccess != err, Exit );

	err = Texas2_WriteRegister( kTexas2VolumeCtrlReg, oldVolume );				//	[2965804] and restore volume setting
	FailIf ( kIOReturnSuccess != err, Exit );

Exit:
	debugIOLog (3,  "- SelectOutputAndLoadEQ" );
}
	
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[2855519]	All manipulations of the amplifier mutes must avoid a 
//	"break before make" behavior on CPUs that implement the Codec RESET
//	by ANDing the active mute state of the headphone and speaker amplifier.
//	All output port selections must unmute the new target amplifier before
//	muting the amplifier the old target amplifier to avoid applying a 
//	Codec RESET.
IOReturn AppleTexas2Audio::SetActiveOutput (UInt32 output, Boolean touchBiquad) {
	IOReturn			err;
	
	debugIOLog (3, "+ SetActiveOutput (output = %ld, touchBiquad = %d)", output, touchBiquad);

	err = kIOReturnSuccess;
	//	[2855519] begin {
		switch (output) {
			case kSndHWOutputNone:
				SetMixerState ( kMixMute );
				if (touchBiquad) {
					SetUnityGainAllPass ();
				}
				break;
			case kSndHWOutput1:
				err = SelectHeadphoneAmplifier();
				break;
			case kSndHWOutput2:																//	fall through to kSndHWOutput3
			case kSndHWOutput3:
				err = SelectSpeakerAmplifier();
				break;
			case kSndHWOutput4:
				if ( NULL != masterMuteGpio ) {
					SelectMasterMuteAmplifier();
				} else if ( NULL != lineOutMuteGpio ) {
					SelectLineOutAmplifier();
				} else {
					SelectSpeakerAmplifier();
				}
				break;
		}
		if ( kIOReturnSuccess == err ) {
			activeOutput = output;
		}
	//	[2855519]	} end

	debugIOLog (3, "- SndHWSetActiveOutput err %d", err);
	return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[2855519]	Leave the speaker amplifier unmuted until the headphone
//	amplifier has been unmuted to avoid applying a Codec RESET.
IOReturn AppleTexas2Audio::SelectHeadphoneAmplifier( void )
{
    IOReturn	err;
    
    err = SetMixerState ( kMix0dB );
    if ( kIOReturnSuccess == err ) {
        err = SetAnalogPowerDownMode ( kPowerNormalAnalog );
        if ( kIOReturnSuccess == err ) {
			SetAmplifierMuteState (kHEADPHONE_AMP, NEGATE_GPIO (hdpnActiveState));		//	unmute
            if ( hasANDedReset ) {
                IOSleep ( kCodecResetMakeBreakDuration );
            }
			SetAmplifierMuteState (kSPEAKER_AMP, ASSERT_GPIO (ampActiveState));			//	mute
			if ( masterMuteGpio ) {															//	[2933090] always unmute master
				SetAmplifierMuteState (kMASTER_AMP, NEGATE_GPIO (masterMuteActiveState));	//	unmute
			} else {
			SetAmplifierMuteState (kLINEOUT_AMP, ASSERT_GPIO (lineOutMuteActiveState));	//	mute
			}
			SetAmplifierMuteState (kLINEOUT_AMP, NEGATE_GPIO (lineOutMuteActiveState));	//	unmute
			IOSleep (kAmpRecoveryMuteDuration);
        }
    }
	if ( kIOReturnSuccess != err ) {
		debugIOLog (3, "AppleTexas2Audio::SelectHeadphoneAmplifier err %d", err);
	}
    return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[2855519]	The line output amplifier does not exist on any configuration
//	that implements a Codec RESET with an AND of the headphone and speaker
//	amplifier mute signals in the active state.  
IOReturn AppleTexas2Audio::SelectLineOutAmplifier( void )
{
    IOReturn	err;
    
    err = SetMixerState ( kMix0dB );
    if ( kIOReturnSuccess == err ) {
        err = SetAnalogPowerDownMode ( kPowerNormalAnalog );
        if ( kIOReturnSuccess == err ) {
			SetAmplifierMuteState (kSPEAKER_AMP, ASSERT_GPIO (ampActiveState));			//	mute
			SetAmplifierMuteState (kHEADPHONE_AMP, ASSERT_GPIO (ampActiveState));		//	mute
			SetAmplifierMuteState (kLINEOUT_AMP, NEGATE_GPIO (lineOutMuteActiveState));	//	unmute
			IOSleep (kAmpRecoveryMuteDuration);
        }
    }
	if ( kIOReturnSuccess != err ) {
		debugIOLog (3, "AppleTexas2Audio::SelectLineOutAmplifier err %d", err);
	}
    return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexas2Audio::SelectSpeakerAmplifier( void )
{
    IOReturn	err;
    
    err = SetMixerState ( kMix0dB );
    if ( kIOReturnSuccess == err ) {
        err = SetAnalogPowerDownMode ( kPowerNormalAnalog );
        if ( kIOReturnSuccess == err ) {
			SetAmplifierMuteState (kSPEAKER_AMP, NEGATE_GPIO (ampActiveState));			//	unmute
			if ( masterMuteGpio ) {															//	[2933090] always unmute master
				SetAmplifierMuteState (kMASTER_AMP, NEGATE_GPIO (masterMuteActiveState));	//	unmute
			} else if ( hasANDedReset ) {
                IOSleep ( kCodecResetMakeBreakDuration );
            }
			SetAmplifierMuteState (kHEADPHONE_AMP, ASSERT_GPIO (hdpnActiveState));		//	mute
			SetAmplifierMuteState (kLINEOUT_AMP, NEGATE_GPIO (lineOutMuteActiveState));	//	unmute
			IOSleep (kAmpRecoveryMuteDuration);
        }
    }
	if ( kIOReturnSuccess != err ) {
		debugIOLog (3, "AppleTexas2Audio::SelectSpeakerAmplifier err %d", err);
	}
    return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[2933090]
IOReturn AppleTexas2Audio::SelectMasterMuteAmplifier( void )
{
    IOReturn	err;
    
    err = SetMixerState ( kMix0dB );
    if ( kIOReturnSuccess == err ) {
        err = SetAnalogPowerDownMode ( kPowerNormalAnalog );
        if ( kIOReturnSuccess == err ) {
			SetAmplifierMuteState (kSPEAKER_AMP, ASSERT_GPIO (ampActiveState));			//	mute
			SetAmplifierMuteState (kMASTER_AMP, NEGATE_GPIO (masterMuteActiveState));	//	unmute
			if ( !IsHeadphoneConnected() ) {
				SetAmplifierMuteState (kHEADPHONE_AMP, ASSERT_GPIO (hdpnActiveState));	//	mute
			} else {
				SetAmplifierMuteState (kHEADPHONE_AMP, NEGATE_GPIO (hdpnActiveState));	//	mute
			}
			IOSleep (kAmpRecoveryMuteDuration);
        }
    }
	if ( kIOReturnSuccess != err ) {
		debugIOLog (3, "AppleTexas2Audio::SelectMasterMuteAmplifier err %d", err);
	}
    return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[2855519]	Leave the headphone amplifier unmuted until the speaker
//	amplifier has been unmuted to avoid applying a Codec RESET.
IOReturn AppleTexas2Audio::SetMixerState ( UInt32 mixerState )
{
    IOReturn	err;
    UInt8		mixerData[kTexas2MIXERGAINwidth];
    
    err = Texas2_ReadRegister( kTexas2MixerLeftGainReg, mixerData );
	FailIf ( kIOReturnSuccess != err, Exit );

	switch ( mixerState ) {
		case kMix0dB:			mixerData[0] = 0x10;		break;
		case kMixMute:			mixerData[0] = 0x00;		break;
	}
	
	err = Texas2_WriteRegister ( kTexas2MixerLeftGainReg, mixerData );
	FailIf ( kIOReturnSuccess != err, Exit );

	err = Texas2_WriteRegister ( kTexas2MixerRightGainReg, mixerData );
	FailIf ( kIOReturnSuccess != err, Exit );
Exit:
    return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTexas2Audio::SetBiquadInfoToUnityAllPass (void) {
	UInt32			index;
	
	for (index = 0; index < kNumberOfBiquadCoefficients; index++) {
		biquadGroupInfo[index++] = 1.0;				//	b0
		biquadGroupInfo[index++] = 0.0;				//	b1
		biquadGroupInfo[index++] = 0.0;				//	b2
		biquadGroupInfo[index++] = 0.0;				//	a1
		biquadGroupInfo[index] = 0.0;				//	a2
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This routine will only restore unity gain all pass coefficients to the
//	biquad registers.  All other coefficients to be passed through exported
//	functions via the sound hardware plug-in manager libaray.
void AppleTexas2Audio::SetUnityGainAllPass (void) {
	int				biquadRefnum;
	DRCInfo			localDRC;
	int				numBiquads;
	UInt8			mcr2Data[kTexas2MC2Rwidth];
	IOReturn		err;

	debugIOLog (3,  "+ SetUnityGainAllPass" );
	
	//	Set fast load mode to pause the DSP so that as the filter
	//	coefficients are applied, the filter will not become unstable
	//	and result in output instability.
	mcr2Data[0] = 0;
	err = Texas2_ReadRegister( kTexas2MainCtrl2Reg, mcr2Data );
	FailIf ( kIOReturnSuccess != err, Exit );

	mcr2Data[0] &= ~( kFilter_MASK << kAP );
	mcr2Data[0] |= ( kAllPassFilter << kAP );
	err= Texas2_WriteRegister( kTexas2MainCtrl2Reg, mcr2Data );
	FailIf ( kIOReturnSuccess != err, Exit );

	numBiquads = kNumberOfTexas2BiquadsPerChannel;
	
	//	Set the biquad coefficients in the shadow registers to 'unity all pass' so that
	//	any future attempt to set the biquads is applied to the hardware registers (i.e.
	//	make sure that the shadow register accurately reflects the current state so that
	//	 a data compare in the future does not cause a write operation to be bypassed).
	for (biquadRefnum = 0; biquadRefnum < numBiquads; biquadRefnum++) {
		err = Texas2_WriteRegister (kTexas2LeftBiquad0CtrlReg  + biquadRefnum, (UInt8*)kBiquad0db );
		FailIf ( kIOReturnSuccess != err, Exit );
		err = Texas2_WriteRegister (kTexas2RightBiquad0CtrlReg + biquadRefnum, (UInt8*)kBiquad0db );
		FailIf ( kIOReturnSuccess != err, Exit );
	}
	SetBiquadInfoToUnityAllPass ();	//	update stored coefficients but don't touch hardware
	
	//	Need to restore volume & mixer control registers after going to fast load mode
	
	localDRC.compressionRatioNumerator		= kDrcRatioNumerator;
	localDRC.compressionRatioDenominator	= kDrcRationDenominator;
	localDRC.threshold						= kDrcUnityThresholdHW;
	localDRC.maximumVolume					= kDefaultMaximumVolume;
	localDRC.enable							= false;

	err = SndHWSetDRC (&localDRC);
	FailIf ( kIOReturnSuccess != err, Exit );
Exit:	
	debugIOLog (3,  "- SetUnityGainAllPass" );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	When disabling Dynamic Range Compression, don't check the other elements
//	of the DRCInfo structure.  When enabling DRC, clip the compression threshold
//	to a valid range for the target hardware & validate that the compression
//	ratio is supported by the target hardware.	The maximumVolume argument
//	will dynamically apply the zero index reference point into the volume
//	gain translation table and will force an update of the volume registers.
IOReturn AppleTexas2Audio::SndHWSetDRC( DRCInfoPtr theDRCSettings ) {
	IOReturn		err;
	UInt8			regData[kTexas2DRCwidth];
	Boolean			enableUpdated;

	debugIOLog (3,  "+ SndHWSetDRC" );
	err = kIOReturnSuccess;
	FailWithAction( NULL == theDRCSettings, err = -50, Exit );
	FailWithAction( kDrcRatioNumerator != theDRCSettings->compressionRatioNumerator, err = -50, Exit );
	FailWithAction( kDrcRationDenominator != theDRCSettings->compressionRatioDenominator, err = -50, Exit );
	
	enableUpdated = drc.enable != theDRCSettings->enable ? true : false ;
	drc.enable = theDRCSettings->enable;

	//	The Texas2 DRC threshold has a range of 0.0 dB through -89.625 dB.  The lowest value
	//	is rounded down to -90.0 dB so that a generalized formula for calculating the hardware
	//	value can be used.  The hardware values decrement two counts for each 0.75 dB of
	//	threshold change toward greater attenuation (i.e. more negative) where a 0.0 dB setting
	//	translates to a hardware setting of #-17 (i.e. kDRCUnityThreshold).  Since the threshold
	//	is passed in as a dB X 1000 value, the threshold is divided by the step size X 1000 or
	//	750, then multiplied by the hardware decrement value of 2 and the total is subtracted
	//	from the unity threshold hardware setting.  Note that the -90.0 dB setting actually
	//	would result in a hardware setting of -89.625 dB as the hardware settings become
	//	non-linear at the very lowest value.
	
	if (layoutID == layoutQ16) {
		regData[DRC_Threshold]		= (UInt8)(kDRCUnityThreshold + (kDRC_CountsPerStep * (theDRCSettings->threshold / kDRC_ThreholdStepSize)));
		regData[DRC_AboveThreshold]	= theDRCSettings->enable ? 0x30 : kDisableDRC;		// 1.6:1
		regData[DRC_BelowThreshold]	= 0x32;		// 1.6:1
		regData[DRC_Integration]	= 0x60;		// 6.7 ms
		regData[DRC_Attack]			= 0x90;		// 53 ms
		regData[DRC_Decay]			= 0xB0;		// 212 ms
	} else if (layoutID == layoutP72D) {
		regData[DRC_Threshold]		= (UInt8)(kDRCUnityThreshold + (kDRC_CountsPerStep * (theDRCSettings->threshold / kDRC_ThreholdStepSize)));
		regData[DRC_AboveThreshold]	= theDRCSettings->enable ? 0x58 : kDisableDRC;		// 3.2:1
		regData[DRC_BelowThreshold]	= 0x22;		// 1.33:1
		regData[DRC_Integration]	= 0x60;		// 6.7 ms
		regData[DRC_Attack]			= 0x90;		// 53 ms
		regData[DRC_Decay]			= 0xB0;		// 212 ms
	} else {
		regData[DRC_Threshold]		= (UInt8)(kDRCUnityThreshold + (kDRC_CountsPerStep * (theDRCSettings->threshold / kDRC_ThreholdStepSize)));
		regData[DRC_AboveThreshold]	= theDRCSettings->enable ? kDRCAboveThreshold3to1 : kDisableDRC ;
		regData[DRC_BelowThreshold]	= kDRCBelowThreshold1to1;
		regData[DRC_Integration]	= kDRCIntegrationThreshold;
		regData[DRC_Attack]			= kDRCAttachThreshold;
		regData[DRC_Decay]			= kDRCDecayThreshold;
	}
	err = Texas2_WriteRegister( kTexas2DynamicRangeCtrlReg, regData );
	FailIf ( kIOReturnSuccess != err, Exit );

	//	The current volume setting needs to be scaled against the new range of volume 
	//	control and applied to the hardware.
	if( drc.maximumVolume != theDRCSettings->maximumVolume || enableUpdated ) {
		drc.maximumVolume = theDRCSettings->maximumVolume;
	}
	
	drc.compressionRatioNumerator		= theDRCSettings->compressionRatioNumerator;
	drc.compressionRatioDenominator		= theDRCSettings->compressionRatioDenominator;
	drc.threshold						= theDRCSettings->threshold;
	drc.maximumVolume					= theDRCSettings->maximumVolume;

Exit:
	debugIOLog (3,  "- SndHWSetDRC err = %d", err );
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	NOTE:	This function does not utilize fast mode loading as to do so would
//	revert all biquad coefficients not addressed by this execution instance to
//	unity all pass.	 Expect DSP processing delay if this function is used.	It
//	is recommended that SndHWSetOutputBiquadGroup be used instead.  THIS FUNCTION
//	WILL NOT ENABLE THE FILTERS.  DO NOT EXPORT THIS INTERFACE!!!
IOReturn AppleTexas2Audio::SndHWSetOutputBiquad( UInt32 streamID, UInt32 biquadRefNum, FourDotTwenty *biquadCoefficients )
{
	IOReturn		err;
	UInt32			coefficientIndex;
	UInt32			Texas2BiquadIndex;
	UInt32			biquadGroupIndex;
	UInt8			Texas2Biquad[kTexas2CoefficientsPerBiquad * kTexas2NumBiquads];
	
	err = kIOReturnSuccess;
	FailWithAction( kTexas2MaxBiquadRefNum < biquadRefNum || NULL == biquadCoefficients, err = -50, Exit );
	FailWithAction( kStreamStereo != streamID && kStreamFrontLeft != streamID && kStreamFrontRight != streamID, err = -50, Exit );
	
	Texas2BiquadIndex = 0;
	biquadGroupIndex = biquadRefNum * kTexas2CoefficientsPerBiquad;
	if( kStreamFrontRight == streamID )
		biquadGroupIndex += kNumberOfTexas2BiquadCoefficientsPerChannel;

	for( coefficientIndex = 0; coefficientIndex < kTexas2CoefficientsPerBiquad; coefficientIndex++ )
	{
		Texas2Biquad[Texas2BiquadIndex++] = biquadCoefficients[coefficientIndex].integerAndFraction1;
		Texas2Biquad[Texas2BiquadIndex++] = biquadCoefficients[coefficientIndex].fraction2;
		Texas2Biquad[Texas2BiquadIndex++] = biquadCoefficients[coefficientIndex].fraction3;
	}
	
	err = SetOutputBiquadCoefficients( streamID, biquadRefNum, Texas2Biquad );

Exit:
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexas2Audio::SndHWSetOutputBiquadGroup( UInt32 biquadFilterCount, FourDotTwenty *biquadCoefficients )
{
	UInt32			index;
	IOReturn		err;
	UInt8			mcr2Data[kTexas2MC2Rwidth];
	

	FailWithAction( 0 == biquadFilterCount || NULL == biquadCoefficients, err = -50, Exit );
	err = kIOReturnSuccess;
	
	err = Texas2_ReadRegister( kTexas2MainCtrl2Reg, mcr2Data );			//	bypass the filter while loading coefficients
	FailIf ( kIOReturnSuccess != err, Exit );

	mcr2Data[0] &= ~( kFilter_MASK << kAP );
	mcr2Data[0] |= ( kAllPassFilter << kAP );

	err = Texas2_WriteRegister( kTexas2MainCtrl2Reg, mcr2Data );
	FailIf ( kIOReturnSuccess != err, Exit );

	err = InitEQSerialMode( kSetFastLoadMode, kDontRestoreOnNormal );		//	pause the DSP while loading coefficients
	FailIf ( kIOReturnSuccess != err, Exit );
	
	index = 0;
	do {
		if( index >= ( biquadFilterCount / 2 ) ) {
			err = SndHWSetOutputBiquad( kStreamFrontRight, index - ( biquadFilterCount / 2 ), biquadCoefficients );
			FailIf ( kIOReturnSuccess != err, Exit );
		} else {
			err = SndHWSetOutputBiquad( kStreamFrontLeft, index, biquadCoefficients );
			FailIf ( kIOReturnSuccess != err, Exit );
		}
		index++;
		biquadCoefficients += kNumberOfCoefficientsPerBiquad;
	} while ( ( index < biquadFilterCount ) && ( kIOReturnSuccess == err ) );
	
	err = InitEQSerialMode( kSetNormalLoadMode, kRestoreOnNormal );		//	enable the DSP
	FailIf ( kIOReturnSuccess != err, Exit );

	err = Texas2_ReadRegister( kTexas2MainCtrl2Reg, mcr2Data );			//	enable the filters
	FailIf ( kIOReturnSuccess != err, Exit );

	mcr2Data[0] &= ~( kFilter_MASK << kAP );
	mcr2Data[0] |= ( kNormalFilter << kAP );
	err = Texas2_WriteRegister( kTexas2MainCtrl2Reg, mcr2Data );
	FailIf ( kIOReturnSuccess != err, Exit );

Exit:
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexas2Audio::SetOutputBiquadCoefficients( UInt32 streamID, UInt32 biquadRefNum, UInt8 *biquadCoefficients )
{
	IOReturn			err;
	
	err = kIOReturnSuccess;
	FailWithAction ( kTexas2MaxBiquadRefNum < biquadRefNum || NULL == biquadCoefficients, err = -50, Exit );
	FailWithAction ( kStreamStereo != streamID && kStreamFrontLeft != streamID && kStreamFrontRight != streamID, err = -50, Exit );

	switch ( biquadRefNum ) {
		case kBiquadRefNum_0:
			switch( streamID ) {
				case kStreamFrontLeft:	err = Texas2_WriteRegister( kTexas2LeftBiquad0CtrlReg, biquadCoefficients );	break;
				case kStreamFrontRight:	err = Texas2_WriteRegister( kTexas2RightBiquad0CtrlReg, biquadCoefficients );	break;
				case kStreamStereo:		err = Texas2_WriteRegister( kTexas2LeftBiquad0CtrlReg, biquadCoefficients );
										FailIf ( kIOReturnSuccess != err, Exit );
										err = Texas2_WriteRegister( kTexas2RightBiquad0CtrlReg, biquadCoefficients );	break;
			}
			FailIf ( kIOReturnSuccess != err, Exit );
			break;
		case kBiquadRefNum_1:
			switch( streamID ) {
				case kStreamFrontLeft:	err = Texas2_WriteRegister( kTexas2LeftBiquad1CtrlReg, biquadCoefficients );	break;
				case kStreamFrontRight:	err = Texas2_WriteRegister( kTexas2RightBiquad1CtrlReg, biquadCoefficients );	break;
				case kStreamStereo:		err = Texas2_WriteRegister( kTexas2LeftBiquad1CtrlReg, biquadCoefficients );
										FailIf ( kIOReturnSuccess != err, Exit );
										err = Texas2_WriteRegister( kTexas2RightBiquad1CtrlReg, biquadCoefficients );	break;
			}
			FailIf ( kIOReturnSuccess != err, Exit );
			break;
		case kBiquadRefNum_2:
			switch( streamID ) {
				case kStreamFrontLeft:	err = Texas2_WriteRegister( kTexas2LeftBiquad2CtrlReg, biquadCoefficients );	break;
				case kStreamFrontRight:	err = Texas2_WriteRegister( kTexas2RightBiquad2CtrlReg, biquadCoefficients );	break;
				case kStreamStereo:		err = Texas2_WriteRegister( kTexas2LeftBiquad2CtrlReg, biquadCoefficients );
										FailIf ( kIOReturnSuccess != err, Exit );
										err = Texas2_WriteRegister( kTexas2RightBiquad2CtrlReg, biquadCoefficients );	break;
			}
			FailIf ( kIOReturnSuccess != err, Exit );
			break;
		case kBiquadRefNum_3:
			switch( streamID ) {
				case kStreamFrontLeft:	err = Texas2_WriteRegister( kTexas2LeftBiquad3CtrlReg, biquadCoefficients );	break;
				case kStreamFrontRight:	err = Texas2_WriteRegister( kTexas2RightBiquad3CtrlReg, biquadCoefficients );	break;
				case kStreamStereo:		err = Texas2_WriteRegister( kTexas2LeftBiquad3CtrlReg, biquadCoefficients );
										FailIf ( kIOReturnSuccess != err, Exit );
										err = Texas2_WriteRegister( kTexas2RightBiquad3CtrlReg, biquadCoefficients );	break;
			}
			FailIf ( kIOReturnSuccess != err, Exit );
			break;
		case kBiquadRefNum_4:
			switch( streamID ) {
				case kStreamFrontLeft:	err = Texas2_WriteRegister( kTexas2LeftBiquad4CtrlReg, biquadCoefficients );	break;
				case kStreamFrontRight:	err = Texas2_WriteRegister( kTexas2RightBiquad4CtrlReg, biquadCoefficients );	break;
				case kStreamStereo:		err = Texas2_WriteRegister( kTexas2LeftBiquad4CtrlReg, biquadCoefficients );
										FailIf ( kIOReturnSuccess != err, Exit );
										err = Texas2_WriteRegister( kTexas2RightBiquad4CtrlReg, biquadCoefficients );	break;
			}
			FailIf ( kIOReturnSuccess != err, Exit );
			break;
		case kBiquadRefNum_5:
			switch( streamID ) {
				case kStreamFrontLeft:	err = Texas2_WriteRegister( kTexas2LeftBiquad5CtrlReg, biquadCoefficients );	break;
				case kStreamFrontRight:	err = Texas2_WriteRegister( kTexas2RightBiquad5CtrlReg, biquadCoefficients );	break;
				case kStreamStereo:		err = Texas2_WriteRegister( kTexas2LeftBiquad5CtrlReg, biquadCoefficients );
										FailIf ( kIOReturnSuccess != err, Exit );
										err = Texas2_WriteRegister( kTexas2RightBiquad5CtrlReg, biquadCoefficients );	break;
			}
			FailIf ( kIOReturnSuccess != err, Exit );
			break;
		case kBiquadRefNum_6:
			switch( streamID ) {
				case kStreamFrontLeft:	err = Texas2_WriteRegister( kTexas2LeftBiquad6CtrlReg, biquadCoefficients );	break;
				case kStreamFrontRight:	err = Texas2_WriteRegister( kTexas2RightBiquad6CtrlReg, biquadCoefficients );	break;
				case kStreamStereo:		err = Texas2_WriteRegister( kTexas2LeftBiquad6CtrlReg, biquadCoefficients );
										FailIf ( kIOReturnSuccess != err, Exit );
										err = Texas2_WriteRegister( kTexas2RightBiquad6CtrlReg, biquadCoefficients );	break;
			}
			FailIf ( kIOReturnSuccess != err, Exit );
			break;
	}

Exit:
	return err;
}

#pragma mark +I2C FUNCTIONS
// Taken from PPCDACA.cpp

// --------------------------------------------------------------------------
// Method: getI2CPort
//
// Purpose:
//		  returns the i2c port to use for the audio chip.
UInt32 AppleTexas2Audio::getI2CPort()
{
	if(ourProvider) {
		OSData *t;
		
		t = OSDynamicCast(OSData, ourProvider->getProperty("AAPL,i2c-port-select"));	// we don't need a port select on Tangent, but look anyway
		if (t != NULL) {
			UInt32 myPort = *((UInt32*)t->getBytesNoCopy());
			return myPort;
		}
		// else
		//	debugIOLog (3,  "AppleTexas2Audio::getI2CPort missing property port, but that's not necessarily a problem");
	}

	return 0;
}

// --------------------------------------------------------------------------
// Method: openI2C
//
// Purpose:
//		  opens and sets up the i2c bus
bool AppleTexas2Audio::openI2C()
{
	FailIf (NULL == interface, Exit);

	// Open the interface and sets it in the wanted mode:
	FailIf (!interface->openI2CBus (getI2CPort()), Exit);
	interface->setStandardSubMode ();

	interface->setPollingMode (false);

	return true;

Exit:
	return false;
}


// --------------------------------------------------------------------------
// Method: closeI2C
//
// Purpose:
//		  closes the i2c bus
void AppleTexas2Audio::closeI2C ()
{
	// Closes the bus so other can access to it:
	interface->closeI2CBus ();
}

// --------------------------------------------------------------------------
// Method: findAndAttachI2C
//
// Purpose:
//	 Attaches to the i2c interface:
bool AppleTexas2Audio::findAndAttachI2C(IOService *provider)
{
	const OSSymbol	*i2cDriverName;
	IOService		*i2cCandidate;

	// Searches the i2c:
	i2cDriverName = OSSymbol::withCStringNoCopy("PPCI2CInterface.i2c-mac-io");
	i2cCandidate = waitForService(resourceMatching(i2cDriverName));
	// interface = OSDynamicCast(PPCI2CInterface, i2cCandidate->getProperty(i2cDriverName));
	interface = (PPCI2CInterface*)i2cCandidate->getProperty(i2cDriverName);

	if (interface == NULL) {
		debugIOLog (3, "AppleTexas2Audio::findAndAttachI2C can't find the i2c in the registry");
		return false;
	}

	// Make sure that we hold the interface:
	interface->retain();

	return true;
}

// --------------------------------------------------------------------------
// Method: detachFromI2C
//
// Purpose:
//	 detaches from the I2C
bool AppleTexas2Audio::detachFromI2C(IOService* /*provider*/)
{
	if (interface) {
		// delete interface;
		interface->release();
		interface = 0;
	}
		
	return (true);
}

#if 0	//	{
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline void AppleTexas2Audio::I2SSetSerialFormatReg(UInt32 value)
{
	OSWriteLittleInt32( ioBaseAddress, kI2S0BaseOffset + kI2SSerialFormatOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline UInt32 AppleTexas2Audio::I2SGetSerialFormatReg(void)
{
	return OSReadLittleInt32( ioBaseAddress, kI2S0BaseOffset + kI2SSerialFormatOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline void AppleTexas2Audio::I2SSetDataWordSizeReg(UInt32 value)
{
	WriteWordLittleEndian( ioBaseAddress, kI2S0BaseOffset + kI2SFrameMatchOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline UInt32 AppleTexas2Audio::I2SGetDataWordSizeReg(void)
{
	return OSReadLittleInt32( ioBaseAddress, kI2S0BaseOffset + kI2SFrameMatchOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline void AppleTexas2Audio::I2S1SetSerialFormatReg(UInt32 value)
{
	OSWriteLittleInt32( ioBaseAddress, kI2S1BaseOffset + kI2SSerialFormatOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline UInt32 AppleTexas2Audio::I2S1GetSerialFormatReg(void)
{
	return OSReadLittleInt32( ioBaseAddress, kI2S1BaseOffset + kI2SSerialFormatOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline void AppleTexas2Audio::I2S1SetDataWordSizeReg(UInt32 value)
{
	OSWriteLittleInt32(  ioBaseAddress, kI2S1BaseOffset + kI2SFrameMatchOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline UInt32 AppleTexas2Audio::I2S1GetDataWordSizeReg(void)
{
	return OSReadLittleInt32( ioBaseAddress, kI2S1BaseOffset + kI2SFrameMatchOffset);
}
#endif	//	{

#if 0	//	{
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline void AppleTexas2Audio::Fcr1SetReg(UInt32 value)
{
	OSWriteLittleInt32((UInt8*)ioBaseAddress, kFCR1Offset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline UInt32 AppleTexas2Audio::Fcr1GetReg(void)
{
	return OSReadLittleInt32(ioBaseAddress, kFCR1Offset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline void AppleTexas2Audio::Fcr3SetReg(UInt32 value)
{
	OSWriteLittleInt32(ioBaseAddress, kFCR3Offset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline UInt32 AppleTexas2Audio::Fcr3GetReg(void)
{
	return OSReadLittleInt32(ioBaseAddress, kFCR3Offset);
}
#endif	//	{

#if 0	//	{
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Access to Keylargo registers:
inline void AppleTexas2Audio::KLSetRegister(void *klRegister, UInt32 value)
{
	UInt32 *reg = (UInt32*)klRegister;
	*reg = value;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline UInt32 AppleTexas2Audio::KLGetRegister(void *klRegister)
{
	UInt32 *reg = (UInt32*)klRegister;
	return (*reg);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline UInt32 AppleTexas2Audio::I2SGetIntCtlReg()
{
	return OSReadLittleInt32(ioBaseAddress, kI2S0BaseOffset + kI2SIntCtlOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline UInt32 AppleTexas2Audio::I2S1GetIntCtlReg()
{
	return OSReadLittleInt32(ioBaseAddress, kI2S1BaseOffset + kI2SIntCtlOffset);
}

// --------------------------------------------------------------------------
// Method: setSampleRate
//
// Purpose:
//		  Sets the sample rate on the I2S bus
bool AppleTexas2Audio::setSampleParameters(UInt32 sampleRate, UInt32 mclkToFsRatio)
{
	UInt32	mclkRatio;
	UInt32	reqMClkRate;

	mclkRatio = mclkToFsRatio;			// remember the MClk ratio required

	if ( mclkRatio == 0 )																				   // or make one up if MClk not required
		mclkRatio = 64;				// use 64 x ratio since that will give us the best characteristics
	
	reqMClkRate = sampleRate * mclkRatio;	// this is the required MClk rate

	// look for a source clock that divides down evenly into the MClk
	if ((kClock18MHz % reqMClkRate) == 0) {
		// preferential source is 18 MHz
		clockSource = kClock18MHz;
	}
	else if ((kClock45MHz % reqMClkRate) == 0) {
		// next check 45 MHz clock
		clockSource = kClock45MHz;
	}
	else if ((kClock49MHz % reqMClkRate) == 0) {
		// last, try 49 Mhz clock
		clockSource = kClock49MHz;
	}
	else {
		debugIOLog (3, "AppleTexas2Audio::setSampleParameters Unable to find a suitable source clock (no globals changes take effect)");
		return false;
	}

	// get the MClk divisor
	debugIOLog (3, "AppleTexas2Audio:setSampleParameters %ld / %ld =", (UInt32)clockSource, (UInt32)reqMClkRate); 
	mclkDivisor = clockSource / reqMClkRate;
	debugIOLog (3, "%ld", mclkDivisor);
	switch (serialFormat)					// SClk depends on format
	{
		case kSndIOFormatI2SSony:
		case kSndIOFormatI2S64x:
			sclkDivisor = mclkRatio / k64TicksPerFrame; // SClk divisor is MClk ratio/64
			break;
		case kSndIOFormatI2S32x:
			sclkDivisor = mclkRatio / k32TicksPerFrame; // SClk divisor is MClk ratio/32
			break;
		default:
			debugIOLog (3, "AppleTexas2Audio::setSampleParameters Invalid serial format");
			return false;
			break;
	}

	return true;
 }


// --------------------------------------------------------------------------
// Method: setSerialFormatRegister
//
// Purpose:
//		  Set global values to the serial format register
void AppleTexas2Audio::setSerialFormatRegister(ClockSource clockSource, UInt32 mclkDivisor, UInt32 sclkDivisor, SoundFormat serialFormat)
{
	UInt32	regValue = 0;

	debugIOLog (3, "AppleTexas2Audio::SetSerialFormatRegister(%d,%d,%d,%d)",(int)clockSource, (int)mclkDivisor, (int)sclkDivisor, (int)serialFormat);

	switch ((int)clockSource)
	{
		case kClock18MHz:			regValue = kClockSource18MHz;			break;
		case kClock45MHz:			regValue = kClockSource45MHz;			break;
		case kClock49MHz:			regValue = kClockSource49MHz;			break;
		default:
			debugIOLog (3, "AppleTexas2Audio::SetSerialFormatRegister(%d,%d,%d,%d): Invalid clock source",(int)clockSource, (int)mclkDivisor, (int)sclkDivisor, (int)serialFormat);
			break;
	}

	switch (mclkDivisor)
	{
		case 1:			regValue |= kMClkDivisor1;																break;
		case 3:			regValue |= kMClkDivisor3;																break;
		case 5:			regValue |= kMClkDivisor5;																break;
		default:		regValue |= (((mclkDivisor / 2) - 1) << kMClkDivisorShift) & kMClkDivisorMask;			break;
	}

	switch ((int)sclkDivisor)
	{
		case 1:			regValue |= kSClkDivisor1;																break;
		case 3:			regValue |= kSClkDivisor3;																break;
		default:		regValue |= (((sclkDivisor / 2) - 1) << kSClkDivisorShift) & kSClkDivisorMask;			break;
	}
	regValue |= kSClkMaster;										// force master mode

	switch (serialFormat)
	{
		case kSndIOFormatI2SSony:			regValue |= kSerialFormatSony;			break;
		case kSndIOFormatI2S64x:			regValue |= kSerialFormat64x;			break;
		case kSndIOFormatI2S32x:			regValue |= kSerialFormat32x;			break;
		default:	
			debugIOLog (3, "AppleTexas2Audio::SetSerialFormatRegister(%d,%d,%d,%d): Invalid serial format",(int)clockSource, (int)mclkDivisor, (int)sclkDivisor, (int)serialFormat);
			break;
	}

	switch ( i2SInterfaceNumber ) {
		case kUseI2SCell0:	
			// Set up the data word size register for stereo for I2S0 (input and output)
			I2SSetDataWordSizeReg ((kI2sStereoChannels << kNumChannelsInShift)|(kI2sStereoChannels << kNumChannelsOutShift)|kDataIn16|kDataOut16);
			// Set up the serial format register for I2S0
			I2SSetSerialFormatReg(i2sSerialFormat);
			break;
		case kUseI2SCell1:	
			// Set up the data word size register for stereo for I2S1 (input and output)
			I2S1SetDataWordSizeReg ((kI2sStereoChannels << kNumChannelsInShift)|(kI2sStereoChannels << kNumChannelsOutShift)|kDataIn16|kDataOut16);
			// Set up the serial format register for I2S1
			I2S1SetSerialFormatReg(i2sSerialFormat);
			break;
	}
}
#endif	//	}

// --------------------------------------------------------------------------
// Method: frameRate
//
// Purpose:
//		  returns the frame rate as in the registry, if it is
//		  not found in the registry, it returns the default value.
#define kCommonFrameRate 44100

UInt32 AppleTexas2Audio::frameRate(UInt32 index)
{
	if(ourProvider) {
		OSData *t;

		t = OSDynamicCast(OSData, ourProvider->getProperty("sample-rates"));
		if (t != NULL) {
			UInt32 *fArray = (UInt32*)(t->getBytesNoCopy());

			if ((fArray != NULL) && (index < fArray[0])){
				// I could do >> 16, but in this way the code is portable and
				// however any decent compiler will recognize this as a shift
				// and do the right thing.
				UInt32 fR = fArray[index + 1] / (UInt32)65536;

				debugIOLog (3,  "AppleTexas2Audio::frameRate (%ld)",	fR);
				return fR;
			}
		}
	}

	return (UInt32)kCommonFrameRate;
}
