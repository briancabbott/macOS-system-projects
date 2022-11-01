/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 *  IOFireWireLib.h
 *  IOFireWireLib
 *
 *  Created on Thu Apr 27 2000.
 *  Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 */

/*! @header IOFireWireLib.h
IOFireWireLib is the software used by user space software to communicate with FireWire
devices and control the FireWire bus. IOFireWireLib is the lowest-level FireWire interface available
in user space.

To communicate with a device on the FireWire bus, an instance of IOFireWireDeviceInterface (a struct
which is defined below) is created. The methods of IOFireWireDeviceInterface allow you
to communicate with the device and create instances of other interfaces which provide extended 
functionality (for example, creation of unit directories on the local machine).

References to interfaces should be kept using the interface reference typedefs defined herein.
For example, you should use IOFireWireLibDeviceRef to refer to instances of IOFireWireDeviceInterface, 
IOFireWireLibCommandRef to refer to instances of IOFireWireCommandInterface, and so on.

To obtain an IOFireWireDeviceInterface for a device on the FireWire bus, use the function 
IOCreatePlugInInterfaceForService() defined in IOKit/IOCFPlugIn.h. (Note the "i" in "PlugIn" is 
always upper-case.) Quick usage reference:<br>
<ul>
	<li>'service' is a reference to the IOKit registry entry of the kernel object 
		(usually of type IOFireWireDevice) representing the device
		of interest. This reference can be obtained using the functions defined in
		IOKit/IOKitLib.h.</li>
	<li>'plugInType' should be CFUUIDGetUUIDBytes(kIOCFPlugInInterfaceID)</li>
	<li>'interfaceType' should be CFUUIDGetUUIDBytes(kIOFireWireLibTypeID) when using IOFireWireLib</li>
</ul>
The interface returned by IOCreatePlugInInterfaceForService() should be deallocated using 
IODestroyPlugInInterface(). Do not call Release() on it.

*/

#ifndef __IOFireWireLib_H__
#define __IOFireWireLib_H__

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/firewire/IOFireWireFamilyCommon.h>

#include <IOKit/firewire/IOFWIsoch.h>

// === [CFPlugIn support constants] ========================================

#pragma mark -
#pragma mark --- device interface UUIDs ---
// ============================================================
// device/unit/nub interfaces (newest first)
// ============================================================

//
// 	version 4 interfaces
//
//		availability: 
//				Mac OS X "Jaguar" and later
//
//	kIOFireWireNubInterface_v4
//		uuid string: 939151B8-6945-11D6-BEC7-0003933F84F0
#define kIOFireWireNubInterfaceID_v4	CFUUIDGetConstantUUIDWithBytes( kCFAllocatorDefault,\
											0x93, 0x91, 0x51, 0xB8, 0x69, 0x45, 0x11, 0xD6,\
											0xBE, 0xC7, 0x00, 0x03, 0x93, 0x3F, 0x84, 0xF0 )

//	kIOFireWireUnitInterface_v4
//		uuid string: D1A395C9-6945-11D6-9B32-0003933F84F0
#define kIOFireWireUnitInterfaceID_v4	CFUUIDGetConstantUUIDWithBytes( kCFAllocatorDefault,\
											0xD1, 0xA3, 0x95, 0xC9, 0x69, 0x45, 0x11, 0xD6,\
											0x9B, 0x32, 0x00, 0x03, 0x93, 0x3F, 0x84, 0xF0 )

//	kIOFireWireDeviceInterface_v4
//		uuid string: F4B3748B-6945-11D6-8299-0003933F84F0
#define kIOFireWireDeviceInterfaceID_v4	CFUUIDGetConstantUUIDWithBytes( kCFAllocatorDefault,\
											0xF4, 0xB3, 0x74, 0x8B, 0x69, 0x45, 0x11, 0xD6,\
											0x82, 0x99, 0x00, 0x03, 0x93, 0x3F, 0x84, 0xF0 )

//
// 	version 3 interfaces (include isochronous functions)
//
//		availability: 
//				Mac OS X 10.1.2 and later
//

//	kIOFireWireNubInterfaceID_v3
//		uuid string: F70FE149-E393-11D5-958A-0003933F84F0
#define kIOFireWireNubInterfaceID_v3	CFUUIDGetConstantUUIDWithBytes( kCFAllocatorDefault,\
											0xF7, 0x0F, 0xE1, 0x49, 0xE3, 0x93, 0x11, 0xD5,\
											0x95, 0x8A, 0x00, 0x03, 0x93, 0x3F, 0x84, 0xF0 )

//	kIOFireWireUnitInterfaceID_v3
//		uuid string: FE7A02EB-E393-11D5-8A61-0003933F84F0
#define kIOFireWireUnitInterfaceID_v3	CFUUIDGetConstantUUIDWithBytes( kCFAllocatorDefault,\
											0xFE, 0x7A, 0x02, 0xEB, 0xE3, 0x93, 0x11, 0xD5,\
											0x8A, 0x61, 0x00, 0x03, 0x93, 0x3F, 0x84, 0xF0 )
											
//	kIOFireWireDeviceInterfaceID_v3
//  	uuid string: 00EB71A0-E394-11D5-829A-0003933F84F0
#define kIOFireWireDeviceInterfaceID_v3	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x00, 0xEB, 0x71, 0xA0, 0xE3, 0x94, 0x11, 0xD5,\
											0x82, 0x9A, 0x00, 0x03, 0x93, 0x3F, 0x84, 0xF0 )

//
// 	version 2 interfaces (include isochronous functions)
//
//		availability: 
//				Mac OS X 10.1 and later
//

//	kIOFireWireNubInterfaceID
//		uuid string: 2575E4C4-B6C1-11D5-8F73-003065AF75CC
#define kIOFireWireNubInterfaceID		CFUUIDGetConstantUUIDWithBytes( kCFAllocatorDefault,\
											0x25, 0x75, 0xE4, 0xC4, 0xB6, 0xC1, 0x11, 0xD5,\
											0x8F, 0x73, 0x00, 0x30, 0x65, 0xAF, 0x75, 0xCC )

//	kIOFireWireUnitInterfaceID
//		uuid string: A02CC5D4-B6C1-11D5-AEA8-003065AF75CC
#define kIOFireWireUnitInterfaceID		CFUUIDGetConstantUUIDWithBytes( kCFAllocatorDefault,\
											0xA0, 0x2C, 0xC5, 0xD4, 0xB6, 0xC1, 0x11, 0xD5,\
											0xAE, 0xA8, 0x00, 0x30, 0x65, 0xAF, 0x75, 0xCC )

//	kIOFireWireDeviceInterfaceID_v2
//  	uuid string: B3993EB8-56E2-11D5-8BD0-003065423456
#define kIOFireWireDeviceInterfaceID_v2	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0xB3, 0x99, 0x3E, 0xB8, 0x56, 0xE2, 0x11, 0xD5,\
											0x8B, 0xD0, 0x00, 0x30, 0x65, 0x42, 0x34, 0x56)

//
//	version 1 interfaces
//
//		availablity: 
//				Mac OS X 10.0.0 and later
//

//	kIOFireWireDeviceInterfaceID
// 	(obsolete: do not use. may be removed in the future.)
//		uuid string: E3DF4460-F197-11D4-8AC8-000502072F80
#define kIOFireWireDeviceInterfaceID	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0xE3, 0xDF, 0x44, 0x60, 0xF1, 0x97, 0x11, 0xD4,\
											0x8A, 0xC8, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)


// ============================================================
// plugin loading
// ============================================================
#pragma mark -
#pragma mark --- plugin loading UUIDs ---

// 	uuid string: A1478010-F197-11D4-A28B-000502072F80
#define	kIOFireWireLibFactoryID			CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0xA1, 0x47, 0x80, 0x10,0xF1, 0x97, 0x11, 0xD4,\
											0xA2, 0x8B, 0x00, 0x05,0x02, 0x07, 0x2F, 0x80)
											
//	uuid string: CDCFCA94-F197-11D4-87E6-000502072F80
#define kIOFireWireLibTypeID			CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0xCD, 0xCF, 0xCA, 0x94, 0xF1, 0x97, 0x11, 0xD4,\
											0x87, 0xE6, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)

// ============================================================
// command objects
// ============================================================

//
//	version 2 interfaces:
//
//		availability: 
//				Mac OS X "Jaguar" and later
//

//	kIOFireWireCompareSwapCommandInterfaceID_v2
//	uuid string: 6100FEC9-6946-11D6-8A49-0003933F84F0
#define kIOFireWireCompareSwapCommandInterfaceID_v2		CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x61, 0x00, 0xFE, 0xC9, 0x69, 0x46, 0x11, 0xD6,\
											0x8A, 0x49, 0x00, 0x03, 0x93, 0x3F, 0x84, 0xF0 )

//
//	version 1 interfaces
//

//	uuid string: F8B6993A-F197-11D4-A3F1-000502072F80
#define kIOFireWireCommandInterfaceID	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0xF8, 0xB6, 0x99, 0x3A, 0xF1, 0x97, 0x11, 0xD4,\
											0xA3, 0xF1, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)

//  uuid string: AB26F124-76E9-11D5-86D5-003065423456
#define kIOFireWireReadCommandInterfaceID_v2 CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0xAB, 0x26, 0xF1, 0x24, 0x76, 0xE9, 0x11, 0xD5,\
											0x86, 0xD5, 0x00, 0x30, 0x65, 0x42, 0x34, 0x56)

//	uuid string: 1023605C-76EA-11D5-B82A-003065423456
#define kIOFireWireWriteCommandInterfaceID_v2 CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x10, 0x23, 0x60, 0x5C, 0x76, 0xEA, 0x11, 0xD5,\
											0xB8, 0x2A, 0x00, 0x30, 0x65, 0x42, 0x34, 0x56)

//	uuid string: 70C10E38-F64A-11D4-AFE7-0050E4D93B36
#define kIOFireWireCompareSwapCommandInterfaceID	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x70, 0xC1, 0x0E, 0x38, 0xF6, 0x4A, 0x11, 0xD4,\
											0xAF, 0xE7, 0x00, 0x50, 0xE4, 0xD9, 0x3B, 0x36)

// obsolete: do not use. may be removed in the future.
//	uuid string: 3D72672A-F64A-11D4-9683-0050E4D93B36
#define kIOFireWireReadQuadletCommandInterfaceID	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x3D, 0x72, 0x67, 0x2A, 0xF6, 0x4A, 0x11, 0xD4,\
											0x96, 0x83, 0x00, 0x50, 0xE4, 0xD9, 0x3B, 0x36)

// obsolete: do not use. may be removed in the future.
//	uuid string: 5C9423CE-F64A-11D4-AB7B-0050E4D93B36
#define kIOFireWireWriteQuadletCommandInterfaceID	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x5C, 0x94, 0x23, 0xCE, 0xF6, 0x4A, 0x11, 0xD4,\
											0xAB, 0x7B, 0x00, 0x50, 0xE4, 0xD9, 0x3B, 0x3)

