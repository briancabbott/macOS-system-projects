#ifndef __IrDAUser__
#define __IrDAUser__

#include <IOKit/IOUserClient.h>
#include <IOKit/IOLib.h>

class AppleIrDASerial;

class IrDAUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IrDAUserClient)

public:
    static IrDAUserClient *withTask(task_t owningTask);     // factory create
    virtual IOReturn clientClose(void);
    virtual IOReturn clientDied(void);

    virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type);   // not impl
    virtual IOReturn  connectClient(IOUserClient *client);
    virtual IOExternalMethod *getTargetAndMethodForIndex(IOService **target, UInt32 index);
    virtual bool start(IOService *provider);

    IOReturn userPostCommand(void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize);
    IOReturn getIrDALog(void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize);
    IOReturn getIrDAStatus(void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize);
    IOReturn setIrDAState(bool state);
    
private:
    static const IOExternalMethod sMethods[];
    AppleIrDASerial     *fDriver;
    task_t               fTask;
};

#endif  // __IrDAUser__