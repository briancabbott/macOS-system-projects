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
 *  @header SLPRegistrar
 *  class to keep track of all registered services
 */
 
#include <stdio.h>
#include <string.h>
#include <sys/un.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslpd_store.h"
#include "mslp_dat.h"
#include "mslpd.h"
#include "mslpd_mask.h"

#include "mslpd_parse.h"

#include "slpipc.h"
#include "ServiceInfo.h"
#include "SLPRegistrar.h"

#include "SLPComm.h"
#include "CNSLTimingUtils.h"

#ifdef USE_SLPREGISTRAR

SLPRegistrar* 	gsSLPRegistrar = NULL;
int				gTotalServices = 0;

void GetServiceTypeFromURL(	const char* readPtr,
							UInt32 theURLLength,
							char*	URLType );
                            
SLPReturnError HandleRegDereg( SLPBoolean isReg, const char* pcInBuf, int iInSz, struct sockaddr_in* sinIn );

long GetStatelessBootTime( void )
{
    return SLPRegistrar::TheSLPR()->GetStatelessBootTime();
}

long GetCurrentTime( void )
{
    struct	timeval curTime;
    
    if ( gettimeofday( &curTime, NULL ) != 0 )
        LOG_STD_ERROR_AND_RETURN( SLP_LOG_ERR, "call to gettimeofday returned error: %s", errno );
    
    return curTime.tv_sec;
}

SLPReturnError HandleRegistration( const char* pcInBuf, int iInSz, struct sockaddr_in* sinIn )
{
    return HandleRegDereg( SLP_TRUE, pcInBuf, iInSz, sinIn );
}

SLPReturnError HandleDeregistration( const char* pcInBuf, int iInSz, struct sockaddr_in* sinIn )
{
    return HandleRegDereg( SLP_FALSE, pcInBuf, iInSz, sinIn );
}

SLPReturnError HandleRegDereg( SLPBoolean isReg, const char* pcInBuf, int iInSz, struct sockaddr_in* sinIn )
{
    UInt16 			lifeTime;
    const char* 	serviceURL;
    UInt16 			serviceURLLen;
    const char* 	serviceTypePtr;
    UInt16 			serviceTypeLen;
    const char* 	scopeListPtr;
    char			scopeToRegIn[256] = {0};
    UInt16 			scopeListLen;
    const char* 	attributesPtr;
    UInt16 			attributesLen;
    
    ServiceInfo*	newSI = NULL;
    
    SLPReturnError	error = ParseOutRegDereg(	pcInBuf,
                                            iInSz, 
                                            &lifeTime, 
                                            &serviceURL, 
                                            &serviceURLLen, 
                                            &serviceTypePtr,
                                            &serviceTypeLen, 
                                            &scopeListPtr,
                                            &scopeListLen,
                                            &attributesPtr,
                                            &attributesLen );
    
#ifdef ENABLE_SLP_LOGGING
    if ( error )
    {
        SLP_LOG( SLP_LOG_DROP, "Error in Parsing RegDereg header" );
    }
#endif    
    if ( !error )
    {
        memcpy( scopeToRegIn, scopeListPtr, (scopeListLen<sizeof(scopeToRegIn))?scopeListLen:sizeof(scopeToRegIn)-1 );
        
        if ( !list_intersection( SLPGetProperty("com.apple.slp.daScopeList"), scopeToRegIn ) )
        {
            error = SCOPE_NOT_SUPPORTED;
            
#ifdef ENABLE_SLP_LOGGING
            char	errorMsg[512];
            
            sprintf( errorMsg, "Service Agent: %s tried registering in an invalid scope (%s): %s", inet_ntoa(sinIn->sin_addr), scopeToRegIn, slperror(SLP_SCOPE_NOT_SUPPORTED) );
            SLP_LOG( SLP_LOG_DA, errorMsg );
#endif
        }
    }
    
    if ( !error )
    {
        error = CreateNewServiceInfo(	lifeTime, 
                                        serviceURL, 
                                        serviceURLLen, 
                                        serviceTypePtr, 
                                        serviceTypeLen, 
                                        scopeListPtr, 
                                        scopeListLen, 
                                        attributesPtr, 
                                        attributesLen, 
                                        sinIn->sin_addr.s_addr, 
                                        &newSI );
                                        
        if ( error )
            SLP_LOG( SLP_LOG_ERR, "CreateNewServiceInfo returned error" );
    }
    
    if ( !error )
    {    
        SLPRegistrar::Lock();
        
        if ( isReg )
        {
#ifdef ENABLE_SLP_LOGGING
            static int counter = 1;
            
            SLP_LOG( SLP_LOG_MSG, "SLPRegistrar::TheSLPR()->RegisterService called for %dth time", counter++);
#endif            
            SLPRegistrar::TheSLPR()->RegisterService( newSI );
        }
        else
        {
            SLPRegistrar::TheSLPR()->DeregisterService( newSI );
        }
           
        newSI->RemoveInterest();		// need to free this up
         
            
        SLPRegistrar::Unlock();
    }
    
    return error;
}

SLPReturnError DAHandleRequest( SAState *psa, struct sockaddr_in* sinIn, SLPBoolean viaTCP, Slphdr *pslphdr, const char *pcInBuf,
                            int iInSz, char **ppcOutBuf, int *piOutSz, int *piGot )
{
    int 				err;
    SLPReturnError 		result = NO_ERROR;
    SLPInternalError	iErr = SLP_OK;
    char*				previousResponderList = NULL;
    char* 				serviceType = NULL;
    char* 				scopeList = NULL;
    char* 				attributeList = NULL;
    UInt16				serviceTypeLen = 0;
    UInt16				scopeListLen = 0;
    UInt16				attributeListLen = 0;
    
    // first we need to parse this request
	if ((err = srvrqst_in(pslphdr, pcInBuf, iInSz, &previousResponderList, &serviceType, &scopeList, &attributeList))<0)
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DROP,"DAHandleRequest: drop request due to parse in error" );
#endif
    }
    else
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "DAHandleRequest: calling SLPRegistrar::TheSLPR()->CreateReplyToServiceRequest" );
#endif        
        if ( !SDstrcasecmp(serviceType,"service:directory-agent") || !SDstrcasecmp(serviceType,"service:service-agent") )
        {
#ifdef ENABLE_SLP_LOGGING
            if ( !SDstrcasecmp(serviceType,"service:directory-agent") )
            {
                if ( viaTCP )
                    SLP_LOG( SLP_LOG_DA, "received TCP service:directory-agent request from %s", inet_ntoa(sinIn->sin_addr) );
                else
                    SLP_LOG( SLP_LOG_DA, "received service:directory-agent request from %s", inet_ntoa(sinIn->sin_addr) );
            }
            else
            {
                SLP_LOG( SLP_LOG_SR, "service:service-agent from %s", inet_ntoa(sinIn->sin_addr) );
            }
#endif            
            iErr = (SLPInternalError)store_request( psa, viaTCP, pslphdr, pcInBuf, iInSz, ppcOutBuf, piOutSz, piGot);
            
            if ( iErr )
                result = InternalToReturnError( iErr );
        }
        else
        {
#ifdef ENABLE_SLP_LOGGING
	        SLP_LOG( SLP_LOG_SR, "service: %s request from %s", serviceType, inet_ntoa(sinIn->sin_addr) );
#endif
	        if ( serviceType )
	            serviceTypeLen = strlen( serviceType );
	            
	        if ( scopeList )
	            scopeListLen = strlen( scopeList );
	            
	        if ( attributeList )
	            attributeListLen = strlen( attributeList );

	        SLPRegistrar::Lock();

	        result = InternalToReturnError( SLPRegistrar::TheSLPR()->CreateReplyToServiceRequest(	
	                                                                        viaTCP,
	                                                                        pslphdr,
	                                                                        pcInBuf,
	                                                                        iInSz,
	                                                                        serviceType,
	                                                                        serviceTypeLen,
	                                                                        scopeList,
	                                                                        scopeListLen,
	                                                                        attributeList,
	                                                                        attributeListLen,
	                                                                        ppcOutBuf,
	                                                                        piOutSz ) );
	        *piGot = 1;
	        
	        SLPRegistrar::Unlock();
	        
#ifdef ENABLE_SLP_LOGGING
	        if ( !*ppcOutBuf )
	            SLP_LOG( SLP_LOG_ERR, "*ppcOutBuf is NULL after parsing request from %s", inet_ntoa(sinIn->sin_addr) );
            else
                SLP_LOG( SLP_LOG_MSG, "Sending back a %ld size message to %s", *piOutSz, inet_ntoa(sinIn->sin_addr) );
#endif
	   }
   }
    
    if (serviceType) 
        SLPFree((void*)serviceType);
    
    if (previousResponderList) 
        SLPFree((void*)previousResponderList);
    
    if (scopeList) 
        SLPFree((void*)scopeList);
    
    if (attributeList) 
        SLPFree((void*)attributeList);
    
    return result;
}

