/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 */

#include "PB5_1_PlatformMonitor.h"

static const OSSymbol 			*gIOPMonPowerStateKey;
static const OSSymbol 			*gIOPMonThermalStateKey;
static const OSSymbol 			*gIOPMonClamshellStateKey;
static const OSSymbol 			*gIOPMonCPUActionKey;
static const OSSymbol 			*gIOPMonGPUActionKey;
	
static const OSSymbol			*gIOPMonState0;
static const OSSymbol			*gIOPMonState1;
static const OSSymbol			*gIOPMonState2;
static const OSSymbol			*gIOPMonState3;
static const OSSymbol			*gIOPMonFull;
static const OSSymbol			*gIOPMonReduced;
static const OSSymbol			*gIOPMonSlow;

static 	IOService			*provider;

// Possible platform actions
static bool actionFullPower ();
static bool actionPower1 ();
static bool actionPower1GPU1 ();
static bool actionPower1GPU2 ();

/*
 * The platformActionGrid, which is platform-dependent, is an n-dimension array, where n corresponds
 * to kMaxSensors.  The depth of each array dimension corresponds to the maxStates value for each sensor.
 * Each element in the array indicates the action to take (one of the above possible platform actions)
 * based on the current overall state of the system.
 *
 * Note that "sensor" can be intrepreted fairly loosely.  For example, user selection of "Highest" vs.
 * "Reduced" processor performance in the Energy Saver preferences is viewed as "sensory input" and
 * is handled in the table accordingly.
 */
static IOPlatformMonitorAction platformActionGrid[kMaxPowerStates][kMaxThermalStates][kNumClamshellStates] =
	{
		{
			{
				actionFullPower,		// kPowerState0 / kThermalState0 / kClamShellStateOpen
				actionPower1			// kPowerState0 / kThermalState0 / kClamShellStateClosed
			},
			{
				actionPower1,			// kPowerState0 / kThermalState1 / kClamShellStateOpen
				actionPower1			// kPowerState0 / kThermalState1 / kClamShellStateClosed
			},
			{
				actionPower1GPU1,		// kPowerState0 / kThermalState2 / kClamShellStateOpen
				actionPower1GPU1		// kPowerState0 / kThermalState2 / kClamShellStateClosed
			},
			{
				actionPower1GPU2,		// kPowerState0 / kThermalState3 / kClamShellStateOpen
				actionPower1GPU2		// kPowerState0 / kThermalState3 / kClamShellStateClosed
			},
		},
		{
			{
				actionPower1,			// kPowerState1 / kThermalState0 / kClamShellStateOpen
				actionPower1			// kPowerState1 / kThermalState0 / kClamShellStateClosed
			},
			{
				actionPower1,			// kPowerState1 / kThermalState1 / kClamShellStateOpen
				actionPower1			// kPowerState1 / kThermalState1 / kClamShellStateClosed
			},
			{
				actionPower1GPU1,		// kPowerState1 / kThermalState2 / kClamShellStateOpen
				actionPower1GPU1		// kPowerState1 / kThermalState2 / kClamShellStateClosed
			},
			{
				actionPower1GPU2,		// kPowerState1 / kThermalState3 / kClamShellStateOpen
				actionPower1GPU2		// kPowerState1 / kThermalState3 / kClamShellStateClosed
			},
		}
	};

/*
 * conSensorArray, like platformActionArray is platform-dependent.  One element for each primary
 * controller/sensor and each must register itself with us before we can use it.
 *
 * conSensorArray is initialized in the start routine.
 */
static ConSensorInfo conSensorArray[kMaxConSensors];

/*
 * The subSensorArray contains information about secondary sensors and maps those sensors into the primary 
 * sensor.  There are kMaxSensorIndex subSensors and each must map to a primary sensor.  On this platform, 
 * all individual sensors are thermal so all map to kThermalSensor, although that may not always be the case.
 * If more than one thermal zone is to be managed then a primary thermal sensor would be created for each 
 * zone and the subSensorArray can be used to map each sensor into a particular zone.
 *
 * subSensorArray is initialized in the start routine.
 */
static ConSensorInfo subSensorArray[kMaxSensorIndex];

/*
 * The thermalThresholdInfoArray is another n-dimensional array detailing threshold and state information
 * for each sensor.  For this platform, an additional factor is clamshell state, so that is one of the
 * dimensions here.
 */
