/*
 *  IOFWLocalIsochPort.cpp
 *  IOFireWireFamily
 *
 *  Created by Niels on Tue Mar 25 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$ Log:IOFWLocalIsochPort.cpp,v $
 */

#import "IOFWLocalIsochPort.h"
#import "IOFWDCLProgram.h"
#import "IOFireWireController.h"
#import "FWDebugging.h"

OSDefineMetaClassAndStructors(IOFWLocalIsochPort, IOFWIsochPort)
OSMetaClassDefineReservedUnused(IOFWLocalIsochPort, 0);
OSMetaClassDefineReservedUnused(IOFWLocalIsochPort, 1);

bool 
IOFWLocalIsochPort::init (
		IODCLProgram *			program, 
		IOFireWireController *	control)
{
	DebugLog("IOFWLocalIsochPort<%p>::init - program = %p\n", this, program );
	
	if ( ! IOFWIsochPort::init( ) )
		return false;
		
	fProgram = program; // belongs to us.
	
	fControl = control;

	return true;
}

void IOFWLocalIsochPort::free()
{
	if ( fProgram )
	{
		fProgram->stop() ;

		fProgram->release () ;
		fProgram = NULL ;
	}
	
	IOFWIsochPort::free();
}

// Return maximum speed and channels supported
// (bit n set = chan n supported)
IOReturn IOFWLocalIsochPort::getSupported(IOFWSpeed &maxSpeed, UInt64 &chanSupported)
{
	maxSpeed = kFWSpeedMaximum;
	chanSupported = ~(UInt64)0;
	return kIOReturnSuccess;
}

/*
 * Allocate hardware resources for port, via workloop
 * Then compile program, not on workloop.
 */
IOReturn IOFWLocalIsochPort::allocatePort(IOFWSpeed speed, UInt32 chan)
{
	IOReturn res;

	res = fProgram->allocateHW(speed, chan);
	
	if(kIOReturnSuccess != res)
		return res; 
		
	return fProgram->compile(speed, chan);	// Not on workloop
}

IOReturn IOFWLocalIsochPort::releasePort()
{
	IOReturn res;

//	fControl->closeGate();
	res = fProgram->releaseHW();
//	fControl->openGate();
	return res;
}

IOReturn IOFWLocalIsochPort::start()
{
	IOReturn res;

//	fControl->closeGate();
	res = fProgram->start();
//	fControl->openGate();
	return res;
}

IOReturn IOFWLocalIsochPort::stop()
{
//	fControl->closeGate();
	fProgram->stop();
//	fControl->openGate();
	return kIOReturnSuccess;
}

IOReturn IOFWLocalIsochPort::notify( IOFWDCLNotificationType notificationType,
	DCLCommand** dclCommandList, UInt32 numDCLCommands)
{
	IOReturn res;
	
	res = fProgram->notify(notificationType,
								dclCommandList, numDCLCommands);
	return res;
}