pthread_mutex_t	SLPRegistrar::msObjectLock;
pthread_mutex_t	SLPRegistrar::msSLPRLock;

Boolean			gsLocksInitiailzed = false;

void TurnOnDA( void )
{
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.isDA\" to true" );
#endif
    SLPSetProperty("com.apple.slp.isDA", "true");
    
    if ( gsSLPRegistrar )
        gsSLPRegistrar->SendRAdminSLPStarted();
}

void TurnOffDA( void )
{
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.isDA\" to false" );
#endif
    SLPSetProperty("com.apple.slp.isDA", "false");
    
    if ( gsSLPRegistrar )
        gsSLPRegistrar->SendRAdminSLPStopped();
}

void InitSLPRegistrar( void )
{
    if ( !gsSLPRegistrar )
    {
        gsSLPRegistrar = new SLPRegistrar();
        gsSLPRegistrar->Initialize();
    }
}

void TearDownSLPRegistrar( void )
{
    if ( gsSLPRegistrar )
    {
        gsSLPRegistrar->SetStatelessBootTime( 0 );			// this tells others that we are shutting down
        gsSLPRegistrar->DoDeregisterAllServices();
    }
}

void ResetStatelessBootTime( void )
{
    if ( gsSLPRegistrar )
    {
        struct timeval          currentTime;
        struct timezone         tz;
        
        gettimeofday( &currentTime, &tz ); 
        gsSLPRegistrar->SetStatelessBootTime( currentTime.tv_sec );		// number of seconds relative to 0h on 1 January 1970
    }
}

// CF Callback function prototypes
CFStringRef SLPCliqueValueCopyDesctriptionCallback ( const void *value );
Boolean SLPCliqueValueEqualCallback ( const void *value1, const void *value2 );
void SLPCliqueHandlerFunction(const void *inKey, const void *inValue, void *inContext);

CFStringRef SLPServiceInfoCopyDesctriptionCallback ( const void *item );
Boolean SLPServiceInfoEqualCallback ( const void *item1, const void *item2 );
CFHashCode SLPServiceInfoHashCallback(const void *key);
void SLPServiceInfoHandlerFunction(const void *inValue, void *inContext);

CFStringRef SLPCliqueValueCopyDesctriptionCallback ( const void *value )
{
   SLPClique*		clique = (SLPClique*)value;
    
    return clique->GetValueRef();
}

Boolean SLPCliqueValueEqualCallback ( const void *value1, const void *value2 )
{
    SLPClique*		clique1 = (SLPClique*)value1;
    SLPClique*		clique2 = (SLPClique*)value2;
    
    return ( clique1 == clique2 || CFStringCompare( clique1->GetValueRef(), clique2->GetValueRef(), kCFCompareCaseInsensitive ) == kCFCompareEqualTo );
}

void SLPCliqueHandlerFunction(const void *inKey, const void *inValue, void *inContext)
{
    SLPClique*					curClique = (SLPClique*)inValue;
    SLPCliqueHandlerContext*	context = (SLPCliqueHandlerContext*)inContext;
    
    switch ( context->message )
    {
        case kDeleteSelf:
            delete curClique;
        break;
        
        case kDeleteCliquesMatchingScope:
            if ( strcmp(curClique->CliqueScopePtr(), (char*)(context->dataPtr)) == 0 )
            {
                ::CFDictionaryRemoveValue( context->dictionary, curClique->GetKeyRef() );		// remove it from the dictionary
                delete curClique;
            }
        break;
        
        
        case kReportServicesToRAdmin:
            curClique->ReportAllServicesToRAdmin();
        break;
        
        case kUpdateCache:
		{
#ifdef ENABLE_SLP_LOGGING
            SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar Updating Cache" );
#endif
            if ( curClique->CacheIsDirty() )
            {
                SLPInternalError status = curClique->UpdateCachedReplyForClique();
            
#ifdef ENABLE_SLP_LOGGING
                if ( status == SLP_DA_BUSY_NOW )
                {
                    SLP_LOG( SLP_LOG_MSG, "SLPRegistrar Updating Cache received a DA_BUSY_NOW error, will postpone update" );
                }
#endif
			}
		}
        break;
        
        case kDoTimeCheckOnTTLs:
        {
            SLPServiceInfoHandlerContext	context = {kDoTimeCheckOnTTLs, curClique};
    
            ::CFSetApplyFunction( curClique->GetSetOfServiceInfosRef(), SLPServiceInfoHandlerFunction, &context );
        }
        break;
    };
}

#pragma mark ��� Public Methods for SLPRegistrar ���
void SLPRegistrar::Lock( void ) 
{ 
    pthread_mutex_lock( &SLPRegistrar::msObjectLock ); 
}

SLPRegistrar::SLPRegistrar()
{
    mListOfSLPCliques = NULL;
	mOneOrMoreCliquesNeedUpdating = false;
	mLastTTLTimeCheck = 0;
	
	mAlreadyReadRegFile = false;
	mStatelessBootTime = 0;
    mRAdminNotifier = NULL;
	mSelfPtr = NULL;
}

SLPRegistrar::~SLPRegistrar()
{
	SLPCliqueHandlerContext	context = {mListOfSLPCliques, kDeleteSelf, this};
    
	mSelfPtr = NULL;
	
    Lock();

	if ( mListOfSLPCliques )
	{
		::CFDictionaryApplyFunction( mListOfSLPCliques, SLPCliqueHandlerFunction, &context );
        ::CFDictionaryRemoveAllValues( mListOfSLPCliques );
        ::CFRelease( mListOfSLPCliques );
        mListOfSLPCliques = NULL;
	}

    Unlock();
}

SLPRegistrar* SLPRegistrar::TheSLPR( void )
{
    return gsSLPRegistrar;
}

void SLPRegistrar::Initialize( void )
{
	if ( !gsLocksInitiailzed )
    {
        // mutex lock initialization
        pthread_mutex_init( &msObjectLock, NULL );
        pthread_mutex_init( &msSLPRLock, NULL );
        gsLocksInitiailzed = true;
    }
    
	// bootstamp initialization
    ResetStatelessBootTime();
    
	// database initialization
    CFDictionaryValueCallBacks	valueCallBack;

    valueCallBack.version = 0;
    valueCallBack.retain = NULL;
    valueCallBack.release = NULL;
    valueCallBack.copyDescription = SLPCliqueValueCopyDesctriptionCallback;
    valueCallBack.equal = SLPCliqueValueEqualCallback;
    
    mListOfSLPCliques = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &valueCallBack);

	mSelfPtr = this;
}

