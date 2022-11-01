/*
 * Copyright 1998-2003  by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.	Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/* 
 * KClient 1.9 deprecated API
 *
 * $Header$
 */

#ifndef	__KCLIENTDEPRECATED__
#define	__KCLIENTDEPRECATED__

#if defined(macintosh) || (defined(__MACH__) && defined(__APPLE__))
#    include <TargetConditionals.h>
#    include <AvailabilityMacros.h>
#    if TARGET_RT_MAC_CFM
#        error "Use KfM 4.0 SDK headers for CFM compilation."
#    endif
#endif

#ifndef DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER
#define DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER
#endif

#if TARGET_OS_MAC
#    include <Kerberos/krb.h>
#    include <Kerberos/KClientTypes.h>
#else
#    include <kerberosIV/krb.h>
#    include <KClientTypes.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if TARGET_OS_MAC
#    pragma pack(push,2)
#endif
    
/* Max error text length returned by KClientGetErrorTextDeprecated */
#define kKClientMaxErrorTextLength 512

/*
 * Important!
 *
 * The following functions are deprecated. They will be removed from the library
 * and the header files in the future. See documentation for moving to KClient
 * 3.0 API to see how you can update your code.
 */

OSStatus
KClientCacheInitialTicketDeprecated (
	KClientSession*			inSession,
	char*					inService)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientGetLocalRealmDeprecated (
	char*					outRealm)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientSetLocalRealmDeprecated (
	const char*				inRealm)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientGetRealmDeprecated (
	const char*				inHost,
	char*					outRealm)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientAddRealmMapDeprecated (
	char*					inHost,
	char*					inRealm)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientDeleteRealmMapDeprecated (
	char*					inHost)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientGetNthRealmMapDeprecated (
	SInt32					inIndex,
	char*					outHost,
	char*					outRealm)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientGetNthServerDeprecated (
	SInt32					inIndex,
	char*					outHost,
	char*					inRealm,
	Boolean					inAdmin)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientAddServerMapDeprecated (
	char*					inHost,
	char*					inRealm,
	Boolean					inAdmin)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientDeleteServerMapDeprecated (
	char*					inHost,
	char*					inRealm)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientGetNthServerMapDeprecated (
	SInt32					inIndex,
	char*					outHost,
	char*					outRealm,
	Boolean*				outAdmin)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientGetNthServerPortDeprecated (
	SInt32					inIndex,
	UInt16*					outPort)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientSetNthServerPortDeprecated (
	SInt32					inIndex,
	UInt16					inPort)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientGetNumSessionsDeprecated (
	SInt32*					outSessions)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientGetNthSessionDeprecated (
	SInt32					inIndex,
	char*					outName,
	char*					outInstance,
	char*					outRealm)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientDeleteSessionDeprecated (
	char*					inName,
	char*					inInstance,
	char*					inRealm)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientGetCredentialsDeprecated (
	char*					inName,
	char*					inInstance,
	char*					inRealm,
	CREDENTIALS*			outCred)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientAddCredentialsDeprecated (
	char*					inName,
	char*					inInstance,
	char*					inRealm,
	CREDENTIALS*			inCred)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientDeleteCredentialsDeprecated (
	char*					inName,
	char*					inInstance,
	char*					inRealm, 
	char*					inSname,
	char*					inSinstance,
	char*					inSrealm)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus KClientGetNumCredentialsDeprecated (
	SInt32*					outNumCredentials,
	char*					inName,
	char*					inInstance,
	char*					inRealm)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus
KClientGetNthCredentialDeprecated (
	SInt32					inIndex,
	char*					inName,
	char*					inInstance,
	char*					inRealm,
	char*					inSname,
	char*					inSinstance,
	char*					inSrealm)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus
KClientGetUserNameDeprecated (
	char*					outUserName)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
void
KClientGetErrorTextDeprecated (
	OSErr					inError,
	char*					outBuffer)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;


/*
 * Warning!
 *
 * The following are K5Client calls. Not only are they deprecated, but they should
 * never have existed in the first place. They are here so that KClient can swallow
 * K5Client (Yummmmmm)
 */
	
OSStatus
K5ClientGetTicketForServiceDeprecated (
	char*			inService,
	void*			outBuffer,
	UInt32*			outBufferLength)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSStatus
K5ClientGetAuthenticatorForServiceDeprecated (
	char*			inService,
	char*			inApplicationVersion,
	void*			outBuffer,
	UInt32*			outBufferLength)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

#if TARGET_OS_MAC
#    pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __KCLIENTDEPRECATED__ */
