/* -*- mode: C; coding: macintosh; -*-
 *
 *	AppleEvent Printer Function
 *	by Jens Peter Alfke
 *
 *	Copyright �1991-1993 Apple Computer, Inc. All rights reserved.
 *
 *	APPLE CONFIDENTIAL
 */


/*
	CHANGE HISTORY:
	 ...zillions of changes from 2/91 until...
	 7/08/93	jpa		Allow for bufStr to be NULL, and return length, to support preflighting.
	 					Don't truncate hex dumps.
	 7/09/93	jpa		No, break length-counting into separate function (AEPrintLength).
	 9/29/93	jpa		Remove calls to vsprintf (cripping floating-point display, oh well...)
	10/11/93	jpa		Display floats by coercing from 'sing' -> 'TEXT'.
	 4/14/94	jpa		Fix warnings for Metrowerks.
	 9/08/94	jpa		Added bufputOSType to fix quoting problems when re-parsing output.
	10/03/94	jpa		Fixed heinous bug in printDescList that was always changing type to
						'aevt' on the way out.
	 3/20/95	jpa		**AEGizmos 1.4
*/


#include <ctype.h>
#include <stdarg.h>
#ifndef __CARBON__
#include <Errors.h>
#include <Packages.h>
#include <NumberFormatting.h>
#include <AppleEvents.h>
#include <CFString.h>
#endif
#include "AEPrintCarbon.h"

typedef struct {
	char *str;
	long len;
} Buf;


static OSErr printDesc( Buf *buf, const register AEDesc *desc );


#ifdef THINK_C
#if !__option(macsbug_names)
	// Always turn on macsbug names for the non-static functions.
	#define NO_NAMES
	#pragma options(macsbug_names)
#endif
#endif


/* AE_PRINT  The big kahuna. Print any AEDesc into bufStr, up to bufLen chars */
OSErr
AEPrint( const AEDesc *desc, char *bufStr, long bufSize )
{
	Buf buf;
	
	if( !desc || !bufStr )
		return paramErr;
	buf.str = bufStr;
	buf.len = bufSize -1;
	bufStr[0] = '\0';
	
	return printDesc(&buf,desc);
}


/* AE_PRINT_SIZE  Compute the size of string (incl. null byte) AEPrint will build. */
OSErr
AEPrintSize( const AEDesc *desc, long *stringLength )
{
	Buf buf;
	OSErr err;
	
	if( !desc || !stringLength )
		return paramErr;
	buf.str = NULL;
	buf.len = 0x7FFFFFF0;
	
	err= printDesc(&buf,desc);
	
	*stringLength = 0x7FFFFFF0 +1 - buf.len;		// Set to size of returned string (length+1)
	return err;
}


#ifdef NO_NAMES
	#undef NO_NAMES
	#pragma options(!macsbug_names)
#endif


/*******************************  THE UTILITY BELT  ************************************/


/* BUF_PUT  Copy data to the buffer */
static void
bufput( register Buf *buf, void *data, register long len )
{
	if( buf->len > 0 ) {
		if( buf->len < len )
			len = buf->len;
		if( buf->str ) {						// Only write data if we have a place for it
                        if ( len > 0 ) {
                            memmove( buf->str, data, len );
                        }
			buf->str += len;
			buf->str[0] = '\0';
		}
		buf->len -= len;
	}
}


/* BUF_PUT_C  Copy a single char to the buffer */
static void
bufputc( register Buf *buf, char c )
{
	if( buf->len > 0 ) {
		buf->len--;
		if( buf->str ) {
			*(buf->str)++ = c;
			buf->str[0] = '\0';
		}
	}
}


/* BUF_PUT_S  Write a C string to the buffer */
static void
bufputs( Buf *buf, register char *str )
{
	register long len = buf->len;
	register char *dst= buf->str;
	
	if( buf->str ) {
		for(; *str && len>0; len-- )
			*dst++ = *str++;
		*dst = '\0';
		buf->str = dst;
	} else
		for(; *str && len>0; len-- )			// No buffer, just count length of str
			str++;
	buf->len = len;
}