#pragma mark -
void SLPRegistrar::EnableRAdminNotification( void )
{
	SLPSetProperty( "com.apple.slp.RAdminNotificationsEnabled", "true" );
    
    if ( !mRAdminNotifier )
    {
        mRAdminNotifier = new SLPRAdminNotifier();
        
        if ( mRAdminNotifier )
        {
            mRAdminNotifier->Resume();
        }
        else
            SLP_LOG( SLP_LOG_ERR, "SLPRegistrar couldn't create its Notifier Thread!" );
    }
    // now we should send all currently registered services back to ServerAdmin
    if ( mRAdminNotifier )
        mRAdminNotifier->RepostAllRegisteredData();
}

Boolean SLPRegistrar::IsServiceAlreadyRegistered( ServiceInfo* service )
{
    Boolean 	isAlreadyRegistered = false;
	
	if ( service )
	{
		SLPClique* clique = FindClique( service );
			
		if ( clique )
			isAlreadyRegistered = clique->IsServiceInfoInClique( service );
	}
	
	return isAlreadyRegistered;
}

SLPInternalError SLPRegistrar::RegisterService( ServiceInfo* service )
{
	
	SLPInternalError	status = SLP_OK;
	SLPClique*	clique = NULL;

#ifdef USE_EXCEPTIONS
	try
#endif
	{
		if ( service )
		{			
			clique = FindClique( service );
			
			if ( clique )
			{
				status = clique->AddServiceInfoToClique( service );
                
				if ( !status )
					mOneOrMoreCliquesNeedUpdating = true;
			}
			else
			{
#ifdef USE_EXCEPTIONS
				try
#endif
				{
					clique = new SLPClique();
#ifdef USE_EXCEPTIONS
					ThrowIfNULL_( clique );
#endif					
					if ( clique )
					{
						clique->Initialize( service );
						
						::CFDictionaryAddValue( mListOfSLPCliques, clique->GetKeyRef(), clique );
						
						status = clique->AddServiceInfoToClique( service );
						
						if ( !status )
							mOneOrMoreCliquesNeedUpdating = true;
					}
				}
				
#ifdef USE_EXCEPTIONS
				catch ( int inErr )
				{
					status = (SLPInternalError)inErr;
					if ( clique )
						delete clique;
				}
#endif
			}
		}
#ifdef ENABLE_SLP_LOGGING
        else
            SLP_LOG( SLP_LOG_DA, "SLPRegistrar::RegisterService was passed a NULL service!");
#endif
	}
	
#ifdef USE_EXCEPTIONS
	catch ( int inErr )
	{
		status = (SLPInternalError)inErr;
	}
#endif		
	return status;
}

SLPInternalError SLPRegistrar::DeregisterService( ServiceInfo* service )
{
	
	SLPInternalError	status = SLP_OK;
	
#ifdef USE_EXCEPTIONS
	try
#endif
	{
		if ( service && service->SafeToUse() )
		{			
			SLPClique* clique = FindClique( service );
			
			if ( clique )
			{
				status = clique->RemoveServiceInfoFromClique( service );
			
				if ( clique->GetSetOfServiceInfosRef() == NULL )		// no more services, get rid of clique
				{
					::CFDictionaryRemoveValue( mListOfSLPCliques, clique->GetKeyRef() );
					
					delete clique;
				}
				
			}
			else
				status = SERVICE_NOT_REGISTERED;
		}
	}
	
#ifdef USE_EXCEPTIONS
	catch ( int inErr )
	{
        SLP_LOG( SLP_LOG_ERR, "SLPRegistrar::DeregisterService Caught_ inErr: %ld", (SLPInternalError)inErr );

		status = (SLPInternalError)inErr;
	}
#endif		
#ifdef ENABLE_SLP_LOGGING
	SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar::DeregisterService finished, returning status: %d", status );
#endif
	return status;
}

SLPInternalError SLPRegistrar::RemoveScope( char* scope )
{
	SLPCliqueHandlerContext	context = {mListOfSLPCliques,kDeleteCliquesMatchingScope, scope};
    SLPInternalError				error = SLP_OK;
		
    Lock();

	if ( mListOfSLPCliques )
	{
    	::CFDictionaryApplyFunction( mListOfSLPCliques, SLPCliqueHandlerFunction, &context );
    }
    
    if ( RAdminNotificationEnabled() )
    {
        SendRAdminDeletedScope( scope );
    }

    Unlock();
    
    return error;
}

void SLPRegistrar::AddNotification( char* buffer, UInt32 bufSize )
{
    if ( mRAdminNotifier )
        mRAdminNotifier->AddNotificationToQueue( buffer, bufSize );
}

void SLPRegistrar::DoSendRAdminAllCurrentlyRegisteredServices( void )
{
    if ( mRAdminNotifier )
        mRAdminNotifier->RepostAllRegisteredData();
}

void SLPRegistrar::SendRAdminSLPStarted( void )
{
    UInt32		dataSendBufferLen;
    char*		dataSendBuffer = NULL;
    
    MakeSLPDAStatus( kSLPDARunning, &dataSendBufferLen, &dataSendBuffer  );

    AddNotification( dataSendBuffer, dataSendBufferLen );		// mRAdminNotifier takes control of buffer free
}

void SLPRegistrar::SendRAdminSLPStopped( void )
{
    UInt32		dataSendBufferLen;
    char*		dataSendBuffer = NULL;
    
    MakeSLPDAStatus( kSLPDANotRunning, &dataSendBufferLen, &dataSendBuffer  );

    AddNotification( dataSendBuffer, dataSendBufferLen );		// mRAdminNotifier takes control of buffer free
}

void SLPRegistrar::SendRAdminDeleteAllScopes( void )
{
    char*			pcScope = NULL;
    const char*		pcList = SLPGetProperty("com.apple.slp.daScopeList");
    int				offset=0;
    char			c;
    
    while( (pcScope = get_next_string(",",pcList,&offset,&c)) )
    {
#ifdef ENABLE_SLP_LOGGING
        mslplog( SLP_LOG_RADMIN, "SendRAdminDeleteAllScopes removing scope: ", pcScope );
#endif
        RemoveScope( pcScope );
        
        free(pcScope);
    }
}

void SLPRegistrar::SendRAdminAllScopes( void )
{
    char*			pcScope = NULL;
    const char*		pcList = SLPGetProperty("com.apple.slp.daScopeList");
    int				offset=0;
    char			c;
    
    while( (pcScope = get_next_string(",",pcList,&offset,&c)) )
    {
        SendRAdminAddedScope( pcScope );
        
        free(pcScope);
    }
}

void SLPRegistrar::SendRAdminAddedScope( const char* newScope )
{
    UInt32		dataSendBufferLen = sizeof(SLPdMessageHeader) + strlen(newScope);
    char*		dataSendBuffer = (char*)malloc(dataSendBufferLen);
    
    SLPdMessageHeader*	header = (SLPdMessageHeader*)dataSendBuffer;
    header->messageType = kSLPAddScope;
    header->messageLength = dataSendBufferLen;
    header->messageStatus = 0;

    char* curPtr = (char*)header + sizeof(SLPdMessageHeader);

    ::memcpy( curPtr, newScope, strlen(newScope) );

    AddNotification( dataSendBuffer, dataSendBufferLen );		// mRAdminNotifier takes control of buffer free
}

void SLPRegistrar::SendRAdminDeletedScope( const char* oldScope )
{
    UInt32		dataSendBufferLen = sizeof(SLPdMessageHeader) + strlen(oldScope);
    char*		dataSendBuffer = (char*)malloc(dataSendBufferLen);
    
    SLPdMessageHeader*	header = (SLPdMessageHeader*)(dataSendBuffer);
    header->messageType = kSLPDeleteScope;
    header->messageLength = dataSendBufferLen;
    header->messageStatus = 0;

    char* curPtr = (char*)header + sizeof(SLPdMessageHeader);

    ::memcpy( curPtr, oldScope, strlen(oldScope) );

    AddNotification( dataSendBuffer, dataSendBufferLen );		// mRAdminNotifier takes control of buffer free
}

