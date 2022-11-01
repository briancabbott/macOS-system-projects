/*
 * Copyright (c) 1998-2014 Apple Computer, Inc. All rights reserved.
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

#include "IOAudioDebug.h"
#include "IOAudioEngine.h"
#include "IOAudioStream.h"
#include "IOAudioTypes.h"

#include <vecLib/vecLib.h>																							// <rdar://14058728>

IOReturn IOAudioEngine::mixOutputSamples(const void *sourceBuf, void *mixBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	IOReturn result = kIOReturnBadArgument;
	
    if (sourceBuf && mixBuf) 
	{
		const float * floatSource1Buf;
		const float * floatSource2Buf;
		float * floatMixBuf;
		
        UInt32 numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
		
		__darwin_size_t numSamps = numSamplesLeft;
 		
		floatMixBuf = &(((float *)mixBuf)[firstSampleFrame * streamFormat->fNumChannels]);
		floatSource2Buf = floatMixBuf;
		floatSource1Buf = (const float *)sourceBuf;
		
		__darwin_ptrdiff_t strideOne=1, strideTwo=1, resultStride=1;
		
		vDSP_vadd(floatSource1Buf, strideOne, floatSource2Buf, strideTwo, floatMixBuf, resultStride, numSamps);
		
        result = kIOReturnSuccess;
    }
    
    return result;
}
