/*
 * Copyright � 1998-2012 Apple Inc. All rights reserved.
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

#ifndef _IOKIT_IOUSBInterfaceClass_H
#define _IOKIT_IOUSBInterfaceClass_H

#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USB.h>
#include <asl.h>

#include <AvailabilityMacros.h>


#include "IOUSBIUnknown.h"

class IOUSBInterfaceClass : public IOUSBIUnknown
{
private:
    // Disable copy constructors
    IOUSBInterfaceClass(IOUSBInterfaceClass &src);
    void operator =(IOUSBInterfaceClass &src);

protected:
    IOUSBInterfaceClass();
    virtual ~IOUSBInterfaceClass();

    static IOCFPlugInInterface			sIOCFPlugInInterfaceV1;
    static IOUSBInterfaceInterface550  	sUSBInterfaceInterfaceV550;

    struct InterfaceMap					fUSBInterface;
    io_service_t						fService;
    io_service_t						fDevice;
    io_connect_t						fConnection;
    IONotificationPortRef				fAsyncPort;
    CFRunLoopSourceRef					fCFSource;
    bool								fIsOpen;
    UInt8								fClass;
    UInt8								fSubClass;
    UInt8								fProtocol;
    UInt8								fConfigValue;
    UInt8								fInterfaceNumber;
    UInt8								fAlternateSetting;
    UInt8								fNumEndpoints;
    UInt8								fStringIndex;
    // these fields are actually in the device, but are repeated here because they are useful
    // and you don't need to access the device to get them.
    UInt16								fVendor;
    UInt16								fProduct;
    UInt16								fDeviceReleaseNumber;
    UInt32								fLocationID;
    UInt8								fNumConfigurations;
    // Support for low latency buffers
    UInt32								fNextCookie;
    LowLatencyUserBufferInfoV3			*fUserBufferInfoListHead;
    UInt32								fConfigLength;
    IOUSBInterfaceDescriptorPtr			fInterfaceDescriptor;
    IOUSBConfigurationDescriptorPtr		*fConfigurations;
    bool								fConfigDescCacheValid;
	UInt8								fCurrentConfigIndex;
	bool								fNeedContiguousMemoryForLowLatencyIsoch;
	bool								fNeedsToReleasefDevice;
	aslclient							fASLClient;
	bool								fInterfaceIsAttached;
    
public:
    static IOCFPlugInInterface			**alloc();

    virtual HRESULT						queryInterface(REFIID iid, void **ppv);

    virtual IOReturn					probe(CFDictionaryRef propertyTable, io_service_t service, SInt32 *order);
    virtual IOReturn					start(CFDictionaryRef propertyTable, io_service_t service);
	virtual IOReturn					stop(void);
    virtual IOReturn					CreateInterfaceAsyncEventSource(CFRunLoopSourceRef *source);
    virtual CFRunLoopSourceRef			GetInterfaceAsyncEventSource();
    virtual IOReturn					CreateInterfaceAsyncPort(mach_port_t *port);
    virtual mach_port_t					GetInterfaceAsyncPort();
    virtual IOReturn					USBInterfaceOpen(bool seize);
    virtual IOReturn					USBInterfaceClose();
    virtual IOReturn					GetInterfaceClass(UInt8 *intfClass);
    virtual IOReturn					GetInterfaceSubClass(UInt8 *intfSubClass);
    virtual IOReturn					GetInterfaceProtocol(UInt8 *intfProtocol);
    virtual IOReturn					GetDeviceVendor(UInt16 *devVendor);
    virtual IOReturn					GetDeviceProduct(UInt16 *devProduct);
    virtual IOReturn					GetDeviceReleaseNumber(UInt16 *devRelNum);
    virtual IOReturn					GetConfigurationValue(UInt8 *configVal);
    virtual IOReturn					GetInterfaceNumber(UInt8 *intfNumber);
    virtual IOReturn					GetAlternateSetting(UInt8 *intfAlternateSetting);
    virtual IOReturn					GetNumEndpoints(UInt8 *intfNumEndpoints);
    virtual IOReturn					GetLocationID(UInt32 *locationID);
    virtual IOReturn					GetDevice(io_service_t *device);
    virtual IOReturn					SetAlternateInterface(UInt8 alternateSetting);
    virtual IOReturn					GetBusFrameNumber(UInt64 *frame, AbsoluteTime *atTime);
    virtual IOReturn					GetBandwidthAvailable(UInt32 *bandwidth);
    virtual IOReturn					GetEndpointProperties(UInt8 alternateSetting, UInt8 endpointNumber, UInt8 direction, UInt8 *transferType, UInt16 *maxPacketSize, UInt8 *interval);
    virtual IOReturn					ControlRequest(UInt8 pipeRef, IOUSBDevRequestTO *req);
    virtual IOReturn					ControlRequestAsync(UInt8 pipeRef, IOUSBDevRequestTO *req, IOAsyncCallback1 callback, void *refCon);
    virtual IOReturn					GetPipeProperties(UInt8 pipeRef, UInt8 *direction, UInt8 *address, UInt8 *attributes, UInt16 *maxpacketSize, UInt8 *interval);
    virtual IOReturn					GetPipeStatus(UInt8 pipeRef);
    virtual IOReturn					AbortPipe(UInt8 pipeRef);
    virtual IOReturn					ResetPipe(UInt8 pipeRef);
    virtual IOReturn					ClearPipeStall(UInt8 pipeRef, bool bothEnds);
    virtual IOReturn					SetPipePolicy(UInt8 pipeRef, UInt16 maxPacketSize, UInt8 maxInterval);
    virtual IOReturn					ReadPipe(UInt8 pipeRef, void *buf, UInt32 *size, UInt32 noDataTimeout, UInt32 completionTimeout);
    virtual IOReturn					WritePipe(UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout);
    virtual IOReturn					ReadPipeAsync(UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon);
    virtual IOReturn					WritePipeAsync(UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon);
    virtual IOReturn					ReadIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon);
    virtual IOReturn					WriteIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon);
    // ----- new with 1.9.2
    virtual IOReturn					LowLatencyReadIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, UInt32 updateFrequency, IOUSBLowLatencyIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon);
    virtual IOReturn					LowLatencyWriteIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, UInt32 updateFrequency, IOUSBLowLatencyIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon);
    virtual IOReturn					LowLatencyCreateBuffer(void * *buffer, IOByteCount size, UInt32 bufferType);
    virtual IOReturn					LowLatencyDestroyBuffer(void * buffer);
    // ----- new with 1.9.7
    virtual IOReturn					GetBusMicroFrameNumber(UInt64 *microFrame, AbsoluteTime *atTime);
    virtual IOReturn					GetFrameListTime(UInt32 *microsecondsInFrame);
    virtual IOReturn					GetIOUSBLibVersion(NumVersion *ioUSBLibVersion, NumVersion *usbFamilyVersion);
    // ----- new with 2.2.0
    virtual IOUSBDescriptorHeader		*FindNextAssociatedDescriptor( const void * currentDescriptor, UInt8 descriptorType);
    virtual IOUSBDescriptorHeader		*FindNextAltInterface( const void * currentDescriptor, IOUSBFindInterfaceRequest *request);
    

    virtual void						AddDataBufferToList( LowLatencyUserBufferInfoV3 * insertBuffer );
    virtual bool						RemoveDataBufferFromList( LowLatencyUserBufferInfoV3 * removeBuffer );
    virtual LowLatencyUserBufferInfoV3	*FindBufferAddressInList( void * address );
    virtual LowLatencyUserBufferInfoV3	*FindBufferAddressRangeInList( void * address, UInt32 size );

    virtual IOReturn					GetInterfaceStringIndex(UInt8 *intfSI);
    virtual IOReturn					CacheConfigDescriptor();
    virtual const IOUSBDescriptorHeader *FindNextDescriptor(const void *cur, UInt8 descType);
    virtual IOUSBDescriptorHeader		*NextDescriptor(const void *desc);
    // ----- new with 3.0.0
    virtual IOReturn					GetBusFrameNumberWithTime(UInt64 *frame, AbsoluteTime *atTime);
    // ----- new with 5.0.0
	virtual IOReturn					GetPipePropertiesV2(UInt8 pipeRef, UInt8 *direction, UInt8 *address, UInt8 *attributes, UInt16 *maxpacketSize, UInt8 *interval, UInt8 *maxBurst, UInt8 *mult, UInt16 *bytesPerInterval);
    // ----- new with 5.5.0
	virtual IOReturn					GetPipePropertiesV3(UInt8 pipeRef, IOUSBEndpointProperties *properties);
    virtual IOReturn					GetEndpointPropertiesV3(IOUSBEndpointProperties *properties);
	virtual IOReturn 					SupportsStreams(UInt8 pipeRef, UInt32 *supportsStreams);
	virtual IOReturn					CreateStreams(UInt8 pipeRef, UInt32 streamID);
	virtual IOReturn					GetConfiguredStreams(UInt8 pipeRef, UInt32 *configuredStreams);
	virtual IOReturn					ReadStreamsPipeTO(UInt8 pipeRef, UInt32 streamID, void *buf, UInt32 *size, UInt32 noDataTimeout, UInt32 completionTimeout);
	virtual IOReturn					WriteStreamsPipeTO(UInt8 pipeRef, UInt32 streamID, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout);
	virtual IOReturn					ReadStreamsPipeAsyncTO(UInt8 pipeRef, UInt32 streamID, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon);
	virtual IOReturn					WriteStreamsPipeAsyncTO(UInt8 pipeRef, UInt32 streamID, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon);
	virtual IOReturn					AbortStreamsPipe(UInt8 pipeRef, UInt32 streamID);
private:
    IOReturn							GetPropertyInfo(void);

/*
 * Routing gumf for CFPlugIn interfaces
 */
