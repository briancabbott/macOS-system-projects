/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
#include "transit.h"

using namespace Security::Tokend;


//
// Server inbound transition layer.
// These are C functions in the global namespace, called by the MIG dispatch.
//


//
// Probe: Examine token, determine score, and generate token uid string
//
kern_return_t tokend_server_probe(TOKEND_ARGS, TokenScore *score, TokenUidString uid)
{
	BEGIN_IPC
	uid[0] = '\0';	// default to no uid obtained
	CALL(probe, (kSecTokendCallbacksDefault, score, uid));
    // We do not support 32 bit tokends on 10.6 and later
    // since securityd always runs 64 bit
    if (sizeof(int*) == 4)
    {
		Syslog::error("32-bit tokends not supported");
		CssmError::throwMe(CSSMERR_CSP_DEVICE_ERROR);
    }
	uid[TOKEND_MAX_UID-1] = '\0'; // enforce null termination
	server->tokenUid(uid);
	END_IPC(CSSM)
}


//
// Establish: take securityd's information and stash it away;
// determine MDS resources and return them.
//
kern_return_t tokend_server_establish(TOKEND_ARGS,
	CSSM_GUID guid, uint32 ssid, SecTokendEstablishFlags flags,
	const char *cacheDirectory, const char *workDirectory,
	char mdsDirectory[PATH_MAX], char printName[PATH_MAX])
{
	BEGIN_IPC
	mdsDirectory[0] = '\0';		// default to all resources
	printName[0] = '\0';		// default to no print name
	CALL(establish, (&guid, ssid, flags, cacheDirectory, workDirectory, mdsDirectory, printName));
	END_IPC(CSSM)
}


//
// Induce termination
//
kern_return_t tokend_server_terminate(mach_port_t servicePort, uint32 reason, uint32 options)
{
	server->termination(reason, options);
}


//
// Find/retrieve interface
//
kern_return_t tokend_server_findFirst(TOKEND_ARGS,
	COPY_IN(CssmQuery, query),
	COPY_IN(CssmDbRecordAttributeData, inAttributes),
	boolean_t getData, DATA_OUT(outData), KeyHandle *hKey,
	COPY_OUT(CssmDbRecordAttributeData, outAttributes),
	SearchHandle *hSearch, RecordHandle *hRecord)
{
	BEGIN_IPC
	relocate(query, queryBase);
	relocate(inAttributes, inAttributesBase);
	DataRetrieval interface(inAttributes, getData);
    CSSM_HANDLE searchHandle = 0;
    *hSearch = 0; // to prevent uninitialized value when following CALL ends with exception
	CALL(findFirst, (query, &interface, &searchHandle));
	interface.returnData(*hKey, *hRecord, *outData, *outDataLength,
		*outAttributes, *outAttributesLength, *outAttributesBase);
	/*
		findFirst and findNext do not return an error if nothing was found.
		Instead, hKey and hRecord are set to 0. In this case, we don't want to
		set up a search handle
	*/
	if (searchHandle)
		*hSearch = TokendHandleObject::make(searchHandle)->ipcHandle();
	server->releaseWhenDone(Allocator::standard(), *outAttributes);
	END_IPC(DL)
}


kern_return_t tokend_server_findNext(TOKEND_ARGS, SearchHandle hSearch,
	COPY_IN(CssmDbRecordAttributeData, inAttributes),
	boolean_t getData, DATA_OUT(outData), KeyHandle *hKey,
	COPY_OUT(CssmDbRecordAttributeData, outAttributes),
	RecordHandle *hRecord)
{
	BEGIN_IPC
	relocate(inAttributes, inAttributesBase);
	DataRetrieval interface(inAttributes, getData);
    CSSM_HANDLE searchHandle = TokendHandleObject::findTDHandle(hSearch);
	CALL(findNext, (searchHandle, &interface));
	interface.returnData(*hKey, *hRecord, *outData, *outDataLength,
		*outAttributes, *outAttributesLength, *outAttributesBase);
	server->releaseWhenDone(Allocator::standard(), *outAttributes); 
	END_IPC(DL)
}