/* BUF_PUT_OSTYPE  Write a 4-character code to the buffer */
static void
bufputOSType( Buf *buf, OSType type )
{
	// If type contains any nonalphabetic chars, quote it.
	// $$$$$ This still won't handle embedded single quotes.
	char string[5];
	short i;
	unsigned char c;
	Boolean space=false, alpha=false;
	
	/* Convert OSType to UTF */
	string[0] = (char) (type >> 24);
	string[1] = (char) (type >> 16);
	string[2] = (char) (type >>  8);
	string[3] = (char) (type);
	string[4] = '\0';

	for( i=0; i<4; i++ )
		if( isalpha( c=string[i] ) ) {
			if( space )
				break;						// alpha after a space is bad
			alpha = true;
		} else if( c==' ' && alpha )
			space = true;					// space is ok after alpha chars
		else
			break;

	if( i<4 )
		bufputc(buf,'\'');
	bufput(buf,string,sizeof(type));
	if( i<4 )
		bufputc(buf,'\'');
}


/* BUF_PUT_FLOAT  Write a floating-point number to a buffer */
static OSErr
bufputfloat( Buf *buf, float f )
{
	AEDesc desc, textDesc;
	OSErr err;
	
	err= AECreateDesc(typeIEEE32BitFloatingPoint,&f,sizeof(f), &desc);
	if( err ) return err;
	err= AECoerceDesc(&desc,'TEXT',&textDesc);
	AEDisposeDesc(&desc);
	if( !err ) {
	#if TARGET_API_MAC_CARBON
		long len = AEGetDescDataSize(&textDesc);
		char *theData = ckalloc(len);
		AEGetDescData(&textDesc, theData, len);
		bufput(buf, theData, len);
		ckfree(theData);
	#else
		bufput(buf, *textDesc.dataHandle,GetHandleSize(textDesc.dataHandle));
	#endif
		AEDisposeDesc(&textDesc);
	}
	return err;
}


/***************************  SUPPORT FOR PRINTING EVENTS  ********************************/


static OSErr
getOptionalParams( const AppleEvent *event, AEKeyword** *keys )
{
	OSErr err;
	AEDescList optionals;
	long n, realSize;
	AEKeyword key, optionalKey;
	DescType type;
	
	*keys = NULL;
	err= AEGetAttributeDesc( event, keyOptionalKeywordAttr,typeAEList, &optionals );
	if( err ) {
		if( err==errAEDescNotFound )
			return noErr;
		else
			return err;
	}

	err= AECountItems(&optionals, &n);
	if( err ) goto exit;
	if( n<=0 )
		goto exit;
	*keys = (void*) NewHandle(n*sizeof(AEKeyword));
	if( (err= MemError()) != noErr ) goto exit;
	
	for( ; n>0; n-- ) {
		err= AEGetNthPtr(&optionals,n,typeKeyword, &key,&type,
						 (Ptr)&optionalKey,sizeof(optionalKey),&realSize);
		if( err ) goto exit;
		(**keys)[n-1] = optionalKey;
	}

exit:
	AEDisposeDesc(&optionals);
	if( err && *keys ) {
		DisposeHandle((Handle)*keys);
		*keys = NULL;
	}
	return err;
}

/*******************************  THE MAIN ROUTINES  ***********************************/


