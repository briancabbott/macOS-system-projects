/*
 * Copyright (c) 2008-2015 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').	You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
g++ -W -Wall -I/System/Library/Frameworks/System.framework/PrivateHeaders -I/System/Library/Frameworks/Kernel.framework/PrivateHeaders -lutil -DPRIVATE -D__APPLE_PRIVATE -O -arch ppc -arch i386 -arch x86_64 -o UMCLogger UMCLogger.cpp
*/


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <libutil.h>

#include <mach/clock_types.h>
#include <mach/mach_time.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#ifndef KERNEL_PRIVATE
#define KERNEL_PRIVATE
#include <sys/kdebug.h>
#undef KERNEL_PRIVATE
#else
#include <sys/kdebug.h>
#endif /*KERNEL_PRIVATE*/

#include <IOKit/scsi/IOSCSIArchitectureModelFamilyDebugging.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>

#include "../IOUSBMassStorageClassTimestamps.h"

#include <IOKit/usb/USB.h>

#define DEBUG 			0

//-----------------------------------------------------------------------------
//	Structures
//-----------------------------------------------------------------------------

typedef struct ReturnCodeSpec
{
	unsigned int	returnCode;
	const char *	string;
} ReturnCodeSpec;


//-----------------------------------------------------------------------------
//	Constants
//-----------------------------------------------------------------------------

enum
{

	// Generic USB Storage					0x052D0000 - 0x052D03FF
	kAbortedTaskCode                        = UMC_TRACE ( kAbortedTask ),
	kCompleteSCSICommandCode				= UMC_TRACE ( kCompleteSCSICommand ),
	kNewCommandWhileTerminatingCode         = UMC_TRACE ( kNewCommandWhileTerminating ),
	kLUNConfigurationCompleteCode			= UMC_TRACE ( kLUNConfigurationComplete ),
	kIOUMCStorageCharacDictFoundCode		= UMC_TRACE ( kIOUMCStorageCharacDictFound ),
	kNoProtocolForDeviceCode				= UMC_TRACE ( kNoProtocolForDevice ),
	kIOUSBMassStorageClassStartCode         = UMC_TRACE ( kIOUSBMassStorageClassStart ),
	kIOUSBMassStorageClassStopCode          = UMC_TRACE ( kIOUSBMassStorageClassStop ),
	kAtUSBAddressCode						= UMC_TRACE ( kAtUSBAddress ),
	kMessagedCalledCode						= UMC_TRACE ( kMessagedCalled ),
	kWillTerminateCalledCode				= UMC_TRACE ( kWillTerminateCalled ),
	kDidTerminateCalledCode					= UMC_TRACE ( kDidTerminateCalled ),
	kCDBLog1Code							= UMC_TRACE ( kCDBLog1 ),
	kCDBLog2Code							= UMC_TRACE ( kCDBLog2 ),
	kClearEndPointStallCode					= UMC_TRACE ( kClearEndPointStall ),
	kGetEndPointStatusCode					= UMC_TRACE ( kGetEndPointStatus ),
	kHandlePowerOnUSBResetCode				= UMC_TRACE ( kHandlePowerOnUSBReset ),
	kUSBDeviceResetWhileTerminatingCode		= UMC_TRACE ( kUSBDeviceResetWhileTerminating ),
	kUSBDeviceResetAfterDisconnectCode		= UMC_TRACE ( kUSBDeviceResetAfterDisconnect ),
	kUSBDeviceResetReturnedCode				= UMC_TRACE ( kUSBDeviceResetReturned ),
	kAbortCurrentSCSITaskCode				= UMC_TRACE ( kAbortCurrentSCSITask ),
	kCompletingCommandWithErrorCode			= UMC_TRACE ( kCompletingCommandWithError ),
	kDeviceInformationCode                  = UMC_TRACE ( kDeviceInformation ),
    kSuspendPortCode                        = UMC_TRACE ( kSuspendPort ),
    kSubclassUseCode                        = UMC_TRACE ( kSubclassUse ),
    
	// CBI Specific							0x052D0400 - 0x052D07FF
	kCBIProtocolDeviceDetectedCode			= UMC_TRACE ( kCBIProtocolDeviceDetected ),
	kCBICommandAlreadyInProgressCode		= UMC_TRACE ( kCBICommandAlreadyInProgress ),
	kCBISendSCSICommandReturnedCode			= UMC_TRACE ( kCBISendSCSICommandReturned ),
	kCBICompletionCode                      = UMC_TRACE ( kCBICompletion ),
    
	// UFI Specific							0x052D0800 - 0x052D0BFF

	// Bulk-Only Specific					0x052D0C00 - 0x052D0FFF
	kBODeviceDetectedCode					= UMC_TRACE ( kBODeviceDetected ),
	kBOPreferredMaxLUNCode					= UMC_TRACE ( kBOPreferredMaxLUN ),
	kBOGetMaxLUNReturnedCode				= UMC_TRACE ( kBOGetMaxLUNReturned ),
	kBOCommandAlreadyInProgressCode			= UMC_TRACE ( kBOCommandAlreadyInProgress ),
	kBOSendSCSICommandReturnedCode			= UMC_TRACE ( kBOSendSCSICommandReturned ),	
	kBOCBWDescriptionCode					= UMC_TRACE ( kBOCBWDescription ),
	kBOCBWBulkOutWriteResultCode			= UMC_TRACE ( kBOCBWBulkOutWriteResult ),
	kBODoubleCompleteionCode				= UMC_TRACE ( kBODoubleCompleteion ),
	kBOCompletionDuringTerminationCode		= UMC_TRACE ( kBOCompletionDuringTermination ),
	kBOCompletionCode						= UMC_TRACE ( kBOCompletion )
	
};


static const char * kBulkOnlyStateNames[] = {	" ",
												"BulkOnlyCommandSent",
												"BulkOnlyCheckCBWBulkStall",
												"BulkOnlyClearCBWBulkStall",
												"BulkOnlyBulkIOComplete",
												"BulkOnlyCheckBulkStall",
												"BulkOnlyClearBulkStall",
												"BulkOnlyCheckBulkStallPostCSW",
												"BulkOnlyClearBulkStallPostCSW",
												"BulkOnlyStatusReceived",
												"BulkOnlyStatusReceived2ndTime",
												"BulkOnlyResetCompleted",
												"BulkOnlyClearBulkInCompleted",
												"BulkOnlyClearBulkOutCompleted" };


#define kTraceBufferSampleSize			60000
#define kMicrosecondsPerSecond			1000000
#define kMicrosecondsPerMillisecond		1000
#define kFilePathMaxSize                256
#define kInvalid						0xdeadbeef
#define kDivisorEntry					0xfeedface


//-----------------------------------------------------------------------------
//	Globals
//-----------------------------------------------------------------------------

int					gNumCPUs					= 1;
double				gDivisor					= 0.0;		/* Trace divisor converts to microseconds */
kd_buf *			gTraceBuffer				= NULL;
boolean_t			gTraceEnabled				= FALSE;
boolean_t			gSetRemoveFlag				= TRUE;
boolean_t			gEnableTraceOnly			= FALSE;
const char *		gProgramName				= NULL;
uint32_t			gSavedTraceMask				= 0;
boolean_t			gHideBusyRejectedCommands	= FALSE;

boolean_t           gWriteToTraceFile           = FALSE;
boolean_t           gReadTraceFile              = FALSE;
FILE *              gTraceFileStream			= NULL;
char				gTraceFilePath [ kFilePathMaxSize ] = { 0 };

u_int8_t			fullCDB [ 16 ]				= { 0 };
int64_t 			current_usecs				= 0;
int64_t 			prev_usecs					= 0;
int64_t				delta_usecs					= 0;


