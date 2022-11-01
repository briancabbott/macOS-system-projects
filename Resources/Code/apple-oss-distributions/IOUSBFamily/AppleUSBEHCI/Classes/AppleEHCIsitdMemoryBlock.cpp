/*
 * Copyright � 1998-2012 Apple Inc.  All rights reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/usb/IOUSBLog.h>

#include "AppleEHCIsitdMemoryBlock.h"

#define super OSObject
OSDefineMetaClassAndStructors(AppleEHCIsitdMemoryBlock, OSObject);

AppleEHCIsitdMemoryBlock*
AppleEHCIsitdMemoryBlock::NewMemoryBlock(void)
{
    AppleEHCIsitdMemoryBlock 	*me = new AppleEHCIsitdMemoryBlock;
    IOByteCount					len;
	IODMACommand				*dmaCommand = NULL;
	UInt64						offset = 0;
	IODMACommand::Segment32		segments;
	UInt32						numSegments = 1;
	IOReturn					status = kIOReturnSuccess;
    
    if (me)
	{
		// Use IODMACommand to get the physical address
		dmaCommand = IODMACommand::withSpecification(kIODMACommandOutputHost32, 32, PAGE_SIZE, (IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly));
		if (!dmaCommand)
		{
			USBError(1, "AppleEHCIsitdMemoryBlock::NewMemoryBlock - could not create IODMACommand");
			return NULL;
		}
		USBLog(6, "AppleEHCIsitdMemoryBlock::NewMemoryBlock - got IODMACommand %p", dmaCommand);
		
		// allocate one page on a page boundary below the 4GB line
		me->_buffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIOMemoryUnshared | kIODirectionInOut, kEHCIPageSize, kEHCIStructureAllocationPhysicalMask);
		
		// allocate exactly one physical page
		if (me->_buffer) 
		{
			status = me->_buffer->prepare();
			if (status)
			{
				USBError(1, "AppleEHCIsitdMemoryBlock::NewMemoryBlock - could not prepare buffer");
				me->_buffer->release();
				me->_buffer = NULL;
				me->release();
				dmaCommand->release();
				return NULL;
			}
			me->_sharedLogical = (EHCISplitIsochTransferDescriptorSharedPtr)me->_buffer->getBytesNoCopy();
			bzero(me->_sharedLogical, kEHCIPageSize);
			status = dmaCommand->setMemoryDescriptor(me->_buffer);
			if (status)
			{
				USBError(1, "AppleEHCIsitdMemoryBlock::NewMemoryBlock - could not set memory descriptor");
				me->_buffer->complete();
				me->_buffer->release();
				me->_buffer = NULL;
				me->release();
				dmaCommand->release();
				return NULL;
			}
			status = dmaCommand->gen32IOVMSegments(&offset, &segments, &numSegments);
			dmaCommand->clearMemoryDescriptor();
			dmaCommand->release();
			if (status || (numSegments != 1) || (segments.fLength != kEHCIPageSize))
			{
				USBError(1, "AppleEHCIsitdMemoryBlock::NewMemoryBlock - could not get physical segment");
				me->_buffer->complete();
				me->_buffer->release();
				me->_buffer = NULL;
				me->release();
				return NULL;
			}
			me->_sharedPhysical = segments.fIOVMAddr;
		}
		else
		{
			USBError(1, "AppleEHCIsitdMemoryBlock::NewMemoryBlock, could not allocate buffer!");
			me->release();
			me = NULL;
		}
	}
	else
	{
		USBError(1, "AppleEHCIsitdMemoryBlock::NewMemoryBlock, constructor failed!");
    }
    return me;
}


UInt32
AppleEHCIsitdMemoryBlock::NumTDs(void)
{
    return SITDsPerBlock;
}



IOPhysicalAddress				
AppleEHCIsitdMemoryBlock::GetPhysicalPtr(UInt32 index)
{
    IOPhysicalAddress		ret = NULL;
	
    if (index < SITDsPerBlock)
		ret = _sharedPhysical + (index * sizeof(EHCISplitIsochTransferDescriptorShared));
	
    return ret;
}


EHCISplitIsochTransferDescriptorSharedPtr
AppleEHCIsitdMemoryBlock::GetLogicalPtr(UInt32 index)
{
    EHCISplitIsochTransferDescriptorSharedPtr ret = NULL;
	
    if (index < SITDsPerBlock)
		ret = &_sharedLogical[index];
	
    return ret;
}


AppleEHCIsitdMemoryBlock*
AppleEHCIsitdMemoryBlock::GetNextBlock(void)
{
    return _nextBlock;
}



void
AppleEHCIsitdMemoryBlock::SetNextBlock(AppleEHCIsitdMemoryBlock* next)
{
    _nextBlock = next;
}


void
AppleEHCIsitdMemoryBlock::free()
{
	if (_buffer)
	{
		_buffer->complete();
		_buffer->release();
	}
	super::free();
}

