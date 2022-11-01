/*
 *	I2STransportInterface.cpp
 *
 *	Interface class for audio data transport
 *
 *  Created by Ray Montagne on Mon Mar 12 2003.
 *  Copyright (c) 2003 AppleComputer. All rights reserved.
 *
 *	This instance of a TransportInterface object supports the I2S I/O module
 *	that is available in KeyLargo, Pangea and the K2 system I/O controllers.
 *
 */
#include "I2STransportInterface.h"
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOTimerEventSource.h>
#include "AudioHardwareUtilities.h"
#include "PlatformInterface.h"

#define super TransportInterface

OSDefineMetaClassAndStructors ( I2STransportInterface, TransportInterface );

#pragma mark #--------------------
#pragma mark # PUBLIC METHODS
#pragma mark #--------------------

//	--------------------------------------------------------------------------------
bool		I2STransportInterface::init ( PlatformInterface * inPlatformInterface ) {
	bool			success;
	IOReturn		err = kIOReturnError;
	
	debugIOLog (3,  "+ I2STransportInterface[%p]::init ( %d )", this, (unsigned int)inPlatformInterface );

	success = super::init ( inPlatformInterface );
	FailIf ( !success, Exit );	

	success = FALSE;
	
	FailIf ( NULL == mPlatformObject, Exit );
	
	super::transportSetTransportInterfaceType ( kTransportInterfaceType_I2S );	

	requestClockSources ();

	mPlatformObject->setClockMux ( kGPIO_MuxSelectDefault );
	super::transportMakeClockSelect ( kTRANSPORT_MASTER_CLOCK );

	
	mMclkMultiplier = 256;		//	TAS3004, CS84xx run MCLK at 256 * fs
	mSclkMultiplier =  64;		//	TAS3004, CS84xx run MCLK at 64 * fs
	
	//	Setup for a 48.000 Khz sample rate with stereo 16 bit channels on both input and output.
	mDataWordSize = ( kDataIn16 << kDataInSizeShift ) | ( kDataOut16 << kDataOutSizeShift );
	mDataWordSize |=( kI2sStereoChannels << kNumChannelsOutShift ) | ( kI2sStereoChannels << kNumChannelsInShift );
	err = calculateSerialFormatRegisterValue ( 44100 );
	FailIf ( kIOReturnSuccess != err, Exit );

	err = super::transportSetSampleRate ( 44100 );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = restartTransport ();
	FailIf ( kIOReturnSuccess != err, Exit );
	
	success = true;
Exit:
	
	debugIOLog (3,  "- I2STransportInterface[%p (%ld)]::init ( %d ) = %d", this, mInstanceIndex, (unsigned int)inPlatformInterface, (unsigned int)success );
	return success;
}


//	--------------------------------------------------------------------------------
void I2STransportInterface::free () {

	releaseClockSources ();	
	super::free();
	
	return;
}

//	--------------------------------------------------------------------------------
//  [3683602]   Restarting the transport now has dedicated support and does not
//  rely on using 'transportSetSampleRate' which can modify the I2S I/O Module
//  registers.
IOReturn	I2STransportInterface::restartTransport ( void ) {
	IOReturn	result = kIOReturnBadArgument;

	debugIOLog (3,  "+ I2STransportInterface[%ld]::restartTransport (), serialFormat = 0x%0.8X, dataWordSize = 0x%0.8X", mInstanceIndex, mSerialFormat, mDataWordSize );
	
	result = mPlatformObject->setI2SEnable ( TRUE );
	FailIf ( kIOReturnSuccess != result, Exit );
	IOSleep ( 1 );
	
	result = mPlatformObject->setI2SCellEnable ( TRUE );
	FailIf ( kIOReturnSuccess != result, Exit );
	IOSleep ( 1 );

	//	Set the I2S cell to a clocks stopped mode so that the clocks
	//	stop low and then tristate so that a new format can be applied.
	mPlatformObject->setI2SIOMIntControl ( 1 << kClocksStoppedPendingShift );	//	Clear the clock stop status
	result = mPlatformObject->setI2SClockEnable ( FALSE );
	FailIf ( kIOReturnSuccess != result, Exit );
	IOSleep ( 1 );
	
	waitForClocksStopped ();
	
	result = mPlatformObject->setSerialFormatRegister ( mSerialFormat );
	FailIf ( kIOReturnSuccess != result, Exit );

	result = mPlatformObject->setDataWordSizes ( mDataWordSize );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	result = mPlatformObject->setI2SClockEnable ( TRUE );
	FailIf ( kIOReturnSuccess != result, Exit );
	IOSleep ( 1 );

Exit:		
	debugIOLog (3,  "- I2STransportInterface[%ld]::restartTransport () returns %lX", mInstanceIndex, result );
	return result;
}


