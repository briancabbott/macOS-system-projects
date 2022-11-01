/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 */
 
#include "IOPlatformMonitor.h"
#include "MacRISC2.h"
class MacRISC2PE;

enum {
	// Sensor states
	kPowerState0			= 0,						// Fast/high
	kPowerState1			= 1,						// Slow/low
	kMaxPowerStates			= kPowerState1 + 1,
    
	kThermalState0			= 0,						// Low
	kThermalState1			= 1,						// Overtemp
	kMaxThermalStates		= kThermalState1 + 1,
    
	kClamshellStateOpen		= 0,						// Open/normal/high
	kClamshellStateClosed	= 1,						// Closed/low
	kNumClamshellStates		= kClamshellStateClosed + 1,
	
    kNewMaxThermalStates	= 2,						// Either over or under, will do cleaner as time goes on
    
	// Controller states
	kCPUPowerState0			= 0,
	kCPUPowerState1			= 1,
	
	kGPUPowerState0			= 0,
	kGPUPowerState1			= 1,
	kGPUPowerState2			= 2,
	
	// sensors always numbered 0 through n
	kPowerSensor			= 0,						// 0
	kThermalSensor			= kPowerSensor + 1,			// 1
	kClamshellSensor		= kThermalSensor + 1,		// 2
	kMaxSensors				= kClamshellSensor + 1,		// 3
	kCPUController			= kMaxSensors,				// 3
	kGPUController			= kCPUController + 1,		// 4
	kSlewController			= kGPUController + 1,		// 5
	kMaxConSensors			= kSlewController + 1,		// 6
	
	// sensor-index(s) - assigned by Open Firmware, and unique system wide
	kMaxSensorIndex			= 6					// See subSensorArray

};

// IOPMon status keys and values
#define kIOPMonPowerStateKey 		"IOPMonPowerState"
#define kIOPMonThermalStateKey 		"IOPMonThermalState"
#define kIOPMonClamshellStateKey 	"IOPMonClamshellState"
#define kIOPMonCPUActionKey 		"IOPMonCPUAction"
#define kIOPMonGPUActionKey 		"IOPMonGPUAction"
#define kIOPMonSlewActionKey 		"target-value"

#define kIOPMonState0				"State0"
#define kIOPMonState1				"State1"
#define kIOPMonState2				"State2"
#define kIOPMonState3				"State3"

#define kIOClamshellStateOpen		"Open"
#define kIOClamshellStateClosed		"Closed"

#define kIOPMonFull					"FullSpeed"
#define kIOPMonReduced				"ReducedSpeed"
#define kIOPMonSlow					"SlowSpeed"

class Portable_PlatformMonitor : public IOPlatformMonitor
{
    OSDeclareDefaultStructors(Portable_PlatformMonitor)

private:
	OSDictionary		*dictPowerLow, *dictPowerHigh;
	OSDictionary		*dictClamshellOpen, *dictClamshellClosed;
    MacRISC2PE			*macRISC2PE;
    int					machineModel;
    bool				goingToSleep;
    
    UInt32				initialPowerState;
    UInt32				initialCPUPowerState;
    UInt32				wakingPowerState;
    
	bool				useBusSlewing;
    bool				machineUtilizes65W;
    bool				machineReducesOnNoBattery;
    UInt32				whichCPUSpeedController;

protected:

    static IOReturn iopmonCommandGateCaller(OSObject *object, void *arg0, void *arg1, void *arg2, void *arg3);

	virtual IOReturn monitorPower (OSDictionary *dict, IOService *provider);
	virtual void updateIOPMonStateInfo (UInt32 type, UInt32 state);
    
    virtual void applyPowerSettings(UInt32	actionToTake);

    virtual void slewBusSpeed(UInt32 newLevel);
    virtual bool setCPUSpeed(bool setPowerHigh);
    virtual bool setGPUSpeed(UInt32 newGPUSpeed);  
    
    virtual void createThermalThresholdArray(LimitsInfo	*curThermalLimits);

    virtual void setSensorThermalThresholds(UInt32 sensorID, ThermalValue lowTemp, ThermalValue highTemp);
    virtual void resetThermalSensorThresholds(void);

	virtual bool handlePowerEvent (IOPMonEventData *eventData);
	virtual bool handleThermalEvent (IOPMonEventData *eventData);
	virtual bool handleClamshellEvent (IOPMonEventData *eventData);
    
	virtual UInt32 lookupThermalStateFromValue (UInt32 sensorIndex, ThermalValue value);
        
public:

	virtual bool start(IOService *provider);
	virtual IOReturn powerStateWillChangeTo (IOPMPowerFlags, unsigned long, IOService*);
	virtual IOReturn powerStateDidChangeTo (IOPMPowerFlags, unsigned long, IOService*);
	virtual IOReturn setAggressiveness(unsigned long selector, unsigned long newLevel);
	virtual IOReturn message( UInt32 type, IOService * provider, void * argument = 0 );

	virtual bool initPlatformState ();
	virtual void savePlatformState ();
	virtual void restorePlatformState ();
	virtual bool adjustPlatformState ();

	virtual IOReturn registerConSensor (OSDictionary *dict, IOService *conSensor);
	virtual bool unregisterSensor (UInt32 sensorID);
	virtual bool lookupConSensorInfo (OSDictionary *dict, IOService *conSensor, 
		UInt32 *type, UInt32 *index, UInt32 *subIndex);
};
