/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 2002 Apple Computer
 *
 * User Client CLI tool for the Sun GEM Ethernet Controller 
 *
 */



#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/network/IONetworkLib.h>
#include <IOKit/IOTypes.h>

#include <mach/mach.h>
#include <mach/mach_interface.h>

#include <sys/time.h>
#include <sys/file.h>



	int		DoIt( int );
	void	OutputBuffer();


		// find this elsewhere ...
	kern_return_t	io_connect_method_structureI_structureO
	(
		mach_port_t				connection,
		int						selector,
		io_struct_inband_t		input,
		mach_msg_type_number_t	inputCnt,
		io_struct_inband_t		output,
		mach_msg_type_number_t	*outputCnt
	);


	enum	/* request codes to send to the user client:	*/
	{
		kRltkUserCmd_GetLog			= 0x30,		// get entire Rltk ELG buffer
		kRltkUserCmd_GetRegs		= 0x31,		// get all Rltk registers
		kRltkUserCmd_GetOneReg		= 0x32,		// get one particular Rltk register
		kRltkUserCmd_GetTxRing		= 0x33,		// get Tx DMA elements
		kRltkUserCmd_GetRxRing		= 0x34,		// get Rx DMA elements
		kRltkUserCmd_WriteOneReg	= 0x35,		// write one particular Rltk register

		kRltkUserCmd_ReadAllMII	= 0x50,		// read MII registers 0 thru 31
		kRltkUserCmd_ReadMII	= 0x51,		// read one MII register
		kRltkUserCmd_WriteMII	= 0x52		// write one MII register
	};


	typedef struct					/* User Client Request structure:	*/
	{
		UInt32		reqID;			/* kRltkUserCmd_GetLog				*/
		UInt8		*pBuffer;		/* address of the buffer			*/
		UInt32		bufferSz;		/* size of the buffer				*/
	} UCRequest;


		/* Globals:	*/

	UCRequest		gInUCRequest;
	UInt8			gBuffer[ 4096 ];		// a page should fit the returned data.


		/* Prototypes:	*/

	kern_return_t	doRequest(	io_connect_t			connection,
								unsigned char			reqID,
            					void					*inputData,
								unsigned long			inputDataSize,
								void					*outputData,
								mach_msg_type_number_t	*outputDataSize );
	io_object_t		getInterfaceWithName( mach_port_t masterPort, char *className );




int main( int argc, char **argv )
{
	UInt32	regNum, regValue;
	int		rc;


	gInUCRequest.reqID		= 0;
	gInUCRequest.pBuffer	= 0;
	gInUCRequest.bufferSz	= 0;

	if ( argc == 1 )
		return DoIt( kRltkUserCmd_GetRegs );

	if ( argc == 2 && strcmp( argv[1], "-r" ) == 0 )
		return DoIt( kRltkUserCmd_GetRegs );

	if ( argc == 3 && strcmp( argv[1], "-r" ) == 0 )
	{
		rc = sscanf( argv[2], "%lx", &regNum );
		if ( rc == 1 && regNum < 0x9060 )
		{
			gInUCRequest.pBuffer = (UInt8*)regNum;
		//	printf( "Reading Rltk register %04lx\n", regNum );
			return DoIt( kRltkUserCmd_GetOneReg );
		}
		else
			printf( "Bad register number?\n" );
		return 0;
	}

	if ( argc == 4 && strcmp( argv[1], "-w" ) == 0 )
	{
		if ( strncmp( argv[2], "0x", 2) == 0 )		// skip over any leading 0x
			argv[2] += 2;
		rc = sscanf( argv[2], "%lx", &regNum );
		if ( rc == 1 && regNum < 0x9060 )
		{
			if ( strncmp( argv[3], "0x", 2) == 0 )		// skip over any leading 0x
				argv[3] += 2;
			rc = sscanf( argv[3], "%lx", &regValue );
			if ( rc == 1 )
			{
				gInUCRequest.pBuffer	= (UInt8*)regNum;
				gInUCRequest.bufferSz	= regValue;
			//	printf( "Writing Rltk register %04lx with 0x%lx\n", regNum, regValue );
				return DoIt( kRltkUserCmd_WriteOneReg );
			}
			else
				printf( "Bad value?\n" );
		}
		else
			printf( "Bad register number?\n" );
		return 0;
	}

	printf( "\n\t\t\tUsage:\n" );
	printf( "%s\t\tto dump all Rltk registers.\n", argv[0] );
	printf( "%s -r\t\t\t\tditto.\n", argv[0] );
	printf( "%s -r 0xAAAA \t\tto dump  Rltk register AAAA.\n", argv[0] );
	printf( "%s -w 0xAAAA VVVVVVVV\tto write Rltk register AAAA with VVVVVVVVV.\n", argv[0] );
	return 1;
}/* end main */


	/* Search the registry for an IONetworkInterface object with	*/
	/* the given name. If a match is found, the object is returned.	*/

