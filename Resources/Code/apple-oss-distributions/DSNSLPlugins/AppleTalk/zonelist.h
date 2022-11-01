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
 *  @header zonelist
 */
 
#ifndef __ZONELIST__
#define __ZONELIST__

#include <pthread.h>
#include "nbputilities.h"

#define	kIOSize	64

#define ZIP_FIRST_ZONE 1
#define ZIP_NO_MORE_ZONES 0
#define ZIP_DEF_INTERFACE NULL

#define LOOKUP_ALL	0		/* perform lookup on all zones */
#define LOOKUP_LOCAL	1		/* perform lookup on local zones */
#define LOOKUP_CURRENT	2		/* perform lookup on current zone */


int ZIPGetZoneList(int lookupType, char *buffer, int bufferSize, long *actualCount);

extern "C" {
/* zip_getzonelist() will return the zone count on success, 
   and -1 on failure. */

int zip_getzonelist(
	char *ifName,
		/* If ifName is a null pointer (ZIP_DEF_INTERFACE) the default
		   interface will be used.
		*/
	int *context,
		/* *context should be set to ZIP_FIRST_ZONE for the first call.
		   The returned value may be used in the next call, unless it
		   is equal to ZIP_NO_MORE_ZONES.
		*/
	u_char *zones,
		/* Pointer to the beginning of the "zones" buffer.
		   Zone data returned will be a sequence of at_nvestr_t
		   Pascal-style strings, as it comes back from the 
		   ZIP_GETZONELIST request sent over ATP 
		*/
	int size
		/* Length of the "zones" buffer; must be at least 
		   (ATP_DATA_SIZE+1) bytes in length.
		*/
);

/* zip_getlocalzones() will return the zone count on success, 
   and -1 on failure. */

int zip_getlocalzones(
	char *ifName,
		/* If ifName is a null pointer (ZIP_DEF_INTERFACE) the default
		   interface will be used.
		*/
	int *context,
		/* *context should be set to ZIP_FIRST_ZONE for the first call.
		   The returned value may be used in the next call, unless it
		   is equal to ZIP_NO_MORE_ZONES.
		*/
	u_char *zones,
		/* Pointer to the beginning of the "zones" buffer.
		   Zone data returned will be a sequence of at_nvestr_t
		   Pascal-style strings, as it comes back from the 
		   ZIP_GETLOCALZONES request sent over ATP 
		*/
	int size
		/* Length of the "zones" buffer; must be at least 
		   (ATP_DATA_SIZE+1) bytes in length.
		*/
);

int zip_getmyzone(
	char *ifName,
		/* If ifName is a null pointer (ZIP_DEF_INTERFACE) the default
		   interface will be used.
		*/
	at_nvestr_t *zone
);

int at_getdefaultzone(
     char *ifName,
          /* ifName must not be a null pointer; a pointer to a valid 
	     interface name must be supplied. */
     at_nvestr_t *zone
          /* The return value for the default zone, from persistent 
             storage */
);

};

#endif