void SLPRegistrar::SendRAdminAllCurrentlyRegisteredServices( void )
{
	SLPCliqueHandlerContext	context = {mListOfSLPCliques, kReportServicesToRAdmin, NULL};
    
	Lock();
    
	if ( mListOfSLPCliques )
		::CFDictionaryApplyFunction( mListOfSLPCliques, SLPCliqueHandlerFunction, &context );
    
	Unlock();
}

void SLPRegistrar::DoDeregisterAllServices( void )
{
    if ( mRAdminNotifier )
        mRAdminNotifier->DeregisterAllRegisteredData();
}

void SLPRegistrar::DeregisterAllServices( void )
{
	SLPCliqueHandlerContext	context = {mListOfSLPCliques, kDeleteSelf, NULL};
    
    Lock();

	if ( mListOfSLPCliques )
	{
    	::CFDictionaryApplyFunction( mListOfSLPCliques, SLPCliqueHandlerFunction, &context );
        ::CFDictionaryRemoveAllValues( mListOfSLPCliques );

// don't get rid of mListOfSLPCliques, we could just be turning our selves off.  We can release it in our destructor
    }
    
    Unlock();
}

SLPInternalError SLPRegistrar::CreateReplyToServiceRequest(	
											SLPBoolean viaTCP,
                                            Slphdr *pslphdr,
                                            const char* originalHeader, 
											UInt16 originalHeaderLength,
											char* serviceType,
											UInt16 serviceTypeLen,
											char* scopeList,
											UInt16 scopeListLen,
											char* attributeList,
											UInt16 attributeListLen,
											char** returnBuffer, 
                                            int *piOutSz )
{
	
	SLPInternalError			error = SLP_OK;	// ok, set this as default
	UInt16				returnBufferLen;
	char*				tempPtr = NULL;
	char*				endPtr = NULL;
    int 				iMTU = strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10);
    int 				iOverflow = 0;
	SLPClique*			matchingClique = NULL;

#ifdef ENABLE_SLP_LOGGING
	SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar::CreateReplyToServiceRequest called" );
#endif 
	matchingClique = FindClique( serviceType, serviceTypeLen, scopeList, scopeListLen, attributeList, attributeListLen );
	if ( returnBuffer && matchingClique )
	{
		if ( matchingClique->CacheIsDirty() )
			error = matchingClique->UpdateCachedReplyForClique();	// ok to call here as we are allowed to allocate memory
			
#ifdef ENABLE_SLP_LOGGING
		if ( !error )
			SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar::CreateReplyToServiceRequest(), Copying Cached Return Data" );
#endif
        *piOutSz = matchingClique->GetCacheSize();
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar::CreateReplyToServiceRequest for %s in %s, *piOutSz: %ld", serviceType, scopeList, *piOutSz );
#endif        
        if ( !viaTCP && *piOutSz > iMTU )	// just send 0 results and set the overflow bit
        {
            *piOutSz = GETHEADERLEN(originalHeader) + 4;
            iOverflow = 1;
#ifdef ENABLE_SLP_LOGGING
            SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar::CreateReplyToServiceRequest(), sending back overflow message" );
#endif
        }
        
		if ( !error )
			tempPtr = safe_malloc(*piOutSz, 0, 0);
		
		if( !tempPtr ) 
			error = SLP_PARAMETER_BAD;
		
		if ( !error )
		{
        	if ( iOverflow )
            {
                /* parse header out */
                SETVER(tempPtr,2);
                SETFUN(tempPtr,SRVRPLY);
                SETLEN(tempPtr,*piOutSz);
                SETLANG(tempPtr,pslphdr->h_pcLangTag);
                SETFLAGS(tempPtr,OVERFLOWFLAG);
            }
            else
                error = matchingClique->CopyCachedReturnData( tempPtr, matchingClique->GetCacheSize(), &returnBufferLen );
        }
        
		if ( !error )
			SETXID(tempPtr, pslphdr->h_usXID );
        
		if ( error )
			LOG_SLP_ERROR_AND_RETURN( SLP_LOG_ERR, "SLPRegistrar::CreateReplyToServiceRequest, CopyCachedReturnData returned error", error );
	
        if ( !error )
            *returnBuffer = tempPtr;
	}
	else if ( returnBuffer )
	{
		error = SLP_OK;		// we have nothing registered here yet, so just reply with no results
		
#ifdef ENABLE_SLP_LOGGING
		SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar::CreateReplyToServiceRequest(), sending back empty result" );
#endif		
		*piOutSz = GETHEADERLEN(originalHeader) + 4;
		tempPtr = safe_malloc(*piOutSz, 0, 0);
		
		if ( tempPtr )
		{
			SETVER(tempPtr,2);
			SETFUN(tempPtr,SRVRPLY);
			SETLEN(tempPtr,*piOutSz);
			SETLANG(tempPtr,pslphdr->h_pcLangTag);
		}
		else
			error = SLP_PARAMETER_BAD;
			
		*returnBuffer = tempPtr;
	}
#ifdef ENABLE_SLP_LOGGING
	else
	{
        SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar::CreateReplyToServiceRequest, returnBuffer is NULL!" );
	}
#endif
	return error;
}

#pragma mark -
#pragma mark -
SLPClique* SLPRegistrar::FindClique( ServiceInfo* service )
{
	
	UInt16				serviceTypeLen = strlen(service->PtrToServiceType( &serviceTypeLen ));
	SLPClique*	foundClique = FindClique( service->PtrToServiceType( &serviceTypeLen ), serviceTypeLen, service->GetScope(), service->GetScopeLen(), service->GetAttributeList(), service->GetAttributeListLen() );
	
	return foundClique;
}

void SLPRegistrar::UpdateDirtyCaches( void )
{
#ifdef USE_EXCEPTIONS
	try
#endif
	{
			
        SLPCliqueHandlerContext	context = {mListOfSLPCliques, kUpdateCache, NULL};
        
        Lock();
    
        if ( mListOfSLPCliques && mOneOrMoreCliquesNeedUpdating )
        {
            ::CFDictionaryApplyFunction( mListOfSLPCliques, SLPCliqueHandlerFunction, &context );
		}
        
        mOneOrMoreCliquesNeedUpdating = false;	// done
        
        Unlock();
	}
	
#ifdef USE_EXCEPTIONS
	catch ( int inErr )
	{
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DA, "SLPRegistrar::UpdateDirtyCaches Caught Err: %d", (SLPInternalError)inErr );
#endif
	}
#endif
}

#ifdef DO_TIME_CHECKS_ON_TTLS
SLPInternalError SLPRegistrar::DoTimeCheckOnTTLs( void )
{
	SLPInternalError		status = SLP_OK;
	
	// only check this every kTimeBetweenTTLTimeChecks ticks
	if ( GetCurrentTime() > mLastTTLTimeCheck + kTimeBetweenTTLTimeChecks )
	{
		// We have two basic functions we want to accomplish here:
		// 1) if we are a DA, then we need to dereg any services that have expired
		// 2) if we are not a DA, then we need to rereg any services that are close
		//		to expiring (assuming we have registered them with another DA)
        SLPCliqueHandlerContext	context = {mListOfSLPCliques, kDoTimeCheckOnTTLs, NULL};
        
        Lock();
    
        if ( mListOfSLPCliques && mOneOrMoreCliquesNeedUpdating )
        {
            ::CFDictionaryApplyFunction( mListOfSLPCliques, SLPCliqueHandlerFunction, &context );
		}

        Unlock();

		mLastTTLTimeCheck = GetCurrentTime();
	}
	
	return status;
}
#endif //#ifdef DO_TIME_CHECKS_ON_TTLS

