/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 
 
#import "FWTracepoints.h"
 
// the controls 

// *** START HERE: disable debug logging
#define FWLOGGING 0
#define FWASSERTS 1

///////////////////////////////////////////

#if FWLOGGING
	#define FWKLOG( x )					IOLog x
	#define DebugLog( x... )			IOLog( x ) ;
	#define DebugLogCond( x, y... ) 	{ if (x) DebugLog( y ) ; }
#else
	#define FWKLOG( x... )				do {} while (0)
	#define DebugLog( x... )			do {} while (0)
	#define DebugLogCond( x, y... )		do {} while (0)
#endif

#if FWLOCALLOGGING
#define FWLOCALKLOG(x) IOLog x
#else
#define FWLOCALKLOG(x) do {} while (0)
#endif

#if FWASSERTS
#define FWKLOGASSERT(a) { if(!(a)) { IOLog( "File %s, line %d: assertion '%s' failed.\n", __FILE__, __LINE__, #a); } }
#else
#define FWKLOGASSERT(a) do {} while (0)
#endif

#if FWASSERTS
#define FWPANICASSERT(a) { if(!(a)) { panic( "File "__FILE__", line %d: assertion '%s' failed.\n", __LINE__, #a); } }
#else
#define FWPANICASSERT(a) do {} while (0)
#endif

#if FWASSERTS
#define FWASSERTINGATE(a) { if(!((a)->inGate())) { IOLog( "File "__FILE__", line %d: warning - workloop lock is not held.\n", __LINE__); } }
#else
#define FWASSERTINGATE(a) do {} while (0)
#endif

#define DoErrorLog( x... ) { IOLog( "ERROR: " x ) ; }
#define DoDebugLog( x... ) { IOLog( x ) ; }

#define ErrorLog(x...) 				DoErrorLog( x ) ;
#define	ErrorLogCond( x, y... )		{ if (x) ErrorLog ( y ) ; }

#if IOFIREWIREDEBUG > 0
#	define DebugLogOrig(x...)			DoDebugLog( x ) ;
#	define DebugLogCondOrig( x, y... ) 	{ if (x) DebugLogOrig ( y ) ; }
#else
#	define DebugLogOrig(x...)			do {} while (0)
#	define DebugLogCondOrig( x, y... )	do {} while (0)
#endif

#define TIMEIT( doit, description ) \
{ \
	AbsoluteTime start, end; \
	IOFWGetAbsoluteTime( & start ); \
	{ \
		doit ;\
	}\
	IOFWGetAbsoluteTime( & end ); \
	SUB_ABSOLUTETIME( & end, & start ) ;\
	UInt64 nanos ;\
	absolutetime_to_nanoseconds( end, & nanos ) ;\
	DebugLogOrig("%s duration %llu us\n", "" description, nanos/1000) ;\
}

#define InfoLog(x...) do {} while (0)


