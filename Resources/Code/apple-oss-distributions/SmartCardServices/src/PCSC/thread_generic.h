/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 */

/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : thread_generic.h
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 3/24/00
	    License: Copyright (C) 2000 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This provides system specific thread calls. 
	            
********************************************************************/

#ifndef __thread_generic_h__
#define __thread_generic_h__

#include <pthread.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef WIN32
#define PCSCLITE_THREAD_T                HANDLE
#define PCSCLITE_MUTEX                   CRITICAL_SECTION
#define PCSCLITE_MUTEX_T                 CRITICAL_SECTION*
#define PCSCLITE_THREAD_FUNCTION(f)      void *(*f)(void *)
#else
#define PCSCLITE_THREAD_T                pthread_t
#define PCSCLITE_MUTEX                   pthread_mutex_t
#define PCSCLITE_MUTEX_T                 pthread_mutex_t*
#define PCSCLITE_THREAD_FUNCTION(f)      void *(*f)(void *)
#endif

/* thread attributes */
#define THREAD_ATTR_DEFAULT			0
#define THREAD_ATTR_DETACHED		1

	int SYS_MutexInit(PCSCLITE_MUTEX_T);
	int SYS_MutexDestroy(PCSCLITE_MUTEX_T);
	int SYS_MutexLock(PCSCLITE_MUTEX_T);
	int SYS_MutexUnLock(PCSCLITE_MUTEX_T);
	int SYS_ThreadCreate(PCSCLITE_THREAD_T *, int, PCSCLITE_THREAD_FUNCTION( ), LPVOID);
	int SYS_ThreadCancel(PCSCLITE_THREAD_T *);
	int SYS_ThreadDetach(PCSCLITE_THREAD_T);
	int SYS_ThreadJoin(PCSCLITE_THREAD_T *, LPVOID*);
	int SYS_ThreadExit(LPVOID);
	PCSCLITE_THREAD_T SYS_ThreadSelf(void);
	int SYS_ThreadEqual(PCSCLITE_THREAD_T *, PCSCLITE_THREAD_T *);

#ifdef __cplusplus
}
#endif

#endif							/* __thread_generic_h__ */