#pragma mark ��� Protected Methods for SLPRegistrar ���

SLPClique* SLPRegistrar::FindClique( 	char* serviceType,
                                                UInt16 serviceTypeLen,
                                                char* scopeList,
                                                UInt16 scopeListLen,
                                                char* attributeList,
                                                UInt16 attributeListLen )
{
	SLPClique*				curClique = NULL;
	CFStringRef				keyRef;
    char					keyString[512] = {0};
    
    if ( mListOfSLPCliques )
    {
        memcpy( keyString, serviceType, serviceTypeLen );
        memcpy( keyString+serviceTypeLen, "://", 3 );
        memcpy( keyString+serviceTypeLen+3, scopeList, scopeListLen );
        
        keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, keyString, kCFStringEncodingUTF8 );

        if( keyRef )
		{
			if ( ::CFDictionaryGetCount( mListOfSLPCliques ) > 0 && ::CFDictionaryContainsKey( mListOfSLPCliques, keyRef ) )
				curClique = (SLPClique*)::CFDictionaryGetValue( mListOfSLPCliques, keyRef );
#ifdef ENABLE_SLP_LOGGING
			else
			{
				SLP_LOG( SLP_LOG_DROP, "Unable to find SLPClique for request %s in %s.  Dictionary count: %d", serviceType, scopeList,  ::CFDictionaryGetCount( mListOfSLPCliques ) );
			}
#endif		
			::CFRelease( keyRef );
		}
    }
    
	return curClique;
}

#pragma mark -
CFStringRef SLPServiceInfoCopyDesctriptionCallback ( const void *item )
{
    ServiceInfo*		serviceInfo = (ServiceInfo*)item;
    
    return serviceInfo->GetURLRef();
}

Boolean SLPServiceInfoEqualCallback ( const void *item1, const void *item2 )
{
    ServiceInfo*		serviceInfo1 = (ServiceInfo*)item1;
    ServiceInfo*		serviceInfo2 = (ServiceInfo*)item2;
    
    return ( serviceInfo1 == serviceInfo2 || *serviceInfo1 == *serviceInfo2 );
}

#define HAVENT_TESTED_HASH_CALLBACK
CFHashCode SLPServiceInfoHashCallback(const void *key)
{
#ifdef HAVENT_TESTED_HASH_CALLBACK
    return 2;
#elif	
	// this should really be something like the following (but haven't tested it yet)
	ServiceInfo*		serviceInfo = (ServiceInfo*)key;
	
	if ( serviceInfo->GetURLRef() )
		return CFHash(serviceInfo->GetURLRef());
	else	
		return 0;
#endif
}

void SLPServiceInfoHandlerFunction(const void *inValue, void *inContext)
{
    ServiceInfo*					curServiceInfo = (ServiceInfo*)inValue;
    SLPServiceInfoHandlerContext*	context = (SLPServiceInfoHandlerContext*)inContext;
    SLPClique*						clique = (SLPClique*)context->dataPtr;
    
    switch ( context->message )
    {
        case kDeleteSelf:
            clique->RemoveServiceInfoFromClique( curServiceInfo );		// this will do the right clean up and notify RAdmin
        break;
        
        case kUpdateCache:
            clique->AddToCachedReply( curServiceInfo );
        break;
        
        case kReportServicesToRAdmin:
            clique->NotifyRAdminOfChange( kServiceAdded, curServiceInfo );
        break;
        
        case kDoTimeCheckOnTTLs:
            if ( AreWeADirectoryAgent() )
            {
                if ( curServiceInfo->IsTimeToExpire() )
                {
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_EXP, "Service URL Expired: URL=%s, SCOPE= %s", curServiceInfo->GetURLPtr(), curServiceInfo->GetScope() );
#endif
                    clique->RemoveServiceInfoFromClique(curServiceInfo);
                }
            }
        break;
        
        default:
#ifdef ENABLE_SLP_LOGGING
            SLP_LOG( SLP_LOG_DEBUG, "SLPServiceInfoHandlerFunction - received unhandled message type!" );
#endif
        break;
    };
}

#pragma mark ��� Public Methods for SLPClique ���
SLPClique::SLPClique()
{
	mSelfPtr = NULL;
	mSetOfServiceInfos = NULL;
	mCliqueScope = NULL;
	mCliqueServiceType = NULL;
	mCachedReplyForClique = NULL;
	mCacheSize = 0;
	mLastRegDeregistrationTime = 0;
	mIsCacheDirty = true;
	mNotifyRAdminOfChanges = false;
    mKeyRef = NULL;
}

SLPClique::~SLPClique()
{
	
	if ( mSetOfServiceInfos )
	{
        SLPServiceInfoHandlerContext	context = {kDeleteSelf, this};

    	::CFSetApplyFunction( mSetOfServiceInfos, SLPServiceInfoHandlerFunction, &context );
        ::CFSetRemoveAllValues( mSetOfServiceInfos );
        ::CFRelease( mSetOfServiceInfos );
        mSetOfServiceInfos = NULL;
	}

	if ( mCliqueScope )
	{
		free( mCliqueScope );
		mCliqueScope = NULL;
	}

	if ( mCliqueServiceType )
	{
		free( mCliqueServiceType );
		mCliqueServiceType = NULL;
	}

	if ( mCachedReplyForClique )
	{
		free( mCachedReplyForClique );
		mCachedReplyForClique = NULL;
	}
    
    if ( mKeyRef )
    {
        CFRelease( mKeyRef );
        mKeyRef = NULL;
    }
}

void SLPClique::IntitializeInternalParams( ServiceInfo* exampleSI )
{
#ifdef USE_EXCEPTIONS
	ThrowIfNULL_( exampleSI );

	try
#else
	if ( !exampleSI || !exampleSI->GetScope() )
		return;
#endif
	{
#ifdef USE_EXCEPTIONS
		ThrowIfNULL_( exampleSI->GetScope() );
#endif
		mCliqueScope = (char*)malloc( exampleSI->GetScopeLen()+1 );			// scope for this clique
#ifdef USE_EXCEPTIONS
		ThrowIfNULL_( mCliqueScope );
#endif
		if ( mCliqueScope )
		{
			::strcpy( mCliqueScope, exampleSI->GetScope() );
	
			UInt16		serviceTypeLen;
			char*		serviceTypePtr = exampleSI->PtrToServiceType(&serviceTypeLen) ;
	
			mCliqueServiceType = (char*)malloc( serviceTypeLen+1 );
#ifdef USE_EXCEPTIONS
			ThrowIfNULL_( mCliqueServiceType );
#endif
			if ( mCliqueServiceType )
			{
				::memcpy( mCliqueServiceType, serviceTypePtr, serviceTypeLen );
				mCliqueServiceType[serviceTypeLen] = '\0';
				
				char		keyString[512];
				sprintf( keyString, "%s://%s", mCliqueServiceType, mCliqueScope );

				mKeyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, keyString, kCFStringEncodingUTF8 );       
			}
		}
    }
	
#ifdef USE_EXCEPTIONS
	catch ( int inErr )
	{
		if ( mCliqueScope )
		{
			free( mCliqueScope );
			mCliqueScope = NULL;
		}
		
		if ( mCliqueServiceType )
		{
			free( mCliqueServiceType );
			mCliqueServiceType = NULL;
		}
		
		Throw_( inErr );
	}
#endif
}	