//	--------------------------------------------------------------------------------
//	Maps the sample rate request to register values and register sequences
IOReturn	I2STransportInterface::transportSetSampleRate ( UInt32 sampleRate ) {
	IOReturn	result = kIOReturnBadArgument;
	
	debugIOLog (3,  "+ I2STransportInterface[%ld]::transportSetSampleRate ( %d )", mInstanceIndex, (unsigned int)sampleRate );
	
	if ( 0 != sampleRate ) {	//  [3686032]   dont assert if invalid sample rate on external clock when clock is missing
		result = calculateSerialFormatRegisterValue ( sampleRate );
		FailIf ( kIOReturnSuccess != result, Exit );
		FailIf ( NULL == mPlatformObject, Exit );
		
		result = restartTransport ();	//  [3683602]   
		FailIf ( kIOReturnSuccess != result, Exit );
			
		result = super::transportSetSampleRate ( sampleRate );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:	
	debugIOLog (3,  "- I2STransportInterface[%ld]::transportSetSampleRate ( %d ) returns 0x%0.8X", mInstanceIndex, (unsigned int)sampleRate, result );
	return result;
}

//	--------------------------------------------------------------------------------
//	Sets the data word sizes to configure the data transfer sizes between the
//	DMA engine and the I2S bus.  SampleWidth indicates the width of the sample
//	being conveyed on the I2S audio transport bus while DMA width indicates the
//	width of data being transacted between the I2S I/O Module and the DMA engine.
//
//	Sequence:	1.	Stop I2S Clocks
//				2.	Wait for I2S clocks stopped
//				3.	Set data word sizes
//				4.	Enable I2S Clocks
//
IOReturn	I2STransportInterface::transportSetSampleWidth ( UInt32 sampleDepth, UInt32 dmaWidth ) {
	IOReturn	result = kIOReturnError;
	
	FailIf ( NULL == mPlatformObject, Exit );
	if ( 16 == sampleDepth && 16 == dmaWidth ) {
		mDataWordSize = ( kDataIn16 << kDataInSizeShift ) | ( kDataOut16 << kDataOutSizeShift );
	} else if ( 24 == sampleDepth && 32 == dmaWidth ) {
		mDataWordSize = ( kDataIn24 << kDataInSizeShift ) | ( kDataOut24 << kDataOutSizeShift );
	} else {
		FailIf ( TRUE, Exit );
	}
	mDataWordSize |= ( ( kI2sStereoChannels << kNumChannelsOutShift ) | ( kI2sStereoChannels << kNumChannelsInShift ) );

	//	Set the I2S cell to a clocks stopped mode so that the clocks
	//	stop low and then tristate so a new data word size can be applied
	mPlatformObject->setI2SIOMIntControl ( 1 << kClocksStoppedPendingShift );	//	Clear the clock stop status
	result = mPlatformObject->setI2SClockEnable ( FALSE );
	FailIf ( kIOReturnSuccess != result, Exit );
	IOSleep ( 1 );
	
	waitForClocksStopped ();
	
	result = mPlatformObject->setDataWordSizes ( mDataWordSize );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	result = mPlatformObject->setI2SEnable ( TRUE );
	FailIf ( kIOReturnSuccess != result, Exit );
	IOSleep ( 1 );
	
	result = mPlatformObject->setI2SClockEnable ( TRUE );
	FailIf ( kIOReturnSuccess != result, Exit );
	IOSleep ( 1 );

	result = super::transportSetSampleWidth ( sampleDepth, dmaWidth );

Exit:	
	return result;
}

//	--------------------------------------------------------------------------------
//	Sets the I2S module to sleep state by stopping the clocks, disabling the I2S
//	I/O Module and releasing the clock source.
//
//	Sequence:	1.	Stop I2S Clocks
//				2.	Wait for I2S clocks stopped
//				3.	Disable the I2S I/O Module
//				4.	Release the source clock
//
IOReturn	I2STransportInterface::performTransportSleep ( void ) {
	IOReturn			result = kIOReturnError;

	debugIOLog (3,  "+ I2STransportInterface[%ld]::performTransportSleep ()", mInstanceIndex );
	
	FailIf ( NULL == mPlatformObject, Exit );
	
	//	Set the I2S cell to a clocks stopped mode so that the clocks
	//	stop low and then tristate.
	mPlatformObject->setI2SIOMIntControl ( 1 << kClocksStoppedPendingShift );	//	Clear the clock stop status
	result = mPlatformObject->setI2SClockEnable ( FALSE );
	FailIf ( kIOReturnSuccess != result, Exit );
	IOSleep ( 1 );

	waitForClocksStopped ();
	
	result = mPlatformObject->setI2SCellEnable ( FALSE );		
	FailIf ( kIOReturnSuccess != result, Exit );
	
	result = mPlatformObject->releaseI2SClockSource ( mClockSelector );

	releaseClockSources ();
Exit:	
	debugIOLog (3,  "- I2STransportInterface[%ld]::performTransportSleep () = %d", mInstanceIndex, (unsigned int)result );
	return result;
}

//	--------------------------------------------------------------------------------
//	Sets the I2S module to the active state by stopping the clocks, disabling the
//	I2S I/O Module and releasing the clock source.
//
IOReturn	I2STransportInterface::performTransportWake ( void ) {
	IOReturn			result = kIOReturnError;
	
	debugIOLog (3,  "+ I2STransportInterface[%ld]::performTransportWake ()", mInstanceIndex );

	requestClockSources ();
	
	result = mPlatformObject->setDataWordSizes ( mDataWordSize );
	FailIf (kIOReturnSuccess != result, Exit);

	result = mPlatformObject->setI2SCellEnable ( TRUE );		
	FailIf ( kIOReturnSuccess != result, Exit );

	result = transportSetSampleRate ( mTransportState.transportSampleRate );
	FailIf (kIOReturnSuccess != result, Exit);

	result = mPlatformObject->setI2SClockEnable ( TRUE );
	FailIf ( kIOReturnSuccess != result, Exit );

	debugIOLog (3,  "- I2STransportInterface[%ld]::performTransportWake () = %d", mInstanceIndex, (unsigned int)result );

Exit:
	return result;
}

//	------------------------------------------------------------------------------------------------------------------------------------
//	Switching bewteen a system mastered clock and an external clock, such as a recovered clock from an S/PDIF AES3 stream, requires a 
//	"BREAK BEFORE MAKE" sequence to avoid having two hardware drivers connected together.  If selecting the master clock then the 
//	external MUX must be disconnected prior to enabling the system master clock.  If selecting an external MUX clock source then the 
//	internal system master clock must be disconnected first.  Sequences are:
//
//	TRANSITION						CYCLE					ACTION
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
//	kTRANSPORT_MASTER_CLOCK to	|	1 Transport Break	|	Set MUX to alternate clock source, set I2S to SLAVE (BCLKMaster = SLAVE).
//	kTRANSPORT_SLAVE_CLOCK		|	2 Topaz Break		|	Stop CS84xx & mute TX.  Set all registers to act as a clock master.
//								|						|	A.	Data Flow Control Register:		TXD = 01, SPD = 10, SRCD = 0
//								|						|	B.	Clock Source Control Register:	OUTC = 1, INC = 0, RXD = 01
//								|						|	C.	Serial Input Control Register:	SIMS = 0
//								|						|	D.	Serial Output Control Register:	SOMS = 1
//								|	3 TAS3004 Break		|	No Action.
//								|	4 Transport Make	|	No Action.
//								|	5 Topaz Make		|	Start CS84xx.  Send request to restart transport hardware.
//								|	6 TAS3004 Make		|	Reset and flush register cache to hardware.
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
//	kTRANSPORT_SLAVE_CLOCK to	|	1 Transport Break	|	No Action.
//	kTRANSPORT_MASTER_CLOCK		|	2 Topaz Break		|	Stop CS84xx & disable TX.  Set all registers to act as a clock slave.
//								|						|	A.	Data Flow Control Register:		TXD = 01, SPD = 00, SRCD = 1
//								|						|	B.	Clock Source Control Register:	OUTC = 0, INC = 0, RXD = 01
//								|						|	C.	Serial Input Control Register:	SIMS = 0
//								|						|	D.	Serial Output Control Register:	SOMS = 0
//								|	3 TAS3004 Break		|	No Action.
//								|	4 Transport Make	|	Set MUX to default clock source, set I2S to SLAVE (BCLKMaster = MASTER).
//								|	5 Topaz Make		|	Start CS84xx & unmute TX.  Send request to restart transport hardware.
//								|						|	A.	Clear pending receiver errors.
//								|						|	B.	Enable receiver errors.
//								|						|	C.	Set CS8420 to RUN.
//								|						|	D.	Request a restart of the I2S transport.
//								|	6 TAS3004 Make		|	Reset and flush register cache to hardware.
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
IOReturn	I2STransportInterface::transportBreakClockSelect ( UInt32 clockSource ) {
	IOReturn	result = kIOReturnError;
	
	debugIOLog ( 5,  "+ I2STransportInterface[%ld]::transportBreakClockSelect ( %d )", mInstanceIndex, (unsigned int)clockSource );
	
	FailIf ( NULL == mPlatformObject, Exit );
	
	//	Update the super so that setting the sample rate will correctly manipulate
	//	the BCLKMaster control bit in the Serial Format Register
	super::transportBreakClockSelect ( clockSource );
	
	result = calculateSerialFormatRegisterValue ( mTransportState.transportSampleRate );
	FailIf ( kIOReturnSuccess != result, Exit );
	result = mPlatformObject->setSerialFormatRegister ( mSerialFormat );
	FailIf ( kIOReturnSuccess != result, Exit );

	result = mPlatformObject->setDataWordSizes ( mDataWordSize );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	//	Set the I2S cell to a clocks stopped mode so that the clocks
	//	stop low and then tristate.  This performs the 'break'!!!
	mPlatformObject->setI2SIOMIntControl ( 1 << kClocksStoppedPendingShift );	//	Clear the clock stop status
	result = mPlatformObject->setI2SClockEnable ( FALSE );

	FailIf ( kIOReturnSuccess != result, Exit );
	waitForClocksStopped ();
	

	switch ( clockSource ) {
		case kTRANSPORT_MASTER_CLOCK:
			break;
		case kTRANSPORT_SLAVE_CLOCK:
			//	Set the clock mux to the ALTERNATE clock source (i.e. externally generated).
			//	The alternate clock mux will not exist if running on a slave only transport.
			debugIOLog (5,  "  ... setting clock mux to ALTERNATE source" );
			if ( kTransportInterfaceType_I2S == mTransportState.transportInterfaceType ) {
				result = mPlatformObject->setClockMux ( kGPIO_MuxSelectAlternate );
				FailIf ( kIOReturnSuccess != result, Exit );
			} else {
				result = kIOReturnSuccess;
			}
			break;
		default:
			FailIf ( TRUE, Exit );
			break;
	}
	
Exit:
	debugIOLog ( 5,  "- I2STransportInterface[%ld]::transportBreakClockSelect ( %d ) = %X", mInstanceIndex, (unsigned int)clockSource, (unsigned int)result );
	return result;
}

//	------------------------------------------------------------------------------------------------------------------------------------
//	Switching bewteen a system mastered clock and an external clock, such as a recovered clock from an S/PDIF AES3 stream, requires a 
//	"BREAK BEFORE MAKE" sequence to avoid having two hardware drivers connected together.  If selecting the master clock then the 
//	external MUX must be disconnected prior to enabling the system master clock.  If selecting an external MUX clock source then the 
//	internal system master clock must be disconnected first.  Sequences are:
//
//	TRANSITION						CYCLE					ACTION
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
//	kTRANSPORT_MASTER_CLOCK to	|	1 Transport Break	|	Set MUX to alternate clock source, set I2S to SLAVE (BCLKMaster = SLAVE).
//	kTRANSPORT_SLAVE_CLOCK		|	2 Topaz Break		|	Stop CS84xx & mute TX.  Set all registers to act as a clock master.
//								|						|	A.	Data Flow Control Register:		TXD = 01, SPD = 10, SRCD = 0
//								|						|	B.	Clock Source Control Register:	OUTC = 1, INC = 0, RXD = 01
//								|						|	C.	Serial Input Control Register:	SIMS = 0
//								|						|	D.	Serial Output Control Register:	SOMS = 1
//								|	3 TAS3004 Break		|	No Action.
//								|	4 Transport Make	|	No Action.
//								|	5 Topaz Make		|	Start CS84xx.  Send request to restart transport hardware.
//								|	6 TAS3004 Make		|	Reset and flush register cache to hardware.
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
//	kTRANSPORT_SLAVE_CLOCK to	|	1 Transport Break	|	No Action.
//	kTRANSPORT_MASTER_CLOCK		|	2 Topaz Break		|	Stop CS84xx & disable TX.  Set all registers to act as a clock slave.
//								|						|	A.	Data Flow Control Register:		TXD = 01, SPD = 00, SRCD = 1
//								|						|	B.	Clock Source Control Register:	OUTC = 0, INC = 0, RXD = 01
//								|						|	C.	Serial Input Control Register:	SIMS = 0
//								|						|	D.	Serial Output Control Register:	SOMS = 0
//								|	3 TAS3004 Break		|	No Action.
//								|	4 Transport Make	|	Set MUX to default clock source, set I2S to SLAVE (BCLKMaster = MASTER).
//								|	5 Topaz Make		|	Start CS84xx & unmute TX.  Send request to restart transport hardware.
//								|						|	A.	Clear pending receiver errors.
//								|						|	B.	Enable receiver errors.
//								|						|	C.	Set CS8420 to RUN.
//								|						|	D.	Request a restart of the I2S transport.
//								|	6 TAS3004 Make		|	Reset and flush register cache to hardware.
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
IOReturn	I2STransportInterface::transportMakeClockSelect ( UInt32 clockSource ) {
	IOReturn	result = kIOReturnError;
	
	debugIOLog ( 5,  "+ I2STransportInterface[%ld]::transportMakeClockSelect ( %d )", mInstanceIndex, (unsigned int)clockSource );
	
	FailIf ( NULL == mPlatformObject, Exit );
	
	switch ( clockSource ) {
		case kTRANSPORT_MASTER_CLOCK:
			//	Set the clock mux to the DEFAULT clock source (i.e. system generated)
			debugIOLog (5,  "  ... setting clock mux to DEFAULT source" );
			if ( kTransportInterfaceType_I2S == mTransportState.transportInterfaceType ) {
				result = mPlatformObject->setClockMux ( kGPIO_MuxSelectDefault );
				FailIf ( kIOReturnSuccess != result, Exit );
			} else {
				result = kIOReturnSuccess;
			}
			//	Stop the cell and restart the cell to get I2S running
			debugIOLog (7,  "  ... setting kBClkMasterShift to MASTER" );
			break;
		case kTRANSPORT_SLAVE_CLOCK:
			debugIOLog (7,  "  ... kTRANSPORT_SLAVE_CLOCK requires no action" );
			result = kIOReturnSuccess;
			break;
		default:
			FailIf ( TRUE, Exit );
			break;
	}

Exit:
	result = mPlatformObject->setI2SClockEnable ( TRUE );
	
	debugIOLog ( 5,  "- I2STransportInterface[%ld]::transportMakeClockSelect ( %d ) = %X", mInstanceIndex, (unsigned int)clockSource, (unsigned int)result );
	return result;
}


//	--------------------------------------------------------------------------------
UInt32		I2STransportInterface::transportGetSampleRate ( void ) {
	UInt32			curSerialFormat;
	UInt32			result = 0;
	
	if ( kTRANSPORT_SLAVE_CLOCK == mTransportState.clockSource ) {
		curSerialFormat = mPlatformObject->getSerialFormatRegister ();
		curSerialFormat &= 0x00000FFF;
		if ( ( kSampleRate_8Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_8Khz_UpperLimt ) ) {
			result = 8000;
		} else if ( ( kSampleRate_16Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_16Khz_UpperLimt ) ) {
			result = 11025;
		} else if ( ( kSampleRate_16Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_16Khz_UpperLimt ) ) {
			result = 16000;
		} else if ( ( kSampleRate_22Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_22Khz_UpperLimt ) ) {
			result = 22050;
		} else if ( ( kSampleRate_24Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_24Khz_UpperLimt ) ) {
			result = 24000;
		} else if ( ( kSampleRate_32Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_32Khz_UpperLimt ) ) {
			result = 32000;
		} else if ( ( kSampleRate_44Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_44Khz_UpperLimt ) ) {
			result = 44100;
		} else if ( ( kSampleRate_48Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_48Khz_UpperLimt ) ) {
			result = 48000;
		} else if ( ( kSampleRate_64Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_64Khz_UpperLimt ) ) {
			result = 64000;
		} else if ( ( kSampleRate_88Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_88Khz_UpperLimt ) ) {
			result = 88200;
		} else if ( ( kSampleRate_96Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_96Khz_UpperLimt ) ) {
			result = 96000;
		} else if ( ( kSampleRate_176Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_176Khz_UpperLimt ) ) {
			result = 176400;
		} else if ( ( kSampleRate_192Khz_LowerLimt <= curSerialFormat ) && ( curSerialFormat <= kSampleRate_192Khz_UpperLimt ) ) {
			result = 19200;
		} else {
			//	The rate is not in the standard set so return the rate verbatim.  The
			//	number of counts in the least sigificant 12 bits of the serial format
			//	register is the number of 18.4320 Mhz non-synchronized clock cycles that 
			//	were counted during a single sample subframe.  That is 54.250 nanoseconds
			//	per clock.  Since the 18.4320 Mhz clock is not synchronized, these rates
			//	are only approximate so only integer math is being used here to generate
			//	the sample rate approximation.
			curSerialFormat *= 5425;
			curSerialFormat = 100000000 / curSerialFormat;
			result = curSerialFormat * 1000;
		}
	} else {
		result = super::transportGetSampleRate ();
	}
	return result;
}


//	--------------------------------------------------------------------------------
IOReturn	I2STransportInterface::transportSetPeakLevel ( UInt32 channelTarget, UInt32 levelMeterValue ) {
	IOReturn		result;
	
	debugIOLog (7,  "+ I2STransportInterface::transportSetPeakLevel ( '%4s', %lX )", (char*)&channelTarget, levelMeterValue );
	switch ( channelTarget ) {
		case kStreamFrontLeft:
		case kStreamFrontRight:		result = mPlatformObject->setPeakLevel ( channelTarget, levelMeterValue );			break;
		default:					result = kIOReturnBadArgument;														break;
	}
	debugIOLog (7,  "- I2STransportInterface::transportSetPeakLevel ( '%4s', %lX ) returns %X", (char*)&channelTarget, levelMeterValue, result );
	return result;
}


//	--------------------------------------------------------------------------------
UInt32		I2STransportInterface::transportGetPeakLevel ( UInt32 channelTarget ) {
	UInt32			result = 0;

	debugIOLog (7,  "+ I2STransportInterface::transportGetPeakLevel ( '%4s' )", (char*)&channelTarget );
	switch ( channelTarget ) {
		case kStreamFrontLeft:
		case kStreamFrontRight:
			result = mPlatformObject->getPeakLevel ( channelTarget );
			//	If there was a new peak level stored then the meter needs to be set
			//	back into run mode from hold mode.
			if ( ( 1 << kNewPeakInShift ) & result ) {
				mPlatformObject->setPeakLevel ( channelTarget, ( 1 << kNewPeakInShift ) | ( 0 << kHoldPeakInShift ) );
				mPlatformObject->setPeakLevel ( channelTarget, ( 0 << kNewPeakInShift ) | ( 0 << kHoldPeakInShift ) );
				mPlatformObject->setPeakLevel ( channelTarget, ( 0 << kNewPeakInShift ) | ( 1 << kHoldPeakInShift ) );
			}
			result &= kPeakValueMask;
			break;
	}
	
	debugIOLog (7,  "- I2STransportInterface::transportGetPeakLevel ( '%4s' ) returns %lX", (char*)&channelTarget, result );
	return result;
}

#pragma mark #--------------------
#pragma mark # PRIVATE METHODS
#pragma mark #--------------------


//	--------------------------------------------------------------------------------
IOReturn I2STransportInterface::requestClockSources () {
	IOReturn	result = kIOReturnError;

	debugIOLog ( 3, "+ I2STransportInterface[%ld]::requestClockSources ()", mInstanceIndex );
	//	Audio needs all three clocks in order to support setting sample 
	//	rates and/or clock sources.
	if (FALSE == mHave45MHzClock) {
		result = mPlatformObject->requestI2SClockSource (kI2S_45MHz);
		if (kIOReturnSuccess != result) {
			debugIOLog ( 3, "  FAILED to acquire 45.1584 MHz clock.");
		} else {
			mHave45MHzClock = TRUE;
		}
	}
	if (FALSE == mHave49MHzClock) {
		result = mPlatformObject->requestI2SClockSource (kI2S_49MHz);
		if (kIOReturnSuccess != result) {
			debugIOLog ( 3, "  FAILED to acquire 49.1520 MHz clock.");
		} else {
			mHave49MHzClock = TRUE;
		}
	}
	if (FALSE == mHave18MHzClock) {
		result = mPlatformObject->requestI2SClockSource (kI2S_18MHz);
		if (kIOReturnSuccess != result) {
			debugIOLog ( 3, "  FAILED to acquire 18.4320 MHz clock.");
		} else {
			mHave18MHzClock = TRUE;
		}
	}
	if (mHave18MHzClock && mHave45MHzClock && mHave49MHzClock) {
		result = kIOReturnSuccess;
	}

	debugIOLog ( 3, "- I2STransportInterface[%ld]::requestClockSources () returns 0x%0.8X", mInstanceIndex, result );
	return result;
}


//	--------------------------------------------------------------------------------
IOReturn I2STransportInterface::releaseClockSources () {
	IOReturn	result = kIOReturnError;

	debugIOLog ( 3, "+ I2STransportInterface[%ld]::releaseClockSources ()", mInstanceIndex );
	//	Audio needs all three clocks in order to support setting sample 
	//	rates and/or clock sources.
	
	if (TRUE == mHave45MHzClock) {
		result = mPlatformObject->releaseI2SClockSource (kI2S_45MHz);
		if (kIOReturnSuccess != result) {
			debugIOLog ( 3, "  FAILED to release 45.1584 MHz clock.");
		} else {
			mHave45MHzClock = FALSE;
		}
	}
	if (TRUE == mHave49MHzClock) {
		result = mPlatformObject->releaseI2SClockSource (kI2S_49MHz);
		if (kIOReturnSuccess != result) {
			debugIOLog ( 3, "  FAILED to release 49.1520 MHz clock.");
		} else {
			mHave49MHzClock = FALSE;
		}
	}
	if (TRUE == mHave18MHzClock) {
		result = mPlatformObject->releaseI2SClockSource (kI2S_18MHz);
		if (kIOReturnSuccess != result) {
			debugIOLog ( 3, "  FAILED to release 18.4320 MHz clock.");
		} else {
			mHave18MHzClock = FALSE;
		}
	}

	result = kIOReturnSuccess;
	debugIOLog ( 3, "- I2STransportInterface[%ld]::releaseClockSources () returns 0x%0.8X", mInstanceIndex, result );
	return result;
}


//	--------------------------------------------------------------------------------
IOReturn		I2STransportInterface::calculateSerialFormatRegisterValue ( UInt32 sampleRate ) {
	IOReturn	result = kIOReturnError;
	bool		validClockSource = FALSE;
	
	FailIf ( NULL == mPlatformObject, Exit );
	
	mMClkFrequency = sampleRate * mMclkMultiplier;
	mSClkFrequency = sampleRate * mSclkMultiplier;
	
	//	The sample rate can be derived from 45.1584 MHz, 49.1520 MHz or 18.432 Mhz.
	//	The clock that the sample rate is derived from must be an integral multiple
	//	of the sample rate.  Some common sample rates are setup as follows:
	//
	//	SAMPLE RATE		CLOCK SOURCE	MCLK DIVISOR	SCLK DIVISOR	CODEC		MCLK RATIO
	//
	//	 32.000 Khz		49.1520 Mhz			6				4			TAS3004		256 * fs
	//	 44.100 Khz		45.1584 Mhz			4				4			TAS3004		256 * fs
	//	 48.000 Khz		49.1520 Mhz			4				4			TAS3004		256 * fs
	//	 96.000 Khz		49.1520 Mhz			2				4			PCM3052		256 * fs
	//	192.000 Khz		49.1520 Mhz			1				4			PCM3052		256 * fs
	//
	if ( 0 == ( kClock49MHz % mMClkFrequency ) ) {
		mClockSourceFrequency = kClock49MHz;
		mClockSelector = kI2S_49MHz;
		mSerialFormat = ( kClockSource49MHz << kClockSourceShift );
		validClockSource = TRUE;
	} else if ( 0 == ( kClock45MHz % mMClkFrequency ) ) {
		mClockSourceFrequency = kClock45MHz;
		mClockSelector = kI2S_45MHz;
		mSerialFormat = ( kClockSource45MHz << kClockSourceShift );
		validClockSource = TRUE;
	} else if ( 0 == ( kClock18MHz % mMClkFrequency ) ) {
		mClockSourceFrequency = kClock18MHz;
		mClockSelector = kI2S_18MHz;
		mSerialFormat = ( kClockSource18MHz << kClockSourceShift );
		validClockSource = TRUE;
	} else {
		//	The sample rate cannot be derived from available clock sources
		debugIOLog ( 1, "  I2STransportInterface[%ld] UNABLE TO DERIVE SAMPLE RATE %ld (i.e. rate * N[...] � any clock)", mInstanceIndex, sampleRate);
	}
	if ( validClockSource ) {		//  [3686032]   dont assert on external clock when clock is missing
		debugIOLog ( 6, "  I2STransportInterface[%ld]::calculateSerialFormatRegisterValue: mClockSourceFrequency = %ld", mInstanceIndex, mClockSourceFrequency);

		//	The BCLkMaster bit is set according to the clock source (i.e. system mastered
		//	clock or externally mastered clock such as a recovered clock from a S/PDIF AES3 stream.
		if ( kTRANSPORT_MASTER_CLOCK == mTransportState.clockSource ) {
			mSerialFormat |= ( kSClkMaster << kBClkMasterShift );
		} else if ( kTRANSPORT_SLAVE_CLOCK == mTransportState.clockSource ) {
			mSerialFormat |= ( kSClkSlave << kBClkMasterShift );
		} else {
			FailIf ( true, Exit );
		}
		//	The I2S I/O Module has exceptions for register configurations based on the clock 
		//	divisor which must be resolved to determine register confuration MCLK values.
		mMClkDivisor = mClockSourceFrequency / mMClkFrequency;
		debugIOLog ( 6, "  I2STransportInterface[%ld]::calculateSerialFormatRegisterValue: mMClkDivisor = %ld", mInstanceIndex, mMClkDivisor);
		switch ( mMClkDivisor ) {
			// exception cases require decimal divider constants (not hex!)
			case 1:			mMClkDivider = 14;											break;
			case 3:			mMClkDivider = 13;											break;
			case 5:			mMClkDivider = 12;											break;
			default:		mMClkDivider = ( ( mMClkDivisor / 2 ) - 1 );				break;
		}
		mSerialFormat |= ( mMClkDivider << kMClkDivisorShift );
		//	The I2S I/O Module has exceptions for register configurations based on the clock 
		//	divisor which must be resolved to determine register confuration SCLK values.
		mSClkDivisor = ( mClockSourceFrequency / mMClkDivisor ) / mSClkFrequency;
			debugIOLog ( 6, "  I2STransportInterface[%ld]::calculateSerialFormatRegisterValue: mSClkDivisor = %ld", mInstanceIndex, mSClkDivisor);
		switch ( mSClkDivisor ) {
			// exception cases require decimal divider constants (not hex!)
			case 1:			mSClkDivider = 8;											break;
			case 3:			mSClkDivider = 9;											break;
			default:		mSClkDivider = ( ( mSClkDivisor / 2 ) - 1 );				break;
		}
		mSerialFormat |= ( mSClkDivider << kSClkDivisorShift );
		switch ( mSclkMultiplier ) {
			case 32:	mSerialFormat |= ( kSerialFormat32x << kSerialFormatShift );	break;
			case 64:	mSerialFormat |= ( kSerialFormat64x << kSerialFormatShift );	break;
			default:	FailIf ( true, Exit );											break;
		}
		result = kIOReturnSuccess;
	}
Exit:	
	debugIOLog ( 5, "� I2STransportInterface[%ld]::calculateSerialFormatRegisterValue: sampleRate = %ld, mMClkFrequency = %ld, mSClkFrequency = %ld, result = 0x%0.8X", mInstanceIndex, sampleRate, mMClkFrequency, mSClkFrequency, result );
	return result;
}


//	--------------------------------------------------------------------------------
void		I2STransportInterface::waitForClocksStopped ( void ) {
	UInt32		timeOut = 100;
	
	while ( ( ( mPlatformObject->getI2SIOMIntControl () & ( 1 << kClocksStoppedPendingShift ) ) == 0 ) && timeOut ) {
		IODelay ( 10 );
		timeOut--;
	}
	mPlatformObject->setI2SIOMIntControl ( 1 << kClocksStoppedPendingShift );	//	Clear the clock stop status
}


#pragma mark #--------------------
#pragma mark # USER CLIENT
#pragma mark #--------------------

//	--------------------------------------------------------------------------------
IOReturn	I2STransportInterface::getTransportInterfaceState ( TransportStateStructPtr outState ) {
	IOReturn		result;
	
	result = super::getTransportInterfaceState ( outState );
	if ( NULL != outState && kIOReturnSuccess == result ) {
		((TransportStateStructPtr)outState)->instanceState[0] = mMclkMultiplier;
		((TransportStateStructPtr)outState)->instanceState[1] = mSclkMultiplier;
		((TransportStateStructPtr)outState)->instanceState[2] = mMClkFrequency;
		((TransportStateStructPtr)outState)->instanceState[3] = mSClkFrequency;
		((TransportStateStructPtr)outState)->instanceState[4] = mClockSourceFrequency;
		((TransportStateStructPtr)outState)->instanceState[5] = mMClkDivisor;
		((TransportStateStructPtr)outState)->instanceState[6] = mSClkDivisor;
		((TransportStateStructPtr)outState)->instanceState[7] = mMClkDivider;
		((TransportStateStructPtr)outState)->instanceState[8] = mSClkDivider;
		((TransportStateStructPtr)outState)->instanceState[9] = mSerialFormat;
		((TransportStateStructPtr)outState)->instanceState[10] = mDataWordSize;
		((TransportStateStructPtr)outState)->instanceState[11] = mClockSelector;
		((TransportStateStructPtr)outState)->instanceState[12] = (UInt32)mHave45MHzClock;
		((TransportStateStructPtr)outState)->instanceState[13] = (UInt32)mHave49MHzClock;
		((TransportStateStructPtr)outState)->instanceState[14] = (UInt32)mHave18MHzClock;
		((TransportStateStructPtr)outState)->instanceState[15] = transportGetPeakLevel ( kStreamFrontLeft );
		((TransportStateStructPtr)outState)->instanceState[16] = transportGetPeakLevel ( kStreamFrontRight );
	}
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn	I2STransportInterface::setTransportInterfaceState ( TransportStateStructPtr inState ) {
	return super::setTransportInterfaceState ( inState );
}


//	--------------------------------------------------------------------------------
//	[3514709]	Attempt detection and recovery from I2S errors here.  NOTE:  This
//				code should be handled by a hardware interrupt dispatch through the
//				I2S driver.
void		I2STransportInterface::poll ( void ) {
	UInt32			ackData = 0;
	
	if ( NULL != mPlatformObject ) {
		UInt32 errorStatus = mPlatformObject->getI2SIOMIntControl();
		if ( 0 != ( ( 1 << kFrameCountPendingShift ) & errorStatus ) ) {
			//	The 'Frame Count' has matched the value of the 'Frame Count To Match'.
			ackData |= ( 1 << kFrameCountPendingShift );
		}
		if ( 0 != ( ( 1 << kMsgFlagPendingShift ) & errorStatus ) ) {
			//	An incoming codec message has been recieved (should not happen with I2S mode).
			ackData |= ( 1 << kMsgFlagPendingShift );
		}
		if ( 0 != ( ( 1 << kNewPeakPendingShift ) & errorStatus ) ) {
			//	Incoming codec data word has exceeded the previous peak.
			ackData |= ( 1 << kNewPeakPendingShift );
		}
		if ( 0 != ( ( 1 << kClocksStoppedPendingShift ) & errorStatus ) ) {
			//	I2S clocks have been shutdown correctly.
			ackData |= ( 1 << kClocksStoppedPendingShift );
		}
		if ( 0 != ( ( 1 << kExtSyncErrorPendingShift ) & errorStatus ) ) {
			//	An 'LRCLK' edge was expected but did not occur.  This error should be
			//	expected when transitioning between master and slave modes (see I2S serial
			//	format register 'BclkMaster' bit description).
			ackData |= ( 1 << kExtSyncErrorPendingShift );
		}
		if ( 0 != ( ( 1 << kExtSyncOKPendingShift ) & errorStatus ) ) {
			//	Valid only in slave mode, indicates that the I2S IOM has locked to
			//	the external 'LRCLK'.
			ackData |= ( 1 << kExtSyncOKPendingShift );
		}
		if ( 0 != ( ( 1 << kNewSampleRatePendingShift ) & errorStatus ) ) {
			//	Valid only in slave mode and when I2S IOM has locked to an external clock,
			//	indicates that the sample rate has changed (NOTE: highly inaccurate as the
			//	sample rate is derived from the number of 18 mHz clocks that occur in a 
			//	single sample frame).
			ackData |= ( 1 << kNewSampleRatePendingShift );
		}
		if ( 0 != ( ( 1 << kStatusFlagPendingShift ) & errorStatus ) ) {
			//	Valid only in DAV audio mode (DAV audio transports control/status information
			//	from register I/O simultaneous to audio samples from DMA I/O -- all within
			//	a single audio frame).  Indicates that DAV status is available for register
			//	access.
			ackData |= ( 1 << kStatusFlagPendingShift );
		}
		if ( 0 != ackData ) {
			mPlatformObject->setI2SIOMIntControl( ackData );
		}
	}
}