protected:

    static inline IOUSBInterfaceClass *getThis(void *self)
        { return (IOUSBInterfaceClass *) ((InterfaceMap *) self)->obj; };

    // Methods for routing the iocfplugin Interface v1r1
    static IOReturn				interfaceProbe(void *self, CFDictionaryRef propertyTable, io_service_t service, SInt32 *order);
    static IOReturn				interfaceStart(void *self, CFDictionaryRef propertyTable, io_service_t service);
    static IOReturn				interfaceStop(void *self);	// Calls close()
    static IOReturn				interfaceCreateInterfaceAsyncEventSource(void *self, CFRunLoopSourceRef *source);
    static CFRunLoopSourceRef	interfaceGetInterfaceAsyncEventSource(void *self);
    static IOReturn				interfaceCreateInterfaceAsyncPort(void *self, mach_port_t *port);
    static mach_port_t			interfaceGetInterfaceAsyncPort(void *self);
    static IOReturn				interfaceUSBInterfaceOpen(void *self);
    static IOReturn				interfaceUSBInterfaceClose(void *self);
    static IOReturn				interfaceGetInterfaceClass(void *self, UInt8 *devClass);
    static IOReturn				interfaceGetInterfaceSubClass(void *self, UInt8 *devSubClass);
    static IOReturn				interfaceGetInterfaceProtocol(void *self, UInt8 *devProtocol);
    static IOReturn				interfaceGetDeviceVendor(void *self, UInt16 *devVendor);
    static IOReturn				interfaceGetDeviceProduct(void *self, UInt16 *devProduct);
    static IOReturn				interfaceGetDeviceReleaseNumber(void *self, UInt16 *devRelNum);
    static IOReturn				interfaceGetConfigurationValue(void *self, UInt8 *configVal);
    static IOReturn				interfaceGetInterfaceNumber(void *self, UInt8 *intfNumber);
    static IOReturn				interfaceGetAlternateSetting(void *self, UInt8 *intfAlternateSetting);
    static IOReturn				interfaceGetNumEndpoints(void *self, UInt8 *intfNumEndpoints);
    static IOReturn				interfaceGetLocationID(void *self, UInt32 *locationID);
    static IOReturn				interfaceGetDevice(void *self, io_service_t *device);
    static IOReturn				interfaceSetAlternateInterface(void *self, UInt8 alternateSetting);
    static IOReturn				interfaceGetBusFrameNumber(void *self, UInt64 *frame, AbsoluteTime *atTime);
    static IOReturn				interfaceControlRequest(void *self, UInt8 pipeRef, IOUSBDevRequest *req);
    static IOReturn				interfaceControlRequestAsync(void *self, UInt8 pipeRef, IOUSBDevRequest *req, IOAsyncCallback1 callback, void *refCon);
    static IOReturn				interfaceGetPipeProperties(void *self, UInt8 pipeRef, UInt8 *direction, UInt8 *address, UInt8 *attributes,  UInt16 *maxpacketSize, UInt8 *interval);
    static IOReturn				interfaceGetPipeStatus(void *self, UInt8 pipeRef);
    static IOReturn				interfaceAbortPipe(void *self, UInt8 pipeRef);
    static IOReturn				interfaceResetPipe(void *self, UInt8 pipeRef);
    static IOReturn				interfaceClearPipeStall(void *self, UInt8 pipeRef);
    static IOReturn				interfaceReadPipe(void *self, UInt8 pipeRef, void *buf, UInt32 *size);
    static IOReturn				interfaceWritePipe(void *self, UInt8 pipeRef, void *buf, UInt32 size);
    static IOReturn				interfaceReadPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt32 size, IOAsyncCallback1 callback, void *refcon);
    static IOReturn				interfaceWritePipeAsync(void *self, UInt8 pipeRef, void *buf, UInt32 size, IOAsyncCallback1 callback, void *refcon);
    static IOReturn				interfaceReadIsochPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon);
    static IOReturn				interfaceWriteIsochPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon);
    // ----------------- added in 1.8.2
    static IOReturn				interfaceControlRequestTO(void *self, UInt8 pipeRef, IOUSBDevRequestTO *req);
    static IOReturn				interfaceControlRequestAsyncTO(void *self, UInt8 pipeRef, IOUSBDevRequestTO *req, IOAsyncCallback1 callback, void *refCon);
    static IOReturn				interfaceReadPipeTO(void *self, UInt8 pipeRef, void *buf, UInt32 *size, UInt32 noDataTimeout, UInt32 completionTimeout);
    static IOReturn				interfaceWritePipeTO(void *self, UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout);
    static IOReturn				interfaceReadPipeAsyncTO(void *self, UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon);
    static IOReturn				interfaceWritePipeAsyncTO(void *self, UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon);
    static IOReturn				interfaceGetInterfaceStringIndex(void *self, UInt8 *intfSI);
    // ----------------- added in 1.8.3
    static IOReturn				interfaceUSBInterfaceOpenSeize(void *self);
    // ----------------- added in 1.9.0
    static IOReturn				interfaceClearPipeStallBothEnds(void *self, UInt8 pipeRef);
    static IOReturn				interfaceSetPipePolicy(void *self, UInt8 pipeRef, UInt16 maxPacketSize, UInt8 maxInterval);
    static IOReturn				interfaceGetBandwidthAvailable(void *self, UInt32 *bandwidth);
    static IOReturn				interfaceGetEndpointProperties(void *self, UInt8 alternateSetting, UInt8 endpointNumber, UInt8 direction, UInt8 *transferType, UInt16 *maxPacketSize, UInt8 *interval);
    // ----------------- added in 1.9.2
    static IOReturn				interfaceLowLatencyReadIsochPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, UInt32 updateFrequency, IOUSBLowLatencyIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon);
    static IOReturn				interfaceLowLatencyWriteIsochPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, UInt32 updateFrequency, IOUSBLowLatencyIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon);
    static IOReturn				interfaceLowLatencyCreateBuffer(void *self, void * *buffer, IOByteCount size, UInt32 bufferType);
    static IOReturn				interfaceLowLatencyDestroyBuffer(void *self, void * buffer );
    // ----------------- added in 1.9.7
    static IOReturn				interfaceGetBusMicroFrameNumber(void *self, UInt64 *microFrame, AbsoluteTime *atTime);
    static IOReturn				interfaceGetFrameListTime(void *self, UInt32 *microsecondsInFrame);
    static IOReturn				interfaceGetIOUSBLibVersion( void *self, NumVersion *ioUSBLibVersion, NumVersion *usbFamilyVersion);
    // ----------------- added in 2.2.0
    static IOUSBDescriptorHeader *interfaceFindNextAssociatedDescriptor( void *self, const void *currentDescriptor, UInt8 descriptorType);
    static IOUSBDescriptorHeader *interfaceFindNextAltInterface( void *self, const void *currentDescriptor, IOUSBFindInterfaceRequest *request);
    // ----------------- added in 3.0.0
    static IOReturn				interfaceGetBusFrameNumberWithTime(void *self, UInt64 *frame, AbsoluteTime *atTime);
	// ----------------- added in 5.0.0
    static IOReturn				interfaceGetPipePropertiesV2(void *self, UInt8 pipeRef, UInt8 *direction, UInt8 *address, UInt8 *attributes,  UInt16 *maxpacketSize, UInt8 *interval, UInt8 *maxBurst, UInt8 *mult, UInt16 *bytesPerInterval);
	// ----------------- added in 5.5.0
    static IOReturn				interfaceGetPipePropertiesV3(void *self, UInt8 pipeRef, IOUSBEndpointProperties *properties);
    static IOReturn				interfaceGetEndpointPropertiesV3(void *self, IOUSBEndpointProperties *properties);
	static IOReturn 			interfaceSupportsStreams(void *self, UInt8 pipeRef, UInt32 *supportsStreams);
	static IOReturn				interfaceCreateStreams(void *self, UInt8 pipeRef, UInt32 streamID);
	static IOReturn				interfaceGetConfiguredStreams(void *self, UInt8 pipeRef, UInt32 *configuredStreams);
	static IOReturn				interfaceReadStreamsPipeTO(void *self, UInt8 pipeRef, UInt32 streamID, void *buf, UInt32 *size, UInt32 noDataTimeout, UInt32 completionTimeout);
	static IOReturn				interfaceWriteStreamsPipeTO(void *self, UInt8 pipeRef, UInt32 streamID, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout);
	static IOReturn				interfaceReadStreamsPipeAsyncTO(void *self, UInt8 pipeRef, UInt32 streamID, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon);
	static IOReturn				interfaceWriteStreamsPipeAsyncTO(void *self, UInt8 pipeRef, UInt32 streamID, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon);
	static IOReturn				interfaceAbortStreamsPipe(void *self, UInt8 pipeRef, UInt32 streamID);
};

#endif /* !_IOKIT_IOUSBInterfaceClass_H */