// obsolete: do not use. may be removed in the future.
//	uuid string: 6E32F9D4-F63A-11D4-A194-003065423456
#define kIOFireWireReadCommandInterfaceID	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x6E, 0x32, 0xF9, 0xD4, 0xF6, 0x3A, 0x11, 0xD4,\
											0xA1, 0x94, 0x00, 0x30, 0x65, 0x42, 0x34, 0x56)

// obsolete: do not use. may be removed in the future.
//	uuid string: 4EDDED10-F64A-11D4-B7A5-0050E4D93B36
#define kIOFireWireWriteCommandInterfaceID	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x4E, 0xDD, 0xED, 0x10, 0xF6, 0x4A, 0x11, 0xD4,\
											0xB7, 0xA5, 0x00, 0x50, 0xE4, 0xD9, 0x3B, 0x36)


// ============================================================
// address spaces
// ============================================================

//	uuid string: 0D32AC50-F198-11D4-8DB5-000502072F80
#define kIOFireWirePseudoAddressSpaceInterfaceID CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x0D, 0x32, 0xAC, 0x50, 0xF1, 0x98, 0x11, 0xD4,\
											0x8D, 0xB5, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)

//	uuid string: 489110F6-F198-11D4-8BEB-000502072F80
#define kIOFireWirePhysicalAddressSpaceInterfaceID CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x48, 0x91, 0x10, 0xF6, 0xF1, 0x98, 0x11, 0xD4,\
											0x8B, 0xEB, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)

// ============================================================
// config ROM
// ============================================================

//	uuid string: 69CA4D74-F198-11D4-B325-000502072F80
#define kIOFireWireLocalUnitDirectoryInterfaceID CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x69, 0xCA, 0x4D, 0x74, 0xF1, 0x98, 0x11, 0xD4,\
											0xB3, 0x25, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)

//  uuid string: 7D43B506-F198-11D4-AA10-000502072F80
#define kIOFireWireConfigDirectoryInterfaceID CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x7D, 0x43, 0xB5, 0x06, 0xF1, 0x98, 0x11, 0xD4,\
											0xAA, 0x10, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)

// ============================================================
//
// IOFireWireLib interface typedefs
//
// ============================================================

typedef struct 	IOFireWireDeviceInterface_t**	 			IOFireWireLibDeviceRef ;
typedef		   	IOFireWireLibDeviceRef						IOFireWireLibUnitRef ;
typedef			IOFireWireLibDeviceRef						IOFireWireLibNubRef ;
typedef struct 	IOFireWirePseudoAddressSpaceInterface_t**	IOFireWireLibPseudoAddressSpaceRef ;
typedef struct 	IOFireWirePhysicalAddressSpaceInterface_t**	IOFireWireLibPhysicalAddressSpaceRef ;
typedef struct 	IOFireWireLocalUnitDirectoryInterface_t**	IOFireWireLibLocalUnitDirectoryRef ;
typedef struct 	IOFireWireConfigDirectoryInterface_t**		IOFireWireLibConfigDirectoryRef ;

typedef struct 	IOFireWireCommandInterface_t**				IOFireWireLibCommandRef ;
typedef struct 	IOFireWireReadCommandInterface_t**			IOFireWireLibReadCommandRef ;
typedef struct 	IOFireWireReadQuadletCommandInterface_t**	IOFireWireLibReadQuadletCommandRef ;
typedef struct 	IOFireWireWriteCommandInterface_t**			IOFireWireLibWriteCommandRef ;
typedef struct 	IOFireWireWriteQuadletCommandInterface_t**	IOFireWireLibWriteQuadletCommandRef ;
typedef struct 	IOFireWireCompareSwapCommandInterface_t**	IOFireWireLibCompareSwapCommandRef ;

// --- isoch interfaces ----------
typedef struct 	IOFireWireIsochChannelInterface_t**			IOFireWireLibIsochChannelRef ;
typedef struct 	IOFireWireIsochPortInterface_t**			IOFireWireLibIsochPortRef ;
typedef struct 	IOFireWireRemoteIsochPortInterface_t**		IOFireWireLibRemoteIsochPortRef ;
typedef struct 	IOFireWireLocalIsochPortInterface_t**		IOFireWireLibLocalIsochPortRef ;
typedef struct 	IOFireWireDCLCommandPoolInterface_t**		IOFireWireLibDCLCommandPoolRef ;

// ============================================================
//
// IOFireWireLib callback typedefs
//
// ============================================================

/*!	@functiongroup group_1 */
/*!	@function IOFireWirePseudoAddressSpaceReadHandler
	@abstract This callback is called to handle read requests to pseudo address spaces. This function
		should fill in the specified area in the pseudo address space backing store and call
		ClientCommandIsComplete with the specified command ID
	@param addressSpace The address space to which the request is being made
	@param commandID An FWClientCommandID which should be passed to ClientCommandIsComplete when
		the buffer has been filled in
	@param packetLen number of bytes requested
	@param packetOffset number of bytes from beginning of address space backing store
	@param srcNodeID nodeID of the requester
	@param destAddressHi high 16 bits of destination address on this computer
	@param destAddressLo low 32 bits of destination address on this computer
	@param refCon user specified reference number passed in when the address space was created
*/
typedef UInt32	(*IOFireWirePseudoAddressSpaceReadHandler)(
					IOFireWireLibPseudoAddressSpaceRef	addressSpace,
					FWClientCommandID					commandID,
					UInt32								packetLen,
					UInt32								packetOffset,
					UInt16								srcNodeID,		// nodeID of requester
					UInt32								destAddressHi,	// destination on this node
					UInt32								destAddressLo,
					UInt32								refCon) ;

/*!	@typedef IOFireWirePseudoAddressSpaceSkippedPacketHandler
	@abstract Callback called when incoming packets have been dropped from the internal queue
	@param addressSpace The address space which dropped the packet(s)
	@param commandID An FWClientCommandID to be passed to ClientCommandIsComplete()
	@param skippedPacketCount The number of skipped packets
*/
typedef void	(*IOFireWirePseudoAddressSpaceSkippedPacketHandler)(
					IOFireWireLibPseudoAddressSpaceRef	addressSpace,
					FWClientCommandID					commandID,
					UInt32								skippedPacketCount) ;

/*! @typedef IOFireWirePseudoAddressSpaceWriteHandler
	@abstract Callback called to handle write requests to a pseudo address space.
	@param addressSpace The address space to which the write is being made
	@param commandID An FWClientCommandID to be passed to ClientCommandIsComplete()
	@param packetLen Length in bytes of incoming packet
	@param packet Pointer to the received data
	@param srcNodeID Node ID of the sender
	@param destAddressHi high 16 bits of destination address on this computer
	@param destAddressLo low 32 bits of destination address on this computer
	@param refCon user specified reference number passed in when the address space was created
*/
typedef UInt32 (*IOFireWirePseudoAddressSpaceWriteHandler)(
					IOFireWireLibPseudoAddressSpaceRef	addressSpace,
					FWClientCommandID					commandID,
					UInt32								packetLen,
					void*								packet,
					UInt16								srcNodeID,		// nodeID of sender
					UInt32								destAddressHi,	// destination on this node
					UInt32								destAddressLo,
					UInt32								refCon) ;

/*!	@typedef IOFireWireBusResetHandler
	@abstract Called when a bus reset has occured, but before FireWire has completed
		configuring the bus.
	@param interface A reference to the device on which the callback was installed
	@param commandID An FWClientCommandID to be passed to ClientCommandIsComplete()
*/
typedef void 	(*IOFireWireBusResetHandler)(
					IOFireWireLibDeviceRef				interface,
					FWClientCommandID					commandID );	// parameters may change
					
/*!
	@typedef IOFireWireBusResetDoneHandler
	@abstract Called when a bus reset has occured and FireWire has completed configuring
		the bus.
	@param interface A reference to the device on which the callback was installed
	@param commandID An FWClientCommandID to be passed to ClientCommandIsComplete()
*/
typedef void 	(*IOFireWireBusResetDoneHandler)(
					IOFireWireLibDeviceRef				interface,
					FWClientCommandID					commandID ) ;	// parameters may change

/*!	@typedef IOFireWireLibCommandCallback
	@abstract Callback called when an asynchronous command has completed executing
	@param refCon A user specified reference value set before command object was submitted
*/
typedef void	(*IOFireWireLibCommandCallback)(
					void*								refCon,
					IOReturn							completionStatus) ;

// unused.
typedef void (*IOFireWireDeviceAddedCallback)() ;
typedef void (*IOFireWireDeviceRemovedCallback)() ;

// unused.
typedef struct FWInterfaceCallBacks_t
{
	IOFireWireBusResetHandler					busResetHandler ;
	IOFireWireBusResetDoneHandler				busResetDoneHandler ;
} FWInterfaceCallBacks ;

// unused.
typedef struct IOFireWirePseudoAddressSpaceCallbacks_t
{
	IOFireWirePseudoAddressSpaceReadHandler				readHandler ;
	IOFireWirePseudoAddressSpaceSkippedPacketHandler	skippedPacketHandler ;
	IOFireWirePseudoAddressSpaceWriteHandler			writeHandler ;
} IOFireWirePseudoAddressSpaceCallbacks ;

#pragma mark -
#pragma mark --- IOFireWireDeviceInterface ----------

// ============================================================
//
// IOFireWireDeviceInterface
//
// ============================================================