void
IOFWLocalIsochPort::printDCLProgram (
	const DCLCommand *		dcl,
	UInt32					count,
	void (*printFN)( const char *format, ...),
	unsigned 				lineDelayMS ) 
{
	const DCLCommand *	currentDCL = dcl ;
	UInt32 				index		= 0 ;
	
//	if( printFN == NULL )
//	{
//		// nothing to print if there's no printer function
//		return;
//	}
		
	DebugLog("printDCLProgram program %p, count %x\n", currentDCL, count) ;
	
	while ( ( count == 0 || index < count ) && currentDCL )
	{
		DebugLog("#%x  @%p   next=%p, cmplrData=%p, op=%u ",
			index, 
			currentDCL,
			currentDCL->pNextDCLCommand,
			currentDCL->compilerData,
			(int) currentDCL->opcode) ;

		switch(currentDCL->opcode & ~kFWDCLOpFlagMask)
		{
			case kDCLSendPacketStartOp:
			//case kDCLSendPacketWithHeaderStartOp:
			case kDCLSendPacketOp:
			case kDCLReceivePacketStartOp:
			case kDCLReceivePacketOp:
				DebugLog("(DCLTransferPacket) buffer=%p, size=%d",
					((DCLTransferPacket*)currentDCL)->buffer,
					((DCLTransferPacket*)currentDCL)->size) ;
				break ;
				
			case kDCLSendBufferOp:
			case kDCLReceiveBufferOp:
				DebugLog("(DCLTransferBuffer) buffer=%p, size=%d, packetSize=%d, bufferOffset=0x%08x",
					((DCLTransferBuffer*)currentDCL)->buffer,
					((DCLTransferBuffer*)currentDCL)->size,
					((DCLTransferBuffer*)currentDCL)->packetSize,
					(UInt32)((DCLTransferBuffer*)currentDCL)->bufferOffset) ;
				break ;
	
			case kDCLCallProcOp:
				DebugLog("(DCLCallProc) proc=%p, procData=%p",
					((DCLCallProc*)currentDCL)->proc,
					((DCLCallProc*)currentDCL)->procData ) ;
				break ;
				
			case kDCLLabelOp:
				DebugLog("(DCLLabel)") ;
				break ;
				
			case kDCLJumpOp:
				DebugLog("(DCLJump) pJumpDCLLabel=%p",
					((DCLJump*)currentDCL)->pJumpDCLLabel) ;
				break ;
				
			case kDCLSetTagSyncBitsOp:
				DebugLog("(DCLSetTagSyncBits) tagBits=0x%04x, syncBits=0x%04x",
					((DCLSetTagSyncBits*)currentDCL)->tagBits,
					((DCLSetTagSyncBits*)currentDCL)->syncBits) ;
				break ;
				
			case kDCLUpdateDCLListOp:
				DebugLog("(DCLUpdateDCLList) dclCommandList=%p, numDCLCommands=%d\n",
					((DCLUpdateDCLList*)currentDCL)->dclCommandList,
					((DCLUpdateDCLList*)currentDCL)->numDCLCommands) ;

				for(UInt32 listIndex=0; listIndex < ((DCLUpdateDCLList*)currentDCL)->numDCLCommands; ++listIndex)
				{
					DebugLog("%p ", (((DCLUpdateDCLList*)currentDCL)->dclCommandList)[listIndex]) ;
					if ( listIndex % 10 == 0 )
						DebugLog("\n") ;
					IOSleep(8) ;
				}
				
				DebugLog("\n") ;
				break ;
	
			case kDCLPtrTimeStampOp:
				DebugLog("(DCLPtrTimeStamp) timeStampPtr=%p",
					((DCLPtrTimeStamp*)currentDCL)->timeStampPtr) ;
				break ;
				
			case kDCLSkipCycleOp:
				DebugLog("(DCLSkipCycleOp)") ;
				break ;
			
			case kDCLNuDCLLeaderOp:
				DebugLog("(DCLNuDCLLeaderOp) DCL pool=%p", ((DCLNuDCLLeader*)currentDCL)->program ) ;
				break ;
		}
		
		DebugLog("\n") ;
		
		currentDCL = currentDCL->pNextDCLCommand ;
		++index ;
		
		if ( lineDelayMS )
		{
			IOSleep( lineDelayMS ) ;
		}
	}

	if ( count > 0 )
	{
		if ( index != count )
			DebugLog("unexpected end of program\n") ;
		
		if ( currentDCL != NULL )
			DebugLog("program too long for count\n") ;
	}
}

IOReturn
IOFWLocalIsochPort::setIsochResourceFlags (
	IOFWIsochResourceFlags		flags )
{
	fProgram->setIsochResourceFlags( flags ) ;
	return kIOReturnSuccess ;
}

IODCLProgram *
IOFWLocalIsochPort::getProgramRef() const
{
	fProgram->retain() ;
	return fProgram ;
}

IOReturn
IOFWLocalIsochPort::synchronizeWithIO()
{
	return fProgram->synchronizeWithIO() ;
}