/* PRINT_DESC_LIST  Print a descriptor list -- AEDescList, AERecord or AppleEvent */
static OSErr
printDescList( Buf *buf,  AEDescList *desc, DescType originalType )
{
	DescType type = desc->descriptorType;
	long i, size;
	long itemNo = 0;
	AEKeyword keyword;
	AEDesc tempDesc;
	short meta=false, doMeta=false;
	AEKeyword **optionals = NULL;
	OSErr err;
	
	if( type=='aevt' ) {
		AEEventClass eventClass;
		AEEventID eventID;
		DescType realType;
		long realSize;
		
		err= AEGetAttributePtr(	desc, keyEventClassAttr,typeType, &realType,
								(Ptr)&eventClass, sizeof(eventClass), &realSize );
		if( err ) goto exit;
		bufputOSType(buf,eventClass);
		
		err= AEGetAttributePtr(	desc, keyEventIDAttr,typeType, &realType,
								(Ptr)&eventID, sizeof(eventID), &realSize );
		if( err ) goto exit;
		bufputc(buf, '\\');
		bufputOSType(buf,eventID);
		
		err= getOptionalParams(desc, &optionals);
		if( err ) goto exit;
		
		doMeta = true;
		
		bufputc(buf, '{');
		
	} else if( type=='list' )
		bufputc(buf, '[');								/* Open-bracket for list */
		
	else {
		if( originalType != 'reco' )
			bufputOSType(buf, originalType);			/* Show type for coerced record */
		bufputc(buf, '{');								/* Open-brace for record */
	}
	
	// Now get all the items and print them.
	// If this is an Apple event, we will go around twice: one to print the parameters,
	// and again to print the attributes.

		err= AECountItems(desc, &size);
		if( err ) goto exit;
		
		for( i=1; i<=size; i++ ) {							/* Loop through items: */
			err= AEGetNthDesc(desc,i, typeWildCard, &keyword,&tempDesc);
			if( err )
				goto exit;
				
			if( meta && keyword=='optk' )						// Skip optional-params attribute
				continue;
				
			if( itemNo++ >0 )
				bufputs(buf, ", ");
			if( type!='list' ) {
				bufputOSType(buf,keyword);						/* If we're in a record, show key */
				bufputc(buf,':');
			}
			err= printDesc(buf,&tempDesc);						/* Recursively print item */
			AEDisposeDesc(&tempDesc);
			if( err ) goto exit;
		}
		
	    if (doMeta) {
		AEKeyword	attribute[] = {keyOptionalKeywordAttr /* JEG - 13 Nov 2004 - Bug 1217 */,
						keyTransactionIDAttr,  keyReturnIDAttr, 
						keyAddressAttr, keyTimeoutAttr, keyInteractLevelAttr, 
						keyEventSourceAttr, keyMissedKeywordAttr, keyOriginalAddressAttr,
						keyAcceptTimeoutAttr, 
#if TARGET_API_MAC_CARBON						
						keyUserNameAttr, keyUserPasswordAttr, keyDisableAuthenticationAttr, 
						keyXMLDebuggingAttr, kAEUseHTTPProxyAttr, kAEHTTPProxyPortAttr, kAEHTTPProxyHostAttr, 
						kAEUseSocksAttr, kAESocksProxyAttr, kAESocksHostAttr, kAESocksPortAttr, kAESocksUserAttr, kAESocksPasswordAttr,
#endif
						0L};
		
		for( i=0; attribute[i]; i++ ) {							/* Loop through attributes: */
			err = AEGetAttributeDesc(desc, attribute[i], typeWildCard, &tempDesc);
			if (err == errAEDescNotFound) {
				err = noErr;
				continue;
			}
			
			if(err != noErr)
				goto exit;
				
//			if (attribute[i] == keyTransactionIDAttr && 
				
			if( itemNo++ >0 )
				bufputs(buf, ", ");

			bufputc(buf,'&');								// Prefix for metaparam (attribute)
			bufputOSType(buf, attribute[i]);						/* show key */
			bufputc(buf,':');
			
			err= printDesc(buf,&tempDesc);						/* Recursively print item */
			AEDisposeDesc(&tempDesc);
			if( err ) goto exit;
		}
	    }
	
	if( type=='list' )					/* Close list or record */
		bufputc(buf, ']');
	else
		bufputc(buf, '}');

exit:
	if( optionals )
		DisposeHandle((Handle)optionals);
	return err;
}


/* HEX_DUMP_DESC  Official punting routine. Produce hex dump of descriptor */
static OSErr
hexDumpDesc( Buf *buf, const AEDesc *desc )
{
	unsigned char *data;
	long count,n;
	unsigned char byte;
	char hex[] = "0123456789ABCDEF";
	#if TARGET_API_MAC_CARBON
	char *theData;
	#endif
	
	bufputOSType(buf, desc->descriptorType);
	bufputs(buf, "(�");
	
	#if TARGET_API_MAC_CARBON
	count = AEGetDescDataSize(desc);
	theData = ckalloc(count);
	data = (void*)theData;
	AEGetDescData(desc, theData, count);
	#else
	data = (void*)*desc->dataHandle;
	count = GetHandleSize(desc->dataHandle);
	#endif


	n = count;
#ifdef _MAX_HEX_DUMP_LENGTH_
	if( n>_MAX_HEX_DUMP_LENGTH_ )
		n = _MAX_HEX_DUMP_LENGTH_;
#endif
	if( n*2>buf->len )
		n = buf->len>>1;						// No sense going past end of buffer

	while( n-- >0 ) {
		byte = *data++;
		bufputc(buf,hex[byte>>4]);
		bufputc(buf,hex[byte&0x0F]);
	}
#ifdef _MAX_HEX_DUMP_LENGTH_
	if( count>_MAX_HEX_DUMP_LENGTH_ )
		bufputc(buf,'�');
#endif
	bufputs(buf,"�)");
	#if TARGET_API_MAC_CARBON
	ckfree(theData);
	#endif
	return noErr;
}


