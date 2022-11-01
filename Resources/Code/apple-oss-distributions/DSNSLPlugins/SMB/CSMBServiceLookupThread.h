/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 
/*!
 *  @header CSMBServiceLookupThread
 */
#ifndef _CSMBServiceLookupThread_
#define _CSMBServiceLookupThread_ 1

#include "CNSLServiceLookupThread.h"

class CSMBServiceLookupThread : public CNSLServiceLookupThread
{
public:
                            CSMBServiceLookupThread		( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep, CFArrayRef lmbListRef, const char* winsServer );
    virtual					~CSMBServiceLookupThread	();
        
	virtual void*			Run							( void );
protected:
			void			GetWorkgroupServers			( char* workgroup );
			void			DoBroadcastLookup			( char* workgroup, CFStringRef workgroupRef );
			char*			CopyNextMachine				( char** buffer );
			
			void			AddServiceResult			( CFStringRef workgroupRef, CFStringRef netBIOSRef, CFStringRef commentRef );
			Boolean			UseWINSURLMechanism			( void );

private:
			CFArrayRef		mLMBsRef;
	const	char*			mWINSServer;
	CFMutableArrayRef		mResultList;
};

#endif		// #ifndef