io_object_t getInterfaceWithName( mach_port_t masterPort, char *className )
{
	io_iterator_t	ite;
	io_object_t		obj = 0;
	io_name_t		name;
	kern_return_t	rc;
	kern_return_t	kr;


    kr = IORegistryCreateIterator(	masterPort,
									kIOServicePlane,
									true,					/* recursive */
									&ite );
	if ( kr != kIOReturnSuccess )
	{
		printf( "IORegistryCreateIterator() error %08lx\n", (unsigned long)kr );
		return 0;
	}

	while ( (obj = IOIteratorNext( ite )) )
	{
		if ( IOObjectConformsTo( obj, (char*)className ) )
		{
		//	printf( "Found Realtek UserClient !!\n" );
			break;
		}
		else
		{
			rc = IOObjectGetClass( obj, name );
			if ( rc == kIOReturnSuccess )
			{
			//	printf( "Skipping class %s\n", name );
			}
		}
		IOObjectRelease( obj );
		obj = 0;
	}

    IORegistryDisposeEnumerator( ite );

    return obj;
}/* end getInterfaceWithName */


void OutputBuffer()
{
	UInt8	*pb = (UInt8*)gBuffer;			// pointer to LONG
	UInt32	i;

	for ( i = 0; i < 0x80; i++ )
	{
		if ( (i & 0xF) == 0 )					/* 4 values per line	*/
			printf( "\n%02x:", (unsigned int)i );
		if ( (i & 0x3) == 0 )
			printf( " " );						// extra space between LONGs

		printf( " %02x", pb[i] );
	}

	printf( "\n\n\tEnd of Realtek register dump.\n\n" );
	return;
}/* end OutputBuffer */


int DoIt( int doWhat )
{
    mach_port_t		masterPort;
    io_object_t		netif;		// network interface
    io_connect_t	conObj;		// connection object
    kern_return_t	kr;
	UInt32			outSize = sizeof( gBuffer );


	    /* Get master device port	*/

    kr = IOMasterPort( bootstrap_port, &masterPort );
    if ( kr != KERN_SUCCESS )
	{
		printf( "IOMasterPort() failed: %08lx\n", (unsigned long)kr );
		exit( 0 );
    }

///	netif = getInterfaceWithName( masterPort, "Realtek" );
    netif = getInterfaceWithName( masterPort, "com_apple_driver_RTL8139" );
    if ( !netif )
	{
		printf( "getInterfaceWithName failed.\n" );
    	exit( 0 );
	}

	kr = IOServiceOpen( netif, mach_task_self(), 'Rltk', &conObj );
	if ( kr != kIOReturnSuccess )
	{
		printf( "open device failed 0x%x\n", kr );
		IOObjectRelease( netif );
		exit( 0 );
	}

//	printf( "open device succeeded.\n" );

	gInUCRequest.reqID	= doWhat;

	kr = io_connect_method_structureI_structureO(
			conObj,									/* connection object			*/
			0,										/* method index for doRequest	*/
			(void*)&gInUCRequest,					/* input struct					*/
			sizeof( gInUCRequest ),					/* input size					*/
			(void*)&gBuffer,						/* output buffer				*/
			(mach_msg_type_number_t*)&outSize );	/* output size					*/

	if ( kr != kIOReturnSuccess )
	{
		printf( "Request failed 0x%x\n", kr );
	}
	else
	{
		switch ( doWhat )
		{
		case kRltkUserCmd_GetRegs:
			OutputBuffer();
			break;

		case kRltkUserCmd_GetOneReg:
			printf( "Register %04lx: %08lx\n", (UInt32)gInUCRequest.pBuffer, *(UInt32*)gBuffer );
			break;

		case kRltkUserCmd_WriteOneReg:
			printf( "Writing register %02lx with %08lx.\n",
					(UInt32)gInUCRequest.pBuffer,	gInUCRequest.bufferSz );
			break;
		}
	}

	IOServiceClose( conObj );
//	printf( "Closed device.\n" );

	IOObjectRelease( netif );
    exit( 0 );
}/* end DoIt */
