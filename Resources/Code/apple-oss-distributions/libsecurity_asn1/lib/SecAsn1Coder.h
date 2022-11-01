/*
 * Copyright (c) 2003-2006 Apple Computer, Inc. All Rights Reserved.
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
 *
 * SecAsn1Coder.h: ANS1 encode/decode object.
 *
 * A SecAsn1Coder is capable of encoding and decoding both DER and BER data
 * streams, based on caller-supplied templates which in turn are based
 * upon ASN.1 specifications. A SecAsn1Coder allocates memory during encode
 * and decode using a memory pool which is owned and managed by the SecAsn1Coder
 * object, and which is freed when the SecAsn1Coder object os released. 
 */
 
#ifndef	_SEC_ASN1_CODER_H_
#define _SEC_ASN1_CODER_H_

#include <Security/cssmtype.h>
#include <Security/SecBase.h>
#include <Security/SecAsn1Types.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Opaque reference to a SecAsn1Coder object.
 */
typedef struct SecAsn1Coder *SecAsn1CoderRef;

/*
 * Create/destroy SecAsn1Coder object. 
 */
OSStatus SecAsn1CoderCreate(
	SecAsn1CoderRef  *coder);
	
OSStatus SecAsn1CoderRelease(
	SecAsn1CoderRef  coder);
	
/*
 * DER decode an untyped item per the specified template array. 
 * The result is allocated in this SecAsn1Coder's memory pool and 
 * is freed when this object is released.
 *
 * The templates argument points to a an array of SecAsn1Templates 
 * defining the object to be decoded; the end of the array is 
 * indicated by a SecAsn1Template with file kind equalling 0. 
 *
 * The dest pointer is a template-specific struct allocated by the caller 
 * and must be zeroed by the caller. 
 *
 * Returns errSecUnknownFormat on decode-specific error.
 */
OSStatus SecAsn1Decode(
	SecAsn1CoderRef			coder,
	const void				*src,		// DER-encoded source
	size_t					len,
	const SecAsn1Template 	*templates,	
	void					*dest);
		
/* 
 * Convenience routine, decode from a CSSM_DATA.
 */
OSStatus SecAsn1DecodeData(
	SecAsn1CoderRef			coder,
	const CSSM_DATA			*src,
	const SecAsn1Template 	*templ,	
	void					*dest);

/*
 * DER encode. The encoded data (in dest.Data) is allocated in this 
 * SecAsn1Coder's memory pool and is freed when this object is released.
 *
 * The src pointer is a template-specific struct.
 *
 * The templates argument points to a an array of SecAsn1Templates 
 * defining the object to be decoded; the end of the array is 
 * indicated by a SecAsn1Template with file kind equalling 0. 
 */
OSStatus SecAsn1EncodeItem(
	SecAsn1CoderRef			coder,
	const void				*src,
	const SecAsn1Template 	*templates,	
	CSSM_DATA				*dest);
		
/*
 * Some alloc-related methods which come in handy when using
 * this object. All memory is allocated using this object's 
 * memory pool. Caller never has to free it. Used for
 * temp allocs of memory which only needs a scope which is the
 * same as this object. 
 *
 * All except SecAsn1Malloc return a memFullErr in the highly 
 * unlikely event of a malloc failure.
 *
 * SecAsn1Malloc() returns a pointer to allocated memory, like 
 * malloc().
 */
void *SecAsn1Malloc(
	SecAsn1CoderRef			coder,
	size_t					len); 
	
/* Allocate item.Data, set item.Length */
OSStatus SecAsn1AllocItem(
	SecAsn1CoderRef			coder,
	CSSM_DATA				*item,
	size_t					len);
	
/* Allocate and copy, various forms */
OSStatus SecAsn1AllocCopy(
	SecAsn1CoderRef			coder,
	const void				*src,		/* memory copied from here */
	size_t					len,		/* length to allocate & copy */
	CSSM_DATA				*dest);		/* dest->Data allocated and copied to;
										 *   dest->Length := len */
	
OSStatus SecAsn1AllocCopyItem(
	SecAsn1CoderRef			coder,
	const CSSM_DATA			*src,		/* src->Length bytes allocated and copied from 
										 *   src->Data */
	CSSM_DATA				*dest);		/* dest->Data allocated and copied to;
										 *   dest->Length := src->Length */
		
#ifdef __cplusplus
}
#endif

#endif	/* _SEC_ASN1_CODER_H_ */
