/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

/*
 * Modification History
 *
 * May 1, 2003	Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#ifndef	_CACHE_H
#define	_CACHE_H


#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>


__BEGIN_DECLS

void			cache_open			();

CFPropertyListRef	cache_SCDynamicStoreCopyValue	(SCDynamicStoreRef	store,
							 CFStringRef		key);

void			cache_SCDynamicStoreSetValue	(SCDynamicStoreRef	store,
							 CFStringRef		key,
							 CFPropertyListRef	value);

void			cache_SCDynamicStoreRemoveValue	(SCDynamicStoreRef	store,
							 CFStringRef		key);

void			cache_SCDynamicStoreNotifyValue	(SCDynamicStoreRef	store,
							 CFStringRef		key);

void			cache_write			(SCDynamicStoreRef	store);

void			cache_close			();

__END_DECLS

#endif	/* _CACHE_H */
