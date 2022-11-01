/*
 *  Copyright (c) 2004-2007 Apple Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  PIVAttributeCoder.cpp
 *  TokendPIV
 */

/* ---------------------------------------------------------------------------
 *
 *		This file should not need to be modified except for replacing
 *		"piv" with the name of your token
 *
 * ---------------------------------------------------------------------------
*/

#include "PIVAttributeCoder.h"

#include "MetaAttribute.h"
#include "MetaRecord.h"
#include "PIVRecord.h"

using namespace Tokend;

//
// PIVDataAttributeCoder
//
PIVDataAttributeCoder::~PIVDataAttributeCoder()
{
}

void PIVDataAttributeCoder::decode(TokenContext *tokenContext,
	const MetaAttribute &metaAttribute, Record &record)
{
	PIVRecord &pivRecord = dynamic_cast<PIVRecord &>(record);
	record.attributeAtIndex(metaAttribute.attributeIndex(),
		pivRecord.getDataAttribute(tokenContext));
}

//
// PIVKeySizeAttributeCoder
//
PIVKeySizeAttributeCoder::~PIVKeySizeAttributeCoder() {}

void PIVKeySizeAttributeCoder::decode(Tokend::TokenContext *tokenContext,
								   const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record)
{
	uint32 keySize = dynamic_cast<PIVKeyRecord &>(record).sizeInBits();
	record.attributeAtIndex(metaAttribute.attributeIndex(), new Attribute(keySize));
}