/*!	@interface IOFireWireDeviceInterface
	@abstract IOFireWireDeviceInterface is your primary gateway to the functionality contained in
		IOFireWireLib.
	@discussion	
		You can use IOFireWireDeviceInterface to:<br>
	<ul>
		<li>perform synchronous read, write and lock operations</li>
		<li>perform other miscellanous bus operations, such as reset the FireWire bus. </li>
		<li>create FireWire command objects and interfaces used to perform
			synchronous/asynchronous read, write and lock operations. These include:</li>
		<ul type="square">
			<li>IOFireWireReadCommandInterface
			<li>IOFireWireReadQuadletCommandInterface
			<li>IOFireWireWriteCommandInterface
			<li>IOFireWireWriteQuadletCommandInterface
			<li>IOFireWireCompareSwapCommandInterface
		</ul>
		<li>create interfaces which provide a other extended services. These include:</li>
		<ul type="square">	
			<li>IOFireWirePseudoAddressSpaceInterface -- pseudo address space services</li>
			<li>IOFireWirePhysicalAddressSpaceInterface -- physical address space services</li>
			<li>IOFireWireLocalUnitDirectoryInterface -- manage local unit directories in the mac</li>
			<li>IOFireWireConfigDirectoryInterface -- access and browse remote device config directories</li>
		</ul>
		<li>create interfaces which provide isochronous services (see IOFireWireLibIsoch.h). These include:</li>
		<ul type="square">	
			<li>IOFireWireIsochChannelInterface -- create/manage talker and listener isoch channels</li>
			<li>IOFireWireLocalIsochPortInterface -- create local isoch ports</li>
			<li>IOFireWireRemoteIsochPortInterface -- create remote isoch ports</li>
			<li>IOFireWireDCLCommandPoolInterface -- create a DCL command pool allocator.</li>
		</ul>
	</ul>

*/
typedef struct IOFireWireDeviceInterface_t
{
	IUNKNOWN_C_GUTS ;

	UInt32 version, revision ; // version/revision
	
		// --- maintenance methods -------------
    /*!
	@functiongroup class_group_1
    */
    /*!
        @function InterfaceIsInited
        @abstract Determine whether interface has been properly inited.
        @param self The device interface to use.
        @result Returns true if interface is inited and false if is it not.
    */
	Boolean				(*InterfaceIsInited)(IOFireWireLibDeviceRef self) ;

    /*!
        @function GetDevice
        @abstract Get the IOKit service to which this interface is connected.
        @param self The device interface to use.
        @result Returns an io_object_t corresponding to the device the interface is
			using
    */
	io_object_t			(*GetDevice)(IOFireWireLibDeviceRef self) ;

    /*!
	@functiongroup class_group_2
    */
    /*!
        @function Open
        @abstract Open the connected device for exclusive access. When you have
			the device open using this method, all accesses by other clients of 
			this device will be denied until Close() is called.
        @param self The device interface to use.
        @result An IOReturn error code
    */
	IOReturn			(*Open)(IOFireWireLibDeviceRef self) ;

    /*!
        @function OpenWithSessionRef
        @abstract An open function which allows this interface to have access
			to the device when already opened. The service which has already opened
			the device must be able to provide an IOFireWireSessionRef.
        @param self The device interface to use
		@param IOFireWireSessionRef The sessionRef returned from the client who has
			the device open
        @result An IOReturn error code
    */
	IOReturn			(*OpenWithSessionRef)(IOFireWireLibDeviceRef self, IOFireWireSessionRef sessionRef) ;

    /*!
	@functiongroup class_group_1
    */
    /*!
        @function Close
        @abstract Release exclusive access to the device
        @param self The device interface to use
    */
	void				(*Close)(IOFireWireLibDeviceRef self) ;
	
		// --- notification --------------------
	/*!
		@function NotificationIsOn
		@abstract Determine whether callback notifications for this interface are currently active
        @param self The device interface to use
		@result A Boolean value where true indicates notifications are active
	*/
	const Boolean		(*NotificationIsOn)(IOFireWireLibDeviceRef self) ;
	
	/*!
		@function AddCallbackDispatcherToRunLoop
		@abstract Installs the proper run loop event source to allow callbacks to function. This method
			must be called before callback notifications for this interface or any interfaces
			created using this interface can function.
        @param self The device interface to use.
		@param inRunLoop The run loop on which to install the event source
	*/
	const IOReturn 		(*AddCallbackDispatcherToRunLoop)(IOFireWireLibDeviceRef self, CFRunLoopRef inRunLoop) ;
	
	/*!
		@function RemoveCallbackDispatcherFromRunLoop
		@abstract Reverses the effects of AddCallbackDispatcherToRunLoop(). This method removes 
			the run loop event source that was added to the specified run loop preventing any 
			future callbacks from being called
        @param self The device interface to use.			
	*/
	const void			(*RemoveCallbackDispatcherFromRunLoop)(IOFireWireLibDeviceRef self) ;
	
	/*!
		@function TurnOnNotification
		@abstract Activates any callbacks specified for this device interface. Only works after 
			AddCallbackDispatcherToRunLoop has been called. See also AddIsochCallbackDispatcherToRunLoop().
        @param self The device interface to use.
		@result A Boolean value. Returns true on success.
	*/
	const Boolean 		(*TurnOnNotification)(IOFireWireLibDeviceRef self) ;

	/*!
		@function TurnOffNotification
		@abstract Deactivates and callbacks specified for this device interface. Reverses the 
			effects of TurnOnNotification()
        @param self The device interface to use.
	*/
	void				(*TurnOffNotification)(IOFireWireLibDeviceRef self) ;
	
	/*!
		@function SetBusResetHandler
		@abstract Sets the callback that should be called when a bus reset occurs. Note that this callback
			can be called multiple times before the bus reset done handler is called. (f.ex., multiple bus
			resets might occur before bus reconfiguration has completed.)
        @param self The device interface to use.
		@param handler Function pointer to the handler to install
		@result Returns an IOFireWireBusResetHandler function pointer to the previously installed
			bus reset handler. Returns 0 if none was set.
	*/
	const IOFireWireBusResetHandler	
						(*SetBusResetHandler)(IOFireWireLibDeviceRef self, IOFireWireBusResetHandler handler) ;
	
	/*!
		@function SetBusResetDoneHandler
		@abstract Sets the callback that should be called after a bus reset has occurred and reconfiguration
			of the bus has been completed. This function will only be called once per bus reset.
        @param self The device interface to use.
		@param handler Function pointer to the handler to install
		@result Returns on IOFireWireBusResetDoneHandler function pointer to the previously installed
			bus reset handler. Returns 0 if none was set.
	*/
	const IOFireWireBusResetDoneHandler	
						(*SetBusResetDoneHandler)(IOFireWireLibDeviceRef self, IOFireWireBusResetDoneHandler handler) ;
	/*!
		@function ClientCommandIsComplete
		@abstract This function must be called from callback routines once they have completed processing
			a callback. This function only applies to callbacks which take an IOFireWireLibDeviceRef (i.e. bus reset),
			parameter.
		@param commandID The command ID passed to the callback function when it was called
		@param status An IOReturn value indicating the completion status of the callback function
	*/
	void				(*ClientCommandIsComplete)(IOFireWireLibDeviceRef self, FWClientCommandID commandID, IOReturn status) ;
	
		// --- read/write/lock operations -------
	/*!
		@function Read
		@abstract Perform synchronous block read
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to read. 
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param buf A pointer to a buffer where the results will be stored
		@param size Number of bytes to read
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in generation. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOReturn error code
	*/
	IOReturn			(*Read)(IOFireWireLibDeviceRef	self, 
								io_object_t 		device,
								const FWAddress* addr, 
								void* 				buf, 
								UInt32* 			size, 
								Boolean 			failOnReset, 
								UInt32 				generation) ;

	/*!
		@function ReadQuadlet
		@abstract Perform synchronous quadlet read
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to read. 
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param value A pointer to where to data should be stored
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in generation. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOReturn error code
	*/
	IOReturn			(*ReadQuadlet)(	IOFireWireLibDeviceRef	self, 
										io_object_t 			device,
										const FWAddress* 		addr, 
										UInt32* 				val, 
										Boolean 				failOnReset, 
										UInt32 					generation) ;
	/*!
		@function Write
		@abstract Perform synchronous block write
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param buf A pointer to a buffer where the results will be stored
		@param size Number of bytes to read
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOReturn error code
	*/
	IOReturn			(*Write)(	IOFireWireLibDeviceRef 	self, 
									io_object_t 			device, 
									const FWAddress* 		addr, 
									const void* 			buf, 
									UInt32* 				size,
									Boolean 				failOnReset, 
									UInt32 					generation) ;

	/*!
		@function WriteQuadlet
		@abstract Perform synchronous quadlet write
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param val The value to write
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOReturn error code
	*/
	IOReturn (*WriteQuadlet)(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress* addr, const UInt32 val, Boolean failOnReset, UInt32 generation) ;

	/*!
		@function CompareSwap
		@abstract Perform synchronous lock operation
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param cmpVal The check/compare value
		@param newVal Value to set
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOReturn error code
	*/
	IOReturn (*CompareSwap)(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress* addr, UInt32 cmpVal, UInt32 newVal, Boolean failOnReset, UInt32 generation) ;
	
	// --- FireWire command object methods ---------

	/*!
		@function CreateReadCommand
		@abstract Create a block read command object.
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported. Setting the callback value to nil defaults to synchronous execution.
		@param addr Command target address
		@param buf A pointer to a buffer where the results will be stored
		@param size Number of bytes to read
		@param callback Command completion callback.
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOFireWireLibCommandRef interface. See IOFireWireLibCommandRef.
	*/
	IOFireWireLibCommandRef (*CreateReadCommand)( IOFireWireLibDeviceRef self, io_object_t device, const FWAddress * addr, void* buf, UInt32 size, IOFireWireLibCommandCallback callback, Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid) ;

	/*!	@function CreateReadQuadletCommand
		@abstract Create a quadlet read command object.
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported. Setting the callback value to nil defaults to synchronous execution.
		@param addr Command target address
		@param quads An array of quadlets where results should be stored
		@param numQuads Number of quadlets to read
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@param 
		@result An IOFireWireLibCommandRef interface. See IOFireWireLibCommandRef.*/
	IOFireWireLibCommandRef (*CreateReadQuadletCommand)( IOFireWireLibDeviceRef self, io_object_t device, const FWAddress * addr, UInt32 quads[], UInt32 numQuads, IOFireWireLibCommandCallback callback, Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid) ;

	/*!	@function CreateWriteCommand
		@abstract Create a block write command object.
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported. Setting the callback value to nil defaults to synchronous execution.
		@param addr Command target address
		@param buf A pointer to the buffer containing the data to be written
		@param size Number of bytes to write
		@param callback Command completion callback.
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOFireWireLibCommandRef interface. See IOFireWireLibCommandRef.*/
	IOFireWireLibCommandRef (*CreateWriteCommand)( IOFireWireLibDeviceRef self, io_object_t device, const FWAddress * addr, void* buf, UInt32  size, IOFireWireLibCommandCallback callback, Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid) ;

	/*!
		@function CreateWriteQuadletCommand
		@abstract Create a quadlet write command object.
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported. Setting the callback value to nil defaults to synchronous execution.
		@param addr Command target address
		@param quads An array of quadlets containing quadlets to be written
		@param numQuads Number of quadlets to write
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOFireWireLibCommandRef interface. See IOFireWireLibCommandRef.
	*/
	IOFireWireLibCommandRef (*CreateWriteQuadletCommand)(IOFireWireLibDeviceRef	self, io_object_t device, const FWAddress *	addr, UInt32 quads[], UInt32 numQuads, IOFireWireLibCommandCallback callback, Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid) ;

	/*!
		@function CreateCompareSwapCommand
		@abstract Create a quadlet compare/swap command object.
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported. Setting the callback value to nil defaults to synchronous execution.
		@param addr Command target address
		@param cmpVal 32-bit value expected at target address
		@param newVal 32-bit value to be set at target address
		@param callback Command completion callback.
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOFireWireLibCommandRef interface. See IOFireWireLibCommandRef.	*/
	IOFireWireLibCommandRef (*CreateCompareSwapCommand)( IOFireWireLibDeviceRef self, io_object_t device, const FWAddress *  addr, UInt32      cmpVal, UInt32      newVal, IOFireWireLibCommandCallback callback, Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid) ;

		// --- other methods ---------------------------
	/*!	@function BusReset
		@abstract Cause a bus reset
		@param self The device interface to use. */	
	IOReturn (*BusReset)( IOFireWireLibDeviceRef  self) ;

	/*!	@function GetCycleTime
		@abstract Get bus cycle time.
		@param self The device interface to use.
		@param outCycleTime A pointer to a UInt32 to hold the result
		@result An IOReturn error code.	*/	
	IOReturn (*GetCycleTime)( IOFireWireLibDeviceRef  self, UInt32*  outCycleTime) ;

	/*!	@function GetGenerationAndNodeID
		@abstract (Obsolete) Get bus generation and remote device node ID.
		@discussion Obsolete -- Please use GetBusGeneration() and/or GetRemoteNodeID() in
			interface v4.
		@param self The device interface to use.
		@param outGeneration A pointer to a UInt32 to hold the generation result
		@param outNodeID A pointer to a UInt16 to hold the remote device node ID
		@result An IOReturn error code.	*/	
	IOReturn (*GetGenerationAndNodeID)( IOFireWireLibDeviceRef  self, UInt32*  outGeneration, UInt16*  outNodeID) ;

	/*!	@function GetLocalNodeID
		@abstract (Obsolete) Get local node ID.
		@discussion Obsolete -- Please use GetBusGeneration() and GetLocalNodeIDWithGeneration() in
			interface v4.
		@param self The device interface to use.
		@param outNodeID A pointer to a UInt16 to hold the local device node ID
		@result An IOReturn error code.	*/	
	IOReturn (*GetLocalNodeID)( IOFireWireLibDeviceRef  self, UInt16*  outLocalNodeID) ;

	/*!	@function GetResetTime
		@abstract Get time since last bus reset.
		@param self The device interface to use.
		@param outResetTime A pointer to an AbsolutTime to hold the result.
		@result An IOReturn error code.	*/	
	IOReturn			(*GetResetTime)(
								IOFireWireLibDeviceRef 	self, 
								AbsoluteTime* 			outResetTime) ;

		// --- unit directory support ------------------
	/*!	@function CreateLocalUnitDirectory
		@abstract Creates a local unit directory object and returns an interface to it. An
			instance of a unit directory object corresponds to an instance of a unit 
			directory in the local machine's configuration ROM.
		@param self The device interface to use.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created unit directory object.
		@result An IOFireWireLibLocalUnitDirectoryRef. Returns 0 upon failure */
	 IOFireWireLibLocalUnitDirectoryRef (*CreateLocalUnitDirectory)( IOFireWireLibDeviceRef  self, REFIID  iid) ;

		// --- config directory support ----------------
	/*!	@function GetConfigDirectory
		@abstract Creates a config directory object and returns an interface to it. The
			created config directory object represents the config directory in the remote
			device or unit to which the creating device interface is attached.
		@param self The device interface to use.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created config directory object.
		@result An IOFireWireLibConfigDirectoryRef. Returns 0 upon failure */
	IOFireWireLibConfigDirectoryRef (*GetConfigDirectory)( IOFireWireLibDeviceRef  self, REFIID  iid) ;

	/*!	@function CreateConfigDirectoryWithIOObject
		@abstract This function can be used to create a config directory object and a
			corresponding interface from an opaque IOObject reference. Some configuration
			directory interface methods may return an io_object_t instead of an
			IOFireWireLibConfigDirectoryRef. Use this function to obtain an 
			IOFireWireLibConfigDirectoryRef from an io_object_t.
		@param self The device interface to use.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created config directory object.
		@result An IOFireWireLibConfigDirectoryRef. Returns 0 upon failure */
	IOFireWireLibConfigDirectoryRef (*CreateConfigDirectoryWithIOObject)( IOFireWireLibDeviceRef  self, io_object_t  inObject, REFIID  iid) ;

		// --- address space support -------------------
	/*!	@function CreatePseudoAddressSpace
		@abstract Creates a pseudo address space object and returns an interface to it. This
			will create a pseudo address space (software-backed) on the local machine. 
		@param self The device interface to use.
		@param inSize The size in bytes of this address space
		@param inRefCon A user specified reference value. This will be passed to all callback functions.
		@param inQueueBufferSize The size of the queue which receives packets from the bus before they are handed to
			the client and/or put in the backing store. A larger queue can help eliminate dropped packets
			when receiving large bursts of data. When a packet is received which can not fit into the queue, 
			the packet dropped callback will be called. 
		@param inBackingStore An optional block of allocated memory representing the contents of the address space.
		@param inFlags A UInt32 with bits set corresponding to the flags that should be set
			for this address space.
			<ul>
				<li>kFWAddressSpaceNoFlags -- All flags off</li>
				<li>kFWAddressSpaceNoWriteAccess -- Write access to this address space will be disallowed. 
					Setting this flag also disables compare/swap transactions on this address space.</li>
				<li>kFWAddressSpaceNoReadAccess -- Read access access to this address space will be disallowed. 
					Setting this flag also disables compare/swap transactions on this address space.</li>
				<li>kFWAddressSpaceAutoWriteReply -- Writes will be made automatically, directly modifying the contents
					of the backing store. The user process will not be notified of writes.</li>
				<li>kFWAddressSpaceAutoReadReply -- Reads to this address space will be answered automagically
					using the contents of the backing store. The user process will not be notified of reads.</li>
				<li>kFWAddressSpaceAutoCopyOnWrite -- Writes to this address space will be made directly
					to the backing store at the same time the user process is notified of a write.</li>
			</ul>
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created pseudo address space object.
		@result An IOFireWireLibPseudoAddressSpaceRef. Returns 0 upon failure */
	IOFireWireLibPseudoAddressSpaceRef	(*CreatePseudoAddressSpace)( 
												IOFireWireLibDeviceRef  	self, 
												UInt32  					inSize, 
												void*  						inRefCon, 
												UInt32  					inQueueBufferSize, 
												void*  						inBackingStore, 
												UInt32  					inFlags, 
												REFIID  					iid) ;

	/*!	@function CreatePhysicalAddressSpace
		@abstract Creates a physical address space object and returns an interface to it. This
			will create a physical address space on the local machine. 
		@param self The device interface to use.
		@param inBackingStore An block of allocated memory representing the contents of the address space.
		@param inSize The size in bytes of this address space
		@param inFlags A UInt32 with bits set corresponding to the flags that should be set
			for this address space. For future use -- always pass 0.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created physical address space object.
		@result An IOFireWireLibPhysicalAddressSpaceRef. Returns 0 upon failure */
	IOFireWireLibPhysicalAddressSpaceRef (*CreatePhysicalAddressSpace)( IOFireWireLibDeviceRef  self, UInt32  inSize, void*  inBackingStore, UInt32  inFlags, REFIID  iid) ;
		
		// --- debugging -------------------------------
	IOReturn (*FireBugMsg)( IOFireWireLibDeviceRef  self, const char*  msg) ;

	//
	// NOTE: the following methods available only in interface v2 and later
	//

		// --- eye-sock-run-U.S. -----------------------
	/*!	@function AddIsochCallbackDispatcherToRunLoop
		@abstract This function adds an event source for the isochronous callback dispatcher
			to the specified CFRunLoop. Isochronous related callbacks will not function
			before this function is called. This functions is similar to 
			AddCallbackDispatcherToRunLoop. The passed CFRunLoop can be different
			from that passed to AddCallbackDispatcherToRunLoop. 
		@param self The device interface to use.
		@param inRunLoop A CFRunLoopRef for the run loop to which the event loop source 
			should be added
		@result An IOReturn error code.	*/	
	IOReturn			(*AddIsochCallbackDispatcherToRunLoop)(
								IOFireWireLibDeviceRef 	self, 
								CFRunLoopRef 			inRunLoop) ;

	/*!	@function CreateRemoteIsochPort
		@abstract Creates a remote isochronous port object and returns an interface to it. A
			remote isochronous port object is an abstract entity used to represent a remote
			talker or listener device on an isochronous channel. 
		@param self The device interface to use.
		@param inTalking Pass true if this port represents an isochronous talker. Pass
			false if this port represents an isochronous listener.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created remote isochronous port object.
		@result An IOFireWireLibRemoteIsochPortRef. Returns 0 upon failure */
	IOFireWireLibRemoteIsochPortRef
						(*CreateRemoteIsochPort)(
								IOFireWireLibDeviceRef	self,
								Boolean					inTalking,
								REFIID					iid) ;

	/*!	@function CreateLocalIsochPort
		@abstract Creates a local isochronous port object and returns an interface to it. A
			local isochronous port object is an abstract entity used to represent a
			talking or listening endpoint in the local machine. 
		@param self The device interface to use.
		@param inTalking Pass true if this port represents an isochronous talker. Pass
			false if this port represents an isochronous listener.
		@param inDCLProgram A pointer to the first DCL command struct of the DCL program
			to be compiled and used to send or receive data on this port.
		@param inStartEvent Start event bits
		@param inStartState Start state bits
		@param inStartMask Start mask bits
		@param inDCLProgramRanges This is an optional optimization parameter which can be used
			to decrease the time the local port object spends determining which set of virtual
			ranges the passed DCL program occupies. Pass a pointer to an array of IOVirtualRange
			structs or nil to ignore this parameter.
		@param inDCLProgramRangeCount The number of virtual ranges passed to inDCLProgramRanges.
			Pass 0 for none.
		@param inBufferRanges This is an optional optimization parameter which can be used
			to decrease the time the local port object spends determining which set of virtual
			ranges the data buffers referenced by the passed DCL program occupy. Pass a pointer
			to an array of IOVirtualRange structs or nil to ignore this parameter.
		@param inBufferRangeCount The number of virtual ranges passed to inBufferRanges.
			Pass 0 for none.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created object.
		@result An IOFireWireLibLocalIsochPortRef. Returns 0 upon failure */
	IOFireWireLibLocalIsochPortRef
						(*CreateLocalIsochPort)(
								IOFireWireLibDeviceRef 	self, 
								Boolean					inTalking,
								DCLCommandPtr			inDCLProgram,
								UInt32					inStartEvent,
								UInt32					inStartState,
								UInt32					inStartMask,
								IOVirtualRange			inDCLProgramRanges[],	// optional optimization parameters
								UInt32					inDCLProgramRangeCount,	
								IOVirtualRange			inBufferRanges[],
								UInt32					inBufferRangeCount, 
								REFIID 					iid) ; 

	/*!	@function CreateIsochChannel
		@abstract Creates an isochronous channel object and returns an interface to it. An
			isochronous channel object is an abstract entity used to represent a
			FireWire isochronous channel. 
		@param self The device interface to use.
		@param doIRM Controls whether the channel automatically performs IRM operations. 
			Pass true if the channel should allocate its channel and bandwidth with
			the IRM. Pass false to ignore the IRM.
		@param packetSize Size in bytes of packets being sent or received with this channel.
			This is automatically translated into a bandwidth allocation appropriate
			for the speed passed in prefSpeed.
		@param prefSpeed The preferred bus speed of this channel.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created object.
		@result An IOFireWireLibIsochChannelRef. Returns 0 upon failure */
	IOFireWireLibIsochChannelRef
						(*CreateIsochChannel)(
								IOFireWireLibDeviceRef 	self, 
								Boolean 				doIrm, 
								UInt32 					packetSize, 
								IOFWSpeed 				prefSpeed, 
								REFIID 					iid ) ;

	/*!	@function CreateDCLCommandPool
		@abstract Creates a command pool object and returns an interface to it. The command 
			pool can be used to build DCL programs.
		@param self The device interface to use.
		@param size Starting size of command pool
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created object.
		@result An IOFireWireLibDCLCommandPoolRef. Returns 0 upon failure */
	IOFireWireLibDCLCommandPoolRef
						(*CreateDCLCommandPool)(
								IOFireWireLibDeviceRef	self, 
								IOByteCount 			size, 
								REFIID 					iid ) ;

	// --- refcons ---------------------------------
	/*!	@function GetRefCon
		@abstract Get user reference value set on this interface
		@param self The device interface to use.
		@result Returns the user's reference value set on this interface. */
	void*				(*GetRefCon)(
								IOFireWireLibDeviceRef	self) ;
	/*!	@function SetRefCon
		@abstract Set user reference value on this interface
		@param self The device interface to use.
		@param refCon The reference value to set. */
	void				(*SetRefCon)(
								IOFireWireLibDeviceRef	self,
								const void*				refCon) ;

	// --- debugging -------------------------------
	// do not use this function
	CFTypeRef			(*GetDebugProperty)(
								IOFireWireLibDeviceRef	self,
								void*					interface,
								CFStringRef				inPropertyName,
								CFTypeID*				outPropertyType) ;

	/*!	@function PrintDCLProgram
		@abstract Walk a DCL program linked list and print its contents
		@param self The device interface to use.
		@param inProgram A pointer to the first DCL of the program to print
		@param inLength Number of DCLs expected in the program. PrintDCLProgram() will
			report an error if this number does not match the number of DCLs found
			in the program. */
	void 				(*PrintDCLProgram)(
								IOFireWireLibDeviceRef	self, 
								const DCLCommandPtr		inProgram, 
								UInt32 					inLength) ;

	//
	// NOTE: the following methods available only in interface v3 and later
	//

	// --- v3 functions ----------
	/*!	@function CreateInitialUnitsPseudoAddressSpace
		@abstract Creates a pseudo address space in initial units space.
		@discussion Creates a pseudo address space object in initial units space and returns an interface to it. This
			will create a pseudo address space (software-backed) on the local machine.
			
			Availablilty: IOFireWireDeviceInterface_v3,  and newer
			
		@param self The device interface to use.
		@param inAddressLo The lower 32 bits of the base address of the address space to be created. The address is always
			in initial units space.
		@param inSize The size in bytes of this address space
		@param inRefCon A user specified reference value. This will be passed to all callback functions.
		@param inQueueBufferSize The size of the queue which receives packets from the bus before they are handed to
			the client and/or put in the backing store. A larger queue can help eliminate dropped packets
			when receiving large bursts of data. When a packet is received which can not fit into the queue, 
			the packet dropped callback will be called. 
		@param inBackingStore An optional block of allocated memory representing the contents of the address space.
		@param inFlags A UInt32 with bits set corresponding to the flags that should be set
			for this address space.
			<ul>
				<li>kFWAddressSpaceNoFlags -- All flags off</li>
				<li>kFWAddressSpaceNoWriteAccess -- Write access to this address space will be disallowed. 
					Setting this flag also disables compare/swap transactions on this address space.</li>
				<li>kFWAddressSpaceNoReadAccess -- Read access access to this address space will be disallowed. 
					Setting this flag also disables compare/swap transactions on this address space.</li>
				<li>kFWAddressSpaceAutoWriteReply -- Writes will be made automatically, directly modifying the contents
					of the backing store. The user process will not be notified of writes.</li>
				<li>kFWAddressSpaceAutoReadReply -- Reads to this address space will be answered automagically
					using the contents of the backing store. The user process will not be notified of reads.</li>
				<li>kFWAddressSpaceAutoCopyOnWrite -- Writes to this address space will be made directly
					to the backing store at the same time the user process is notified of a write. Clients
					will only be notified of a write if kFWAddressSpaceAutoWriteReply is not set.</li>
				<li>kFWAddressSpaceShareIfExists -- Allows creation of this address space even if another client
					already has an address space at the requested address. All clients will be notified of writes to
					covered addresses.</li>
			</ul>
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created pseudo address space object.
		@result An IOFireWireLibPseudoAddressSpaceRef. Returns 0 upon failure */
	IOFireWireLibPseudoAddressSpaceRef	(*CreateInitialUnitsPseudoAddressSpace)( 
												IOFireWireLibDeviceRef  	self,
												UInt32						inAddressLo,
												UInt32  					inSize, 
												void*  						inRefCon, 
												UInt32  					inQueueBufferSize, 
												void*  						inBackingStore, 
												UInt32  					inFlags,
												REFIID  					iid) ;
	/*!
		@function AddCallbackDispatcherToRunLoopForMode
		@abstract Add a run loop event source to allow IOFireWireLib callbacks to function.
        @discussion Installs the proper run loop event source to allow callbacks to function. This method
			must be called before callback notifications for this interface or any interfaces
			created using this interface can function. With this function, you can additionally specify
			for which run loop modes this source should be added.
			
			Availability: IOFireWireDeviceInterface_v3, and newer
			
		@param self The device interface to use.
		@param inRunLoop The run loop on which to install the event source
		@param inRunLoopMode The run loop mode(s) for which to install the event source
		@result An IOReturn error code.	*/	
	IOReturn 			(*AddCallbackDispatcherToRunLoopForMode)(
								IOFireWireLibDeviceRef 	self, 
								CFRunLoopRef 			inRunLoop,
								CFStringRef				inRunLoopMode ) ;
	/*!	@function AddIsochCallbackDispatcherToRunLoop
		@abstract Add a run loop event source to allow IOFireWireLib isoch callbacks to function.
		@discussion This function adds an event source for the isochronous callback dispatcher
			to the specified CFRunLoop. Isochronous related callbacks will not be called unless
			this function has been called. This function is similar to AddCallbackDispatcherToRunLoop. 
			The passed CFRunLoop can be different from that passed to AddCallbackDispatcherToRunLoop. 

			Availability: IOFireWireDeviceInterface_v3, and newer

		@param self The device interface to use.
		@param inRunLoop A CFRunLoopRef for the run loop to which the event loop source 
			should be added
		@param inRunLoopMode The run loop mode(s) for which to install the event source
		@result An IOReturn error code.	*/	
	IOReturn			(*AddIsochCallbackDispatcherToRunLoopForMode)(
								IOFireWireLibDeviceRef 	self,
								CFRunLoopRef			inRunLoop,
								CFStringRef 			inRunLoopMode ) ;
	/*!
		@function RemoveIsochCallbackDispatcherFromRunLoop
		@abstract Removes an IOFireWireLib-added run loop event source.
		@discussion Reverses the effects of AddIsochCallbackDispatcherToRunLoop(). This method removes 
			the run loop event source that was added to the specified run loop preventing any 
			future callbacks from being called.
			
			Availability: IOFireWireDeviceInterface_v3, and newer

        @param self The device interface to use.			
	*/
	void				(*RemoveIsochCallbackDispatcherFromRunLoop)(
								IOFireWireLibDeviceRef 	self) ;

	/*!
		@function Seize
		@abstract Seize control of device/unit
		@discussion Allows a user space client to seize control of an in-kernel service even if
			that service has been Opened() by another client or in-kernel driver. This function should be
			used with care. Admin rights are required to use this function.
			
			Calling this method makes it appear to all other drivers that the device has been unplugged.
			Open() should be called after this method has been invoked.
			
			When access is complete, Close() and then IOServiceRequestProbe() should be called to restore 
			normal operation. Calling IOServiceRequestProbe() makes it appear that the device has been "re-plugged."
        @param self The device interface to use.
		@param reserved Reserved for future use. Set to NULL.
	*/
	IOReturn			(*Seize)(
								IOFireWireLibDeviceRef 	self,
								IOOptionBits			inFlags,
								... ) ;

	/*!
		@function FireLog
		@abstract Logs string to in-kernel debug buffer
        @param self The device interface to use.
	*/
	IOReturn			(*FireLog)(
								IOFireWireLibDeviceRef 	self,
								const char*				format,
								... ) ;

	/*!	@function GetBusCycleTime
		@abstract Get bus and cycle time.
		@param self The device interface to use.
		@param outBusTime A pointer to a UInt32 to hold the bus time
		@param outCycleTime A pointer to a UInt32 to hold the cycle time
		@result An IOReturn error code.	*/	
	IOReturn (*GetBusCycleTime)( IOFireWireLibDeviceRef  self, UInt32*  outBusTime, UInt32*  outCycleTime) ;

	//
	// v4
	//

	/*!
		@function CreateCompareSwapCommand64
		@abstract Create a quadlet compare/swap command object and initialize it with 64-bit values.
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported. Setting the callback value to nil defaults to synchronous execution.
		@param addr Command target address
		@param cmpVal 64-bit value expected at target address
		@param newVal 64-bit value to be set at target address
		@param callback Command completion callback.
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOFireWireLibCommandRef interface. See IOFireWireLibCommandRef.	*/
	IOFireWireLibCommandRef (*CreateCompareSwapCommand64)( IOFireWireLibDeviceRef self, io_object_t device, const FWAddress* addr, 
																UInt64 cmpVal, UInt64 newVal, IOFireWireLibCommandCallback callback, 
																Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid) ;

	/*!
		@function CompareSwap64
		@abstract Perform synchronous lock operation
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
			
			If the quadlets stored at 'oldVal' match those passed to 'expectedVal', the lock operation was
			successful.
		@param addr Command target address
		@param expectedVal Pointer to quadlets expected at target.
		@param newVal Pointer to quadlets to atomically set at target if compare is successful.
		@param oldVal Pointer to quadlets to hold value found at target address after transaction if completed.
		@param size Size in bytes of compare swap transaction to perform. Value values are 4 and 8.
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOReturn error code
	*/
	IOReturn (*CompareSwap64)( IOFireWireLibDeviceRef self, io_object_t device, const FWAddress* addr, 
										UInt32* expectedVal, UInt32* newVal, UInt32* oldVal, IOByteCount size, 
										Boolean failOnReset, UInt32 generation) ;

	/*!	@function GetBusGeneration
		@abstract Get bus generation number.
		@discussion The bus generation number stays constant between bus resets and can be
			used in combination with a FireWire node ID to uniquely identify nodes on the bus.
			Pass the generation number to functions that take or return FireWire node IDs.
		
			Availability: IOFireWireDeviceInterface_v4 and newer
			
		@param self The device interface to use.
		@param outGeneration A pointer to a UInt32 to hold the bus generation  number
		@result Returns kIOReturnSuccess if a valid bus generation has been returned in 'outGeneration'.*/	
	IOReturn (*GetBusGeneration)( IOFireWireLibDeviceRef self, UInt32* outGeneration ) ;

	/*!	@function GetLocalNodeIDWithGeneration
		@abstract Get node ID of local machine.
		@discussion Use this function instead of GetLocalNodeID().
		
			Availability: IOFireWireDeviceInterface_v4 and newer
			
		@param self The device interface to use.
		@param checkGeneration A bus generation number obtained from GetBusGeneration()
		@param outLocalNodeID A pointer to a UInt16 to hold the node ID of the local machine.
		@result Returns kIOReturnSuccess if a valid nodeID has been returned in 'outLocalNodeID'. Returns
			kIOFireWireBusReset if 'checkGeneration' does not match the current bus generation number.*/
	IOReturn (*GetLocalNodeIDWithGeneration)( IOFireWireLibDeviceRef self, UInt32 checkGeneration, UInt16* outLocalNodeID ) ;

	/*!	@function GetRemoteNodeID
		@abstract Get node ID of device to which this interface is attached.
		@discussion
		
			Availability: IOFireWireDeviceInterface_v4 and newer
			
		@param self The device interface to use.
		@param checkGeneration A bus generation number obtained from GetBusGeneration()
		@param outRemoteNodeID A pointer to a UInt16 to hold the node ID of the remote device.
		@result Returns kIOReturnSuccess if a valid nodeID has been returned in 'outRemoteNodeID'. Returns
			kIOFireWireBusReset if 'checkGeneration' does not match the current bus generation number.*/
	IOReturn (*GetRemoteNodeID)( IOFireWireLibDeviceRef self, UInt32 checkGeneration, UInt16* outRemoteNodeID ) ;

	/*!	@function GetSpeedToNode
		@abstract Get maximum transfer speed to device to which this interface is attached.
		@discussion
		
			Availability: IOFireWireDeviceInterface_v4 and newer
			
		@param self The device interface to use.
		@param checkGeneration A bus generation number obtained from GetBusGeneration()
		@param outSpeed A pointer to an IOFWSpeed to hold the maximum speed to the remote device.
		@result Returns kIOReturnSuccess if a valid speed has been returned in 'outSpeed'. Returns
			kIOFireWireBusReset if 'checkGeneration' does not match the current bus generation number.*/
	IOReturn (*GetSpeedToNode)( IOFireWireLibDeviceRef self, UInt32 checkGeneration, IOFWSpeed* outSpeed) ;

	/*!	@function GetSpeedBetweenNodes
		@abstract Get maximum transfer speed to device to which this interface is attached.
		@discussion
		
			Availability: IOFireWireDeviceInterface_v4 and newer
			
		@param self The device interface to use.
		@param checkGeneration A bus generation number obtained from GetBusGeneration()
		@param srcNodeID A FireWire node ID.
		@param destNodeID A FireWire node ID.
		@param outSpeed A pointer to an IOFWSpeed to hold the maximum transfer speed between node 'srcNodeID' and 'destNodeID'.
		@result Returns kIOReturnSuccess if a valid speed has been returned in 'outSpeed'. Returns
			kIOFireWireBusReset if 'checkGeneration' does not match the current bus generation number.*/
	IOReturn (*GetSpeedBetweenNodes)( IOFireWireLibDeviceRef self, UInt32 checkGeneration, UInt16 srcNodeID, UInt16 destNodeID,  IOFWSpeed* outSpeed) ;

} IOFireWireDeviceInterface, IOFireWireUnitInterface, IOFireWireNubInterface ;

