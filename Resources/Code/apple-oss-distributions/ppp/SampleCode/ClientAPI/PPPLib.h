/*
 * Copyright (c) 2000, 2001 Apple Computer, Inc. All rights reserved.
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

/*
 * Modification History
 *
 * Feb 10, 2001			Allan Nathanson <ajn@apple.com>
 * - cleanup API
 *
 * Feb 2000			Christophe Allie <callie@apple.com>
 * - initial revision
 */

#ifndef _PPPLIB_H
#define _PPPLIB_H

#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include "ppp_msg.h"

__BEGIN_DECLS

int		PPPInit			(int			*ref);

int		PPPDispose		(int			ref);

int		PPPConnect		(int			ref,
					 u_int8_t 		*serviceid);

int		PPPDisconnect		(int			ref,
					 u_int8_t 		*serviceid);

int		PPPGetOption		(int			ref,
					 u_int8_t 		*serviceid,
					 u_int32_t		option,
					 void			**data,
					 u_int32_t		*dataLen);

int		PPPSetOption		(int			ref,
					 u_int8_t 		*serviceid,
					 u_int32_t		option,
					 void			*data,
					 u_int32_t		dataLen);

int		PPPStatus		(int			ref,
					 u_int8_t 		*serviceid,
					 struct ppp_status	**stat);

int		PPPEnableEvents		(int			ref,
					 u_int8_t 		*serviceid,
					 u_char			enable);

__END_DECLS

#endif	/* _PPPLIB_H */
