/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 */

#ifndef _IOPLATFORMFUNCTIONDRIVER_H
#define _IOPLATFORMFUNCTIONDRIVER_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

#include "IOPlatformFunction.h"

/*!
    @class IOPlatformFunctionDriver
 */
class IOPlatformFunctionDriver : public IOService
{
    OSDeclareDefaultStructors(IOPlatformFunctionDriver)	

private:
	const OSSymbol *instantiateFunctionSymbol;

protected:
	IOReturn instantiatePlatformFunctions (IOService *nub, OSArray **pfArray);

public:

    virtual bool start(IOService *nub);
    virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
                                        void *param1, void *param2,
                                        void *param3, void *param4);

};
#endif 	// _IOPLATFORMFUNCTIONDRIVER_H