// ============================================================
//
// IOFireWirePseudoAddressSpaceInterface
//
// ============================================================

/*!	@interface IOFireWirePseudoAddressSpace (IOFireWireLib)
	@discussion Represents and provides management functions for a pseudo address 
		space (software-backed) in the local machine.

		Pseudo address space objects can be created using IOFireWireDeviceInterface.*/
typedef struct IOFireWirePseudoAddressSpaceInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;

	/*!	@function SetWriteHandler
		@abstract Set the callback that should be called to handle write accesses to
			the corresponding address space
		@param self The address space interface to use.
		@param inWriter The callback to set.
		@result Returns the callback that was previously set or nil for none.*/
	const IOFireWirePseudoAddressSpaceWriteHandler (*SetWriteHandler)( IOFireWireLibPseudoAddressSpaceRef self, IOFireWirePseudoAddressSpaceWriteHandler	inWriter) ;

	/*!	@function SetReadHandler
		@abstract Set the callback that should be called to handle read accesses to
			the corresponding address space
		@param self The address space interface to use.
		@param inReader The callback to set.
		@result Returns the callback that was previously set or nil for none.*/
	const IOFireWirePseudoAddressSpaceReadHandler (*SetReadHandler)( IOFireWireLibPseudoAddressSpaceRef self, IOFireWirePseudoAddressSpaceReadHandler		inReader) ;

	/*!	@function SetSkippedPacketHandler
		@abstract Set the callback that should be called when incoming packets are
			dropped by the address space.
		@param self The address space interface to use.
		@param inHandler The callback to set.
		@result Returns the callback that was previously set or nil for none.*/
	const IOFireWirePseudoAddressSpaceSkippedPacketHandler (*SetSkippedPacketHandler)( IOFireWireLibPseudoAddressSpaceRef self, IOFireWirePseudoAddressSpaceSkippedPacketHandler inHandler) ;

	/*!	@function NotificationIsOn
		@abstract Is notification on?
		@param self The address space interface to use.
		@result Returns true if packet notifications for this address space are active */
	Boolean (*NotificationIsOn)(IOFireWireLibPseudoAddressSpaceRef self) ;

	/*!	@function TurnOnNotification
		@abstract Try to turn on packet notifications for this address space.
		@param self The address space interface to use.
		@result Returns true upon success */
	Boolean (*TurnOnNotification)(IOFireWireLibPseudoAddressSpaceRef self) ;

	/*!	@function TurnOffNotification
		@abstract Force packet notification off.
		@param self The pseudo address interface to use. */
	void (*TurnOffNotification)(IOFireWireLibPseudoAddressSpaceRef self) ;	

	/*!	@function ClientCommandIsComplete
		@abstract Notify the address space that a packet notification handler has completed.
		@discussion Packet notifications are received one at a time, in order. This function
			must be called after a packet handler has completed its work.
		@param self The address space interface to use.
		@param commandID The ID of the packet notification being completed. This is the same
			ID that was passed when a packet notification handler is called.
		@param status The completion status of the packet handler */
	void		(*ClientCommandIsComplete)(IOFireWireLibPseudoAddressSpaceRef self, FWClientCommandID commandID, IOReturn status) ;

		// --- accessors ----------
	/*!	@function GetFWAddress
		@abstract Get the FireWire address of this address space
		@param self The pseudo address interface to use. */
	void (*GetFWAddress)(IOFireWireLibPseudoAddressSpaceRef self, FWAddress* outAddr) ;

	/*!	@function GetBuffer
		@abstract Get a pointer to the backing store for this address space
		@param self The address space interface to use.
		@result A pointer to the backing store of this pseudo address space. Returns
			nil if none. */
	void* (*GetBuffer)(IOFireWireLibPseudoAddressSpaceRef self) ;

	/*!	@function GetBufferSize
		@abstract Get the size in bytes of this address space.
		@param self The address space interface to use.
		@result Size of the pseudo address space in bytes. Returns 0 for none.*/
	const UInt32 (*GetBufferSize)(IOFireWireLibPseudoAddressSpaceRef self) ;

	/*!	@function GetRefCon
		@abstract Returns the user refCon value for this address space.
		@param self The address space interface to use.
		@result Size of the pseudo address space in bytes. Returns 0 for none.*/
	void* (*GetRefCon)(IOFireWireLibPseudoAddressSpaceRef self) ;


} IOFireWirePseudoAddressSpaceInterface ;