kern_return_t tokend_server_findRecordHandle(TOKEND_ARGS, RecordHandle hRecord,
	COPY_IN(CssmDbRecordAttributeData, inAttributes),
	boolean_t getData, DATA_OUT(outData), KeyHandle *hKey,
	COPY_OUT(CssmDbRecordAttributeData, outAttributes))
{
	BEGIN_IPC
	relocate(inAttributes, inAttributesBase);
	DataRetrieval interface(inAttributes, getData);
    CSSM_HANDLE recordHandle = TokendHandleObject::findTDHandle(hRecord);
	CALL(findRecordHandle, (recordHandle, &interface));
	interface.returnData(*hKey, hRecord, *outData, *outDataLength,
		*outAttributes, *outAttributesLength, *outAttributesBase);
	server->releaseWhenDone(Allocator::standard(), *outAttributes);
	END_IPC(DL)
}


kern_return_t tokend_server_insertRecord(TOKEND_ARGS, CSSM_DB_RECORDTYPE recordType,
	COPY_IN(CssmDbRecordAttributeData, attributes), DATA_IN(data),
	RecordHandle *hRecord)
{
	BEGIN_IPC
	relocate(attributes, attributesBase);
	const CssmData dataBlob(DATA(data));
    CSSM_HANDLE recordHandle;
	CALL(insertRecord, (recordType, attributes, data ? &dataBlob : NULL, &recordHandle));
    *hRecord = TokendHandleObject::make(recordHandle)->ipcHandle();
	END_IPC(DL)
}


kern_return_t tokend_server_modifyRecord(TOKEND_ARGS, CSSM_DB_RECORDTYPE recordType,
	RecordHandle *hRecord, COPY_IN(CssmDbRecordAttributeData, attributes),
	boolean_t setData, DATA_IN(data), CSSM_DB_MODIFY_MODE modifyMode)
{
	BEGIN_IPC
	relocate(attributes, attributesBase);
	const CssmData dataBlob(DATA(data));
    CSSM_HANDLE recordHandle;
	CALL(modifyRecord, (recordType, &recordHandle, attributes,
		setData ? &dataBlob : NULL, modifyMode));
    *hRecord = TokendHandleObject::make(recordHandle)->ipcHandle();
	END_IPC(DL)
}


kern_return_t tokend_server_deleteRecord(TOKEND_ARGS, RecordHandle hRecord)
{
	BEGIN_IPC
    CSSM_HANDLE recordHandle = TokendHandleObject::findTDHandle(hRecord);
	CALL(deleteRecord, (recordHandle));
	END_IPC(DL)
}


kern_return_t tokend_server_releaseSearch(TOKEND_ARGS, SearchHandle hSearch)
{
	BEGIN_IPC
    CSSM_HANDLE searchHandle = TokendHandleObject::findTDHandle(hSearch);
	CALL(releaseSearch, (searchHandle));
	END_IPC(DL)
}

kern_return_t tokend_server_releaseRecord(TOKEND_ARGS, RecordHandle hRecord)
{
	BEGIN_IPC
    CSSM_HANDLE recordHandle = TokendHandleObject::findTDHandle(hRecord);
	CALL(releaseRecord, (recordHandle));
	END_IPC(DL)
}


//
// Key management
//
kern_return_t tokend_server_releaseKey(TOKEND_ARGS, KeyHandle hKey)
{
	BEGIN_IPC
    CSSM_HANDLE keyHandle = TokendHandleObject::findTDHandle(hKey);
	CALL(releaseKey, (keyHandle));
	END_IPC(DL)
}

kern_return_t tokend_server_queryKeySizeInBits(TOKEND_ARGS, KeyHandle hKey,
	CSSM_KEY_SIZE *size)
{
	BEGIN_IPC
    CSSM_HANDLE keyHandle = TokendHandleObject::findTDHandle(hKey);
	CALL(getKeySize, (keyHandle, size));
	END_IPC(CSP)
}

kern_return_t tokend_server_getOutputSize(TOKEND_ARGS, CONTEXT_ARGS, KeyHandle hKey,
    uint32 inputSize, boolean_t encrypt, uint32 *outputSize)
{
    BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
    CSSM_HANDLE keyHandle = TokendHandleObject::findTDHandle(hKey);
	CALL(getOutputSize, (&context, keyHandle, inputSize, encrypt, outputSize));
    END_IPC(CSP)
}