//-----------------------------------------------------------------------------
//	Prototypes
//-----------------------------------------------------------------------------

static void
EnableTraceBuffer ( int val );

static void
CollectTrace ( void );

static void
CreateTraceOutputFile ( void );

static void
ParseTraceFile ( void );

static void
ParseKernelTracePoint ( kd_buf inTracePoint );

static void
SignalHandler ( int signal );

static void
GetDivisor ( void );

static void
RegisterSignalHandlers ( void );

static void
AllocateTraceBuffer ( void );

static void
RemoveTraceBuffer ( void );

static void
SetTraceBufferSize ( int nbufs );

static void
Quit ( const char * s );

static void
InitializeTraceBuffer ( void );

static void
ParseArguments ( int argc, const char * argv[] );

static void
PrintUsage ( void );

static void
PrintSCSICommand ( void );

static void
PrintTimeStamp ( void );

static void
LoadUSBMassStorageExtension ( void );

static const char * 
StringFromReturnCode ( unsigned int returnCode );

void
ProcessSubclassTracePoint ( kd_buf inTracePoint );

void
ProcessAppleUSBCardReaderUMCSubclassTracePoint ( kd_buf inTracePoint, UInt32 intSubclassCode );

void
ProcessAppleUSBODDSubclassTracePoint ( kd_buf inTracePoint, UInt32 intSubclassCode );


//-----------------------------------------------------------------------------
//	Main
//-----------------------------------------------------------------------------

int
main ( int argc, const char * argv[] )
{
	
	USBSysctlArgs 	args;
	int				error;
	
	gProgramName = argv[0];
	
	if ( reexec_to_match_kernel ( ) != 0 )
	{
		
		fprintf ( stderr, "Could not re-execute to match kernel architecture, errno = %d\n", errno );
		exit ( 1 );
		
	}
	
	if ( geteuid ( ) != 0 )
	{
		
		fprintf ( stderr, "'%s' must be run as root...\n", gProgramName );
		exit ( 1 );
		
	}
	
	// Get program arguments.
	ParseArguments ( argc, argv );
	
	bzero ( &args, sizeof ( args ) );
	
	args.type = kUSBTypeDebug;
	args.operation = kUSBOperationGetFlags;
	
	error = sysctlbyname ( USBMASS_SYSCTL, NULL, NULL, &args, sizeof ( args ) );
	if ( error != 0 )
	{
		fprintf ( stderr, "sysctlbyname failed to get old umctrace flags\n" );
	}
	
	args.type = kUSBTypeDebug;
	args.operation = kUSBOperationSetFlags;
	args.debugFlags = 1;
	
	error = sysctlbyname ( USBMASS_SYSCTL, NULL, NULL, &args, sizeof ( args ) );
	if ( error != 0 )
	{
		
		LoadUSBMassStorageExtension ( );
		
		error = sysctlbyname ( USBMASS_SYSCTL, NULL, NULL, &args, sizeof ( args ) );
		if ( error != 0 )
		{
			fprintf ( stderr, "sysctlbyname failed to set new umctrace flags\n" );
		}
		
	}
	
#if DEBUG
	printf ( "gSavedTraceMask = 0x%08X\n", gSavedTraceMask );
	printf ( "gPrintfMask = 0x%08X\n", gPrintfMask );
	printf ( "gVerbose = %s\n", gVerbose == TRUE ? "True" : "False" );
	fflush ( stdout );
#endif
	
	// Set up signal handlers.
	RegisterSignalHandlers ( );	
	
	// Allocate trace buffer.
	AllocateTraceBuffer ( );
	
	// Remove the trace buffer.
	RemoveTraceBuffer ( );
	
	// Set the new trace buffer size.
	SetTraceBufferSize ( kTraceBufferSampleSize );
	
	// Initialize the trace buffer.
	InitializeTraceBuffer ( );
	
	// Enable the trace buffer.
	EnableTraceBuffer ( 1 );
	
	// Get the clock divisor.
	GetDivisor ( );
    
    if ( gWriteToTraceFile == TRUE )
    {
        CreateTraceOutputFile ( );
    }
	
	// Does the user only want the trace points enabled and no logging?
	if ( gEnableTraceOnly == FALSE )
	{
        
        if ( gReadTraceFile == FALSE )
        {
            
            // No, they want logging. Start main loop.
            while ( 1 )
            {
                
                usleep ( 20 * kMicrosecondsPerMillisecond );
                CollectTrace ( );
                
            }
            
        }
        
        else
        {
            
            ParseTraceFile ( );
            
        }
		
	}
	
	return 0;
	
}


//-----------------------------------------------------------------------------
//	PrintUsage
//-----------------------------------------------------------------------------

static void
PrintUsage ( void )
{
	
	printf ( "\n" );
	
	printf ( "Usage: %s\n\n", gProgramName );
    printf ( "\t-h help\n" );
    printf ( "\t-b hide rejected SCSI tasks\n" );
    printf ( "\t-d disable\n" );
    printf ( "\t-f <file_path> write traces out directly to a file.\n" );
    printf ( "\t-r <file_path> parses trace file\n" );
				
	printf ( "\n" );
	
	exit ( 0 );
	
}


//-----------------------------------------------------------------------------
//	ParseArguments
//-----------------------------------------------------------------------------

static void
ParseArguments ( int argc, const char * argv[] )
{
	
	int 					c;
    struct option 			long_options[] =
    {
        { "disable",        no_argument,        0, 'd' },
        { "busy",           no_argument,        0, 'b' },
        { "file",           required_argument,  0, 'f' },
        { "read",           required_argument,  0, 'r' },
        { "help",           no_argument,        0, 'h' },
        { 0, 0, 0, 0 }
    };
    
	// If no args specified, enable all logging...
	if ( argc == 1 )
	{
		return;
	}
	
    while ( ( c = getopt_long ( argc, ( char * const * ) argv , "dbf:r:h?", long_options, NULL  ) ) != -1 )
	{
		
        switch ( c )
        {
            
            case 'd':
            {
                
                gSavedTraceMask = 0;
                gSetRemoveFlag = FALSE;
                Quit ( "Quit via user-specified trace disable\n" );
            
            }
            break;
                
            case 'b':
            {
                
                gHideBusyRejectedCommands = TRUE;
                break;
                
            }
                
            case 'f':
            {
                
                gWriteToTraceFile = TRUE;
                
                if ( optarg != NULL )
                {
                    
                    if ( strlcpy ( gTraceFilePath, optarg, sizeof ( gTraceFilePath ) ) >= sizeof ( gTraceFilePath ) )
                    {
                        Quit ( "The path length of raw file is too long\n" );
                    }
                    
                }
                
                else
                {
                    Quit ( "No file specified with -f argument\n");
                }
                
            }
            break;
                
            case 'r':
            {
                
                gReadTraceFile = TRUE;
                
                if ( optarg != NULL )
                {
                    
                    if ( strlcpy ( gTraceFilePath, optarg, sizeof ( gTraceFilePath ) ) >= sizeof ( gTraceFilePath ) )
                    {
                        Quit ( "The path length of raw file is too long\n" );
                    }
                    
                }
                
                else
                {
                    Quit ( "No file specified with -r argument\n");
                }
                
            }
            break;
            
            case 'h':
            {
                PrintUsage ( );
            }
            break;
                
            default:
            {
                
                PrintUsage ( );
                Quit ( "Invalid usage\n");
                
            }
            break;
            
        }
        
	}
	
}


//-----------------------------------------------------------------------------
//	RegisterSignalHandlers
//-----------------------------------------------------------------------------

static void
RegisterSignalHandlers ( void )
{
	
	signal ( SIGINT, SignalHandler );
	signal ( SIGQUIT, SignalHandler );
	signal ( SIGHUP, SignalHandler );
	signal ( SIGTERM, SignalHandler );
	
}