// ============================================================
//
// IOFireWireLocalUnitDirectoryInterface
//
// ============================================================

/*!	@interface IOFireWireLocalUnitDirectoryInterface (IOFireWireLib)
	@discussion Allows creation and management of unit directories in the config
		ROM of the local machine. After the unit directory has been built, 
		Publish() should be called to cause it to appear in the config ROM.
		Unpublish() has the reverse effect as Publish().

		This interface can be created using IOFireWireDeviceInterface::CreateLocalUnitDirectory. */
typedef struct IOFireWireLocalUnitDirectoryInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;

	// --- adding to ROM -------------------
	/*!	@function AddEntry_Ptr
		@abstract Append a data leaf
		@discussion Appends a leaf data node to a unit directory
		@param self The local unit directory interface to use.
		@param key The config ROM key for the data to be added.
		@param inBuffer A pointer to the data to be placed in the added leaf.
		@param inLen Length of the data being added.
		@param inDesc Reserved; set to NULL.  */
	IOReturn			(*AddEntry_Ptr)(IOFireWireLibLocalUnitDirectoryRef self, int key, void* inBuffer, size_t inLen, CFStringRef inDesc) ;

	/*!	@function AddEntry_UInt32
		@abstract Append an immediate leaf
		@discussion Appends an immediate leaf to a unit directory. Note that only the lower 3 bytes
			of the passed in value can appear in the unit directory.
		@param self The local unit directory interface to use.
		@param key The config ROM key for the data to be added.
		@param value The value to be added.
		@param inDesc Reserved; set to NULL.  */
	IOReturn			(*AddEntry_UInt32)(IOFireWireLibLocalUnitDirectoryRef self, int key, UInt32 value, CFStringRef inDesc) ;

	/*!	@function AddEntry_FWAddress
		@abstract Append an offset leaf
		@discussion Appends an offset leaf to a unit directory. The address passed in value should be an
			address in initial unit space of the local config ROM.
		@param self The local unit directory interface to use.
		@param key The config ROM key for the data to be added.
		@param inBuffer A pointer to a FireWire address.
		@param inDesc Reserved; set to NULL.  */
	IOReturn			(*AddEntry_FWAddress)(IOFireWireLibLocalUnitDirectoryRef self, int key, const FWAddress* value, CFStringRef inDesc) ;

	// Use this function to cause your unit directory to appear in the Mac's config ROM.
	/*!	@function Publish
		@abstract Causes a constructed or updated unit directory to appear in the local machine's
			config ROM. Note that this call will cause a bus reset, after which the unit directory will
			be visible to devices on the bus.
		@param self The local unit directory interface to use. */
	IOReturn			(*Publish)(IOFireWireLibLocalUnitDirectoryRef self) ;

	/*!	@function Unpublish
		@abstract Has the opposite effect from Publish(). This call removes a unit directory from the 
			local machine's config ROM. Note that this call will cause a bus reset, after which the unit directory will
			no longer appear to devices on the bus.
		@param self The local unit directory interface to use. */
	IOReturn			(*Unpublish)(IOFireWireLibLocalUnitDirectoryRef self) ;
} IOFireWireLocalUnitDirectoryInterface ;