static OldThresholdInfo	thermalThresholdInfoArray[kMaxSensorIndex][kNumClamshellStates][kMaxThermalStates] =
{
	{	// Sensor 0
		{	// Clamshell open
			//	thresholdLow,			nextStateLow,		thresholdHigh,			nextStateHigh		// currentState
			{	 TEMP_SENSOR_FMT(0),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState1 },	// kThermalState0 
			{	 TEMP_SENSOR_FMT(95),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState2 },	// kThermalState1 
			{	 TEMP_SENSOR_FMT(78),	kThermalState1, 	TEMP_SENSOR_FMT(93), 	kThermalState3 },	// kThermalState2 
			{	 TEMP_SENSOR_FMT(88),	kThermalState2, 	TEMP_SENSOR_FMT(117), 	kThermalState3 },	// kThermalState3 
		},
		{	// Clamshell closed
			//	thresholdLow,			nextStateLow,		thresholdHigh,			nextStateHigh		// currentState
			{	 TEMP_SENSOR_FMT(0),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState1 },	// kThermalState0 
			{	 TEMP_SENSOR_FMT(95),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState2 },	// kThermalState1 
			{	 TEMP_SENSOR_FMT(78),	kThermalState1, 	TEMP_SENSOR_FMT(93), 	kThermalState3 },	// kThermalState2 
			{	 TEMP_SENSOR_FMT(88),	kThermalState2, 	TEMP_SENSOR_FMT(117), 	kThermalState3 },	// kThermalState3 
		},
	},
	{	// Sensor 1
		{	// Clamshell open
			//	thresholdLow,			nextStateLow,		thresholdHigh,			nextStateHigh		// currentState
			{	 TEMP_SENSOR_FMT(0),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState1 },	// kThermalState0 
			{	 TEMP_SENSOR_FMT(95),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState2 },	// kThermalState1 
			{	 TEMP_SENSOR_FMT(78),	kThermalState1, 	TEMP_SENSOR_FMT(93), 	kThermalState3 },	// kThermalState2 
			{	 TEMP_SENSOR_FMT(88),	kThermalState2, 	TEMP_SENSOR_FMT(117), 	kThermalState3 },	// kThermalState3 
		},
		{	// Clamshell closed
			//	thresholdLow,			nextStateLow,		thresholdHigh,			nextStateHigh		// currentState
			{	 TEMP_SENSOR_FMT(0),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState1 },	// kThermalState0 
			{	 TEMP_SENSOR_FMT(95),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState2 },	// kThermalState1 
			{	 TEMP_SENSOR_FMT(78),	kThermalState1, 	TEMP_SENSOR_FMT(93), 	kThermalState3 },	// kThermalState2 
			{	 TEMP_SENSOR_FMT(88),	kThermalState2, 	TEMP_SENSOR_FMT(117), 	kThermalState3 },	// kThermalState3 
		},
	},
	{	// Sensor 2
		{	// Clamshell open
			//	thresholdLow,			nextStateLow,		thresholdHigh,			nextStateHigh		// currentState
			{	 TEMP_SENSOR_FMT(0),	kThermalState0, 	TEMP_SENSOR_FMT(73), 	kThermalState1 },	// kThermalState0 
			{	 TEMP_SENSOR_FMT(68),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState2 },	// kThermalState1 
			{	 TEMP_SENSOR_FMT(78),	kThermalState1, 	TEMP_SENSOR_FMT(93), 	kThermalState3 },	// kThermalState2 
			{	 TEMP_SENSOR_FMT(88),	kThermalState2, 	TEMP_SENSOR_FMT(117), 	kThermalState3 },	// kThermalState3 
		},
		{	// Clamshell closed
			//	thresholdLow,			nextStateLow,		thresholdHigh,			nextStateHigh		// currentState
			{	 TEMP_SENSOR_FMT(0),	kThermalState0, 	TEMP_SENSOR_FMT(73), 	kThermalState1 },	// kThermalState0 
			{	 TEMP_SENSOR_FMT(68),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState2 },	// kThermalState1 
			{	 TEMP_SENSOR_FMT(78),	kThermalState1, 	TEMP_SENSOR_FMT(93), 	kThermalState3 },	// kThermalState2 
			{	 TEMP_SENSOR_FMT(88),	kThermalState2, 	TEMP_SENSOR_FMT(117), 	kThermalState3 },	// kThermalState3 
		},
	},
	{	// Sensor 3
		{	// Clamshell open
			//	thresholdLow,			nextStateLow,		thresholdHigh,			nextStateHigh		// currentState
			{	 TEMP_SENSOR_FMT(0),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState1 },	// kThermalState0 
			{	 TEMP_SENSOR_FMT(95),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState2 },	// kThermalState1 
			{	 TEMP_SENSOR_FMT(78),	kThermalState1, 	TEMP_SENSOR_FMT(93), 	kThermalState3 },	// kThermalState2 
			{	 TEMP_SENSOR_FMT(88),	kThermalState2, 	TEMP_SENSOR_FMT(117), 	kThermalState3 },	// kThermalState3 
		},
		{	// Clamshell closed
			//	thresholdLow,			nextStateLow,		thresholdHigh,			nextStateHigh		// currentState
			{	 TEMP_SENSOR_FMT(0),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState1 },	// kThermalState0 
			{	 TEMP_SENSOR_FMT(95),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState2 },	// kThermalState1 
			{	 TEMP_SENSOR_FMT(78),	kThermalState1, 	TEMP_SENSOR_FMT(93), 	kThermalState3 },	// kThermalState2 
			{	 TEMP_SENSOR_FMT(88),	kThermalState2, 	TEMP_SENSOR_FMT(117), 	kThermalState3 },	// kThermalState3 
		},
	},
	{	// Sensor 4
		{	// Clamshell open
			//	thresholdLow,			nextStateLow,		thresholdHigh,			nextStateHigh		// currentState
			{	 TEMP_SENSOR_FMT(0),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState1 },	// kThermalState0 
			{	 TEMP_SENSOR_FMT(95),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState2 },	// kThermalState1 
			{	 TEMP_SENSOR_FMT(78),	kThermalState1, 	TEMP_SENSOR_FMT(93), 	kThermalState3 },	// kThermalState2 
			{	 TEMP_SENSOR_FMT(88),	kThermalState2, 	TEMP_SENSOR_FMT(117), 	kThermalState3 },	// kThermalState3 
		},
		{	// Clamshell closed
			//	thresholdLow,			nextStateLow,		thresholdHigh,			nextStateHigh		// currentState
			{	 TEMP_SENSOR_FMT(0),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState1 },	// kThermalState0 
			{	 TEMP_SENSOR_FMT(95),	kThermalState0, 	TEMP_SENSOR_FMT(100), 	kThermalState2 },	// kThermalState1 
			{	 TEMP_SENSOR_FMT(78),	kThermalState1, 	TEMP_SENSOR_FMT(93), 	kThermalState3 },	// kThermalState2 
			{	 TEMP_SENSOR_FMT(88),	kThermalState2, 	TEMP_SENSOR_FMT(117), 	kThermalState3 },	// kThermalState3 
		},
	},
};

#ifndef sub_iokit_graphics
#	define sub_iokit_graphics           err_sub(5)
#endif
#ifndef kIOFBLowPowerAggressiveness
#	define kIOFBLowPowerAggressiveness iokit_family_err(sub_iokit_graphics,1)
#endif

#define super IOPlatformMonitor
OSDefineMetaClassAndStructors(PB5_1_PlatformMonitor, IOPlatformMonitor)