//-----------------------------------------------------------------------------
//	AllocateTraceBuffer
//-----------------------------------------------------------------------------

static void
AllocateTraceBuffer ( void )
{
	
	size_t	len;
	int		mib[3];
	
	// grab the number of cpus
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	mib[2] = 0;
	
	len = sizeof ( gNumCPUs );
	
	sysctl ( mib, 2, &gNumCPUs, &len, NULL, 0 );
	
	gTraceBuffer = ( kd_buf * ) malloc ( gNumCPUs * kTraceBufferSampleSize * sizeof ( kd_buf ) );
	if ( gTraceBuffer == NULL )
	{
		Quit ( "Can't allocate memory for tracing info\n" );
	}
			
}


//-----------------------------------------------------------------------------
//	SignalHandler
//-----------------------------------------------------------------------------

static void
SignalHandler ( int signal )
{

#pragma unused ( signal )
	
	EnableTraceBuffer ( 0 );
	RemoveTraceBuffer ( );
	exit ( 0 );
	
}


//-----------------------------------------------------------------------------
//	EnableTraceBuffer
//-----------------------------------------------------------------------------

static void
EnableTraceBuffer ( int val ) 
{
	
	int 		mib[6];
	size_t 		needed;
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDENABLE;		/* protocol */
	mib[3] = val;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 4, NULL, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDENABLE\n" );
	
	if ( val )
		gTraceEnabled = TRUE;
	else
		gTraceEnabled = FALSE;
	
}


//-----------------------------------------------------------------------------
//	SetTraceBufferSize
//-----------------------------------------------------------------------------

static void
SetTraceBufferSize ( int nbufs ) 
{
	
	int 		mib[6];
	size_t 		needed;

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETBUF;
	mib[3] = nbufs;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 4, NULL, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDSETBUF\n" );

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 3, NULL, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDSETUP\n" );
	
}


//-----------------------------------------------------------------------------
//	GetTraceBufferInfo
//-----------------------------------------------------------------------------

static void
GetTraceBufferInfo ( kbufinfo_t * val )
{
	
	int 		mib[6];
	size_t 		needed;

	needed = sizeof ( *val );
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDGETBUF;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 3, val, &needed, 0, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDGETBUF\n" );
	
}


//-----------------------------------------------------------------------------
//	RemoveTraceBuffer
//-----------------------------------------------------------------------------

static void
RemoveTraceBuffer ( void ) 
{
	
	int 		mib[6];
	size_t 		needed;
	
	errno = 0;
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREMOVE;		/* protocol */
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 3, NULL, &needed, NULL, 0 ) < 0 )
	{
		
		gSetRemoveFlag = FALSE;
		
		if ( errno == EBUSY )
			Quit ( "The trace facility is currently in use...\n    fs_usage, sc_usage, and latency use this feature.\n\n" );
		
		else
			Quit ( "Trace facility failure, KERN_KDREMOVE\n" );
		
	}
	
}


//-----------------------------------------------------------------------------
//	InitializeTraceBuffer
//-----------------------------------------------------------------------------