// ============================================================
//
// IOFireWireLibPhysicalAddressSpaceInterface
//
// ============================================================

/*!	@interface IOFireWirePhysicalAddressSpace
	@abstract IOFireWireLib physical address space object. ( interface name: IOFireWirePhysicalAddressSpaceInterface )
	@discussion Represents and provides management functions for a physical address 
		space (hardware-backed) in the local machine.<br>
		Physical address space objects can be created using IOFireWireDeviceInterface.*/
typedef struct IOFireWirePhysicalAddressSpaceInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	/*!	@function GetPhysicalSegments
		@abstract Returns the list of physical memory ranges this address space occupies
			on the local machine.
		@param self The address space interface to use.
		@param ioSegmentCount Pass in a pointer to the number of list entries in 
			outSegments and outAddress. Upon completion, this will contain the actual
			number of segments returned in outSegments and outAddress
		@param outSegments A pointer to an array to hold the function results. Upon
			completion, this will contain the lengths of the physical segments this
			address space occupies on the local machine
		@param outAddress A pointer to an array to hold the function results. Upon
			completion, this will contain the addresses of the physical segments this
			address space occupies on the local machine. */
	void				(*GetPhysicalSegments)(
								IOFireWireLibPhysicalAddressSpaceRef self,
								UInt32*				ioSegmentCount,
								IOByteCount			outSegments[],
								IOPhysicalAddress	outAddresses[]) ;						
	/*!	@function GetPhysicalSegment
		@abstract Returns the physical segment containing the address at a specified offset
			from the beginning of this address space
		@param self The address space interface to use.
		@param offset Offset from beginning of address space
		@param length Pointer to a value which upon completion will contain the length of
			the segment returned by the function.
		@result The address of the physical segment containing the address at the specified
			offset of the address space	*/
	IOPhysicalAddress	(*GetPhysicalSegment)(
								IOFireWireLibPhysicalAddressSpaceRef self,
								IOByteCount 		offset,
								IOByteCount*		length) ;

	/*!	@function GetPhysicalAddress
		@abstract Returns the physical address of the beginning of this address space
		@param self The address space interface to use.
		@result The physical address of the start of this address space	*/
	IOPhysicalAddress	(*GetPhysicalAddress)(
								IOFireWireLibPhysicalAddressSpaceRef self) ;

		// --- accessors ----------
	/*!	@function GetFWAddress
		@abstract Get the FireWire address of this address space
		@param self The address space interface to use.	*/
	void				(*GetFWAddress)(
								IOFireWireLibPhysicalAddressSpaceRef self, 
								FWAddress* outAddr) ;

	/*!	@function GetBuffer
		@abstract Get a pointer to the backing store for this address space
		@param self The address space interface to use.
		@result A pointer to the backing store of this address space.*/
	void*				(*GetBuffer)(
								IOFireWireLibPhysicalAddressSpaceRef self) ;

	/*!	@function GetBufferSize
		@abstract Get the size in bytes of this address space.
		@param self The address space interface to use.
		@result Size of the pseudo address space in bytes.	*/
	const UInt32		(*GetBufferSize)(
								IOFireWireLibPhysicalAddressSpaceRef self) ;

} IOFireWirePhysicalAddressSpaceInterface ;