/*******************************  THE BIG KAHUNA  ************************************/


/* PRINT_DESC  The big kahuna. Print any AEDesc */
static OSErr
printDesc( Buf *buf, const register AEDesc *desc )
{
	OSErr err = noErr;
	AEDesc tempDesc;
	
	#if TARGET_API_MAC_CARBON
	if( AEGetDescDataSize(desc)==0 ) {
	#else
	if( desc->dataHandle==NULL || GetHandleSize(desc->dataHandle)==0 ) {
	#endif
		bufputc(buf,'\'');
		bufputOSType(buf, desc->descriptorType);	/* No data */
		bufputs(buf,"'()");
		return noErr;
	}
	
	switch( desc->descriptorType ) {
		case 'bool':								/* Integer types: */
		case 'shor':
		case 'long':
			err= AECoerceDesc(desc,'long',&tempDesc);		/* Coerce to longint */
			if( !err ) {
                            CFStringRef theString;
                            CFIndex 	len;
                            UInt8	buffer[12];
                            
                            theString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), **(long**)tempDesc.dataHandle);
                            len = CFStringGetLength(theString);
                            CFStringGetBytes(theString, CFRangeMake(0, len), kCFStringEncodingMacRoman, 0, false, buffer, sizeof(buffer), NULL);
                            bufput(buf,buffer,len);
                            CFRelease(theString);
			} else if( err==errAECoercionFail )
				err= hexDumpDesc(buf,desc);
			AEDisposeDesc(&tempDesc);
			break;

		case 'sing':								/* Floating-point types: */
			bufputfloat(buf, **(float**)desc->dataHandle);
			break;
		case 'doub':
			bufputfloat(buf, **(double**)desc->dataHandle);
			break;
		case 'exte':
			bufputfloat(buf, **(long double**)desc->dataHandle);
			break;

		case 'enum':								/* 4-letter code: */
			bufputOSType(buf, **(OSType**)desc->dataHandle);
			break;

		case 'type':								/* 4-letter code as 'type': */
			bufputs(buf,"type(");
			bufputOSType(buf, **(OSType**)desc->dataHandle);
			bufputs(buf,")");
			break;

		case 'TEXT': {								/* Text string: */
			long		len;
			char *		theData;
			int		i;
			
		#if TARGET_API_MAC_CARBON
			len = AEGetDescDataSize(desc);
			theData = ckalloc(len);
			AEGetDescData(desc, theData, len);
		#else
			len = GetHandleSize(desc->dataHandle);
			HLock(desc->dataHandle);
			theData = *desc->dataHandle;
		#endif
			for (i = len - 1; i >= 0; i--) {
			    if (!isprint(theData[i]) || theData[i] == '�' || theData[i] == '�') {
				break;
			    }
			}
																							if (i < 0) {
			    bufputc(buf,'�');
			    bufput(buf, theData, len);
			    bufputc(buf,'�');
			} else {
			    hexDumpDesc(buf, desc);
			}

		#if TARGET_API_MAC_CARBON
			ckfree(theData);
		#else
			HUnlock(desc->dataHandle);
		#endif
			}
			break;

		case 'aevt':								/* Apple event! */
			err= AECoerceDesc(desc,'aevt',&tempDesc);
			if( err==noErr ) {
			err= printDescList(buf,&tempDesc,'aevt');
			} else if( err==errAECoercionFail )
				err= hexDumpDesc(buf,desc);
			AEDisposeDesc(&tempDesc);
			break;
		
		case 'list':								/* AEDescList: */
			err= AECoerceDesc(desc,'list',&tempDesc);
			if( err==noErr ) {
			err= printDescList(buf,&tempDesc,'list');
			} else if( err==errAECoercionFail )
				err= hexDumpDesc(buf,desc);
			AEDisposeDesc(&tempDesc);
			break;
		
		default:									/* AERecord, and everything else: */
			if( desc->descriptorType=='reco' )
				tempDesc = *desc;
			else
				err= AECoerceDesc(desc,'reco',&tempDesc);
			
			if( err==noErr ) {
				err= printDescList(buf,&tempDesc,		/* Made it a record, print it */
									desc->descriptorType);
				if( desc->descriptorType != 'reco' )
					AEDisposeDesc(&tempDesc);
			} else if( err==errAECoercionFail )			/* Couldn't make it a record */
				err= hexDumpDesc(buf,desc);
			break;
	}
	return err;
}