//
// Signatures and MACs
//
kern_return_t tokend_server_generateSignature(TOKEND_ARGS, CONTEXT_ARGS, KeyHandle hKey,
        CSSM_ALGORITHMS signOnlyAlgorithm, DATA_IN(data), DATA_OUT(signature))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	const CssmData dataBlob(DATA(data));
	OutputData sigData(signature, signatureLength);
    CSSM_HANDLE keyHandle = TokendHandleObject::findTDHandle(hKey);
	CALL(generateSignature, (&context, keyHandle, signOnlyAlgorithm,
		&dataBlob, &sigData));
	END_IPC(CSP)
}

kern_return_t tokend_server_verifySignature(TOKEND_ARGS, CONTEXT_ARGS, KeyHandle hKey,
		CSSM_ALGORITHMS verifyOnlyAlgorithm, DATA_IN(data), DATA_IN(signature))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	const CssmData dataBlob(DATA(data));
	const CssmData signatureBlob(DATA(signature));
    CSSM_HANDLE keyHandle = TokendHandleObject::findTDHandle(hKey);
	CALL(verifySignature, (&context, keyHandle, verifyOnlyAlgorithm, &dataBlob, &signatureBlob));
	END_IPC(CSP)
}

kern_return_t tokend_server_generateMac(TOKEND_ARGS, CONTEXT_ARGS, KeyHandle hKey,
		DATA_IN(data), DATA_OUT(mac))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	const CssmData dataBlob(DATA(data));
	OutputData macData(mac, macLength);
    CSSM_HANDLE keyHandle = TokendHandleObject::findTDHandle(hKey);
	CALL(generateMac, (&context, keyHandle, &dataBlob, &macData));
	END_IPC(CSP)
}

kern_return_t tokend_server_verifyMac(TOKEND_ARGS, CONTEXT_ARGS, KeyHandle hKey,
		DATA_IN(data), DATA_IN(mac))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	const CssmData dataBlob(DATA(data));
	const CssmData macBlob(DATA(mac));
    CSSM_HANDLE keyHandle = TokendHandleObject::findTDHandle(hKey);
	CALL(verifyMac, (&context, keyHandle, &dataBlob, &macBlob));
	END_IPC(CSP)
}


//
// Encryption/Decryption
//
kern_return_t tokend_server_encrypt(TOKEND_ARGS, CONTEXT_ARGS, KeyHandle hKey,
	DATA_IN(clear), DATA_OUT(cipher))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	const CssmData clearBlob(DATA(clear));
	OutputData cipherOut(cipher, cipherLength);
    CSSM_HANDLE keyHandle = TokendHandleObject::findTDHandle(hKey);
	CALL(encrypt, (&context, keyHandle, &clearBlob, &cipherOut));
	END_IPC(CSP)
}

kern_return_t tokend_server_decrypt(TOKEND_ARGS, CONTEXT_ARGS, KeyHandle hKey,
	DATA_IN(cipher), DATA_OUT(clear))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	const CssmData cipherBlob(DATA(cipher));
	OutputData clearOut(clear, clearLength);
    CSSM_HANDLE keyHandle = TokendHandleObject::findTDHandle(hKey);
	CALL(decrypt, (&context, keyHandle, &cipherBlob, &clearOut));
	END_IPC(CSP)
}


//
// Key generation
//
kern_return_t tokend_server_generateKey(TOKEND_ARGS, CONTEXT_ARGS,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
	uint32 usage, uint32 attrs, KeyHandle *hKey, COPY_OUT(CssmKey, key))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
    relocate(cred, credBase);
	relocate(owner, ownerBase);
	Return<CssmKey> rKey(key, keyLength, keyBase);
    CSSM_HANDLE keyHandle;
	CALL(generateKey, (&context, cred, owner, usage, attrs, &keyHandle, &rKey));
    *hKey = TokendHandleObject::make(keyHandle)->ipcHandle();
	END_IPC(CSP)
}