void SLPClique::Initialize( ServiceInfo* newService )
{
#ifdef USE_EXCEPTIONS
	ThrowIfNULL_( newService );
#endif
	if ( !newService )
		return;
		
	// database initialization
	CFSetCallBacks	callBack;
    
    callBack.version = 0;
    callBack.retain = NULL;
    callBack.release = NULL;
    callBack.copyDescription = SLPServiceInfoCopyDesctriptionCallback;
    callBack.equal = SLPServiceInfoEqualCallback;
    callBack.hash = SLPServiceInfoHashCallback;
    
    mSetOfServiceInfos = ::CFSetCreateMutable( NULL, 0, &callBack );

	IntitializeInternalParams( newService );
	
	mSelfPtr = this;
}
	
#pragma mark -
Boolean SLPClique::IsServiceInfoOKToJoinClique( ServiceInfo* service )
{
	
#ifdef USE_EXCEPTIONS
	ThrowIfNULL_( service );
	ThrowIfNULL_( service->GetScope() );
#endif
	if ( !service || !service->GetScope() )
		return false;
		
	UInt16		serviceTypeLen;
	
	Boolean		okToJoin = false;

	if ( this->ServiceTypeMatchesClique( service->PtrToServiceType( &serviceTypeLen ), serviceTypeLen ) )
	{
		if ( this->ScopeMatchesClique( service->GetScope(), ::strlen( service->GetScope() ) ) )		// now what if this is a scopeList?????
		{		
			okToJoin = true;
		}
	}
	
	return okToJoin;
}

Boolean SLPClique::IsServiceInfoInClique( ServiceInfo* service )
{
	
	Boolean			serviceFound = false;
    
    if ( mSetOfServiceInfos && service && ::CFSetGetCount( mSetOfServiceInfos ) > 0 )
        serviceFound = ::CFSetContainsValue( mSetOfServiceInfos, service );
		
	return serviceFound;
}

void SLPClique::NotifyRAdminOfChange( ChangeType type, ServiceInfo* service )
{
    if ( SLPRegistrar::TheSLPR()->RAdminNotificationEnabled() && AreWeADirectoryAgent() )
    {
        char*		dataSendBuffer = NULL;
        UInt32		dataSendBufferLen = 0;
        OSStatus	status = noErr;
        
        if ( type == kServiceAdded )
            status = MakeSLPServiceInfoAddedNotificationBuffer( service, &dataSendBufferLen, &dataSendBuffer );
        else
            status = MakeSLPServiceInfoRemovedNotificationBuffer( service, &dataSendBufferLen, &dataSendBuffer );
           
// let's just push this onto a queue for a central thread to process...
        SLPRegistrar::TheSLPR()->AddNotification( dataSendBuffer, dataSendBufferLen );		// mRAdminNotifier takes control of buffer free

#ifdef ENABLE_SLP_LOGGING
        if ( status )
        {
            SLP_LOG( SLP_LOG_DROP, "Error notifing ServerAdmin of Service Change: %s", strerror(status) );
        }
#endif
    }
}

SLPInternalError SLPClique::AddServiceInfoToClique( ServiceInfo* service )
{
	
    SLPInternalError			status = SLP_OK;
    ServiceInfo*		siToNotifyWith = service;
    
    if ( IsServiceInfoInClique( service ) )
    {
        status = SERVICE_ALREADY_REGISTERED;
        siToNotifyWith = (ServiceInfo*)::CFSetGetValue( mSetOfServiceInfos, service );
    }
    else if ( mSetOfServiceInfos )
        CFSetAddValue( mSetOfServiceInfos, service );

    if ( !status )
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_REG, "New Service Registered, URL=%s. SCOPE=%s", service->GetURLPtr(), service->GetScope() );

        SLP_LOG( SLP_LOG_MSG, "Total Services: %d", ++gTotalServices );
#endif    
	// increment the usage count for the serviceInfo
        service->AddInterest();

        mIsCacheDirty = true;
        
        mLastRegDeregistrationTime = GetCurrentTime();
    }
    else
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_MSG, "Duplicate Service, update timestamp, URL=%s, SCOPE=%s", service->GetURLPtr(), service->GetScope() );    
#endif					
        // We need to update the reg time (in future, need to support partial registration!)
        siToNotifyWith->UpdateLastRegistrationTimeStamp();
    }
        
    NotifyRAdminOfChange( kServiceAdded, siToNotifyWith );
        	
	return SLP_OK;		// don't need to return error
}

SLPInternalError SLPClique::RemoveServiceInfoFromClique( ServiceInfo* service )
{
	
	ServiceInfo*	serviceInClique = NULL;		// the SI we are passed in may not be the exact object...
	SLPInternalError		status = SLP_OK;
	
    if ( IsServiceInfoInClique( service ) )
        serviceInClique = (ServiceInfo*)::CFSetGetValue( mSetOfServiceInfos, service );

    if ( serviceInClique )
    {
        ::CFSetRemoveValue( mSetOfServiceInfos, serviceInClique );
        
        {
#ifdef ENABLE_SLP_LOGGING
            SLP_LOG( SLP_LOG_REG, "Service Deregistered, URL=%s, SCOPE=%s", service->GetURLPtr(), service->GetScope() );

            SLP_LOG( SLP_LOG_MSG, "Total Services: %d", --gTotalServices );
#endif
        }
        
        NotifyRAdminOfChange( kServiceRemoved, service );

		// decrement the usage count for the serviceInfo
		serviceInClique->RemoveInterest();
	
		mIsCacheDirty = true;
		mLastRegDeregistrationTime = GetCurrentTime();
    }
    else
    {
		status = SERVICE_NOT_REGISTERED;
	}
	
	return status;
}

SLPInternalError SLPClique::UpdateRegisteredServiceTimeStamp( ServiceInfo* service )
{
	SLPInternalError		status = SERVICE_NOT_REGISTERED;
	
	ServiceInfo*			serviceInClique = NULL;
	
    if ( IsServiceInfoInClique( service ) )
        serviceInClique = (ServiceInfo*)::CFSetGetValue( mSetOfServiceInfos, service );

    if ( serviceInClique )
    {
        serviceInClique->UpdateLastRegistrationTimeStamp();
        status = SLP_OK;
    }
#ifdef ENABLE_SLP_LOGGING
    else
    {
        SLP_LOG( SLP_LOG_DA, "SLPClique::UpdateRegisteredServiceTimeStamp couldn't find service to update!, URL=%s, SCOPE=%s", service->GetURLPtr(), service->GetScope() );
    }
#endif    
    return status;
}

Boolean SLPClique::ServiceTypeMatchesClique( char* serviceType, UInt16 serviceTypeLen )
{
#ifdef USE_EXCEPTIONS
    ThrowIfNULL_( mCliqueServiceType );
    ThrowIfNULL_( serviceType );
#endif
	if ( !mCliqueServiceType || !serviceType )
		return false;
		
    Boolean		match = false;
	
    if ( serviceTypeLen == ::strlen( mCliqueServiceType ) )
        match = ( ::memcmp( mCliqueServiceType, serviceType, serviceTypeLen ) == 0 );
		
    return match;
}

Boolean SLPClique::ScopeMatchesClique( char* scopePtr, UInt16 scopeLen )
{
#ifdef USE_EXCEPTIONS
    ThrowIfNULL_( mCliqueScope );
    ThrowIfNULL_( scopePtr );
#endif
	if ( !mCliqueScope || !scopePtr )
		return false;
		
    Boolean		match = false;
    
    if ( scopeLen == ::strlen( mCliqueScope ) )
        match = ( ::memcmp( mCliqueScope, scopePtr, scopeLen ) == 0 );
            
    return match;
}