// ============================================================
//
// IOFireWireLibPhysicalAddressSpaceInterface
//
// ============================================================

//
// IOFIREWIRELIBCOMMAND_C_GUTS
// Macro used to insert generic superclass function definitions into all subclass of
// IOFireWireCommand. Comments for functions contained in this macro follow below:
//
#define IOFIREWIRELIBCOMMAND_C_GUTS \
	IOReturn			(*GetStatus)(IOFireWireLibCommandRef	self) ;	\
	UInt32				(*GetTransferredBytes)(IOFireWireLibCommandRef self) ; \
	void				(*GetTargetAddress)(IOFireWireLibCommandRef self, FWAddress* outAddr) ; \
	void				(*SetTarget)(IOFireWireLibCommandRef self, const FWAddress* addr) ;	\
	void				(*SetGeneration)(IOFireWireLibCommandRef self, UInt32 generation) ;	\
	void				(*SetCallback)(IOFireWireLibCommandRef self, IOFireWireLibCommandCallback inCallback) ;	\
	void				(*SetRefCon)(IOFireWireLibCommandRef self, void* refCon) ;	\
	const Boolean		(*IsExecuting)(IOFireWireLibCommandRef self) ;	\
	IOReturn			(*Submit)(IOFireWireLibCommandRef self) ;	\
	IOReturn			(*SubmitWithRefconAndCallback)(IOFireWireLibCommandRef self, void* refCon, IOFireWireLibCommandCallback inCallback) ;\
	IOReturn			(*Cancel)(IOFireWireLibCommandRef self, IOReturn reason)

#define IOFIREWIRELIBCOMMAND_C_GUTS_v2	\
	void 				(*SetBuffer)(IOFireWireLibCommandRef self, UInt32 size, void* buf) ;	\
	void 				(*GetBuffer)(IOFireWireLibCommandRef self, UInt32* outSize, void** outBuf) ;	\
	IOReturn			(*SetMaxPacket)(IOFireWireLibCommandRef self, IOByteCount maxPacketSize) ;	\
	void				(*SetFlags)(IOFireWireLibCommandRef self, UInt32 inFlags)

/*!	@interface IOFireWireCommandInterface
	@abstract IOFireWireLib command object.
	@discussion Represents an object that is configured and submitted to issue synchronous
		and asynchronous bus commands. This is a superclass containing all command object
		functionality not specific to any kind of bus transaction.
		
		Note that data may not always be transferred to or from the data buffer
		for command objects at the time the command is submitted. In some cases the
		transfer may happen as soon as SetBuffer() (below, v2 interfaces and newer) 
		is called. You can use the SetFlags() call (below, v2 interfaces and newer) to 
		control this behavior.

*/
typedef struct IOFireWireCommandInterface_t
{
/*
 public:
*/
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	
	/*!	@function GetStatus
		@abstract Return command completion status.
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
		</table>
					
		@param self The command object interface of interest
		@result An IOReturn error code indicating the completion error (if any) returned the last
			time this command object was executed	*/
		/*
		IOReturn (*GetStatus)(IOFireWireLibCommandRef	self) ;
		*/

	/*!	@function GetTransferredBytes
		@abstract Return number of bytes transferred by this command object when it last completed
			execution.
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
		</table>
		@param self The command object interface of interest
		@result A UInt32 containing the bytes transferred value	*/
		/*
		UInt32				(*GetTransferredBytes)(IOFireWireLibCommandRef self) ;
		*/
		
	/*!	@function GetTargetAddress
		@abstract Get command target address.
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
		</table>
		@param self The command object interface of interest
		@param outAddr A pointer to an FWAddress to contain the function result. */
		/*
		void				(*GetTargetAddress)(IOFireWireLibCommandRef self, FWAddress* outAddr) ;
		*/
	
	/*!	@function SetTarget
		@abstract Set command target address
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
		</table>
		@param self The command object interface of interest
		@param addr A pointer to an FWAddress. */
		/*
		void				(*SetTarget)(IOFireWireLibCommandRef self, const FWAddress* addr) ;
		*/

	/*!	@function SetGeneration
		@abstract Set FireWire bus generation for which the command object shall be valid.
			If the failOnReset attribute has been set, the command will only be considered for
			execution during the bus generation specified by this function.
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
		</table>
		@param self The command object interface of interest
		@param generation A bus generation. The current bus generation can be obtained
			from IOFireWireDeviceInterface::GetGenerationAndNodeID().	*/
		/*
		void				(*SetGeneration)(IOFireWireLibCommandRef self, UInt32 generation) ;
		*/
		
	/*!	@function SetCallback
		@abstract Set the completion handler to be called once the command completes
			asynchronous execution .
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
		</table>
		@param self The command object interface of interest
		@param inCallback A callback handler. Passing nil forces the command object to 
			execute synchronously. */
		/*
		void				(*SetCallback)(IOFireWireLibCommandRef self, IOFireWireLibCommandCallback inCallback) ;
		*/
		
	/*!	@function SetRefCon
		@abstract Set the user refCon value. This is the user defined value that will be passed
			in the refCon argument to the completion function.
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
		</table> */
		/*
		void				(*SetRefCon)(IOFireWireLibCommandRef self, void* refCon) ;
		*/
		
	/*!	@function IsExecuting
		@abstract Is this command object currently executing?
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
		</table>
		@param self The command object interface of interest
		@result Returns true if the command object is executing.	*/
		/*
		const Boolean		(*IsExecuting)(IOFireWireLibCommandRef self) ;
		*/
		
	/*!	@function Submit
		@abstract Submit this command object to FireWire for execution.
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
		</table>
		@param self The command object interface of interest
		@result An IOReturn result code indicating whether or not the command was successfully
			submitted */
		/*
		IOReturn			(*Submit)(IOFireWireLibCommandRef self) ;
		*/
		
	/*!	@function SubmitWithRefconAndCallback
		@abstract Set the command refCon value and callback handler, and submit the command
			to FireWire for execution.
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
		</table>
		@param self The command object interface of interest
		@result An IOReturn result code indicating whether or not the command was successfully
			submitted	*/
		/*
		IOReturn			(*SubmitWithRefconAndCallback)(IOFireWireLibCommandRef self, void* refCon, IOFireWireLibCommandCallback inCallback) ;
		*/
		
	/*!	@function Cancel
		@abstract Cancel command execution
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>YES</td>
			</tr>
		</table>
		@param self The command object interface of interest
		@result An IOReturn result code	*/
		/*
		IOReturn			(*Cancel)(IOFireWireLibCommandRef self, IOReturn reason) ;
		*/
		
	IOFIREWIRELIBCOMMAND_C_GUTS ;

	// version 2 interfaces: (appear in IOFireWireReadCommandInterface_v2 and IOFireWireWriteCommandInterface_v2)
	/*!	@function SetBuffer
		@abstract Set the buffer where read data should be stored.
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
		</table>
		@param self The command object interface of interest
		@param size Size in bytes of the receive buffer.
		@param buf A pointer to the receive buffer. */
		/*
		void 				(*SetBuffer)(IOFireWireLibCommandRef self, UInt32 size, void* buf) ;
		*/
		
	/*!	@function GetBuffer
		@abstract Set the command refCon value and callback handler, and submit the command
			to FireWire for execution.
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
		</table>
		@param self The command object interface of interest */
		/*
		void 				(*GetBuffer)(IOFireWireLibCommandRef self, UInt32* outSize, void** outBuf) ;
		*/
		
	/*!	@function SetMaxPacket
		@abstract Set the maximum size in bytes of packets transferred by this command.
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
		</table>
		@param self The command object interface of interest
		@param maxPacketSize Size in bytes of largest packet that should be transferred
			by this command.
		@result An IOReturn result code indicating whether or not the command was successfully
			submitted	*/
		/*
		IOReturn			(*SetMaxPacket)(IOFireWireLibCommandRef self, IOByteCount maxPacketSize) ;
		*/
		
	/*!	@function SetFlags
		@abstract Set flags governing this command's execution.
		@discussion Availability: (for interfaces obtained with ID)
		<table border="0" rules="all">
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteCommandInterfaceID_v2</code></td>
				<td>YES</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireReadQuadletCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireWriteQuadletCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
			<tr>
				<td width="20%"></td>
				<td><code>kIOFireWireCompareSwapCommandInterfaceID</code></td>
				<td>NO</td>
			</tr>
		</table>
		@param self The command object interface of interest
		@param inFlags A UInt32 with bits set corresponding to the flags that should be set
			for this command object. The following values may be used:<br>
			<ul>
				<li>kFWCommandNoFlags -- all flags off</li>
				<li>kFWCommandInterfaceForceNoCopy -- data sent by this command should always be
					received/sent directly from the buffer set with SetBuffer(). Whatever data
					is in the buffer when the command is submitted will be used.</li>
				<li>kFWCommandInterfaceForceCopyAlways -- data will always be copied out of the
					command object data buffer when SetBuffer() is called, up to a maximum
					allowed size (kFWUserCommandSubmitWithCopyMaxBufferBytes). This can result
					in faster data transfer. Changes made to the data buffer contents after
					calling SetBuffer() will be ignored; SetBuffer() should be called whenever 
					the data buffer contents change.</li>
				<li>kFWCommandInterfaceSyncExecute -- Setting this flag causes the command object
					to execute synchronously. The calling context will block until the command
					object has completed execution or an error occurs. Using synchronous execution
					can avoid kernel transitions associated with asynchronous completion and often
					remove the need for a state machine.</li>
			</ul>*/
		/*
		void				(*SetFlags)(IOFireWireLibCommandRef self, UInt32 inFlags) ;
		*/
	IOFIREWIRELIBCOMMAND_C_GUTS_v2 ;

} IOFireWireCommandInterface ;