kern_return_t tokend_server_generateKeyPair(TOKEND_ARGS, CONTEXT_ARGS,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
	CSSM_KEYUSE pubUsage, CSSM_KEYATTR_FLAGS pubAttrs,
	CSSM_KEYUSE privUsage, CSSM_KEYATTR_FLAGS privAttrs,
	KeyHandle *hPubKey, COPY_OUT(CssmKey, pubKey),
	KeyHandle *hPrivKey, COPY_OUT(CssmKey, privKey))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
    relocate(cred, credBase);
	relocate(owner, ownerBase);
	Return<CssmKey> rPubKey(pubKey, pubKeyLength, pubKeyBase);
	Return<CssmKey> rPrivKey(privKey, privKeyLength, privKeyBase);
    CSSM_HANDLE cssmPubKey, cssmPrivKey;
	CALL(generateKeyPair, (&context, cred, owner,
		pubUsage, pubAttrs, privUsage, privAttrs,
		&cssmPubKey, &rPubKey, &cssmPrivKey, &rPrivKey));
    *hPubKey = TokendHandleObject::make(cssmPubKey)->ipcHandle();
    *hPrivKey = TokendHandleObject::make(cssmPrivKey)->ipcHandle();
	END_IPC(CSP)
}


//
// Key wrapping and unwrapping
//
kern_return_t tokend_server_wrapKey(TOKEND_ARGS, CONTEXT_ARGS,
	KeyHandle hWrappingKey, COPY_IN(CssmKey, wrappingKey),
	COPY_IN(AccessCredentials, cred),
	KeyHandle hSubjectKey, COPY_IN(CssmKey, subjectKey),
	DATA_IN(descriptiveData), COPY_OUT(CssmWrappedKey, wrappedKey))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
    relocate(cred, credBase);
	relocate(wrappingKey, wrappingKeyBase);
	relocate(subjectKey, subjectKeyBase);
	CssmData ddata(DATA(descriptiveData));
	Return<CssmKey> rKey(wrappedKey, wrappedKeyLength, wrappedKeyBase);
    CSSM_HANDLE keyHandle = TokendHandleObject::findTDHandle(hSubjectKey);
	CALL(wrapKey, (&context, hWrappingKey, wrappingKey, cred,
		keyHandle, subjectKey, descriptiveData ? &ddata : NULL, &rKey));
	END_IPC(CSP)
}

kern_return_t tokend_server_unwrapKey(TOKEND_ARGS, CONTEXT_ARGS,
	KeyHandle hWrappingKey, COPY_IN(CssmKey, wrappingKey),
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
	KeyHandle hPublicKey, COPY_IN(CssmKey, publicKey),
	COPY_IN(CssmWrappedKey, wrappedKey),
	uint32 usage, uint32 attrs, DATA_OUT(descriptiveData),
    KeyHandle *hUnwrappedKey, COPY_OUT(CssmKey, unwrappedKey))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
    relocate(cred, credBase);
	relocate(owner, ownerBase);
	relocate(wrappingKey, wrappingKeyBase);
	relocate(publicKey, publicKeyBase);
	relocate(wrappedKey, wrappedKeyBase);
	Return<CssmKey> rKey(unwrappedKey, unwrappedKeyLength, unwrappedKeyBase);
	OutputData descriptives(descriptiveData, descriptiveDataLength);
    CSSM_HANDLE cssmUnwrappedKey;
    CSSM_HANDLE keyHandle = TokendHandleObject::findTDHandle(hPublicKey);
	CSSM_HANDLE wrapKeyHandle = TokendHandleObject::findTDHandle(hWrappingKey);
	CALL(unwrapKey, (&context, wrapKeyHandle, wrappingKey, cred, owner,
		keyHandle, publicKey, wrappedKey,
		usage, attrs, &descriptives, &cssmUnwrappedKey, &rKey));
    *hUnwrappedKey = TokendHandleObject::make(cssmUnwrappedKey)->ipcHandle();
	END_IPC(CSP)
}