#pragma mark -
SLPInternalError SLPClique::UpdateCachedReplyForClique( ServiceInfo* addNewServiceInfo )
{
    SLPInternalError		error = SLP_OK;
    
    if ( addNewServiceInfo )
    {
#ifdef ENABLE_SLP_LOGGING
        {
            SLP_LOG( SLP_LOG_DEBUG, "SLPClique::UpdateCachedReplyForClique is calling AddToServiceReply for clique: SCOPE=%s, TYPE=%s", mCliqueScope, mCliqueServiceType );
        }
#endif
        // ok, just append the new one.
        AddToServiceReply( mNeedToUseTCP, addNewServiceInfo, NULL, 0, error, &mCachedReplyForClique );
                        
        if ( error == SLP_REPLY_TOO_BIG_FOR_PROTOCOL )
        {
            error = SLP_OK;
            mNeedToUseTCP = true;
            AddToServiceReply( mNeedToUseTCP, addNewServiceInfo, NULL, 0, error, &mCachedReplyForClique );	// its ok, add it now and TCP is set
        }
    }
    else
    {
        // this isn't just adding a new one, recalc from scratch
#ifdef ENABLE_SLP_LOGGING
        {
            SLP_LOG( SLP_LOG_DEBUG, "SLPClique::UpdateCachedReplyForClique is rebuilding its cached data for clique: SCOPE=%s, TYPE=%s", mCliqueScope, mCliqueServiceType );
        }
#endif
        if ( mCachedReplyForClique )
            free( mCachedReplyForClique );
            
        mCachedReplyForClique = NULL;
        mNeedToUseTCP = false;			// reset this
        
        SLPServiceInfoHandlerContext	context = {kUpdateCache, this};

    	::CFSetApplyFunction( mSetOfServiceInfos, SLPServiceInfoHandlerFunction, &context );
    }
	
    if ( mCachedReplyForClique )
        mCacheSize = GETLEN( mCachedReplyForClique );
    else
        mCacheSize = 0;

    if ( !error )
        mIsCacheDirty = false;

    return error;
}

void SLPClique::AddToCachedReply( ServiceInfo* curSI )
{
    SLPInternalError		error = SLP_OK;
    
    AddToServiceReply( mNeedToUseTCP, curSI, NULL, 0, error, &mCachedReplyForClique );
                            
    if ( error == SLP_REPLY_TOO_BIG_FOR_PROTOCOL )
    {
        error = SLP_OK;
        mNeedToUseTCP = true;
        AddToServiceReply( mNeedToUseTCP, curSI, NULL, 0, error, &mCachedReplyForClique );	// its ok, add it now and TCP is set
    }
#ifdef ENABLE_SLP_LOGGING
    else if ( error )
        mslplog( SLP_LOG_DEBUG, "ServiceAgent::UpdateCachedReplyForClique(), AddToServiceReply() returned:", strerror(error) );
#endif
}

UInt16 SLPClique::GetCacheSize()
{
	
    return mCacheSize;
}

SLPInternalError SLPClique::GetPtrToCacheWithXIDFromCurrentRequest( char* buffer, UInt16 length, char** serviceReply, UInt16* replyLength )
{
    SLPInternalError status = SLP_OK;
    
    if ( !mIsCacheDirty )
    {
        CopyXIDFromRequestToCachedReply( (ServiceLocationHeader*)buffer, (ServiceLocationHeader*)mCachedReplyForClique );
        
        *serviceReply = mCachedReplyForClique;
        *replyLength = mCacheSize;
    }
    else
        status = (SLPInternalError)DA_BUSY_NOW;
            
    return status;
}

#pragma mark -
void SLPClique::ReportAllServicesToRAdmin( void )
{
	SLPServiceInfoHandlerContext	context = {kReportServicesToRAdmin, this};

	if ( mSetOfServiceInfos )
		::CFSetApplyFunction( mSetOfServiceInfos, SLPServiceInfoHandlerFunction, &context );
}

SLPInternalError SLPClique::CopyCachedReturnData( char* returnBuffer, UInt16 maxBufferLen, UInt16* returnBufLen )
{
#ifdef USE_EXCEPTIONS
	ThrowIfNULL_( returnBuffer );
	ThrowIfNULL_( returnBufLen );
#endif
	if ( !returnBuffer || !returnBufLen )
		return SLP_INTERNAL_SYSTEM_ERROR;
		
	SLPInternalError		status = SLP_OK;
	
	if ( mCacheSize <= maxBufferLen )
	{
		memcpy( returnBuffer, mCachedReplyForClique, mCacheSize );
		*returnBufLen = mCacheSize;
	}
	else
	{
		// we have too much data,
		*returnBufLen = GETHEADERLEN(mCachedReplyForClique)+4;

		memcpy( returnBuffer, mCachedReplyForClique, *returnBufLen );
		
		SETLEN( returnBuffer, *returnBufLen );
		SETFLAGS( returnBuffer, OVERFLOWFLAG );
		
		*((UInt16*)(returnBuffer+GETHEADERLEN(returnBuffer))) = 0;					// set the error
		*((UInt16*)((char*)returnBuffer+GETHEADERLEN(returnBuffer)+2)) = 0;		// set the entry count to zero		
	}
		
	return status;
}

#pragma mark -
#pragma mark - ��� SLPClique Protected Methods ���
#pragma mark -
#pragma mark Services in a file stuff
#pragma mark -

#pragma mark -

#pragma mark -
pthread_mutex_t	SLPRAdminNotifier::mQueueLock;

CFStringRef SLPRAdminNotifierCopyDesctriptionCallback ( const void *item )
{
    return kSLPRAdminNotificationSAFE_CFSTR;
}

Boolean SLPRAdminNotifierEqualCallback ( const void *item1, const void *item2 )
{
    return item1 == item2;
}

SLPRAdminNotifier::SLPRAdminNotifier()
	: DSLThread()
{
	CFArrayCallBacks	callBack;
    
#ifdef ENABLE_SLP_LOGGING
    SLPLOG( SLP_LOG_NOTIFICATIONS, "SLPRAdminNotifier Created" );
#endif
    callBack.version = 0;
    callBack.retain = NULL;
    callBack.release = NULL;
    callBack.copyDescription = SLPRAdminNotifierCopyDesctriptionCallback;
    callBack.equal = SLPRAdminNotifierEqualCallback;

    mCanceled = false;
    mNotificationsQueue = ::CFArrayCreateMutable ( NULL, 0, &callBack );
    
    pthread_mutex_init( &mQueueLock, NULL );
    mClearQueue = false;
    mRepostAllScopes = false;
    mRepostAllData = false;
    mDeregisterAllData = false;
}

SLPRAdminNotifier::~SLPRAdminNotifier()
{
    if ( mNotificationsQueue )
        CFRelease( mNotificationsQueue );
        
    mNotificationsQueue = NULL;
}

void SLPRAdminNotifier::Cancel( void )
{
    mCanceled = true;
}

