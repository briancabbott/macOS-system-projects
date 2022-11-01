/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
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
 * RSAKey.h
 * - generate RSA key pair
 */
/* 
 * Modification History
 *
 * April 11, 2013 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */
#ifndef _S_RSAKEY_H
#define _S_RSAKEY_H
#include <CoreFoundation/CFData.h>

/*
 * Function: RSAKeyPairGenerate
 * Purpose:
 *   Generate a key pair of the given size in bits.
 *
 * Returns:
 *   NULL and *ret_pub also NULL on failure,
 *   non-NULL private key data and non-NULL *ret_pub public key data on success.
 */
CFDataRef
RSAKeyPairGenerate(int key_size, CFDataRef * ret_pub);

#endif /* _S_RSAKEY_H */