//
// Key derivation.
//
kern_return_t tokend_server_deriveKey(TOKEND_ARGS, CONTEXT_ARGS,
	KeyHandle hBaseKey, COPY_IN(CssmKey, baseKey),
	COPY_IN(AccessCredentials, cred), COPY_IN(AclEntryPrototype, owner),
    COPY_IN(CssmDeriveData,paramInput), DATA_OUT(paramOutput),
	CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs,
	KeyHandle *hKey, COPY_OUT(CssmKey, key))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
    relocate(cred, credBase);
	relocate(owner, ownerBase);
	relocate(baseKey, baseKeyBase);
	relocate(paramInput, paramInputBase);
	if (!paramInput || paramInput->algorithm != context.algorithm())
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS);
	Return<CssmKey> rKey(key, keyLength, keyBase);
    CSSM_HANDLE cssmKeyHandle;
	CALL(deriveKey, (&context, hBaseKey, baseKey, cred, owner, &paramInput->baseData,
		usage, attrs, &cssmKeyHandle, &rKey));
    *hKey = TokendHandleObject::make(cssmKeyHandle)->ipcHandle();
	OutputData(paramOutput, paramOutputLength) = paramInput->baseData;
	END_IPC(CSP)
}


//
// Random generation
//
kern_return_t tokend_server_generateRandom(TOKEND_ARGS, CONTEXT_ARGS, DATA_OUT(result))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	OutputData results(result, resultLength);
	CALL(generateRandom, (&context, &results));
	END_IPC(CSP)
}


//
// ACL management.
// Watch out for the memory-management tap-dance.
//
kern_return_t tokend_server_getOwner(TOKEND_ARGS, AclKind kind, GenericHandle hHandle,
	COPY_OUT(AclOwnerPrototype, owner))
{
	BEGIN_IPC
	Return<AclOwnerPrototype> proto(owner, ownerLength, ownerBase);
	switch (kind) {
	case dbAcl:
		CALL(getDatabaseOwner, (&proto));
		break;
	case keyAcl:
		{
			CSSM_HANDLE tdHandle = TokendHandleObject::findTDHandle(hHandle);
			CALL(getKeyOwner, (tdHandle, &proto));
		}
		break;
	case objectAcl:
		{
			CSSM_HANDLE tdHandle = TokendHandleObject::findTDHandle(hHandle);
			CALL(getObjectOwner, (tdHandle, &proto));
		}
		break;
	case loginAcl:
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	}
	END_IPC(CSP)
}

kern_return_t tokend_server_setOwner(TOKEND_ARGS, AclKind kind, GenericHandle key,
	COPY_IN(AccessCredentials, cred), COPY_IN(AclOwnerPrototype, owner))
{
	BEGIN_IPC
    relocate(cred, credBase);
	relocate(owner, ownerBase);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	END_IPC(CSP)
}

kern_return_t tokend_server_getAcl(TOKEND_ARGS, AclKind kind, GenericHandle hObject,
	boolean_t haveTag, const char *inTag,
	uint32 *countp, COPY_OUT(AclEntryInfo, acls))
{
	BEGIN_IPC
	const char *tag = haveTag ? inTag : NULL;
	AclEntryInfo *infos;
	switch (kind) {
	case dbAcl:
		CALL(getDatabaseAcl, (tag, countp, (CSSM_ACL_ENTRY_INFO**)&infos));
		break;
	case keyAcl:
		{
			CSSM_HANDLE tdHandle = TokendHandleObject::findTDHandle(hObject);
			CALL(getKeyAcl, (tdHandle, tag, countp, (CSSM_ACL_ENTRY_INFO**)&infos));
		}
		break;
	case objectAcl:
		{
			CSSM_HANDLE tdHandle = TokendHandleObject::findTDHandle(hObject);
			CALL(getObjectAcl, (tdHandle, tag, countp, (CSSM_ACL_ENTRY_INFO**)&infos));
		}
		break;
	case loginAcl:
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	}
	
	// package up (count, infos) as a return blob
	Copier<AclEntryInfo> aclsOut(infos, *countp);
	*aclsLength = aclsOut.length();
	*acls = *aclsBase = aclsOut;
	aclsOut.keep();
	server->releaseWhenDone(Allocator::standard(), *acls);
	END_IPC(CSP)
}