// **********************************************************************************
// actionFullPower
//
// **********************************************************************************
static bool actionFullPower ()
{
	IOService *serv;
	
	// CPU at full power if not already there
	if ((conSensorArray[kCPUController].state != kCPUPowerState0) && 
		(serv = conSensorArray[kCPUController].conSensor)) {
		conSensorArray[kCPUController].state = kCPUPowerState0;
		debug_msg ("IOPMon::actionFullPower - sending CPU aggressiveness 2\n");
		serv->setAggressiveness (kPMSetProcessorSpeed, 2);
		provider->setProperty (gIOPMonCPUActionKey, (OSObject *)gIOPMonFull);
	}
	
	// GPU at full power if not already there
	if ((conSensorArray[kGPUController].state != kGPUPowerState0) &&
		(serv = conSensorArray[kGPUController].conSensor)) {
		conSensorArray[kGPUController].state = kGPUPowerState0;
		debug_msg ("IOPMon::actionFullPower - sending GPU aggressiveness 0\n");
		serv->setAggressiveness (kIOFBLowPowerAggressiveness, 0);
		provider->setProperty (gIOPMonGPUActionKey, (OSObject *)gIOPMonFull);
	}

	return true;
}

// **********************************************************************************
// actionPower1
//
// **********************************************************************************
static bool actionPower1 ()
{	
	IOService *serv;
	
	
	// Step down CPU if not already slow
	if ((conSensorArray[kCPUController].state != kCPUPowerState1) && 
		(serv = conSensorArray[kCPUController].conSensor)) {
		conSensorArray[kCPUController].state = kCPUPowerState1;
		debug_msg ("IOPMon::actionPower1 - sending CPU aggressiveness 3\n");
		serv->setAggressiveness (kPMSetProcessorSpeed, 3);
		provider->setProperty (gIOPMonCPUActionKey, (OSObject *)gIOPMonReduced);
	}
	
	// GPU is still at full power
	if ((conSensorArray[kGPUController].state != kGPUPowerState0) &&
		(serv = conSensorArray[kGPUController].conSensor)) {
		conSensorArray[kGPUController].state = kGPUPowerState0;
		debug_msg ("IOPMon::actionPower1 - sending GPU aggressiveness 0\n");
		serv->setAggressiveness (kIOFBLowPowerAggressiveness, 0);
		provider->setProperty (gIOPMonGPUActionKey, (OSObject *)gIOPMonFull);
	}

	return true;
}

// **********************************************************************************
// actionPower1GPU1
//
// **********************************************************************************
static bool actionPower1GPU1 ()
{	
	IOService *serv;
		
	// Step down CPU if not already slow
	if ((conSensorArray[kCPUController].state != kCPUPowerState1) && 
		(serv = conSensorArray[kCPUController].conSensor)) {
		conSensorArray[kCPUController].state = kCPUPowerState1;
		debug_msg ("IOPMon::actionPower1GPU1 - CPU sending aggressiveness 3\n");
		serv->setAggressiveness (kPMSetProcessorSpeed, 3);
		provider->setProperty (gIOPMonCPUActionKey, (OSObject *)gIOPMonReduced);
	}
	
	// GPU to reduced power
	if ((conSensorArray[kGPUController].state != kGPUPowerState1) &&
		(serv = conSensorArray[kGPUController].conSensor)) {
		conSensorArray[kGPUController].state = kGPUPowerState1;
		debug_msg ("IOPMon::actionPower1GPU1 - sending GPU aggressiveness 1\n");
		serv->setAggressiveness (kIOFBLowPowerAggressiveness, 1);
		provider->setProperty (gIOPMonGPUActionKey, (OSObject *)gIOPMonReduced);
	}

	return true;
}

// **********************************************************************************
// actionPower1GPU2
//
// **********************************************************************************
static bool actionPower1GPU2 ()
{	
	IOService *serv;
	
	// Step down CPU if not already slow
	if ((conSensorArray[kCPUController].state != kCPUPowerState1) && 
		(serv = conSensorArray[kCPUController].conSensor)) {
		conSensorArray[kCPUController].state = kCPUPowerState1;
		debug_msg ("IOPMon::actionPower1GPU2 - CPU sending aggressiveness 3\n");
		serv->setAggressiveness (kPMSetProcessorSpeed, 3);
		provider->setProperty (gIOPMonCPUActionKey, (OSObject *)gIOPMonReduced);
	}
	
	// GPU to lowest power
	if ((conSensorArray[kGPUController].state != kGPUPowerState2) &&
		(serv = conSensorArray[kGPUController].conSensor)) {
		conSensorArray[kGPUController].state = kGPUPowerState2;
		debug_msg ("IOPMon::actionPower1GPU2 - sending GPU aggressiveness 2\n");
		serv->setAggressiveness (kIOFBLowPowerAggressiveness, 2);
		provider->setProperty (gIOPMonGPUActionKey, (OSObject *)gIOPMonSlow);
	}

	return true;
}