static void
InitializeTraceBuffer ( void ) 
{

	int 		mib[6];
	size_t 		needed;
	kd_regtype	kr;
	
	kr.type 	= KDBG_RANGETYPE;
	kr.value1 	= 0;
	kr.value2	= -1;
	
	needed = sizeof ( kd_regtype );
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETREG;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 3, &kr, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDSETREG\n" );
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 3, NULL, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDSETUP\n" );
	
	kr.type 	= KDBG_SUBCLSTYPE;
	kr.value1 	= DBG_IOKIT;
	kr.value2 	= DBG_IOSAM;
	
	needed = sizeof ( kd_regtype );
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETREG;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;
	
	if ( sysctl ( mib, 3, &kr, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDSETREG (subclstype)\n" );
	
}


//-----------------------------------------------------------------------------
//	PrintSCSICommand
//-----------------------------------------------------------------------------

static void
PrintSCSICommand ( void )
{
	
	switch ( fullCDB [0] )
	{
		
		case kSCSICmd_TEST_UNIT_READY:
		{
			
			printf ( " kSCSICmd_TEST_UNIT_READY\n" );
			
		}
		break;
			
		case kSCSICmd_REQUEST_SENSE:
		{
			
			printf ( " kSCSICmd_REQUEST_SENSE\n" );
			
		}
		break;
			
		case kSCSICmd_READ_10:
		{
			
			u_int32_t	LOGICAL_BLOCK_ADDRESS	= 0;
			u_int16_t	TRANSFER_LENGTH			= 0;
			
			LOGICAL_BLOCK_ADDRESS   = fullCDB [2];
			LOGICAL_BLOCK_ADDRESS <<= 8;
			LOGICAL_BLOCK_ADDRESS  |= fullCDB [3];
			LOGICAL_BLOCK_ADDRESS <<= 8;
			LOGICAL_BLOCK_ADDRESS  |= fullCDB [4];
			LOGICAL_BLOCK_ADDRESS <<= 8;
			LOGICAL_BLOCK_ADDRESS  |= fullCDB [5];
			
			TRANSFER_LENGTH   = fullCDB [7];
			TRANSFER_LENGTH <<= 8;
			TRANSFER_LENGTH  |= fullCDB [8];
			
			printf ( "kSCSICmd_READ_10, LBA = 0x%X, length = 0x%X\n", LOGICAL_BLOCK_ADDRESS, TRANSFER_LENGTH );
			
		}
		break;
		
		case kSCSICmd_WRITE_10:
		{
			
			u_int32_t	LOGICAL_BLOCK_ADDRESS	= 0;
			u_int16_t	TRANSFER_LENGTH			= 0;
			
			LOGICAL_BLOCK_ADDRESS   = fullCDB [2];
			LOGICAL_BLOCK_ADDRESS <<= 8;
			LOGICAL_BLOCK_ADDRESS  |= fullCDB [3];
			LOGICAL_BLOCK_ADDRESS <<= 8;
			LOGICAL_BLOCK_ADDRESS  |= fullCDB [4];
			LOGICAL_BLOCK_ADDRESS <<= 8;
			LOGICAL_BLOCK_ADDRESS  |= fullCDB [5];
			
			TRANSFER_LENGTH   = fullCDB [7];
			TRANSFER_LENGTH <<= 8;
			TRANSFER_LENGTH  |= fullCDB [8];
			
			printf ( "kSCSICmd_WRITE_10, LBA = 0x%X, length = 0x%X\n", LOGICAL_BLOCK_ADDRESS, TRANSFER_LENGTH );
			
		}
		break;
			
		default:
		{
			printf ( "This command has not yet been decoded\n" );
		}
		break;
		
	}
	
}


//-----------------------------------------------------------------------------
//	PrintTimeStamp
//-----------------------------------------------------------------------------

static void
PrintTimeStamp ( void )
{
	
	time_t		currentTime = time ( NULL );
	
	if ( prev_usecs == 0 )
	{
		delta_usecs = 0;
	}
	else
	{
		delta_usecs = current_usecs - prev_usecs;
	}

	prev_usecs = current_usecs;
	
/*
	
	if ( delta_usecs > (100 * kMicrosecondsPerMillisecond )
	{
		printf ( "*** " );
	}
	else
	{
		printf ( "    " );
	}

*/
	
	printf ( "%-8.8s [%lld][%10lld us]", &( ctime ( &currentTime )[11] ), current_usecs, delta_usecs );
	
}

//-----------------------------------------------------------------------------
//	CollectTrace
//-----------------------------------------------------------------------------

static void
CollectTrace ( void )
{
	
	int				mib[6];
	int 			index;
	int				count;
	size_t 			needed;
	kbufinfo_t 		bufinfo = { 0, 0, 0, 0, 0 };
	
	/* Get kernel buffer information */
	GetTraceBufferInfo ( &bufinfo );
	
	needed = bufinfo.nkdbufs * sizeof ( kd_buf );
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREADTR;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */
	
	if ( sysctl ( mib, 3, gTraceBuffer, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDREADTR\n" );
	
	count = needed;
	
	if ( bufinfo.flags & KDBG_WRAPPED )
	{
		
		EnableTraceBuffer ( 0 );
		EnableTraceBuffer ( 1 );
		
	}
	
	for ( index = 0; index < count; index++ )
	{
		
        // Print trace data to stdout.
        if ( gWriteToTraceFile == FALSE )
        {
            
            int 				debugID;
            int 				type;
            
            debugID = gTraceBuffer[index].debugid;
            type	= debugID & ~( DBG_FUNC_START | DBG_FUNC_END );
            
            //printf ("type = 0x%x\n", UMC_TRACE ( 0x0 ), UMC_TRACE ( 0xFFF ) );
            ParseKernelTracePoint ( gTraceBuffer [ index ] );
        }
        
        // Save trace point data to a file.
        else
        {
            
            int 				debugID;
            int 				type;
            
            debugID = gTraceBuffer[index].debugid;
            type	= debugID & ~( DBG_FUNC_START | DBG_FUNC_END );
            
            if ( ( type >= 0x05278800 ) && ( type <= 0x05278BFC ) )
            {
                
                fwrite (    ( const void * ) & ( gTraceBuffer [ index ] ),
                            sizeof ( kd_buf ),
                            1,
                            gTraceFileStream );
                
                fflush (    gTraceFileStream );
                
            }
           
        }
		
	}
	
	fflush ( 0 );
	
}




//-----------------------------------------------------------------------------
//	CreateTraceOutputFile
//-----------------------------------------------------------------------------

static void
CreateTraceOutputFile ( void )
{
    
    char timestring [ 30 ];
    time_t currentTime = time ( NULL );
    
    // Did the user not supply a preferred file path?
    if ( gTraceFilePath[0] == 0 )
    {
        
        strftime ( timestring, 30, "./sdxclogger-%y%d%m%H%M%S.bin", localtime ( &currentTime ) );
        
        if ( strlcpy ( gTraceFilePath, timestring, sizeof ( gTraceFilePath ) ) >= sizeof ( gTraceFilePath ) )
        {
            Quit ( "The path length of raw file is too long\n" );
        }
        
    }
    
    gTraceFileStream = fopen ( gTraceFilePath, "w+" );
    
}


//-----------------------------------------------------------------------------
//	ParseTraceFile
//-----------------------------------------------------------------------------

static void
ParseTraceFile ( )
{
    
    FILE * traceFile;
    
    traceFile = fopen ( gTraceFilePath, "r" );
    kd_buf kp;
	bzero( &kp, sizeof ( kd_buf ) );
	
	if ( traceFile )
	{
        
		while ( fread ( &kp, sizeof ( kd_buf ), 1, traceFile ) )
		{
            
			kd_buf tracepoint;
			bzero ( &tracepoint, sizeof ( kd_buf ) );
			
			if ( kp.debugid == kInvalid )
			{
                
				printf ( "Found an invalid entry in raw file.\n" );
				continue;
                
			}
			
			if ( kp.debugid == kDivisorEntry )
			{
                
				gDivisor = ( double )( kp.timestamp );
				printf ( "Found divisor %f as 0x%llx\n", gDivisor, kp.timestamp );
                
			}
			else
			{
				
				// send tracepoint to be processed
				ParseKernelTracePoint ( kp );
                
			}
            
		}
		
		fclose ( traceFile );
        
	}
    
	else
    {
        Quit ( "Could not open specified trace file :(\n" );
    }
    
}


//-----------------------------------------------------------------------------
//	StringFromReturnCode
//-----------------------------------------------------------------------------

static void
ParseKernelTracePoint ( kd_buf inTracePoint )
{
    
    
    int 				debugID;
    int 				type;
    uint64_t 			now;
    const char *		errorString;
    
    debugID = inTracePoint.debugid;
    type	= debugID & ~( DBG_FUNC_START | DBG_FUNC_END );
    
    now = inTracePoint.timestamp & KDBG_TIMESTAMP_MASK;
    current_usecs = ( int64_t )( now / gDivisor );
    
    
    
    if ( ( type >= 0x05278800 ) && ( type <= 0x05278BFC ) && ( type != kCDBLog2Code ) )
    {
        PrintTimeStamp ( );
    }
    
    switch ( type )
    {
            
#pragma mark -
#pragma mark *** Generic UMC Codes ***
#pragma mark -
			
        case kAbortedTaskCode:
        {
            printf ( "[%10p] Task %p Aborted!!!\n", ( void * ) inTracePoint.arg1, ( void * ) inTracePoint.arg2 );
        }
        break;
			
        case kAbortCurrentSCSITaskCode:
        {
            printf ( "[%10p] Aborted currentTask %p DeviceAttached = %d ConsecutiveResetCount = %d\n",
                    ( void * ) inTracePoint.arg1, ( void * ) inTracePoint.arg2,
                    ( int ) inTracePoint.arg3, ( int ) inTracePoint.arg4 );
        }
        break;
            
        case kCompleteSCSICommandCode:
        {
            
            printf ( "[%10p] Task %p Completed with serviceResponse = %d taskStatus = 0x%x\n",
                    ( void * ) inTracePoint.arg1, ( void * ) inTracePoint.arg2,
                    ( int ) inTracePoint.arg3, ( int ) inTracePoint.arg4 );
            PrintTimeStamp ( );
            printf ( "[%10p] -------------------------------------------------\n", ( void * ) inTracePoint.arg1 );
            
        }
        break;
			
        case kCompletingCommandWithErrorCode:
        {
            printf ( "[%10p] !!!!! Hark !!!!! Completing command with an ERROR status!\n", ( void * ) inTracePoint.arg1 );
        }
        break;
            
        case kLUNConfigurationCompleteCode:
        {
            printf ( "[%10p] MaxLUN = %u\n", ( void * ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2 );
        }
        break;
			
        case kNewCommandWhileTerminatingCode:
        {
            printf ( "[%10p] Task = %p received while terminating!!!\n", ( void * ) inTracePoint.arg1, ( void * ) inTracePoint.arg2 );
        }
        break;
			
        case kIOUMCStorageCharacDictFoundCode:
        {
            printf ( "[%10p] This device has a USB Characteristics Dictionary\n", ( void * ) inTracePoint.arg1 );
        }
        break;
			
        case kNoProtocolForDeviceCode:
        {
            printf ( "[%10p] !!! NO USB TRANSPORT PROTOCOL FOR THIS DEVICE !!!\n", ( void * ) inTracePoint.arg1 );
        }
        break;
			
        case kIOUSBMassStorageClassStartCode:
        {
            printf ( "[%10p] Starting up!\n", ( void * ) inTracePoint.arg1 );
        }
        break;
			
        case kIOUSBMassStorageClassStopCode:
        {
            printf ( "[%10p] Stopping!\n", ( void * ) inTracePoint.arg1 );
        }
        break;
			
        case kAtUSBAddressCode:
        {
            printf ( "[%10p] @ USB Address: %u\n", ( void * ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2 );
        }
        break;
			
        case kMessagedCalledCode:
        {
            printf ( "[%10p] Message : %x received\n", ( void * ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2 );
            PrintTimeStamp ( );
            printf ( "[%10p] -------------------------------------------------\n", ( void * ) inTracePoint.arg1 );
        }
        break;
			
        case kWillTerminateCalledCode:
        {
            printf ( "[%10p] willTerminate called, CurrentInterface=%p, isInactive=%u\n",
					( void * ) inTracePoint.arg1, ( void * ) inTracePoint.arg2, ( unsigned int ) inTracePoint.arg3 );
        }
        break;
			
        case kDidTerminateCalledCode:
        {
            printf ( "[%10p] didTerminate called, fTerminationDeferred=%u\n",
                    ( void * ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2 );
        }
        break;
			
        case kCDBLog1Code:
        {
            
            UInt8 *			cdbData;
            unsigned int	i;
            
            printf ( "[%10p] Request %p\n", ( void * ) inTracePoint.arg1, ( void * ) inTracePoint.arg2 );
            PrintTimeStamp ( );
            printf ( "[%10p] ", ( void * ) inTracePoint.arg1 );
            
            cdbData = ( UInt8 * ) &inTracePoint.arg3;
            
            for ( i = 0; i < 4; i++ )
            {
                fullCDB [i] = cdbData[i];
                printf ( "0x%02X : ", cdbData[i] );
            }
            
            cdbData = ( UInt8 * ) &inTracePoint.arg4;
            
            for ( i = 0; i < 4; i++ )
            {
                fullCDB [i+4] = cdbData[i];
                printf ( "0x%02X : ", cdbData[i] );
            }
            
        }
        break;
			
        case kCDBLog2Code:
        {
            
            UInt8 *			cdbData;
            unsigned int 	i;
            
            cdbData = ( UInt8 * ) &inTracePoint.arg3;
            
            for ( i = 0; i < 4; i++ )
            {
                fullCDB [i+8] = cdbData[i];
                printf ( "0x%02X : ", cdbData[i] );
            }
            
            cdbData = ( UInt8 * ) &inTracePoint.arg4;
            
            for ( i = 0; i < 3; i++ )
            {
                fullCDB [i+12] = cdbData[i];
                printf ( "0x%02X : ", cdbData[i] );
            }
            
            fullCDB [i+12] = cdbData[i];
            printf ( "0x%02X\n", cdbData[i] );
            
            PrintTimeStamp ( );
            printf ( "[%10p] ", ( void * ) inTracePoint.arg1 );
            PrintSCSICommand ( );
            
        }
        break;
			
        case kClearEndPointStallCode:
        {
            
            errorString = StringFromReturnCode ( inTracePoint.arg2 );
            printf ( "[%10p] ClearFeatureEndpointStall status=%s (0x%x), endpoint=%u\n",
                    ( void * ) inTracePoint.arg1, errorString, ( unsigned int ) inTracePoint.arg2, ( unsigned int ) inTracePoint.arg3 );
            
        }
        break;
			
        case kGetEndPointStatusCode:
        {
            
            errorString = StringFromReturnCode ( inTracePoint.arg2 );
            printf ( "[%10p] GetEndpointStatus status=%s (0x%x), endpoint=%u\n",
                    ( void * ) inTracePoint.arg1, errorString, ( unsigned int ) inTracePoint.arg2, ( unsigned int ) inTracePoint.arg3 );
            
        }
        break;
			
        case kHandlePowerOnUSBResetCode:
        {
            
            printf ( "[%10p] USB Device Reset on WAKE from SLEEP\n", ( void * ) inTracePoint.arg1 );
            
        }
        break;
			
        case kUSBDeviceResetWhileTerminatingCode:
        {
            
            printf ( "[%10p] Termination started before device reset could be initiated! fTerminating=%u, isInactive=%u\n",
                    ( void * ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2, ( unsigned int ) inTracePoint.arg3 );
            
        }
        break;
			
        case kUSBDeviceResetAfterDisconnectCode:
        {
            
            printf ( "[%10p] Device reset was attempted after the device had been disconnected\n", ( void * ) inTracePoint.arg1 );
            
        }
        break;
			
        case kUSBDeviceResetReturnedCode:
        {
            
            printf ( "[%10p] DeviceReset returned: 0x%08x\n", ( void * ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2 );
            
        }
        break;
            
        case kDeviceInformationCode:
        {
            errorString = StringFromReturnCode ( inTracePoint.arg2 );
            printf ( "[%10p] Device Information status=%s DeviceInformation=0x%x\n", ( void * ) inTracePoint.arg1,
                    errorString, ( int ) inTracePoint.arg3 );
        }
        break;
            
        case kSuspendPortCode:
        {
            
            errorString = StringFromReturnCode ( inTracePoint.arg2 );
            if ( inTracePoint.arg3 == 0 )
            {
                printf ( "[%10p] Suspend Port (RESUME) returned status=%s\n", ( void * ) inTracePoint.arg1, errorString );
            }
            
            else
            {
                printf ( "[%10p] Suspend Port (SUSPEND) returned status=%s\n", ( void * ) inTracePoint.arg1,  errorString );
            }
            
        }
        break;
            
        case kSubclassUseCode:
        {
            
            // This is special code that we let our subclasses use. We have a helper function to handle these.
            ProcessSubclassTracePoint ( inTracePoint );
            
        }
        break;
			
#pragma mark -
#pragma mark *** Control Bulk Interrupt ( CBI ) Codess ***
#pragma mark -
			
        case kCBIProtocolDeviceDetectedCode:
        {
            
            printf ( "[%10p] CBI transport protocol device\n", ( void * ) inTracePoint.arg1 );
            
        }
        break;
			
        case kCBICommandAlreadyInProgressCode:
        {
			
            if ( gHideBusyRejectedCommands == FALSE )
            {
                
                printf ( "[%10p] CBI - Unable to accept task %p, still working on previous command\n",
                        ( void * ) inTracePoint.arg1, ( void * ) inTracePoint.arg2 );
                
            }
            
        }
        break;
			
        case kCBISendSCSICommandReturnedCode:
        {
            
            errorString = StringFromReturnCode ( inTracePoint.arg3 );
            printf ( "[%10p] CBI - SCSI Task %p was sent with status %s (0x%x)\n",
                    ( void * ) inTracePoint.arg1, ( void * ) inTracePoint.arg2, errorString, ( unsigned int ) inTracePoint.arg3 );
            
        }
        break;
            
        case kCBICompletionCode:
        {

            errorString = StringFromReturnCode ( inTracePoint.arg2 );
            printf ( "[%10p] CBI - Completion for state %lu, Task %p was sent with status %s (0x%x)\n",
                        ( void * ) inTracePoint.arg1, inTracePoint.arg3, ( void * ) inTracePoint.arg4, errorString, ( unsigned int ) inTracePoint.arg2 );
            
        }
        break;
			
#pragma mark -
#pragma mark *** Bulk-Only Protocol Codes ***
#pragma mark -
			
        case kBODeviceDetectedCode:
        {
            
            printf ( "[%10p] BULK-ONLY transport protocol device\n", ( void * ) inTracePoint.arg1 );
            
        }
        break;
			
        case kBOCommandAlreadyInProgressCode:
        {
            
            if ( gHideBusyRejectedCommands == FALSE )
            {
                
                printf ( "[%10p] B0 - Unable to accept task %p, still working on previous request\n",
                        ( void * ) inTracePoint.arg1, ( void * ) inTracePoint.arg2 );
                
            }
            
        }
        break;
			
        case kBOSendSCSICommandReturnedCode:
        {
            
            errorString = StringFromReturnCode ( inTracePoint.arg3 );
            printf ( "[%10p] BO - SCSI Task %p was sent with status %s (0x%x)\n",
                    ( void * ) inTracePoint.arg1, ( void * ) inTracePoint.arg2, errorString, ( unsigned int ) inTracePoint.arg3 );
            
        }
        break;
			
        case kBOPreferredMaxLUNCode:
        {
            
            printf ( "[%10p] BO - Preferred MaxLUN: %d\n",
                    ( void * ) inTracePoint.arg1, (int) inTracePoint.arg2 );
            
        }
        break;
			
        case kBOGetMaxLUNReturnedCode:
        {
            
            errorString = StringFromReturnCode ( inTracePoint.arg2 );
            printf ( "[%10p] BO - GetMaxLUN returned: %s (0x%x), triedReset=%u, MaxLun: %d\n",
                    ( void * ) inTracePoint.arg1, errorString, ( unsigned int ) inTracePoint.arg2, ( unsigned int ) inTracePoint.arg4, ( unsigned int ) inTracePoint.arg3 );
            
        }
        break;
			
        case kBOCBWDescriptionCode:
        {
            
            printf ( "[%10p] BO - Request %p, LUN: %u, CBW Tag: %u (0x%x)\n",
                    ( void * ) inTracePoint.arg1, ( void * ) inTracePoint.arg2, ( unsigned int ) inTracePoint.arg3, ( unsigned int ) inTracePoint.arg4, ( unsigned int ) inTracePoint.arg4 );
            
        }
        break;
			
        case kBOCBWBulkOutWriteResultCode:
        {
            
            errorString = StringFromReturnCode ( inTracePoint.arg2 );
            printf ( "[%10p] BO - Request %p, LUN: %u, Bulk-Out Write Status: %s (0x%x)\n",
                    ( void * ) inTracePoint.arg1, ( void * ) inTracePoint.arg4, ( unsigned int ) inTracePoint.arg3, errorString, ( unsigned int ) inTracePoint.arg2 );
            
        }
        break;
            
        case kBODoubleCompleteionCode:
        {
            
            printf ( "[%10p] BO - DOUBLE Completion\n", ( void * ) inTracePoint.arg1 );
            
        }
        break;
			
        case kBOCompletionDuringTerminationCode:
        {
            
            printf ( "[%10p] BO - Completion during termination\n", ( void * ) inTracePoint.arg1 );
            
        }
        break;
			
        case kBOCompletionCode:
        {
            
            errorString = StringFromReturnCode ( inTracePoint.arg2 );
            printf ( "[%10p] BO - Completion, State: %s, Status: %s (0x%x), for Request: %p\n",
                    ( void * ) inTracePoint.arg1, kBulkOnlyStateNames [ (int) inTracePoint.arg3 ],
                    errorString, ( unsigned int ) inTracePoint.arg2, ( void * ) inTracePoint.arg4 );
            
        }
        break;
			
        default:
        {
            
            if ( ( type >= 0x05278800 ) && ( type <= 0x05278BFC ) )
            {
                printf ( "[%10p] ??? - UNEXPECTED USB TRACE POINT - 0x%X\n", ( void * ) inTracePoint.arg1, type );
            }
            
        }
        break;
			
    }
    
}


//-----------------------------------------------------------------------------
//	Quit
//-----------------------------------------------------------------------------

static void
Quit ( const char * s )
{
	
	USBSysctlArgs	args;
	int				error;
	
	if ( gTraceEnabled == TRUE )
		EnableTraceBuffer ( 0 );
	
	if ( gSetRemoveFlag == TRUE )
		RemoveTraceBuffer ( );
	
	args.type = kUSBTypeDebug;
	args.debugFlags = 0;
	
	error = sysctlbyname ( USBMASS_SYSCTL, NULL, NULL, &args, sizeof ( args ) );
	if ( error != 0 )
	{
		fprintf ( stderr, "sysctlbyname failed to set old UMC trace flags back\n" );
	}
	
	fprintf ( stderr, "%s: ", gProgramName );
	if ( s != NULL )
	{
		fprintf ( stderr, "%s", s );
	}
	
	exit ( 1 );
	
}


//-----------------------------------------------------------------------------
//	GetDivisor
//-----------------------------------------------------------------------------

static void
GetDivisor ( void )
{
	
	struct mach_timebase_info	mti;
	
	mach_timebase_info ( &mti );
	
	gDivisor = ( ( double ) mti.denom / ( double ) mti.numer) * 1000;
	
}


//-----------------------------------------------------------------------------
//	LoadUSBMassStorageExtension
//-----------------------------------------------------------------------------

static void
LoadUSBMassStorageExtension ( void )
{

	posix_spawn_file_actions_t	fileActions;
	char * const				argv[]	= { ( char * ) "/sbin/kextload", ( char * ) "/System/Library/Extensions/IOUSBMassStorageClass.kext", NULL };
	char * const				env[]	= { NULL };
	pid_t						child	= 0;
	union wait 					status;
	
	posix_spawn_file_actions_init ( &fileActions );
	posix_spawn_file_actions_addclose ( &fileActions, STDOUT_FILENO );
	posix_spawn_file_actions_addclose ( &fileActions, STDERR_FILENO );
	
	posix_spawn ( &child, "/sbin/kextload", &fileActions, NULL, argv, env );
	
	if ( !( ( wait4 ( child, ( int * ) &status, 0, NULL ) == child ) && ( WIFEXITED ( status ) ) ) )
	{
		printf ( "Error loading USB Mass Storage extension\n" );
	}	
	
	posix_spawn_file_actions_destroy ( &fileActions );

}


//-----------------------------------------------------------------------------
//	StringFromReturnCode
//-----------------------------------------------------------------------------

static const char * 
StringFromReturnCode ( unsigned int returnCode )
{
	
	const char *	string = "UNKNOWN";
	unsigned int	i;
	
    static ReturnCodeSpec	sReturnCodeSpecs[] =
    {
        
        //	USB Return codes
        { ( unsigned int ) kIOUSBUnknownPipeErr,                            "kIOUSBUnknownPipeErr" },
        { ( unsigned int ) kIOUSBTooManyPipesErr,                           "kIOUSBTooManyPipesErr" },
        { ( unsigned int ) kIOUSBNoAsyncPortErr,                            "kIOUSBNoAsyncPortErr" },
        { ( unsigned int ) kIOUSBNotEnoughPipesErr,                         "kIOUSBNotEnoughPipesErr" },
        { ( unsigned int ) kIOUSBNotEnoughPowerErr,                         "kIOUSBNotEnoughPowerErr" },
        { ( unsigned int ) kIOUSBEndpointNotFound,                          "kIOUSBEndpointNotFound" },
        { ( unsigned int ) kIOUSBConfigNotFound,                            "kIOUSBConfigNotFound" },
        { ( unsigned int ) kIOUSBTransactionTimeout,                        "kIOUSBTransactionTimeout" },
        { ( unsigned int ) kIOUSBTransactionReturned,                       "kIOUSBTransactionReturned" },
        { ( unsigned int ) kIOUSBPipeStalled,                               "kIOUSBPipeStalled" },
        { ( unsigned int ) kIOUSBInterfaceNotFound,                         "kIOUSBInterfaceNotFound" },
        { ( unsigned int ) kIOUSBLowLatencyBufferNotPreviouslyAllocated,    "kIOUSBLowLatencyBufferNotPreviouslyAllocated" },
        { ( unsigned int ) kIOUSBLowLatencyFrameListNotPreviouslyAllocated, "kIOUSBLowLatencyFrameListNotPreviouslyAllocated" },
        { ( unsigned int ) kIOUSBHighSpeedSplitError,                       "kIOUSBHighSpeedSplitError" },
        { ( unsigned int ) kIOUSBSyncRequestOnWLThread,                     "kIOUSBSyncRequestOnWLThread" },
        { ( unsigned int ) kIOUSBDeviceNotHighSpeed,                        "kIOUSBDeviceNotHighSpeed" },
        { ( unsigned int ) kIOUSBLinkErr,                                   "kIOUSBLinkErr" },
        { ( unsigned int ) kIOUSBNotSent2Err,                               "kIOUSBNotSent2Err" },
        { ( unsigned int ) kIOUSBNotSent1Err,                               "kIOUSBNotSent1Err" },
        { ( unsigned int ) kIOUSBBufferUnderrunErr,                         "kIOUSBBufferUnderrunErr" },
        { ( unsigned int ) kIOUSBBufferOverrunErr,                          "kIOUSBBufferOverrunErr" },
        { ( unsigned int ) kIOUSBReserved2Err,                              "kIOUSBReserved2Err" },
        { ( unsigned int ) kIOUSBReserved1Err,                              "kIOUSBReserved1Err" },
        { ( unsigned int ) kIOUSBWrongPIDErr,                               "kIOUSBWrongPIDErr" },
        { ( unsigned int ) kIOUSBPIDCheckErr,                               "kIOUSBPIDCheckErr" },
        { ( unsigned int ) kIOUSBDataToggleErr,                             "kIOUSBDataToggleErr" },
        { ( unsigned int ) kIOUSBBitstufErr,                                "kIOUSBBitstufErr" },
        { ( unsigned int ) kIOUSBCRCErr,                                    "kIOUSBCRCErr" },
        
        //	IOReturn codes
        { ( unsigned int ) kIOReturnSuccess,                                "kIOReturnSuccess" },
        { ( unsigned int ) kIOReturnError,                                  "kIOReturnError" },
        { ( unsigned int ) kIOReturnNoMemory,                               "kIOReturnNoMemory" },
        { ( unsigned int ) kIOReturnNoResources,                            "kIOReturnNoResources" },
        { ( unsigned int ) kIOReturnIPCError,                               "kIOReturnIPCError" },
        { ( unsigned int ) kIOReturnNoDevice,                               "kIOReturnNoDevice" },
        { ( unsigned int ) kIOReturnNotPrivileged,                          "kIOReturnNotPrivileged" },
        { ( unsigned int ) kIOReturnBadArgument,                            "kIOReturnBadArgument" },
        { ( unsigned int ) kIOReturnLockedRead,                             "kIOReturnLockedRead" },
        { ( unsigned int ) kIOReturnLockedWrite,                            "kIOReturnLockedWrite" },
        { ( unsigned int ) kIOReturnExclusiveAccess,                        "kIOReturnExclusiveAccess" },
        { ( unsigned int ) kIOReturnBadMessageID,                           "kIOReturnBadMessageID" },
        { ( unsigned int ) kIOReturnUnsupported,                            "kIOReturnUnsupported" },
        { ( unsigned int ) kIOReturnVMError,                                "kIOReturnVMError" },
        { ( unsigned int ) kIOReturnInternalError,                          "kIOReturnInternalError" },
        { ( unsigned int ) kIOReturnIOError,                                "kIOReturnIOError" },
        { ( unsigned int ) kIOReturnCannotLock,                             "kIOReturnCannotLock" },
        { ( unsigned int ) kIOReturnNotOpen,                                "kIOReturnNotOpen" },
        { ( unsigned int ) kIOReturnNotReadable,                            "kIOReturnNotReadable" },
        { ( unsigned int ) kIOReturnNotWritable,                            "kIOReturnNotWritable" },
        { ( unsigned int ) kIOReturnNotAligned,                             "kIOReturnNotAligned" },
        { ( unsigned int ) kIOReturnBadMedia,                               "kIOReturnBadMedia" },
        { ( unsigned int ) kIOReturnStillOpen,                              "kIOReturnStillOpen" },
        { ( unsigned int ) kIOReturnRLDError,                               "kIOReturnRLDError" },
        { ( unsigned int ) kIOReturnDMAError,                               "kIOReturnDMAError" },
        { ( unsigned int ) kIOReturnBusy,                                   "kIOReturnBusy" },
        { ( unsigned int ) kIOReturnTimeout,                                "kIOReturnTimeout" },
        { ( unsigned int ) kIOReturnOffline,                                "kIOReturnOffline" },
        { ( unsigned int ) kIOReturnNotReady,                               "kIOReturnNotReady" },
        { ( unsigned int ) kIOReturnNotAttached,                            "kIOReturnNotAttached" },
        { ( unsigned int ) kIOReturnNoChannels,                             "kIOReturnNoChannels" },
        { ( unsigned int ) kIOReturnNoSpace,                                "kIOReturnNoSpace" },
        { ( unsigned int ) kIOReturnPortExists,                             "kIOReturnPortExists" },
        { ( unsigned int ) kIOReturnCannotWire,                             "kIOReturnCannotWire" },
        { ( unsigned int ) kIOReturnNoInterrupt,                            "kIOReturnNoInterrupt" },
        { ( unsigned int ) kIOReturnNoFrames,                               "kIOReturnNoFrames" },
        { ( unsigned int ) kIOReturnMessageTooLarge,                        "kIOReturnMessageTooLarge" },
        { ( unsigned int ) kIOReturnNotPermitted,                           "kIOReturnNotPermitted" },
        { ( unsigned int ) kIOReturnNoPower,                                "kIOReturnNoPower" },
        { ( unsigned int ) kIOReturnNoMedia,                                "kIOReturnNoMedia" },
        { ( unsigned int ) kIOReturnUnformattedMedia,                       "kIOReturnUnformattedMedia" },
        { ( unsigned int ) kIOReturnUnsupportedMode,                        "kIOReturnUnsupportedMode" },
        { ( unsigned int ) kIOReturnUnderrun,                               "kIOReturnUnderrun" },
        { ( unsigned int ) kIOReturnOverrun,                                "kIOReturnOverrun" },
        { ( unsigned int ) kIOReturnDeviceError,                            "kIOReturnDeviceError" },
        { ( unsigned int ) kIOReturnNoCompletion,                           "kIOReturnNoCompletion" },
        { ( unsigned int ) kIOReturnAborted,                                "kIOReturnAborted" },
        { ( unsigned int ) kIOReturnNoBandwidth,                            "kIOReturnNoBandwidth" },
        { ( unsigned int ) kIOReturnNotResponding,                          "kIOReturnNotResponding" },
        { ( unsigned int ) kIOReturnIsoTooOld,                              "kIOReturnIsoTooOld" },
        { ( unsigned int ) kIOReturnIsoTooNew,                              "kIOReturnIsoTooNew" },
        { ( unsigned int ) kIOReturnNotFound,                               "kIOReturnNotFound" },
        { ( unsigned int ) kIOReturnInvalid,                                "kIOReturnInvalid" }
    };
	
	for ( i = 0; i < ( sizeof ( sReturnCodeSpecs ) / sizeof ( sReturnCodeSpecs[0] ) ); i++ )
	{
		
		if ( returnCode == sReturnCodeSpecs[i].returnCode )
		{
			
			string = sReturnCodeSpecs[i].string;
			break;
			
		}
		
	}
	
	return string;
	
}


//-----------------------------------------------------------------------------
//	ProcessSubclassTracePoint
//-----------------------------------------------------------------------------

void
ProcessSubclassTracePoint ( kd_buf inTracePoint )
{
    
    UInt32 subclassID       = 0;
    UInt32 subclassCode     = 0;
    
    subclassID = ( inTracePoint.arg1 >> 24 ) & 0xFF;
    subclassCode = inTracePoint.arg1 & 0xFFFFFF;
    
    switch ( subclassID )
    {
            
        case kSubclassCode_AppleUSBODD:
        {
            ProcessAppleUSBODDSubclassTracePoint ( inTracePoint, subclassCode );
        }
        break;
            
        case kSubclassCode_AppleUSBCardReaderUMC:
        {
            ProcessAppleUSBCardReaderUMCSubclassTracePoint ( inTracePoint, subclassCode );
        }
        break;
            
        default:
        {
            printf ( "Recieved request for subclassID=0x%lx, but know of no such ssubclassID\n", ( unsigned long ) subclassID );
        }
        
    }
    
    return;
    
}


//-----------------------------------------------------------------------------
//	ProcessAppleUSBODDSubclassTracePoint
//-----------------------------------------------------------------------------

void
ProcessAppleUSBODDSubclassTracePoint ( kd_buf inTracePoint, UInt32 inSubclassCode )
{
    
    switch ( inSubclassCode )
    {
            
        case kAppleUSBODD_probe:
        {
            
            printf ( "[%10p] AUO - Probe returning instance %p\n",
                    ( void * ) inTracePoint.arg2, ( void * ) inTracePoint.arg3 );
            
        }
        break;
            
        case kAppleUSBODD_start:
        {
            
            printf ( "[%10p] AUO - Start returning %d\n",
                    ( void * ) inTracePoint.arg2, ( int ) inTracePoint.arg3 );
            
        }
        break;

        case kAppleUSBODD_requestedExtraPower:
        {
            
            printf ( "[%10p] AUO - BeginProvidedServices requested %ldma of extra power and was granted %ldma\n",
                    ( void * ) inTracePoint.arg2, ( unsigned long ) inTracePoint.arg3, ( unsigned long ) inTracePoint.arg4 );
            
        }
        break;
            
        case kAppleUSBODD_isMacModelSupported:
        {
            
            printf ( "[%10p] AUO - isMacModelSupported returning %d\n",
                    ( void * ) inTracePoint.arg2, ( int ) inTracePoint.arg3 );
            
        }
        break;
            
        case kAppleUSBODD_FindACPIPlatformDevice:
        {
            
            printf ( "[%10p] AUO - FindACPIPlatformDevice found device %p\n",
                    ( void * ) inTracePoint.arg2, ( void * ) inTracePoint.arg3 );
            
        }
        break;
            
        case kAppleUSBODD_CheckForACPIFlags:
        {
            
            printf ( "[%10p] AUO - CheckForACPIFlasgs found device 0x%lx\n",
                    ( void * ) inTracePoint.arg2, ( unsigned long ) inTracePoint.arg3 );
            
        }
        break;
            
        default:
        {
            printf ( "Recieved request for intSubclassCode=0x%lx for AppleUSBODD, but know of no such intSubclassCode\n", ( unsigned long )inSubclassCode );
        }
            
    }
    
}


//-----------------------------------------------------------------------------
//	ProcessAppleUSBCardReaderUMCSubclassTracePoint
//-----------------------------------------------------------------------------

void
ProcessAppleUSBCardReaderUMCSubclassTracePoint ( kd_buf inTracePoint, UInt32 inSubclassCode )
{
    
    switch ( inSubclassCode )
    {
           
        case kAppleUSBCardReaderUMC_HandlePowerChange:
        {
            
            printf ( "[%10p] AUCRU - HandlePowerChange current power state %d, requested power state %d\n",
                    ( void * ) inTracePoint.arg2, ( int ) inTracePoint.arg3, ( int ) inTracePoint.arg4 );
            
        }
        break;
            
        case kAppleUSBCardReaderUMC_start:
        {
            
            printf ( "[%10p] AUCRU - start returning %d\n",
                    ( void * ) inTracePoint.arg2, ( int ) inTracePoint.arg3  );
            
        }
        break;
        
        case kAppleUSBCardReaderUMC_stop:
        {
            printf ( "[%10p] AUCRU - stop fProposedPowerState=%lu fCurrentPowerState=%lu\n",
                    ( void * ) inTracePoint.arg2, ( unsigned long ) inTracePoint.arg3, ( unsigned long ) inTracePoint.arg4 );
        }
        break;
            
        case kAppleUSBCardReaderUMC_stop_2:
        {
            printf ( "[%10p] AUCRU - stop sleeptype=%lu fDeviceRequiresReset=%d\n",
                        ( void * ) inTracePoint.arg2, ( unsigned long ) inTracePoint.arg3, ( int ) inTracePoint.arg4 );
        }
        break;
            
        case kAppleUSBCardReaderUMC_message:
        {
            printf ( "[%10p] AUCRU - message type=0x%x argument=0x%x\n", ( void * ) inTracePoint.arg2,
                        ( int ) inTracePoint.arg3, ( int ) inTracePoint.arg4 );
        }
        break;
            
        case kAppleUSBCardReaderUMC_setProperty:
        {
            printf ( "[%10p] AUCRU - setProperty\n", ( void * ) inTracePoint.arg2 );
        }
        break;
            
        case kAppleUSBCardReaderUMC_gpioMediaDetectFired:
        {
            printf ( "[%10p] AUCRU - GPIO media detect\n", ( void * ) inTracePoint.arg2 );
        }
        break;
            
        case kAppleUSBCardReaderUMC_gpioMediaDetectEnable:
        {
            
            if ( inTracePoint.arg3 == 2 )
            {
                printf ( "[%10p] AUCRU - GPIO media detect SETUP and ENABLED IOIES=%p\n", ( void * ) inTracePoint.arg2, ( void * ) inTracePoint.arg4 );
            }
            
            else if ( inTracePoint.arg3 == 1 )
            {
                printf ( "[%10p] AUCRU - GPIO media detect ENABLED IOIES=%p\n", ( void * ) inTracePoint.arg2, ( void * ) inTracePoint.arg4 );
            }
            
            else
            {
                printf ( "[%10p] AUCRU - GPIO media detect DISABLED IOIES=%p\n", ( void * ) inTracePoint.arg2, ( void * ) inTracePoint.arg4 );
            }
            
        }
        break;

        case kAppleUSBCardReaderUMC_controllerReset:
        {
            printf ( "[%10p] AUCRU - SDControllerReset returned=0x%x\n", ( void * ) inTracePoint.arg2, ( int ) inTracePoint.arg3 );
        }
        break;
            
        case kAppleUSBCardReaderUMC_powerControl:
        {
            
            if ( inTracePoint.arg4 != 0 )
            {
                printf ( "[%10p] AUCRU - SDControllerPower powering ON returned=0x%x\n", ( void * ) inTracePoint.arg2, ( int ) inTracePoint.arg3 );
            }
            
            else
            {
                printf ( "[%10p] AUCRU - SDControllerPower powering OFF returned=0x%x\n", ( void * ) inTracePoint.arg2, ( int ) inTracePoint.arg3 );
            }
            
        }
        break;
            
        case kAppleUSBCardReaderUMC_waitForReconnect:
        {
            
            if ( inTracePoint.arg4 != 0 )
            {
                printf ( "[%10p] AUCRU - GatedWaitForReconnect exiting returned=0x%x\n", ( void * ) inTracePoint.arg2, ( int ) inTracePoint.arg3 );
            }
            
            else
            {
                printf ( "[%10p] AUCRU - GatedWaitForReconnect entered returned=0x%x\n", ( void * ) inTracePoint.arg2, ( int ) inTracePoint.arg3 );
            }
            
        }
        break;
            
        case kAppleUSBCardReaderUMC_systemWillShutdown:
        {
            printf ( "[%10p] AUCRU - systemWillShutdown specifier=0x%x\n", ( void * ) inTracePoint.arg2, ( int ) inTracePoint.arg3 );
        }
        break;
            
        case kAppleUSBCardReaderUMC_generalPurpose:
        {
            printf ( "[%10p] AUCRU - General Purpose 0x%x 0x%x\n", ( void * ) inTracePoint.arg2,
                     ( int ) inTracePoint.arg3, ( int ) inTracePoint.arg4 );
        }
        break;
            
        default:
        {
            printf ( "Recieved request for intSubclassCode=0x%lx for AppleUSBCardReaderUMC, but know of no such intSubclassCode\n", ( unsigned long ) inSubclassCode );
        }
            
    }
    
}


//-----------------------------------------------------------------------------
//	EOF
//-----------------------------------------------------------------------------