kern_return_t tokend_server_changeAcl(TOKEND_ARGS, AclKind kind, GenericHandle key,
	COPY_IN(AccessCredentials, cred), CSSM_ACL_EDIT_MODE mode, CSSM_ACL_HANDLE handle,
	COPY_IN(AclEntryInput, acl))
{
	BEGIN_IPC
    relocate(cred, credBase);
	relocate(acl, aclBase);
	AclEdit edit(mode, handle, acl);
	switch (kind) {
	case dbAcl:
		CALL(changeDatabaseAcl, (cred, &edit));
		break;
	case keyAcl:
		{
			CSSM_HANDLE keyHandle = TokendHandleObject::findTDHandle(key);
			CALL(changeKeyAcl, (keyHandle, cred, &edit));
		}
		break;
	case objectAcl:
		{
			CSSM_HANDLE keyHandle = TokendHandleObject::findTDHandle(key);
			CALL(changeObjectAcl, (keyHandle, cred, &edit));
		}
		break;
	case loginAcl:
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	}
	END_IPC(CSP)
}


kern_return_t tokend_server_authenticate(TOKEND_ARGS,
	CSSM_DB_ACCESS_TYPE mode, COPY_IN(AccessCredentials, cred))
{
	BEGIN_IPC
	relocate(cred, credBase);
	CALL(authenticateDatabase, (mode, cred));
	END_IPC(DL)
}


kern_return_t tokend_server_login(TOKEND_ARGS, COPY_IN(AccessCredentials, cred), DATA_IN(name))
{
	BEGIN_IPC
	relocate(cred, credBase);
	//CALL(login, (cred));
	END_IPC(DL)
}

kern_return_t tokend_server_logout(TOKEND_ARGS)
{
	BEGIN_IPC
	//CALL(logout, ());
	END_IPC(DL)
}


//
// Miscellaneous CSP-related calls
//
kern_return_t tokend_server_getStatistics(TOKEND_ARGS, CSPOperationalStatistics *statistics)
{
	BEGIN_IPC
	CALL(getStatistics, (statistics));
	END_IPC(CSP)
}

kern_return_t tokend_server_getTime(TOKEND_ARGS, CSSM_ALGORITHMS algorithm, DATA_OUT(data))
{
	BEGIN_IPC
	OutputData datas(data, dataLength);
	CALL(getTime, (algorithm, &datas));
	END_IPC(CSP)
}

kern_return_t tokend_server_getCounter(TOKEND_ARGS, DATA_OUT(data))
{
	BEGIN_IPC
	OutputData datas(data, dataLength);
	CALL(getCounter, (&datas));
	END_IPC(CSP)
}

kern_return_t tokend_server_selfVerify(TOKEND_ARGS)
{
	BEGIN_IPC
	CALL(selfVerify, ());
	END_IPC(CSP)
}


//
// Passthrough calls (separate for CSP and DL passthroughs)
//
kern_return_t tokend_server_cspPassThrough(TOKEND_ARGS, uint32 id, CONTEXT_ARGS,
	KeyHandle hKey, COPY_IN(CssmKey, key), DATA_IN(inData), DATA_OUT(outData))
{
	BEGIN_IPC
	relocate(context, contextBase, attributes, attrSize);
	relocate(key, keyBase);
	const CssmData inDataBlob(DATA(inData));
	OutputData outputs(outData, outDataLength);
	CSSM_HANDLE keyHandle = TokendHandleObject::findTDHandle(hKey);
	CALL(cspPassThrough, (id, &context, keyHandle, key, inData ? &inDataBlob : NULL, &outputs));
	END_IPC(CSP)
}

kern_return_t tokend_server_dlPassThrough(TOKEND_ARGS, uint32 id,
	DATA_IN(inData), DATA_OUT(outData))
{
	BEGIN_IPC
	const CssmData inDataBlob(DATA(inData));
	OutputData outputs(outData, outDataLength);
	CALL(dlPassThrough, (id, inData ? &inDataBlob : NULL, &outputs));
	END_IPC(DL)
}

kern_return_t tokend_server_isLocked(TOKEND_ARGS, uint32 *locked)
{
	BEGIN_IPC
	CALL(isLocked, (locked));
	END_IPC(DL)
}