/*!	@interface IOFireWireReadCommandInterface_t
	@abstract IOFireWireLib block read command object.
	@discussion Represents an object that is configured and submitted to issue synchronous
		and asynchronous block read commands.
		
		This interface contains all methods of IOFireWireCommandInterface.
		This interface will contain all v2 methods of IOFireWireCommandInterface
			when instantiated as v2 or newer. */
typedef struct IOFireWireReadCommandInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	
	IOFIREWIRELIBCOMMAND_C_GUTS ;

	// following functions
	// available only in IOFireWireReadCommandInterface interface v2 and newer
	IOFIREWIRELIBCOMMAND_C_GUTS_v2 ;
} IOFireWireReadCommandInterface ;

/*!	@interface IOFireWireWriteCommandInterface
	@abstract IOFireWireLib block read command object.
	@discussion Represents an object that is configured and submitted to issue synchronous
		and asynchronous block read commands.
		
		This interface contains all methods of IOFireWireCommandInterface.
		This interface will contain all v2 methods of IOFireWireCommandInterface
			when instantiated as v2 or newer. */
typedef struct IOFireWireWriteCommandInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	
	IOFIREWIRELIBCOMMAND_C_GUTS ;

	// following functions
	// available only in IOFireWireWriteCommandInterface_v2 and newer
	IOFIREWIRELIBCOMMAND_C_GUTS_v2 ;

} IOFireWireWriteCommandInterface ;


/*! @interface IOFireWireCompareSwapCommandInterface_t
*/
typedef struct IOFireWireCompareSwapCommandInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	
	IOFIREWIRELIBCOMMAND_C_GUTS ;

	/*!	@function SetValues
		@abstract Set values for 32-bit compare swap operation. Calling this function will
			make the command object perform 32-bit compare swap transactions on the bus. To perform
			64-bit compare swap operations, use the SetValues64() call, below.
		@discussion Available in v2 and newer.
		@param self The command object interface of interest
		@param cmpVal The value expected at the address targeted by this command object
		@param newVal The value to be written at the address targeted by this command object */
	void	(*SetValues)(IOFireWireLibCompareSwapCommandRef self, UInt32 cmpVal, UInt32 newVal) ;
	
	// --- v2 ---
	
	/*!	@function SetValues64
		@abstract Set values for 64-bit compare swap operation. Calling this function will
			make the command object perform 64-bit compare swap transactions on the bus. To perform
			32-bit compare swap operations, use the SetValues() call, above.
		@discussion Available in v2 and newer.
		@param self The command object interface of interest
		@param cmpVal The value expected at the address targeted by this command object
		@param newVal The value to be written at the address targeted by this command object */
	void	(*SetValues64)(IOFireWireLibCompareSwapCommandRef self, UInt64 cmpVal, UInt64 newVal) ;

	/*!	@function DidLock
		@abstract Was the last lock operation successful?
		@discussion Available in v2 and newer.
		@param self The command object interface of interest
		@result Returns true if the last lock operation performed by this command object was successful,
			false otherwise. */
	Boolean	(*DidLock)(IOFireWireLibCompareSwapCommandRef self) ;

	/*!	@function Locked
		@abstract Get the 32-bit value returned on the last compare swap operation.
		@discussion Available in v2 and newer.
		@param self The command object interface of interest
		@param oldValue A pointer to contain the value returned by the target of this command
			on the last compare swap operation
		@result Returns kIOReturnBadArgument if the last compare swap operation performed was 64-bit. */
	IOReturn (*Locked)(IOFireWireLibCompareSwapCommandRef self, UInt32* oldValue) ;

	/*!	@function Locked64
		@abstract Get the 64-bit value returned on the last compare swap operation.
		@discussion Available in v2 and newer.
		@param self The command object interface of interest
		@param oldValue A pointer to contain the value returned by the target of this command
			on the last compare swap operation
		@result Returns kIOReturnBadArgument if the last compare swap performed was 32-bit. */
	IOReturn (*Locked64)(IOFireWireLibCompareSwapCommandRef self, UInt64* oldValue) ;

	/*!	@function SetFlags
		@abstract Set flags governing this command's execution.
		@discussion Available in v2 and newer. Same as SetFlags() above.
		@param self The command object interface of interest.
		@param inFlags A UInt32 with bits set corresponding to the flags that should be set. */
	void (*SetFlags)(IOFireWireLibCompareSwapCommandRef self, UInt32 inFlags) ;
	
} IOFireWireCompareSwapCommandInterface ;

//
// obsolete: do not use. Use IOFireWireWriteCommandInterface_v2 and its function SetMaxPacket().
//
/*!	@interface IOFWRdQuadCmdInterface
	@abstract IOFireWireReadQuadletCommandInterface -- IOFireWireLib quadlet read command object.
	@discussion Obsolete; do not use. Use IOFireWireReadCommandInterface v2 or newer
		and its function SetMaxPacket() */
typedef struct IOFireWireReadQuadletCommandInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	
	IOFIREWIRELIBCOMMAND_C_GUTS ;

	/*!	@function SetQuads
		@abstract Set destination for read data
		@param self The command object interface of interest
		@param inQuads An array of quadlets
		@param inNumQuads Number of quadlet in 'inQuads'	*/
	void (*SetQuads)(IOFireWireLibReadQuadletCommandRef self, UInt32 inQuads[], UInt32 inNumQuads) ;
} IOFireWireReadQuadletCommandInterface ;

//
// obsolete: do not use. Use IOFireWireWriteCommandInterface_v2 and its function SetMaxPacket().
//
/*!	@interface IOFireWireWriteQuadletCommandInterface_t
	@abstract IOFireWireLib quadlet read command object.
	@discussion Obsolete; do not use. Use IOFireWireWriteCommandInterface v2 or newer
		and its function SetMaxPacket() */
typedef struct IOFireWireWriteQuadletCommandInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	
	IOFIREWIRELIBCOMMAND_C_GUTS ;

	/*! @function SetQuads */
	void (*SetQuads)(IOFireWireLibWriteQuadletCommandRef self, UInt32 inQuads[], UInt32 inNumQuads) ;
} IOFireWireWriteQuadletCommandInterface ;

// ============================================================
//
// IOFireWireConfigDirectoryInterface
//
// ============================================================

/*!	@interface IOFireWireConfigDirectoryInterface
	@abstract IOFireWireLib device config ROM browsing interface
	@discussion Represents an interface to the config ROM of a remote device. You can use the
		methods of this interface to browser the ROM and obtain key values. You can also
		create additional IOFireWireConfigDirectoryInterface's to represent subdirectories
		within the ROM.*/
typedef struct IOFireWireConfigDirectoryInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	
	/*!	@function Update
		@abstract Causes the ROM data to be updated through the specified byte offset. This
			function should not be called in normal usage.
		@param self The config directory interface of interest
		@param inOffset Offset in bytes indicating length of ROM to be updated.
		@result An IOReturn result code	*/
	IOReturn (*Update)							( IOFireWireLibConfigDirectoryRef self, UInt32 inOffset) ;
    IOReturn (*GetKeyType)						( IOFireWireLibConfigDirectoryRef self, int inKey, IOConfigKeyType* outType);
    IOReturn (*GetKeyValue_UInt32)				( IOFireWireLibConfigDirectoryRef self, int inKey, UInt32* outValue, CFStringRef* outText);
    IOReturn (*GetKeyValue_Data)				( IOFireWireLibConfigDirectoryRef self, int inKey, CFDataRef* outValue, CFStringRef* outText);
    IOReturn (*GetKeyValue_ConfigDirectory)		( IOFireWireLibConfigDirectoryRef self, int inKey, IOFireWireLibConfigDirectoryRef* outValue, REFIID iid, CFStringRef* outText);
    IOReturn (*GetKeyOffset_FWAddress)			( IOFireWireLibConfigDirectoryRef self, int inKey, FWAddress* outValue, CFStringRef* text);
    IOReturn (*GetIndexType)					( IOFireWireLibConfigDirectoryRef self, int inIndex, IOConfigKeyType* 	type);
    IOReturn (*GetIndexKey)						( IOFireWireLibConfigDirectoryRef self, int inIndex, int * key);
    IOReturn (*GetIndexValue_UInt32)			( IOFireWireLibConfigDirectoryRef self, int inIndex, UInt32 * value);
    IOReturn (*GetIndexValue_Data)				( IOFireWireLibConfigDirectoryRef self, int inIndex, CFDataRef * value);
    IOReturn (*GetIndexValue_String)			( IOFireWireLibConfigDirectoryRef self, int inIndex, CFStringRef* outValue);
    IOReturn (*GetIndexValue_ConfigDirectory)	( IOFireWireLibConfigDirectoryRef self, int inIndex, IOFireWireLibConfigDirectoryRef* outValue, REFIID iid);
    IOReturn (*GetIndexOffset_FWAddress)		( IOFireWireLibConfigDirectoryRef self, int inIndex, FWAddress* outValue);
    IOReturn (*GetIndexOffset_UInt32)			( IOFireWireLibConfigDirectoryRef self, int inIndex, UInt32* outValue);
    IOReturn (*GetIndexEntry)					( IOFireWireLibConfigDirectoryRef self, int inIndex, UInt32* outValue);
    IOReturn (*GetSubdirectories)				( IOFireWireLibConfigDirectoryRef self, io_iterator_t* outIterator);
    IOReturn (*GetKeySubdirectories)			( IOFireWireLibConfigDirectoryRef self, int inKey, io_iterator_t* outIterator);
	IOReturn (*GetType)							( IOFireWireLibConfigDirectoryRef self, int* outType) ;
	IOReturn (*GetNumEntries)					( IOFireWireLibConfigDirectoryRef self, int* outNumEntries) ;

} IOFireWireConfigDirectoryInterface ;

#endif //__IOFireWireLib_H__