// **********************************************************************************
// start
//
// **********************************************************************************
bool PB5_1_PlatformMonitor::start ( IOService * nub )
{
	UInt32 i;
	
	if (!initSymbols())
		return false;
		
	if (!initPlatformState ())
		return false;
	
	provider = nub;
	
	commandGateCaller = &iopmonCommandGateCaller;	// Inform superclass about our caller
	
	// Initialize our controller/sensor types (platform-dependent)
	// Primary sensors
	conSensorArray[kPowerSensor].conSensorType = kIOPMonPowerSensor;			// platform power monitor
	conSensorArray[kPowerSensor].conSensor = this;						// platform power monitor
	conSensorArray[kPowerSensor].numStates = kMaxPowerStates;
	conSensorArray[kPowerSensor].sensorValid = true;
	conSensorArray[kPowerSensor].registered = true;						// built-in
	
	conSensorArray[kThermalSensor].conSensorType = kIOPMonThermalSensor;			// primary thermal sensor
	conSensorArray[kThermalSensor].conSensor = this;
	conSensorArray[kThermalSensor].numStates = kMaxThermalStates;
	conSensorArray[kThermalSensor].sensorValid = true;
	conSensorArray[kThermalSensor].registered = true;					// built-in aggregate sensor
	
	conSensorArray[kClamshellSensor].conSensorType = kIOPMonClamshellSensor;		// pmu clamshell sensor
	conSensorArray[kClamshellSensor].numStates = kNumClamshellStates;
	conSensorArray[kClamshellSensor].sensorValid = true;
	conSensorArray[kClamshellSensor].registered = false;

	// Controllers
	conSensorArray[kCPUController].conSensorType = kIOPMonCPUController;			// cpu controller
	conSensorArray[kCPUController].registered = false;

	conSensorArray[kGPUController].conSensorType = kIOPMonGPUController;			// gpu controller
	conSensorArray[kGPUController].state = kGPUPowerState0;
	conSensorArray[kGPUController].registered = true;					// built-in
	
	// Subsensors (all are thermal sensors)
	for (i = 0; i < kMaxSensorIndex; i++) {
		subSensorArray[i].conSensorType = kIOPMonThermalSensor;
		subSensorArray[i].conSensorParent = kThermalSensor;				// Index into primary array of our parent
		subSensorArray[i].numStates = kMaxThermalStates;
		// If a sensor is not to be used, set sensorValid false;
		subSensorArray[i].sensorValid = true;
		subSensorArray[i].registered = false;
	}
	
	if (!(dictPowerLow = OSDictionary::withCapacity (3)))
		return false;
	
	if (!(dictPowerHigh = OSDictionary::withCapacity (3)))
		return false;
	
	if (!(dictClamshellOpen = OSDictionary::withCapacity (2)))
		return false;
	
	if (!(dictClamshellClosed = OSDictionary::withCapacity (2)))
		return false;
	
	// On platforms with more than one CPU these dictionaries must accurately reflect which cpu is involved
	dictPowerLow->setObject (gIOPMonCPUIDKey, OSNumber::withNumber ((unsigned long long)0, 32));
	dictPowerLow->setObject (gIOPMonCurrentValueKey, OSNumber::withNumber ((unsigned long long)kPowerState1, 32));
	dictPowerLow->setObject (gIOPMonTypeKey, gIOPMonTypePowerSens);
	
	dictPowerHigh->setObject (gIOPMonCPUIDKey, OSNumber::withNumber ((unsigned long long)0, 32));
	dictPowerHigh->setObject (gIOPMonCurrentValueKey, OSNumber::withNumber ((unsigned long long)kPowerState0, 32));
	dictPowerHigh->setObject (gIOPMonTypeKey, gIOPMonTypePowerSens);
		
	dictClamshellOpen->setObject (gIOPMonCurrentValueKey, OSNumber::withNumber ((unsigned long long)kClamshellStateOpen, 32));
	dictClamshellOpen->setObject (gIOPMonTypeKey, gIOPMonTypeClamshellSens);
		
	dictClamshellClosed->setObject (gIOPMonCurrentValueKey, OSNumber::withNumber ((unsigned long long)kClamshellStateClosed, 32));
	dictClamshellClosed->setObject (gIOPMonTypeKey, gIOPMonTypeClamshellSens);
	
	if (!gIOPMonPowerStateKey)
		gIOPMonPowerStateKey 				= OSSymbol::withCString (kIOPMonPowerStateKey);
	if (!gIOPMonThermalStateKey)
		gIOPMonThermalStateKey 				= OSSymbol::withCString (kIOPMonThermalStateKey);
	if (!gIOPMonClamshellStateKey)
		gIOPMonClamshellStateKey 			= OSSymbol::withCString (kIOPMonClamshellStateKey);

	if (!gIOPMonCPUActionKey)
		gIOPMonCPUActionKey 				= OSSymbol::withCString (kIOPMonCPUActionKey);
	if (!gIOPMonGPUActionKey)
		gIOPMonGPUActionKey 				= OSSymbol::withCString (kIOPMonGPUActionKey);

	if (!gIOPMonState0)
		gIOPMonState0 						= OSSymbol::withCString (kIOPMonState0);
	if (!gIOPMonState1)
		gIOPMonState1 						= OSSymbol::withCString (kIOPMonState1);
	if (!gIOPMonState2)
		gIOPMonState2 						= OSSymbol::withCString (kIOPMonState2);
	if (!gIOPMonState3)
		gIOPMonState3 						= OSSymbol::withCString (kIOPMonState3);

	if (!gIOPMonFull)
		gIOPMonFull 						= OSSymbol::withCString (kIOPMonFull);
	if (!gIOPMonReduced)
		gIOPMonReduced 						= OSSymbol::withCString (kIOPMonReduced);
	if (!gIOPMonSlow)
		gIOPMonSlow 						= OSSymbol::withCString (kIOPMonSlow);

	lastPowerState = kMaxPowerStates;
	lastThermalState = kMaxThermalStates;
	lastClamshellState = kNumClamshellStates;
	
	// Put initial state and action info into the IORegistry
	updateIOPMonStateInfo(kIOPMonPowerSensor, currentPowerState);
	updateIOPMonStateInfo(kIOPMonThermalSensor, currentThermalState);
	updateIOPMonStateInfo(kIOPMonClamshellSensor, currentClamshellState);
	provider->setProperty (gIOPMonCPUActionKey, (OSObject *)gIOPMonReduced);
	provider->setProperty (gIOPMonGPUActionKey, (OSObject *)gIOPMonFull);

	// Let the world know we're open for business
	publishResource ("IOPlatformMonitor", this);

	if (super::start (nub)) {
		// pmRootDomain (found by our parent) is the recipient of GPU controller messages
		conSensorArray[kGPUController].conSensor = pmRootDomain;
		return true;
	} else
		return false;
}

// **********************************************************************************
// powerStateWillChangeTo
//
// **********************************************************************************
IOReturn PB5_1_PlatformMonitor::powerStateWillChangeTo (IOPMPowerFlags theFlags, unsigned long, IOService*)
{	
    if ( ! (theFlags & IOPMPowerOn) ) {
        // Sleep sequence:
		debug_msg ("PB5_1_PlatformMonitor::powerStateWillChangeTo - sleep\n");
		savePlatformState();

    }
    
    return IOPMAckImplied;
}

