/*
 * Copyright (c) 2002-2007 Apple Inc. All rights reserved.
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
 

#include <IOKit/platform/ApplePlatformExpert.h>

#include "UniN.h"
#include "MacRISC2.h"

#include <sys/cdefs.h>

__BEGIN_DECLS

__END_DECLS

#define super IOService
OSDefineMetaClassAndStructors(AppleUniN,ApplePlatformExpert)

// **********************************************************************************
// start
//
// **********************************************************************************
bool AppleUniN::start ( IOService * nub )
{
    UInt32			uniNArbCtrl, uniNMPCIMemTimeout;
	IOInterruptState 		intState = NULL;
	IOPlatformFunction		*func;
	const OSSymbol			*functionSymbol = OSSymbol::withCString(kInstantiatePlatformFunctions);
	SInt32					retval;

	provider = nub;
	
	// If our PE isn't MacRISC2PE, we shouldn't be here
	if (!OSDynamicCast (MacRISC2PE, getPlatform()))
		return false;

        uniNMemory      = provider->mapDeviceMemoryWithIndex( 0 );
        if ( ! uniNMemory )
        {
                kprintf( "AppleUniN::start - unable to map in device memory\n" );
                return false;
        }
        uniNBaseAddress = (UInt32 *)uniNMemory->getVirtualAddress();
        if (uniNBaseAddress == 0)
        {
                kprintf( "AppleUniN::start - unable to get UniN base address\n" );
                return false;
        }
	
	// sets up the mutex lock:
	mutex = IOSimpleLockAlloc();

	if (mutex != NULL)
		IOSimpleLockInit( mutex );

    // Set QAckDelay depending on the version of Uni-N.
	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);
  
    uniNVersion = readUniNReg(kUniNVersion);

    if (uniNVersion < kUniNVersion150)
    {
        uniNArbCtrl = readUniNReg(kUniNArbCtrl);
        uniNArbCtrl &= ~kUniNArbCtrlQAckDelayMask;

        if (uniNVersion < kUniNVersion107) {
            uniNArbCtrl |= kUniNArbCtrlQAckDelay105 << kUniNArbCtrlQAckDelayShift;
        } else {
            uniNArbCtrl |= kUniNArbCtrlQAckDelay << kUniNArbCtrlQAckDelayShift;
        }
        writeUniNReg(kUniNArbCtrl, uniNArbCtrl);
    }

    // Set Max/PCI Memory Timeout for appropriate Uni-N.   
    if ( ((uniNVersion >= kUniNVersion150) && (uniNVersion <= kUniNVersion200)) || 
         (uniNVersion == kUniNVersionPangea) )
    {
        uniNMPCIMemTimeout = readUniNReg(kUniNMPCIMemTimeout);
        uniNMPCIMemTimeout &= ~kUniNMPCIMemTimeoutMask;
        uniNMPCIMemTimeout |= kUniNMPCIMemGrantTime;
        writeUniNReg(kUniNMPCIMemTimeout, uniNMPCIMemTimeout);
    }
	
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);
  
	// Figure out if we're on a notebook
	if (callPlatformFunction ("PlatformIsPortable", true, (void *) &hostIsMobile, (void *)0,
		(void *)0, (void *)0) != kIOReturnSuccess)
			hostIsMobile = false;

	// Identify any platform-do-functions
	retval = provider->getPlatform()->callPlatformFunction (functionSymbol, true, (void *)provider, 
		(void *)&platformFuncArray, (void *)0, (void *)0);
	if (retval == kIOReturnSuccess && (platformFuncArray != NULL)) {
		unsigned int i, count;
		
		// Examine the functions and for any that are demand, publish the function so callers can find us
        count = platformFuncArray->getCount();
		for (i = 0; i < count; i++)
			if (func = OSDynamicCast (IOPlatformFunction, platformFuncArray->getObject(i)))
				if (func->getCommandFlags() & kIOPFFlagOnDemand)
					func->publishPlatformFunction (this);
	}
	
	if (IsThisAnIntrepid(uniNVersion)) {
		symReadIntrepidClockStopStatus = OSSymbol::withCString("readIntrepidClockStopStatus");
		publishResource(symReadIntrepidClockStopStatus, this);
	}
	
		
	// Create our friends
	createNubs(this, provider->getChildIterator( gIODTPlane ));
  	
	// Come and get it...
	registerService();
	
	return super::start(provider);
}

void AppleUniN::free ()
{

	if (platformFuncArray) {
		platformFuncArray->flushCollection();
		platformFuncArray->release();
	}

	if (mutex != NULL)
		IOSimpleLockFree( mutex );
	
	super::free();

	return;
}

// **********************************************************************************
// callPlatformFunction
//
// **********************************************************************************
IOReturn AppleUniN::callPlatformFunction(const OSSymbol *functionName, bool waitForFunction, 
		void *param1, void *param2, void *param3, void *param4)
{
    if (functionName->isEqualTo("safeReadRegUInt32"))
    {
        UInt32 *returnval = (UInt32 *)param2;
        *returnval = safeReadRegUInt32((UInt32)param1);
        return kIOReturnSuccess;
    }
	
    if (functionName->isEqualTo("safeWriteRegUInt32"))
    {
        safeWriteRegUInt32((UInt32)param1, (UInt32)param2, (UInt32)param3);
        return kIOReturnSuccess;
    }

    if (functionName->isEqualTo(kUniNSetAACKDelay))
    {
        setAACKDelay((UInt32)param1);
        return kIOReturnSuccess;
    }

    if (functionName->isEqualTo(kUniNSetPowerState))
    {
        uniNSetPowerState((UInt32)param1);
        return kIOReturnSuccess;
    }
	
	if (functionName->isEqualTo ("setupUATAforSleep"))
		return setupUATAforSleep();

	if (functionName == symReadIntrepidClockStopStatus)
		return readIntrepidClockStopStatus((UInt32 *)param1, (UInt32 *)param2);

	if (platformFuncArray) {
		UInt32 i, count;
		IOPlatformFunction *pfFunc;
		
        count = platformFuncArray->getCount();
		for (i = 0; i < count; i++)
			if (pfFunc = OSDynamicCast (IOPlatformFunction, platformFuncArray->getObject(i)))
				// Check for on-demand case
				if (pfFunc->platformFunctionMatch (functionName, kIOPFFlagOnDemand, NULL))
					return (performFunction (pfFunc, param1, param2, param3, param4)  ? kIOReturnSuccess : kIOReturnBadArgument);
	}

    return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}


IOReturn AppleUniN::callPlatformFunction(const char *functionName, bool waitForFunction, 
		void *param1, void *param2, void *param3, void *param4)
{
	IOReturn result = kIOReturnNoMemory;
	
	const OSSymbol *functionSymbol = OSSymbol::withCString(functionName);
  
	if (functionSymbol != 0) {
		result = callPlatformFunction(functionSymbol, waitForFunction,
			param1, param2, param3, param4);
		functionSymbol->release();
	}
  
	return result;
}

// **********************************************************************************
// setupUATAforSleep
//
// **********************************************************************************
IOReturn AppleUniN::setupUATAforSleep ()
{
    IOService		*uATANub;
	bool			result;
	
	// For Intrepid notebooks, locate ultra-ata entry so we can reset the ATA bus at sleep
	// This is a gross, disgusting hack because I don't have time to figure out a better way
	if ((!uATABaseAddress) && (IsThisAnIntrepid(uniNVersion)) && hostIsMobile) {
		uATANub = OSDynamicCast (IOService, provider->fromPath("/pci@F4000000/ata-6@D", gIODTPlane));
		if (uATANub) {
			if (uATABaseAddressMap = uATANub->mapDeviceMemoryWithIndex(0)) {
				uATABaseAddress = (volatile UInt32 *) uATABaseAddressMap->getVirtualAddress();
			}
		}
	}
	
	if (uATABaseAddress)
		result = kIOReturnSuccess;
	else
		result = kIOReturnUnsupported;
	
	return result;
}

// **********************************************************************************
// readIntrepidClockStopStatus
//
// **********************************************************************************
IOReturn AppleUniN::readIntrepidClockStopStatus (UInt32 *status0, UInt32 *status1)
{
	if (IsThisAnIntrepid(uniNVersion) && (status0 || status1)) {
		IOInterruptState intState = NULL;
	
		if ( mutex  != NULL )
			intState = IOSimpleLockLockDisableInterrupt(mutex);
	
		if (status0)
			*status0 = readUniNReg(kUniNClockStopStatus0);
		if (status1)
			*status1 = readUniNReg(kUniNClockStopStatus1);
	
		if ( mutex  != NULL )
			IOSimpleLockUnlockEnableInterrupt(mutex, intState);

		return kIOReturnSuccess;
	}
	
	return kIOReturnUnsupported;
}


// **********************************************************************************
// readUniNReg
//
// **********************************************************************************
UInt32 AppleUniN::readUniNReg(UInt32 offset)
{
    return uniNBaseAddress[offset >> 2];
}

// **********************************************************************************
// writeUniNReg
//
// **********************************************************************************
void AppleUniN::writeUniNReg(UInt32 offset, UInt32 data)
{
    uniNBaseAddress[offset >> 2] = data;
    OSSynchronizeIO();
	
	return;
}

// **********************************************************************************
// safeReadRegUInt32
//
// **********************************************************************************
UInt32 AppleUniN::safeReadRegUInt32(UInt32 offset)
{
	IOInterruptState intState = NULL;

	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);
  
	UInt32 currentReg = readUniNReg(offset);
  
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);

	return (currentReg);  
}

// **********************************************************************************
// safeWriteRegUInt32
//
// **********************************************************************************
void AppleUniN::safeWriteRegUInt32(UInt32 offset, UInt32 mask, UInt32 data)
{
	IOInterruptState	intState = NULL;
	UInt32 				currentReg;

	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);

	if (mask == ~0UL)	// Just write out the data
		currentReg = data;
	else {
		// read, modify then write the data
		currentReg = readUniNReg(offset);
		currentReg = (currentReg & ~mask) | (data & mask);
	}
		
	writeUniNReg (offset, currentReg);
  
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);
	
	return;
}

// **********************************************************************************
// enableUniNEthernetClock
//
// **********************************************************************************
void AppleUniN::enableUniNEthernetClock(bool enable, IOService *nub)
{
	if (enable)
		configureUniNPCIDevice(nub);
		
	safeWriteRegUInt32 (kUniNClockControl, kUniNEthernetClockEnable, 
		enable ? kUniNEthernetClockEnable : 0);
	
	// We need to wait a minimum of 4us for the clocks to come up and stabilize on Intrepid 2's
	// The extra 8us delay is a buffer for machines with slow clocks or more ragged signals.
	if (enable && IsThisAnIntrepid2(uniNVersion))
		IODelay(12);
	
	return;
}

// **********************************************************************************
// enableUniNFireWireClock
//
// **********************************************************************************
void AppleUniN::enableUniNFireWireClock(bool enable, IOService *nub)
{
	if (enable)
		configureUniNPCIDevice(nub);		

	safeWriteRegUInt32 (kUniNClockControl, kUniNFirewireClockEnable, 
		enable ? kUniNFirewireClockEnable : 0);
		
	return;
}

// **********************************************************************************
// setAACKDelay
//
// **********************************************************************************
void AppleUniN::setAACKDelay(UInt32 setDelayBit)
{
    UInt32	uniNAACKCtrl = 0;

    if ( setDelayBit )
		uniNAACKCtrl = kUniNAACKCtrlDelayEnable;
    
    safeWriteRegUInt32(kUniNAACKCtrl, kUniNAACKCtrlDelayEnableMask, uniNAACKCtrl);
}

// **********************************************************************************
// configureUniNPCIDevice
//
// **********************************************************************************
void AppleUniN::configureUniNPCIDevice (IOService *nub)
{
	OSData			*cacheData, *latencyData;
	IOPCIDevice		*provider;

	if (nub) provider = OSDynamicCast(IOPCIDevice, nub);
	else provider = NULL;
	
	if (provider) {
		cacheData = (OSData *)provider->getProperty (kIOPCICacheLineSize);
		latencyData = (OSData *)provider->getProperty (kIOPCITimerLatency);
	
		if (cacheData || latencyData) {
			UInt32				configData;	

			configData = provider->configRead32 (kIOPCIConfigCacheLineSize);
				
			if (cacheData) 
				configData = (configData & 0xFFFFFF00) | *(char *) cacheData->getBytesNoCopy();
			
			if (latencyData) 
				configData = (configData & 0xFFFF00FF) | ((*(char *) latencyData->getBytesNoCopy()) << 8);
							
			provider->configWrite32 (kIOPCIConfigCacheLineSize, configData);
		}
	}
	return;
}

// **********************************************************************************
// uniNSetPowerState
//
// **********************************************************************************
void AppleUniN::uniNSetPowerState (UInt32 state)
{
	if (state == kUniNNormal) {
		// Set normal mode
		safeWriteRegUInt32(kUniNPowerMngmnt, kUniNPowerMask, kUniNNormal);
    
		// Set the running state for HWInit.
		safeWriteRegUInt32(kUniNHWInitState, ~0UL, kUniNHWInitStateRunning);

		safeWriteRegUInt32(kUniNVSPSoftReset, ~0UL, saveVSPSoftReset);

		// On Intrepid, take the UATA bus out of reset
		if (uataBusWasReset) {
			uataBusWasReset = false;
			uATAFCR |= (kUniNUATAReset | kUniNUATAEnable);		// Enable the bus
			*(UInt32 *)(uATABaseAddress + 0) = uATAFCR;			// Do it
			OSSynchronizeIO();
		}
	} else if (state == kUniNIdle2) {
        // Set the sleeping state for HWInit.
        safeWriteRegUInt32(kUniNHWInitState, ~0UL, kUniNHWInitStateSleeping);

        // Tell Uni-N to enter sleep mode.
        safeWriteRegUInt32(kUniNPowerMngmnt, kUniNPowerMask, kUniNIdle2);
	} else if (state == kUniNSave) {		// save state
            saveVSPSoftReset = safeReadRegUInt32(kUniNVSPSoftReset);
	} else if (state == kUniNSleep) {
		// On Intrepid, put the UATA bus in reset for notebooks
		if (IsThisAnIntrepid(uniNVersion) && hostIsMobile && uATABaseAddress) {
			uATAFCR = *(UInt32 *)(uATABaseAddress + 0);			// Read byte reversed data
			uATAFCR &= ~(kUniNUATAReset | kUniNUATAEnable);		// Reset the bus
			*(UInt32 *)(uATABaseAddress + 0) = uATAFCR;			// Do it
			OSSynchronizeIO();
			uataBusWasReset = true;
		}
        // Set the sleeping state for HWInit.
        safeWriteRegUInt32(kUniNHWInitState, ~0UL, kUniNHWInitStateSleeping);

        // Tell Uni-N to enter sleep mode.
        safeWriteRegUInt32(kUniNPowerMngmnt, kUniNPowerMask, kUniNSleep);
	}
	
	return;
}

enum
{
  kMCMonitorModeControl = 0,
  kMCCommand,
  kMCPerformanceMonitor0,
  kMCPerformanceMonitor1,
  kMCPerformanceMonitor2,
  kMCPerformanceMonitor3
};

IOReturn AppleUniN::accessUniN15PerformanceRegister(bool write, long regNumber, UInt32 *data)
{
    UInt32 offset;
  
    if (uniNVersion < kUniNVersion150) return kIOReturnUnsupported;
  
    switch (regNumber) {
		case kMCMonitorModeControl  : offset = kUniNMMCR; break;
		case kMCCommand             : offset = kUniNMCMDR; break;
		case kMCPerformanceMonitor0 : offset = kUniNMPMC1; break;
		case kMCPerformanceMonitor1 : offset = kUniNMPMC2; break;
		case kMCPerformanceMonitor2 : offset = kUniNMPMC3; break;
		case kMCPerformanceMonitor3 : offset = kUniNMPMC4; break;
		default                     : return kIOReturnBadArgument;
    }
  
    if (data == 0) return kIOReturnBadArgument;
  
    if (write) {
        writeUniNReg(offset, *data);
    } else {
        *data = readUniNReg(offset);
    }
  
    return kIOReturnSuccess;
}

// **********************************************************************************
// performFunction
//
// **********************************************************************************
bool AppleUniN::performFunction(const IOPlatformFunction *func, void *cpfParam1,
			void *cpfParam2, void *cpfParam3, void *cpfParam4)
{
	static IOLock				*pfLock;
	bool						ret;
	IOPlatformFunctionIterator 	*iter;
	UInt32 						offset, value, valueLen, mask, maskLen, data = 0, writeLen, 
									cmd, cmdLen, result, pHandle, lastCmd = 0,
									param1, param2, param3, param4, param5, 
									param6, param7, param8, param9, param10;

	IOPCIDevice					*nub = NULL;
	
	if (func == 0) return(false);

	if (!pfLock)
		// Use a static lock here as there is only ever one instance of UniN
		pfLock = IOLockAlloc();
	
	if (pfLock)
		IOLockLock (pfLock);

	if (!(iter = ((IOPlatformFunction *)func)->getCommandIterator())) {
		if (pfLock)
			IOLockUnlock (pfLock);

		return false;
	}
	
	pHandle = func->getCommandPHandle();

	ret = true;
	while (iter->getNextCommand (&cmd, &cmdLen, &param1, &param2, &param3, &param4, 
		&param5, &param6, &param7, &param8, &param9, &param10, &result)  && ret) {
		if (result != kIOPFNoError)
			ret = false;
		else
		{
			// Examine the command - not all commands are supported
			switch (cmd)
			{
				case kCommandWriteReg32:
					// IOLog( "AppleUniN::%s - processing platformFunction kCommandWriteReg32 (%ld)\n", __FUNCTION__, cmd );

					offset = param1;
					value = param2;
					mask  = param3;
		
					// If mask isn't all ones, read data and mask it
					if (mask != 0xFFFFFFFF) {
						data = readUniNReg (offset);
						data &= mask;
						data |= value;
					} else	// just write the data
						data = value;
					
					// write the result to the Uni-N register
					writeUniNReg(offset, data);
					
					break;
		
				// Currently only handle config reads of 4 bytes or less
				case kCommandReadConfig:
					// IOLog( "AppleUniN::%s - processing platformFunction kCommandReadReg32 (%ld)\n", __FUNCTION__, cmd );
					offset = param1;
					valueLen = param2;
					
					if (valueLen != sizeof(UInt32)) {
						IOLog ("AppleUniN::%s config reads cannot handle anything other than %ld bytes, found length %ld\n", __FUNCTION__, sizeof(UInt32), valueLen);
						ret = false;
					}
		
					if (!nub) {
						if (!pHandle) {
							IOLog ("AppleUniN::%s config read requires pHandle to locate nub\n", __FUNCTION__);
							ret = false;
						}
						nub = findNubForPHandle (pHandle);
						if (!nub) {
							IOLog ("AppleUniN::%s config read cannot find nub for pHandle 0x%08lx\n", __FUNCTION__, pHandle);
							ret = false;
							// break; ??
						}
					}

					if ( nub )		// don't execute the read if you don't have a nub to send the configRead32 to
					{
						// NOTE - code below assumes read of 4 bytes, i.e., valueLen == 4!!
						data = nub->configRead32 (offset);
						if (cpfParam1)
							*(UInt32 *)cpfParam1 = data;
					}
					
					lastCmd = kCommandReadConfig;
					break;
					
				// Currently only handle config reads/writes of 4 bytes
				case kCommandRMWConfig:
					// IOLog( "AppleUniN::%s - processing platformFunction kCommandRMWConfig (%ld)\n", __FUNCTION__, cmd );
					// data must have been read above
					if (lastCmd != kCommandReadConfig) {
						IOLog ("AppleUniN::performFunction - config modify/write requires prior read\n");
						ret = false;
						// break; ??
					}
					
					offset = param1;
					maskLen = param2;
					valueLen = param3;
					writeLen = param4;
	
					if (writeLen != sizeof(UInt32)) {
						IOLog ("AppleUniN::%s config read/modify/write cannot handle anything other than %ld bytes, found length %ld\n",
						             __FUNCTION__, sizeof(UInt32), writeLen);
						ret = false;
					}
					
					// NOTE - code below makes implicit assumption that mask and value are each 4 bytes!!
					mask = *(UInt32 *)param5;
					value = *(UInt32 *)param6;
		
					if (!nub) {
						if (!pHandle) {
							IOLog ("AppleUniN::%s config read/modify/write requires pHandle to locate nub\n", __FUNCTION__);
							ret = false;
						}
						nub = findNubForPHandle (pHandle);
						if (!nub) {
							IOLog ("AppleUniN::%s config read/modify/write cannot find nub for pHandle 0x%08lx\n", __FUNCTION__, pHandle);
							ret = false;
							// break; ??
						}
					}
					
					// data must have been previously read (i.e., using kCommandReadConfig with result in data)
					data &= mask;
					data |= value;
					if ( nub )	// don't try to do the write unless you have a nub to send the configWrite32 to
						nub->configWrite32 (offset, data);

					break;
		
				default:
					IOLog("AppleUniN::%s -- unsupported command %08lx\n", __FUNCTION__, cmd);
					ret = false;
					break;
			}	// -switch-
		}	// -if- / -else-
	}	// -while-
	
	iter->release();

	if (pfLock)
		IOLockUnlock (pfLock);

	return(ret);
}

IOPCIDevice* AppleUniN::findNubForPHandle( UInt32 pHandleValue )
{
	IORegistryIterator*								iterator;
	IORegistryEntry*								matchingEntry = NULL;

	iterator = IORegistryIterator::iterateOver( gIODTPlane, kIORegistryIterateRecursively );

	if ( iterator == NULL )
		return( NULL );

	while ( ( matchingEntry = iterator->getNextObject() ) != NULL ) {
		OSData*						property;

		if ( ( property = OSDynamicCast( OSData, matchingEntry->getProperty( "AAPL,phandle" ) ) ) != NULL )
			if ( pHandleValue == *( ( UInt32 * ) property->getBytesNoCopy() ) )
				break;
	}
	
	iterator->release();
	
	return( OSDynamicCast (IOPCIDevice, matchingEntry));
}