void* SLPRAdminNotifier::Run( void )
{
    NotificationObject*	notification = NULL;
    
    while ( !mCanceled )
    {
        if ( mClearQueue )
        {
            QueueLock();
#ifdef ENABLE_SLP_LOGGING
            SLPLOG( SLP_LOG_NOTIFICATIONS, "SLPRAdminNotifier clearing queue" );
#endif
            mClearQueue = false;
            ::CFArrayRemoveAllValues( mNotificationsQueue );
            QueueUnlock();
        }
        else if ( mRepostAllScopes )
        {
            mRepostAllScopes = false;
#ifdef ENABLE_SLP_LOGGING
            SLPLOG( SLP_LOG_NOTIFICATIONS, "SLPRAdminNotifier reposting all scopes" );
#endif
            SLPRegistrar::TheSLPR()->SendRAdminAllScopes();
        }
        else if ( mRepostAllData )
        {
            mRepostAllData = false;
#ifdef ENABLE_SLP_LOGGING
            SLPLOG( SLP_LOG_NOTIFICATIONS, "SLPRAdminNotifier reposting all currently registered services" );
#endif
            SLPRegistrar::TheSLPR()->SendRAdminAllScopes();
            SLPRegistrar::TheSLPR()->SendRAdminAllCurrentlyRegisteredServices();
        }
        else if ( mDeregisterAllData )
        {
            mDeregisterAllData = false;
#ifdef ENABLE_SLP_LOGGING
            SLPLOG( SLP_LOG_NOTIFICATIONS, "SLPRAdminNotifier deregistering all services" );
#endif
            SLPRegistrar::TheSLPR()->DeregisterAllServices();
        }
        else
        {
            // grab next element off the queue and process
            QueueLock();
            if ( mNotificationsQueue && ::CFArrayGetCount( mNotificationsQueue ) > 0 )
            {
                notification = (NotificationObject*)::CFArrayGetValueAtIndex( mNotificationsQueue, 0 );		// grab the first one
                ::CFArrayRemoveValueAtIndex( mNotificationsQueue, 0 );
                QueueUnlock();
            }
            else
            {
                QueueUnlock();
                SmartSleep(1*USEC_PER_SEC);
            }
            
            if ( notification )
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_NOTIFICATIONS, "SLPRAdminNotifier Sending %s Notification to ServerAdmin", PrintableHeaderType(notification->data) );
#endif                
                SendNotification( notification );
                delete notification;
                notification = NULL;
            }
        }
    }
    
    return NULL;
}

// delayed action
void SLPRAdminNotifier::AddNotificationToQueue( char* buffer, UInt32 bufSize )
{
    NotificationObject*		newNotification = new NotificationObject( buffer, bufSize );
    
    QueueLock();
    if ( mNotificationsQueue )
    {
        ::CFArrayAppendValue( mNotificationsQueue, newNotification );
        
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "Notification Added to Queue" );
#endif
    }
    QueueUnlock();
}

// delayed action
void SLPRAdminNotifier::RepostAllRegisteredData( void )
{
    ClearQueue();
    RepostAllData();
}

// delayed action
void SLPRAdminNotifier::DeregisterAllRegisteredData( void )
{
    DeregisterAllData();
}

// immediate action
void SLPRAdminNotifier::SendNotification( NotificationObject*	notification )
{
    char*		ignoreReply = NULL;
    UInt32		ignoreReplyLen = 0;
    OSStatus	status = noErr;
    
    status = SendDataToSLPRAdmin( notification->data, notification->dataLen, &ignoreReply, &ignoreReplyLen );
    
    if ( ignoreReply && ((SLPdMessageHeader*)ignoreReply)->messageStatus == kSLPTurnOffRAdminNotifications )
        SLPRegistrar::TheSLPR()->DisableRAdminNotification();			// they don't want notifications!
        
    if ( ignoreReply )
        free( ignoreReply );
        
#ifdef ENABLE_SLP_LOGGING
    if ( status )
    {
        SLP_LOG( SLP_LOG_DROP, "Error notifing ServerAdmin of Service Change: %s", strerror(status) );
    }
#endif
}

#pragma mark -

void AddToServiceReply( Boolean usingTCP, ServiceInfo* serviceInfo, char* requestHeader, UInt16 length, SLPInternalError& error, char** replyPtr )
{
#pragma unused( length)
	char*	siURL = NULL;
    UInt16	siURLLen = 0;
	char*	newReply = NULL;
	char*	curPtr = NULL;
    char*	newReplyTemp = *replyPtr;
	UInt32	replyLength, newReplyLength;
    UInt16	xid;
	
	if ( newReplyTemp == NULL )
	{
        // this is a new reply, fill out empty header + room for error code and url entry count
		if ( requestHeader )
			replyLength = GETHEADERLEN(requestHeader)+4;
		else
			replyLength = HDRLEN+2+4;

		newReplyTemp = (char*)malloc( replyLength );
#ifdef USE_EXCEPTIONS
		ThrowIfNULL_( newReplyTemp );
#endif
		if ( !newReplyTemp )
			return;
			
		if ( requestHeader )
			xid = GETXID( requestHeader );
		else
			xid = 0;
   
        SETVER( newReplyTemp, SLP_VER );
        SETFUN( newReplyTemp, SRVRPLY );
        SETLEN( newReplyTemp, replyLength );
        SETFLAGS( newReplyTemp, 0 );
        SETXID( newReplyTemp, xid );
//        SETLANG( newReplyTemp, SLPGetProperty("net.slp.locale") );
    
        ServiceLocationHeader*	header = (ServiceLocationHeader*)newReplyTemp;
        *((UInt16*)&(header->byte13)) = 2;					// this is slightly bogus.  This determines how many bytes
                                                                            // are in the languageTag.  I'm hard coding to 'en'
    
        *((UInt16*)&(header->byte15)) = *((UInt16*)"en");

		*((UInt16*)(newReplyTemp+GETHEADERLEN(newReplyTemp))) = error;			// set the error
		*((UInt16*)((char*)newReplyTemp+GETHEADERLEN(newReplyTemp)+2)) = 0;		// set the entry count to zero
    }

	replyLength = GETLEN(newReplyTemp);
	
	if ( serviceInfo )
    {
        siURL = serviceInfo->GetURLPtr();
        siURLLen = strlen(siURL);
    }
	
	if ( serviceInfo )
    {
        UInt16 urlEntryLength = 1 + 2 + 2 + siURLLen + 1;				// reserved, lifetime, url len, url, # url authentication blocks
        
        newReplyLength = replyLength + urlEntryLength;
	}
    else
        newReplyLength = replyLength;
        
    if ( newReplyLength > MAX_REPLY_LENGTH )
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_MSG, "Size of reply exceeds maximum allowed by SLP, some services will be ignored." );
#endif
        return;
    }
    
	if ( !usingTCP )
	{
		// check the size to see if the new length is going to be bigger than our UDP Buffer size
		char*	endPtr = NULL;
		if ( newReplyLength > (UInt32)strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10) )
		{
			SETFLAGS(newReplyTemp, OVERFLOWFLAG);
			error = SLP_REPLY_TOO_BIG_FOR_PROTOCOL;
			return;
		}
	}
	
	curPtr = newReplyTemp + GETHEADERLEN(newReplyTemp);
	*((UInt16*)curPtr) = (UInt16)error;	// now this is going to get overridden each time, do we care?
	
	curPtr += 2;	// advance past the error to the urlEntry count;

	if ( !serviceInfo )
    {
        *((UInt16*)curPtr) = 0;	// zero the urlEntry count;
        
        (*replyPtr) = newReplyTemp;
    }
    else
    {
        *((UInt16*)curPtr) += 1;	// increment the urlEntry count;
        
        newReply = (char*)malloc( newReplyLength );
        
#ifdef USE_EXCEPTIONS
        ThrowIfNULL_( newReply );
#endif
		if (!newReply )
			return;
			
        ::memcpy( newReply, newReplyTemp, replyLength );
        free( newReplyTemp );
        
        curPtr = newReply+replyLength;						// now we should be pointing at the end of old data, append new url entry
        
        *curPtr = 0;										// zero out the reserved bit
        curPtr++;
        
        *((UInt16*)curPtr) = serviceInfo->GetLifeTime();	// set lifetime
        curPtr += 2;
        
        *((UInt16*)curPtr) = siURLLen;					// set url length
        curPtr += 2;
        
        if ( siURL )
            ::memcpy( curPtr, siURL, siURLLen );
            
        curPtr += siURLLen;
        
        *curPtr = 0;						// this is for the url auth block (zero of them)
        
        SETLEN(newReply, newReplyLength);
        
        (*replyPtr) = newReply;
    }
}


#endif //#ifdef USE_SLPREGISTRAR