// **********************************************************************************
// powerStateDidChangeTo
//
// **********************************************************************************
IOReturn PB5_1_PlatformMonitor::powerStateDidChangeTo (IOPMPowerFlags theFlags, unsigned long, IOService*)
{	
    if (theFlags & IOPMPowerOn) {
        // Wake sequence:
		debug_msg ("PB5_1_PlatformMonitor::powerStateDidChangeTo - wake\n");
		restorePlatformState();
    } 
    
    return IOPMAckImplied;
}

// **********************************************************************************
// setAggressiveness
//
// setAggressiveness is called by the Power Manager to change our power state
// It is most commonly called when the user changes the Energy Saver preferences
// to change the power state but it can be called for other reasons.
//
// One of these reasons is a forced-reduced-speed condition but we now detect and
// act on this condition immediately (through a clamshell event) so by the time the
// PM calls us here we have already altered the condition and there is nothing to do.
//
// **********************************************************************************
IOReturn PB5_1_PlatformMonitor::setAggressiveness(unsigned long selector, unsigned long newLevel)
{
	IOPMonEventData 	event;
	IOReturn			result;
	    
    result = super::setAggressiveness(selector, newLevel);

    newLevel &= 0x7FFFFFFF;		// mask off high bit... upcoming kernel change will use the high bit to indicate whether setAggressiveness call
                                // was user induced (Energy Saver) or not.  Currently not using this info so mask it off.
	if (selector == kPMSetProcessorSpeed) {
		debug_msg ("PB5_1_PlatformMonitor::setAggressiveness - newLevel %ld\n", newLevel);
		if (newLevel != currentPowerState) {	// This only works if we have two power states
			// create and transmit internal event
			event.event = kIOPMonMessageStateChanged;
			event.conSensor = this;
			event.eventDict = (newLevel == 0) ? dictPowerHigh : dictPowerLow;
	
			// send it
			handleEvent (&event);
		}
	}
	
	return result;
}

// **********************************************************************************
// initPowerState
//
// **********************************************************************************
bool PB5_1_PlatformMonitor::initPowerState ()
{
	currentPowerState = kPowerState0;			// We will have booted fast
	return true;
}

// **********************************************************************************
// savePowerState
//
// **********************************************************************************
void PB5_1_PlatformMonitor::savePowerState ()
{
	return;			// Currently nothing to do
}

// **********************************************************************************
// restorePowerState
//
// **********************************************************************************
bool PB5_1_PlatformMonitor::restorePowerState ()
{
	IOService *serv;

	// This platform is always fast coming out of sleep, so if our current power
	// state is slow (kPowerState1) return true to force an update.
	conSensorArray[kCPUController].state = kCPUPowerState0;	// Set controller state
        
        // This ensures that the CPU speed and L3 are in sync, its possible that the L3 was disabled
        // previously which means when we wake it will still be disabled which would not be in sync
        // with the CPU speed (fast on wake)
        serv = conSensorArray[kCPUController].conSensor;
        debug_msg ("IOPMon::actionFullPower - sending CPU aggressiveness 2\n");
        serv->setAggressiveness (kPMSetProcessorSpeed, 2);
        
	return (currentPowerState == kPowerState1);	
}
	
// **********************************************************************************
// initThermalState
//
// **********************************************************************************
bool PB5_1_PlatformMonitor::initThermalState ()
{
	currentThermalState = kThermalState0;
	return true;
}

// **********************************************************************************
// saveThermalState
//
// **********************************************************************************
void PB5_1_PlatformMonitor::saveThermalState ()
{
	return;	// Nothing to do
}

// **********************************************************************************
// restoreThermalState
//
// **********************************************************************************
bool PB5_1_PlatformMonitor::restoreThermalState ()
{
	OSNumber			*threshLow, *threshHigh;
	UInt32				i;

	// need to get updated info from sensors
	for (i = 0; i < kMaxSensorIndex; i++) {
		if (subSensorArray[i].registered) { // Is sensor registered?
			subSensorArray[i].state = kMaxThermalStates;	//Set indeterminate state
			
			// Set low thresholds - this will cause sensor to update info
			threshLow = (OSNumber *)subSensorArray[i].threshDict->getObject (gIOPMonLowThresholdKey);
			threshLow->setValue((long long)0);
			threshHigh = (OSNumber *)subSensorArray[i].threshDict->getObject (gIOPMonHighThresholdKey);
			threshHigh->setValue((long long)0);
			// Send thresholds to sensor
			subSensorArray[i].conSensor->setProperties (subSensorArray[i].threshDict);
		}
	}
	
	/*
	 * This routine returns false because we don't want to force an adjust of the platform
	 * state just yet.  That will happen automatically when the sensors respond to the low
	 * thresholds
	 */
	return false;
}

// **********************************************************************************
// initClamshellState
//
// **********************************************************************************
bool PB5_1_PlatformMonitor::initClamshellState ()
{
	currentClamshellState = kClamshellStateOpen;
	
	return true;
}

// **********************************************************************************
// saveClamshellState
//
// **********************************************************************************
void PB5_1_PlatformMonitor::saveClamshellState ()
{
	return;				// Nothing to do
}

// **********************************************************************************
// restoreClamshellState
//
// **********************************************************************************
bool PB5_1_PlatformMonitor::restoreClamshellState ()
{
	// Currently nothing to do.  
	// We'll get the right state next time monitor power is called
	return false;
}

// **********************************************************************************
// monitorPower
//
// **********************************************************************************
IOReturn PB5_1_PlatformMonitor::monitorPower (OSDictionary *dict, IOService *provider)
{
	UInt32 				type, index, subIndex, value;
	OSNumber			*num;
	    
	if (lookupConSensorInfo (dict, provider, &type, &index, &subIndex)) {
		// See if any low power conditions exist.
		if (num = OSDynamicCast (OSNumber, dict->getObject (gIOPMonCurrentValueKey))) {
			value = num->unsigned32BitValue();
			value &= ~kIOPMForceLowSpeed;  		// Clear low speed bit
                        
                        // Check if we have the correct adapter plugged in. The high byte of "value" contains the wattage of the adapter
                        // Set the "I have the wrong adapter plugged in" bit if we have less than a 65 Watt adapter plugged in.

// Setting this bit causes the battery monitor to put a dialog indicating to the user that they have inserted the 
// wrong adapter.  Marketing has decided they no longer like this dialog, so I'm commenting out this code because
// I'm sure someone somewhere will change they're mind and want this back....
//                        if ((((value & 0xFF000000) >> 24) > 0) && (((value & 0xFF000000) >> 24) < 0x41))
//                            value |= 1<<11;
//                        else
//                            value &= ~(1<<11);
			
			num->setValue((long long)value);		// Send updated value back
                }
	}
	return kIOReturnBadArgument;
}

