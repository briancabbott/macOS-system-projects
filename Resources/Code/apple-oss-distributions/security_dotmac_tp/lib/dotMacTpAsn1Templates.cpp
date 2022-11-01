/*
 * Copyright (c) 2004-2008 Apple Inc. All Rights Reserved.
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
 * dptMacTpAsn1Templates.cpp - ASN1 templates for .mac TP
 */

#include <security_asn1/secasn1.h>
#include "dotMacTpAsn1Templates.h"

const SecAsn1Template DotMacTpPendingRequestTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(DotMacTpPendingRequest) },
    { SEC_ASN1_UTF8_STRING, offsetof(DotMacTpPendingRequest, userName) },
    { SEC_ASN1_INTEGER,		offsetof(DotMacTpPendingRequest, certTypeTag) },
    { SEC_ASN1_UTF8_STRING, offsetof(DotMacTpPendingRequest, domainName) },
    { 0 }
};