// **********************************************************************************
// updateIOPMonStateInfo
//
// **********************************************************************************
void PB5_1_PlatformMonitor::updateIOPMonStateInfo (UInt32 type, UInt32 state)
{
	const OSSymbol		*stateKey, *stateValue;
	
	stateKey = stateValue = NULL;
	
	if (type == kIOPMonPowerSensor)
		stateKey = gIOPMonPowerStateKey;
	else if (type == kIOPMonThermalSensor)
		stateKey = gIOPMonThermalStateKey;
	else if (type == kIOPMonClamshellSensor)
		stateKey = gIOPMonClamshellStateKey;

	// Thermal has the most states so we use that to look up state values
	if (state == kThermalState0)
		stateValue = gIOPMonState0;
	else if (state == kThermalState1)
		stateValue = gIOPMonState1;
	else if (state == kThermalState2)
		stateValue = gIOPMonState2;
	else if (state == kThermalState3)
		stateValue = gIOPMonState3;

	if (stateKey && stateValue)
		provider->setProperty (stateKey, (OSObject *)stateValue);
	
	return;
}

// **********************************************************************************
// initPlatformState
//
// **********************************************************************************
bool PB5_1_PlatformMonitor::initPlatformState ()
{
	if (initPowerState() &&
		initThermalState() &&
		initClamshellState())
		return true;
	else
		return false;
}

// **********************************************************************************
// savePlatformState
//
//		-- protected by CommandGate - call via IOPlatformMonitor::savePlatformState
//
// **********************************************************************************
void PB5_1_PlatformMonitor::savePlatformState ()
{
	savePowerState();
	saveThermalState();
	saveClamshellState();
	
	return;
}

// **********************************************************************************
// restorePlatformState
//
//		-- protected by CommandGate - call via IOPlatformMonitor::restorePlatformState
//
// **********************************************************************************
void PB5_1_PlatformMonitor::restorePlatformState ()
{
	bool doAdjust;
	
	// Last states are indeterminate
	lastPowerState = kMaxPowerStates;
	lastThermalState = kMaxThermalStates;
	lastClamshellState = kNumClamshellStates;

	// If restoration of any of the states requires action (i.e., if they returned
	// true), force an update
	doAdjust = restorePowerState();
	doAdjust |= restoreThermalState();
	doAdjust |= restoreClamshellState();
	if (doAdjust) {
		lastAction = 0;		// Force update
		adjustPlatformState ();
	}
	
	return;
}

// **********************************************************************************
// adjustPlatformState
//
// This gets called whenever there is a possible state change.  A state change doesn't
// imply that you always have to take an action - in fact, most of the time you don't -
// so only if the action we should take is different from the last action we took do
// we do something.
//
// **********************************************************************************
bool PB5_1_PlatformMonitor::adjustPlatformState ()
{
	IOPlatformMonitorAction		actionToTake;
	bool						result = true;
	
	debug_msg ("PB5_1_PlatformMonitor::adjustPlatformState - entered, cps %ld, cts %ld, ccs %ld\n",
		currentPowerState, currentThermalState, currentClamshellState);
	// Look up action to take for current state
	actionToTake = platformActionGrid[currentPowerState][currentThermalState][currentClamshellState];
	
	// Current state becomes the last state, update info
	if (lastPowerState != currentPowerState) {
		lastPowerState = currentPowerState;
		updateIOPMonStateInfo(kIOPMonPowerSensor, currentPowerState);
	}
	if (lastThermalState != currentThermalState) {
                lastAction = 0;
		lastThermalState = currentThermalState;
		updateIOPMonStateInfo(kIOPMonThermalSensor, currentThermalState);
	}
	if (lastClamshellState != currentClamshellState) {
		lastClamshellState = currentClamshellState;
		updateIOPMonStateInfo(kIOPMonClamshellSensor, currentClamshellState);
	}
	
	if (actionToTake != lastAction) {
		result = (*actionToTake)();		// Do it
		lastAction = actionToTake;		// Remember what it was we did
	}
	
	return result;
	
}


// **********************************************************************************
// registerConSensor
//
// **********************************************************************************
IOReturn PB5_1_PlatformMonitor::registerConSensor (OSDictionary *dict, IOService *conSensor)
{
	UInt32 				csi, subsi, type, initialState, initialValue;
	ConSensorInfo		*csInfo;
		
	if (!lookupConSensorInfo (dict, conSensor, &type, &csi, &subsi))
		return kIOReturnBadArgument;
		
	debug_msg ("PB5_1_PlatformMonitor::registerConSensor - type %ld, csi %ld, subsi %ld\n", type, csi, subsi);
	
	if (subsi < kMaxSensorIndex)	// Is subsensor index valid? If so use subSensorArray
		csInfo = &subSensorArray[subsi];
	else
		csInfo = &conSensorArray[csi];
	csInfo->conSensor = conSensor;
	csInfo->dict = dict;
	dict->retain ();
	
	initialState = initialValue = 0;
	// type dependent initialization
	switch (csInfo->conSensorType) {
		case kIOPMonThermalSensor:
			/*
			 * Thermal sensors get aggregated through the subSensorArray.  The main thermal sensor
			 * entry in conSensorArray only tracks the overall state
			 */
			if (!csInfo->sensorValid)
				return kIOReturnUnsupported;		// Don't need this sensor - tell it to go away
				
			//if (!retrieveCurrentValue (dict, &initialThermalValue))
			//	return kIOReturnBadArgument;
			
			// Figure out our initial state
                        // Just initialize the initial state/initial value to 0 for now and let the normal HandleThermalEvent get called if it needs to later...
			//initialState = lookupThermalStateFromValue (subsi, initialThermalValue);
			
			if (!(csInfo->threshDict = OSDictionary::withCapacity(3)))
				return kIOReturnNoMemory;
				
			csInfo->threshDict->setObject (gIOPMonIDKey, OSNumber::withNumber (subsi, 32));
			csInfo->threshDict->setObject (gIOPMonLowThresholdKey, 
				OSNumber::withNumber (thermalThresholdInfoArray[subsi][currentClamshellState][initialState].thresholdLow, 32));
			csInfo->threshDict->setObject (gIOPMonHighThresholdKey, 
				OSNumber::withNumber (thermalThresholdInfoArray[subsi][currentClamshellState][initialState].thresholdHigh, 32));
			
			// Send thresholds to sensor
			conSensor->setProperties (csInfo->threshDict);
			
			csInfo->registered = true;
			break;
		
		case kIOPMonPowerSensor:
			initialState = kPowerState0;			// Booted fast
			break;
		
		case kIOPMonClamshellSensor:
			if (!retrieveCurrentValue (dict, &initialValue))
				return kIOReturnBadArgument;

			csInfo->registered = true;
			break;
		
		case kIOPMonCPUController:
			initialState = kCPUPowerState0;			// Booted fast
			csInfo->registered = true;
			break;
		
		case kIOPMonGPUController:
			initialState = kGPUPowerState0;			// GPU starts out fast
			break;
		
		default:
			break;
	}
	
	csInfo->value = initialValue;
	csInfo->state = initialState;

	return kIOReturnSuccess;
}

// **********************************************************************************
// unregisterSensor
//
// **********************************************************************************
bool PB5_1_PlatformMonitor::unregisterSensor (UInt32 sensorID)
{
	if (sensorID >= kMaxSensors)
		return false;
	
	conSensorArray[sensorID].conSensor = NULL;
	return true;
}

// **********************************************************************************
// lookupConSensorInfo
//
// **********************************************************************************
bool PB5_1_PlatformMonitor::lookupConSensorInfo (OSDictionary *dict, IOService *conSensor, 
	UInt32 *type, UInt32 *index, UInt32 *subIndex)
{
	OSSymbol	*typeString;
	UInt32		tempType;
	
	OSObject				*obj;
	
	*index = kMaxConSensors;					// assume not found
	*subIndex = kMaxSensorIndex;				// assume not found
	// See if we have a type
	*type = tempType = kIOPMonUnknownSensor;	// assume not
	obj = dict->getObject (gIOPMonTypeKey);
	if (obj && (typeString = OSDynamicCast (OSSymbol, obj))) {
	//if (typeString = OSDynamicCast (OSSymbol, dict->getObject (gIOPMonTypeKey))) {
		if (typeString->isEqualTo (gIOPMonTypePowerSens))
			tempType = kIOPMonPowerSensor;
		else if (typeString->isEqualTo (gIOPMonTypeThermalSens))
			tempType = kIOPMonThermalSensor;
		else if (typeString->isEqualTo (gIOPMonTypeClamshellSens))
			tempType = kIOPMonClamshellSensor;
		else if (typeString->isEqualTo (gIOPMonTypeCPUCon))
			tempType = kIOPMonCPUController;
		else if (typeString->isEqualTo (gIOPMonTypeGPUCon))
			tempType = kIOPMonGPUController;
		else if (typeString->isEqualTo (gIOPMonTypeSlewCon))
			tempType = kIOPMonSlewController;
		else if (typeString->isEqualTo (gIOPMonTypeFanCon))
			tempType = kIOPMonFanController;
	} 
		
	if (retrieveSensorIndex (dict, subIndex)) {
		if (*subIndex >= kMaxSensorIndex)
			return false;
			
		// We're dealing with a subSensor, so set the type and validate it in the subSensor array by index
		*type = (tempType == kIOPMonUnknownSensor) ? subSensorArray[*subIndex].conSensorType : tempType;
			
		return true;
	}
	
	/*
	 * Treat anything else as a primary sensor.
	 * For this platform there is only one of each primary controller/sensor so if we find the type
	 * we have the index.
	 */
	for (*index = 0; *index < kMaxConSensors; (*index)++) {
		if (conSensorArray[*index].conSensorType == tempType) {
			*type = tempType;
			break;
		}
	}
	
	return (*index < kMaxConSensors);
}

// **********************************************************************************
// lookupThermalStateFromValue
//
// **********************************************************************************
UInt32 PB5_1_PlatformMonitor::lookupThermalStateFromValue (UInt32 sensorIndex, ThermalValue value)
{
	UInt32 i;
	
	if (value > thermalThresholdInfoArray[sensorIndex][currentClamshellState][0].thresholdLow) {
		for (i = 0 ; i < subSensorArray[sensorIndex].numStates; i++)
			if (value > thermalThresholdInfoArray[sensorIndex][currentClamshellState][i].thresholdLow &&
				value < thermalThresholdInfoArray[sensorIndex][currentClamshellState][i].thresholdHigh)
					return i;

		// This is bad as sensor's already over the limit - need to figure right response
		debug_msg ("PB5_1_PlatformMonitor::lookupStateFromValue - sensor %ld over limit\n", sensorIndex);
		return kMaxSensorIndex;
	}
	
	// Safely below the lowest threshold
	return kThermalState0;
}

// **********************************************************************************
// handlePowerEvent
//
//		-- protected by CommandGate - call through handleEvent
//
// **********************************************************************************
bool PB5_1_PlatformMonitor::handlePowerEvent (IOPMonEventData *eventData)
{
	UInt32		nextPowerState;
	bool		result;

	if (!(retrieveCurrentValue (eventData->eventDict, &nextPowerState)))
		return false;
		
	result = true;
	if (currentPowerState != nextPowerState) {
		currentPowerState = nextPowerState;
		result = adjustPlatformState ();
	}
	
	return result;
}

// **********************************************************************************
// handleThermalEvent
//
//		-- protected by CommandGate - call through handleEvent
//
// **********************************************************************************
bool PB5_1_PlatformMonitor::handleThermalEvent (IOPMonEventData *eventData)
{
	UInt32					type, csi, subsi, myThermalState;
	ThermalValue			value;
	OSNumber				*threshLow, *threshHigh;
	bool					result;

	result = true; 
	if (!(retrieveCurrentValue (eventData->eventDict, &value)))
		return false;
	
	switch (eventData->event) {		
		case kIOPMonMessageLowThresholdHit:		
		case kIOPMonMessageHighThresholdHit:
			if (lookupConSensorInfo (eventData->eventDict, eventData->conSensor, &type, &csi, &subsi) &&
				subSensorArray[subsi].registered) {
				myThermalState = lookupThermalStateFromValue (subsi, value);
				if (myThermalState >= kMaxSensorIndex)
					result = false;
			} else
				result = false;
			break;
				
		default:
			debug_msg ("PB5_1_PlatformMonitor::handleThermalEvent - event %ld not handled\n", 
				eventData->event);
			result = false;
			break;
	}
	
	if (result) {
		if (myThermalState != subSensorArray[subsi].state) {
			subSensorArray[subsi].state = myThermalState;
			if (!subSensorArray[subsi].registered) 
				// Not registered
				return false;
			
			threshLow = (OSNumber *)subSensorArray[subsi].threshDict->getObject (gIOPMonLowThresholdKey);
			threshLow->setValue((long long)thermalThresholdInfoArray[subsi][currentClamshellState][myThermalState].thresholdLow);
			threshHigh = (OSNumber *)subSensorArray[subsi].threshDict->getObject (gIOPMonHighThresholdKey);
			threshHigh->setValue((long long)thermalThresholdInfoArray[subsi][currentClamshellState][myThermalState].thresholdHigh);
			// Send thresholds to sensor
			subSensorArray[subsi].conSensor->setProperties (subSensorArray[subsi].threshDict);
		}
		
		if (currentThermalState != myThermalState) {
			UInt32 i, maxState;
			
			// Find the current max state among all the sensors
			maxState = 0;
			for (i = 0; i < kMaxSensorIndex; i++) 
				if (subSensorArray[i].registered)
					maxState = (maxState > subSensorArray[i].state) ? maxState : subSensorArray[i].state;
			
			// If new max state is different than current, update platform state
			if (currentThermalState != maxState) {
				currentThermalState = maxState;
				result = adjustPlatformState ();
			}
		}
	}
	
	return result;
}

// **********************************************************************************
// handleClamshellEvent
//
//		-- protected by CommandGate - call through handleEvent
//
// **********************************************************************************
bool PB5_1_PlatformMonitor::handleClamshellEvent (IOPMonEventData *eventData)
{
	UInt32		nextClamshellState;
	bool		result;

	if (!(retrieveCurrentValue (eventData->eventDict, &nextClamshellState)))
		return false;
		
	result = true;
	if (currentClamshellState != nextClamshellState) {
		currentClamshellState = nextClamshellState;
		result = adjustPlatformState ();
	}
	
	return result;
}

// **********************************************************************************
// iopmonCommandGateCaller - invoked by commandGate->runCommand
//
//		This is single threaded!!
//
// **********************************************************************************
//static
IOReturn PB5_1_PlatformMonitor::iopmonCommandGateCaller(OSObject *object, void *arg0, void *arg1, void *arg2, void *arg3)
{
	PB5_1_PlatformMonitor	*me;
	IOPMonCommandThreadSet	*commandSet;
	UInt32					type, conSensorID, subSensorID;
	bool					result;
	
	// Pull our event data and, since we're a static function, a reference to our object
	// out of the parameters
	if (((commandSet = (IOPMonCommandThreadSet *)arg0) == NULL) ||
		((me = OSDynamicCast(PB5_1_PlatformMonitor, commandSet->me)) == NULL))
		return kIOReturnError;
	
	result = true; 

	switch (commandSet->command) {
		case kIOPMonCommandHandleEvent:
			if (!(commandSet->eventData.eventDict)) {
				IOLog ("PB5_1_PlatformMonitor::iopmonCommandGateCaller - bad dictionary\n");
				return kIOReturnBadArgument;
			}
			if (!(commandSet->eventData.conSensor)) {
				IOLog ("PB5_1_PlatformMonitor::iopmonCommandGateCaller - bad conSensor\n");
				return kIOReturnBadArgument;
			}
			if (!me->lookupConSensorInfo (commandSet->eventData.eventDict, commandSet->eventData.conSensor, 
				&type, &conSensorID, &subSensorID)) {
				IOLog ("PB5_1_PlatformMonitor::iopmonCommandGateCaller - bad sensor info lookup\n");
				return kIOReturnBadArgument;
			}
			
			switch (type) {
				case kIOPMonPowerSensor:			// platform power monitor
					result = me->handlePowerEvent (&commandSet->eventData);
					break;
				case kIOPMonThermalSensor:			// zone 1 thermal sensor
					result = me->handleThermalEvent (&commandSet->eventData);
					break;
				case kIOPMonClamshellSensor:		// pmu clamshell sensor
					result = me->handleClamshellEvent (&commandSet->eventData);
					break;
				
				default:
					IOLog ("PB5_1_PlatformMonitor::iopmonCommandGateCaller -  bad sensorType(%ld), sensorID(%ld), subsi(%ld)\n", 
						type, conSensorID, subSensorID);
					result = false;
					break;
			}
			break;
		
		case kIOPMonCommandSaveState:
			me->savePlatformState ();
			break;
			
		case kIOPMonCommandRestoreState:
			me->restorePlatformState ();
			break;
		
		default:
			IOLog ("PB5_1_PlatformMonitor::iopmonCommandGateCaller - bad command %ld\n", 
				commandSet->command);
			result = false;
			break;
	}			
	
	return result ? kIOReturnSuccess : kIOReturnError;
}
