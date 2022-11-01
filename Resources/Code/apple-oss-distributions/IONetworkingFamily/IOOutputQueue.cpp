/*
 * Copyright (c) 1998-2008 Apple Inc. All rights reserved.
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
 * IOOutputQueue.cpp
 *
 * HISTORY
 * 2-Feb-1999       Joe Liu (jliu) created.
 *
 */

#include <IOKit/assert.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/network/IOOutputQueue.h>
#include <IOKit/network/IOBasicOutputQueue.h>
#include <IOKit/network/IOGatedOutputQueue.h>
#include <IOKit/network/IONetworkStats.h>
#include <IOKit/network/IONetworkController.h>
#include "IOMbufQueue.h"
#include <libkern/OSAtomic.h>

//===========================================================================
// IOOutputQueue
//===========================================================================

#define STATE_IS(bits)     (_state == (bits))
#define STATE_HAS(bits)    ((_state & (bits)) == (bits))
#define STATE_SET(bits)    (_state |= (bits))
#define STATE_CLR(bits)    (_state &= ~(bits))

#undef  super
#define super OSObject
OSDefineMetaClassAndAbstractStructors( IOOutputQueue, OSObject )
OSMetaClassDefineReservedUnused( IOOutputQueue,  1);
OSMetaClassDefineReservedUnused( IOOutputQueue,  2);
OSMetaClassDefineReservedUnused( IOOutputQueue,  3);
OSMetaClassDefineReservedUnused( IOOutputQueue,  4);
OSMetaClassDefineReservedUnused( IOOutputQueue,  5);
OSMetaClassDefineReservedUnused( IOOutputQueue,  6);
OSMetaClassDefineReservedUnused( IOOutputQueue,  7);
OSMetaClassDefineReservedUnused( IOOutputQueue,  8);
OSMetaClassDefineReservedUnused( IOOutputQueue,  9);
OSMetaClassDefineReservedUnused( IOOutputQueue, 10);
OSMetaClassDefineReservedUnused( IOOutputQueue, 11);
OSMetaClassDefineReservedUnused( IOOutputQueue, 12);
OSMetaClassDefineReservedUnused( IOOutputQueue, 13);
OSMetaClassDefineReservedUnused( IOOutputQueue, 14);
OSMetaClassDefineReservedUnused( IOOutputQueue, 15);

//---------------------------------------------------------------------------
// Initialize an IOOutputQueue object.

bool IOOutputQueue::init()
{
    if (super::init() == false)
        return false;

    // Allocate and initialize the callout entry for async service.

    _callEntry = thread_call_allocate((thread_call_func_t) &runServiceThread,
                                      (void *) this); /* param0 */
    if (_callEntry == 0)
        return false;

    return true;
}

//---------------------------------------------------------------------------
// Frees the IOOutputQueue object.

void IOOutputQueue::free()
{
    if (_callEntry)
    {
        cancelServiceThread();
        thread_call_free(_callEntry);
        _callEntry = 0;
    }

    super::free();
}

//---------------------------------------------------------------------------
// Schedule a service thread callout, which will run the
// serviceThread() method.

bool IOOutputQueue::scheduleServiceThread(void * param)
{
    return thread_call_enter1(_callEntry, (thread_call_param_t) param);
}

//---------------------------------------------------------------------------
// Cancel any pending service thread callout.

bool IOOutputQueue::cancelServiceThread()
{
    if (_callEntry == 0)
        return false;
    else
        return thread_call_cancel(_callEntry);
}

//---------------------------------------------------------------------------
// A 'C' glue function that is registered as the service thread callout
// handler. This function in turn will call the serviceThread() method.

void
IOOutputQueue::runServiceThread(thread_call_param_t param0,  /* this */
                                thread_call_param_t param1)  /* param */
{
    assert(param0);
    ((IOOutputQueue *) param0)->serviceThread(param1);
}

//---------------------------------------------------------------------------
// Must be implemented by a subclass that calls scheduleServiceThread().
// The default implementation is a placeholder and performs no action.

void IOOutputQueue::serviceThread(void * param)
{
}

//---------------------------------------------------------------------------
// Return an address of a method that is designated to handle
// packets sent to the queue object.

IOOutputAction IOOutputQueue::getOutputHandler() const
{
    return (IOOutputAction) &IOOutputQueue::enqueue;
}

//---------------------------------------------------------------------------
// Return an IONetworkData object containing statistics counters.

IONetworkData * IOOutputQueue::getStatisticsData() const
{
    return 0;
}

OSMetaClassDefineReservedUsed( IOOutputQueue,  0);
//---------------------------------------------------------------------------
// Retrieve a packet's priority, to be overridden by subclasses

UInt32 IOOutputQueue::getMbufPriority(mbuf_t m)
{
    return 0;
}

//===========================================================================
// IOBasicOutputQueue
//===========================================================================

#undef  super
#define super IOOutputQueue
OSDefineMetaClassAndStructors( IOBasicOutputQueue, IOOutputQueue )

#define QUEUE_LOCK      IOLockLock(_queueLock)
#define QUEUE_UNLOCK    IOLockUnlock(_queueLock)

#define kIOOutputQueueSignature      ((void *) 0xfacefeed)

//---------------------------------------------------------------------------
// 'C' function glue to dispatch the IONetworkData notification.

IOReturn
IOBasicOutputQueue::dispatchNetworkDataNotification(void *          target,
                                                    void *          param,
                                                    IONetworkData * data,
                                                    UInt32          type)
{
    IOBasicOutputQueue * self = (IOBasicOutputQueue *) target;
    return self->handleNetworkDataAccess(data, type, param);
}

//---------------------------------------------------------------------------
// Initialize an IOBasicOutputQueue object.

bool IOBasicOutputQueue::init(OSObject *     target,
                              IOOutputAction action,
                              UInt32         capacity,
                              UInt32         priorities)
{
    if (super::init() == false)
        return false;

    if ((target == 0) || (action == 0) || (priorities == 0) || (priorities > 256))
        return false;

    _target = target;
    _action = action;

    // Create a data object for queue statistics.

    _statsData = IONetworkData::withInternalBuffer(
                   kIOOutputQueueStatsKey,
                   sizeof(IOOutputQueueStats),
                   kIONetworkDataBasicAccessTypes,
                   this,
                   (IONetworkData::Action)
                       &IOBasicOutputQueue::dispatchNetworkDataNotification,
                   kIOOutputQueueSignature);

    if (_statsData == 0)
        return false;

    _stats = (IOOutputQueueStats *) _statsData->getBuffer();
    assert(_stats);

    _stats->capacity = capacity;

    // Create queue objects
    _priorities = priorities;
    _primaryQueues = IONew(IOMbufQueue, priorities);
    _shadowQueues = IONew(IOMbufQueue, priorities);
    
    if ( (_primaryQueues == 0) || (_shadowQueues == 0) )
        return false;
    
    // Initialize queues
    for(UInt32 i = 0; i < priorities; i++)
    {
        IOMbufQueueInit(&(_primaryQueues[i]), capacity);
        IOMbufQueueInit(&(_shadowQueues[i]), capacity);
    }
    
    _inQueues = _primaryQueues;

    // Create a lock to protect the queue.

    _queueLock = IOLockAlloc();
    if (_queueLock == 0)
        return false;

    return true;
}

//---------------------------------------------------------------------------
// Factory methods that will construct and initialize an IOBasicOutputQueue 
// object.

IOBasicOutputQueue *
IOBasicOutputQueue::withTarget(IONetworkController * target,
                               UInt32                capacity)
{
    return IOBasicOutputQueue::withTarget(target, capacity, 1 /* priorities */);
}

IOBasicOutputQueue *
IOBasicOutputQueue::withTarget(IONetworkController * target,
                               UInt32                capacity,
                               UInt32                priorities)
{
    IOBasicOutputQueue * queue = new IOBasicOutputQueue;

    if (queue && !queue->init(target, target->getOutputHandler(), capacity, priorities))
    {
        queue->release();
        queue = 0;
    }
    return queue;
}

IOBasicOutputQueue *
IOBasicOutputQueue::withTarget(OSObject *     target,
                               IOOutputAction action,
                               UInt32         capacity)
{
    return IOBasicOutputQueue::withTarget(target, action, capacity, 1 /* priorities */);
}
    
IOBasicOutputQueue *
IOBasicOutputQueue::withTarget(OSObject *     target,
                               IOOutputAction action,
                               UInt32         capacity,
                               UInt32         priorities)
{
    IOBasicOutputQueue * queue = new IOBasicOutputQueue;
    
    if (queue && !queue->init(target, action, capacity, priorities))
    {
        queue->release();
        queue = 0;
    }
    return queue;
}

//---------------------------------------------------------------------------
// Release all resources previously allocated before calling super::free().

void IOBasicOutputQueue::free()
{
    cancelServiceThread();

    if (_queueLock)
    {
        flush();
        IOLockFree(_queueLock);
        _queueLock = 0;
    }

    if(_primaryQueues) IODelete(_primaryQueues, IOMbufQueue, _priorities);
    if(_shadowQueues) IODelete(_shadowQueues, IOMbufQueue, _priorities);
    _primaryQueues = _shadowQueues = 0;

    if (_statsData)
    {
        _statsData->release();
        _statsData = 0;
    }

    super::free();
}

//---------------------------------------------------------------------------
// Provide an implementation for the serviceThread() method defined in 
// IOOutputQueue. This method is called by a callout thread after an
// asynchronous service was scheduled.

void IOBasicOutputQueue::serviceThread(void * param)
{
    QUEUE_LOCK;
    STATE_CLR((uintptr_t) param);
    STATE_SET(kStateOutputActive);
    dequeue();
    QUEUE_UNLOCK;
}

//---------------------------------------------------------------------------
// Add a single packet, or a chain of packets, to the queue object.
// This method can support multiple clients threads.

UInt32 IOBasicOutputQueue::enqueue(mbuf_t m, void * param)
{
    bool success;
    
    UInt32 priority = getMbufPriority(m);
    
    // Set out-of-bounds priority to lowest
    if ( priority >= _priorities )
    {
        priority = _priorities - 1;
    }

    QUEUE_LOCK;

	success = IOMbufQueueEnqueue(&(_inQueues[priority]), m);

    if ( STATE_IS( kStateRunning ) )
    {
        STATE_SET( kStateOutputActive );
        dequeue();
    }

    QUEUE_UNLOCK;

    // Drop the packet if the packet(s) were not queued.
    // But avoid calling m_free() while holding a simple lock.
    // This will not be necessary in the future when m_free()
    // is no longer funneled.

    if (success == false)
    {
        OSAddAtomic( IOMbufFree(m),
                     (SInt32 *) &_stats->dropCount );
    }

    return 0;
}

//---------------------------------------------------------------------------
// Responsible for removing all packets from the queue and pass each packet
// removed to our target. This method returns when the queue becomes empty
// or if the queue is stalled by the target. This method is called with the 
// queue lock held.

void IOBasicOutputQueue::dequeue()
{
    IOMbufQueue * outQueues = _primaryQueues;
    UInt32        newState = 0;
    UInt32        myServiceCount;

    // Switch the input queue. Work on the real queue, while allowing
    // clients to continue to queue packets to the "shadow" queue.

    _inQueues = _shadowQueues;

    // While dequeue is allowed, and incoming queue has packets.

    UInt32 priority = 0;
    while ( STATE_IS( kStateRunning | kStateOutputActive ) &&
            priority < _priorities )
    {
        if (IOMbufQueueGetSize(&(outQueues[priority])) > 0)
        {
            myServiceCount = _serviceCount;

            QUEUE_UNLOCK;

            output( &(outQueues[priority]), &newState );

            QUEUE_LOCK;

            // If driver called service() while the queue lock was released,
            // refuse to honor any stall requests and re-attempt transmission.

            if ( newState )
            {
                if ( myServiceCount != _serviceCount )
                    newState &= ~kStateOutputStalled;

                STATE_SET( newState );
            }

            // Absorb new packets added to the shadow queues.
            // Must empty all shadow queues since the loop might exit
            // before servicing all priority levels due to driver stall.

            int newPriority = -1;
            for (UInt32 i = 0; i < _priorities; i++)
            {
                IOMbufQueueEnqueue( &(outQueues[i]), &(_inQueues[i]));

                if ((newPriority < 0) && (i <= priority) &&
                    (IOMbufQueueGetSize(&(outQueues[i])) > 0))
                {
                    newPriority = i;
                }
            }

            // Service higher or equal priority mbufs if they arrived while
            // the queue lock was dropped.

            if (newPriority >= 0)
            {
                priority = newPriority;
                continue;
            }
        }
        
        priority++;
    }

    _inQueues = _primaryQueues;

    STATE_CLR( kStateOutputActive );

    if ( newState & kStateOutputServiceMask )
    {
        scheduleServiceThread(
            (void *)(uintptr_t) (newState & kStateOutputServiceMask));
    }

    if (_waitDequeueDone)
    {
        // A stop() request is waiting for the transmit thread to
        // complete transmission. Wake up the waiting thread.

        _waitDequeueDone = false;
        thread_wakeup((void *) &_waitDequeueDone);
    }
}

//---------------------------------------------------------------------------
// Transfer all packets from the given queue to the target. Continue until
// the queue becomes empty, or if the target throttle the queue.

void IOBasicOutputQueue::output(IOMbufQueue * queue, UInt32 * state)
{
    mbuf_t pkt;
    UInt32        status;

    do {
        pkt = IOMbufQueueDequeue(queue);
        assert(pkt);

        // Handoff each packet to the controller driver.

        status = (_target->*_action)( pkt, 0 );

        if ( status == ( kIOOutputStatusAccepted | kIOOutputCommandNone ) )
        {
            // Fast-path the typical code path.
            _stats->outputCount++;
        }
        else
        {
            // Look at the return status and update statistics counters.

            switch (status & kIOOutputStatusMask)
            {
                default:
                case kIOOutputStatusAccepted:
                    _stats->outputCount++;
                    break;
    
                case kIOOutputStatusRetry:
                    IOMbufQueuePrepend(queue, pkt);
                    _stats->retryCount++;
                    break;
            }
    
            // Handle the requested action.
    
            switch (status & kIOOutputCommandMask)
            {
                case kIOOutputCommandStall:
                    *state = kStateOutputStalled;
                    _stats->stallCount++;
                    break;
    
                default:
                    break;
            }
        }
    }
	while ( IOMbufQueueGetSize(queue) && (*state == 0) );
}

//---------------------------------------------------------------------------
// Start or enable the queue.

bool IOBasicOutputQueue::start()
{
    QUEUE_LOCK;

    STATE_SET( kStateRunning );
    STATE_CLR( kStateOutputStalled );
    _serviceCount++;

    if ( STATE_IS( kStateRunning ) )
    {
        STATE_SET( kStateOutputActive );
        dequeue();
    }

    QUEUE_UNLOCK;

    return true;   /* always return true */
}

//---------------------------------------------------------------------------
// Stop or disable the queue.

bool IOBasicOutputQueue::stop()
{
    bool wasRunning;

    QUEUE_LOCK;

    wasRunning = STATE_HAS( kStateRunning );

    STATE_CLR( kStateRunning );

    if ( STATE_HAS( kStateOutputActive ) )
    {
        // If dequeue is active, it means that:
        //   1. A thread is about to call dequeue().
        //   2. A thread is in dequeue() and calling the target.
        //
        // Wait for the dequeue thread to complete processing.

        _waitDequeueDone = true;

        assert_wait((void *) &_waitDequeueDone, false);
    }
    
    QUEUE_UNLOCK;

    thread_block((void (*)(void*, int)) 0);

    return wasRunning;
}

//---------------------------------------------------------------------------
// If the queue becomes stalled, then service() must be called by the target
// to restart the queue when the target is ready to accept more packets.

bool IOBasicOutputQueue::service(IOOptionBits options)
{
    bool    doDequeue = false;
    bool    async     = (options & kServiceAsync);
    UInt32  oldState;

    QUEUE_LOCK;

    oldState = _state;

    // Clear the stall condition.

    STATE_CLR( kStateOutputStalled );
    _serviceCount++;
    
    bool workToDo = false;
    for(UInt32 i = 0; i < _priorities; i++)
    {
        if(IOMbufQueueGetSize(&(_primaryQueues[i])) > 0)
        {
            workToDo = true;
            break;
        }
    }

    if ( ( oldState & kStateOutputStalled ) &&
         STATE_IS( kStateRunning )          &&
         workToDo )
    {
        doDequeue = true;
        STATE_SET( kStateOutputActive );
        if (async == false) dequeue();
    }

    QUEUE_UNLOCK;

    if ( doDequeue && async )
    {
        scheduleServiceThread();
    }

    return doDequeue;
}

//---------------------------------------------------------------------------
// Release all packets held by the queue.

UInt32 IOBasicOutputQueue::flush()
{
    UInt32 flushCount;
	mbuf_t m;
    
    flushCount = 0;
    for(UInt32 i = 0; i < _priorities; i++)
    {
        QUEUE_LOCK;
        m = IOMbufQueueDequeueAll( &(_inQueues[i]) );
        QUEUE_UNLOCK;
        flushCount = IOMbufFree(m); 
        OSAddAtomic(flushCount, (SInt32 *) &_stats->dropCount);
    }
    return flushCount;
}

//---------------------------------------------------------------------------
// Change the capacity of the queue.

bool IOBasicOutputQueue::setCapacity(UInt32 capacity)
{
    QUEUE_LOCK;
    for(UInt32 i = 0; i < _priorities; i++)
    {
        IOMbufQueueSetCapacity(&(_primaryQueues[i]), capacity);
        IOMbufQueueSetCapacity(&(_shadowQueues[i]), capacity);
    }
    _stats->capacity = capacity * _priorities;
    QUEUE_UNLOCK;
    return true;
}

//---------------------------------------------------------------------------
// Returns the current queue capacity.

UInt32 IOBasicOutputQueue::getCapacity() const
{
    return _stats->capacity;
}

//---------------------------------------------------------------------------
// Returns the current queue size.

UInt32 IOBasicOutputQueue::getSize() const
{
    UInt32 total = 0;
    for(UInt32 i = 0; i < _priorities; i++)
    {
        total += IOMbufQueueGetSize(&(_primaryQueues[i]));
    }
    return total;
}

//---------------------------------------------------------------------------
// Returns the number of packets dropped by the queue due to over-capacity.

UInt32 IOBasicOutputQueue::getDropCount()
{
    return _stats->dropCount;
}

//---------------------------------------------------------------------------
// Returns the number of packet passed to the target.

UInt32 IOBasicOutputQueue::getOutputCount()
{
    return _stats->outputCount;
}

//---------------------------------------------------------------------------
// Returns the number of times that a kIOOutputStatusRetry status code
// is received from the target.

UInt32 IOBasicOutputQueue::getRetryCount()
{
    return _stats->retryCount;
}

//---------------------------------------------------------------------------
// Returns the number of times that a kIOOutputCommandStall action code
// is received from the target.

UInt32 IOBasicOutputQueue::getStallCount()
{
    return _stats->stallCount;
}

//---------------------------------------------------------------------------
// Returns the current state of the queue object.

UInt32 IOBasicOutputQueue::getState() const
{
    return _state;
}

//---------------------------------------------------------------------------
// This method is called by our IONetworkData object when it receives
// a read or a reset request. We need to be notified to intervene in
// the request handling.

IOReturn
IOBasicOutputQueue::handleNetworkDataAccess(IONetworkData * data,
                                            UInt32          accessType,
                                            void *          arg)
{
    IOReturn ret = kIOReturnSuccess;

    assert(data && (arg == kIOOutputQueueSignature));

    // Check the type of data request.

    switch (accessType)
    {
        case kIONetworkDataAccessTypeRead:
        case kIONetworkDataAccessTypeSerialize:
        {
            UInt32 size;
            QUEUE_LOCK;
            size = getSize();   // _primaryQueues
            for(UInt32 i = 0; i < _priorities; i++)
            {
                size += IOMbufQueueGetSize(&(_shadowQueues[i]));
            }
            QUEUE_UNLOCK;
            _stats->size = size;
            break;
        }

        default:
            ret = kIOReturnNotWritable;
            break;
    }

    return ret;
}

//---------------------------------------------------------------------------
// Return an IONetworkData object containing an IOOutputQueueStats structure.

IONetworkData * IOBasicOutputQueue::getStatisticsData() const
{
    return _statsData;
}

//===========================================================================
// IOGatedOutputQueue
//===========================================================================

#undef  super
#define super IOBasicOutputQueue
OSDefineMetaClassAndStructors( IOGatedOutputQueue, IOBasicOutputQueue )

//---------------------------------------------------------------------------
// Initialize an IOGatedOutputQueue object.

bool IOGatedOutputQueue::init(OSObject *      target,
                              IOOutputAction  action,
                              IOWorkLoop *    workloop,
                              UInt32          capacity,
                              UInt32          priorities)
{
    if (super::init(target, action, capacity, priorities) == false)
        return false;

    // Verify that the IOWorkLoop provided is valid.

    if (OSDynamicCast(IOWorkLoop, workloop) == 0)
        return false;

    // Allocate and attach an IOCommandGate object to the workloop.

    _gate = IOCommandGate::commandGate(this);

    if (!_gate || (workloop->addEventSource(_gate) != kIOReturnSuccess))
        return false;

    // Allocate and attach an IOInterruptEventSource object to the workloop.

    _interruptSrc = IOInterruptEventSource::interruptEventSource(
                    this,
			        (IOInterruptEventSource::Action) restartDeferredOutput
                    );

    if ( !_interruptSrc ||
        (workloop->addEventSource(_interruptSrc) != kIOReturnSuccess) )
        return false;

    return true;
}

//---------------------------------------------------------------------------
// Factory methods that will construct and initialize an IOGatedOutputQueue 
// object.

IOGatedOutputQueue *
IOGatedOutputQueue::withTarget(IONetworkController * target,
                               IOWorkLoop *          workloop,
                               UInt32                capacity)
{
    return IOGatedOutputQueue::withTarget(target, workloop, capacity, 1 /* priorities */);
}

IOGatedOutputQueue *
IOGatedOutputQueue::withTarget(IONetworkController * target,
                               IOWorkLoop *          workloop,
                               UInt32                capacity,
                               UInt32                priorities)
{
    IOGatedOutputQueue * queue = new IOGatedOutputQueue;
    
    if (queue && !queue->init(target, target->getOutputHandler(), workloop,
                              capacity, priorities))
    {
        queue->release();
        queue = 0;
    }
    return queue;
}

IOGatedOutputQueue *
IOGatedOutputQueue::withTarget(OSObject *     target,
                               IOOutputAction action,
                               IOWorkLoop *   workloop,
                               UInt32         capacity)
{
    return IOGatedOutputQueue::withTarget(target, action, workloop, capacity, 1 /* priorities */);
}

IOGatedOutputQueue *
IOGatedOutputQueue::withTarget(OSObject *     target,
                               IOOutputAction action,
                               IOWorkLoop *   workloop,
                               UInt32         capacity,
                               UInt32         priorities)
{
    IOGatedOutputQueue * queue = new IOGatedOutputQueue;
    
    if (queue && !queue->init(target, action, workloop, capacity, priorities))
    {
        queue->release();
        queue = 0;
    }
    return queue;
}

//---------------------------------------------------------------------------
// Free the IOGatedOutputQueue object.

void IOGatedOutputQueue::free()
{
    cancelServiceThread();

    if (_gate)
    {
        IOWorkLoop *wl = _gate->getWorkLoop();
		if(wl) wl->removeEventSource(_gate);
		_gate->release();
        _gate = 0;
    }

    if (_interruptSrc)
    {
        IOWorkLoop * wl = _interruptSrc->getWorkLoop();
        if (wl) wl->removeEventSource(_interruptSrc);
        _interruptSrc->release();
        _interruptSrc = 0;
    }

    super::free();
}

//---------------------------------------------------------------------------
// Called by an IOCommandGate object.

void IOGatedOutputQueue::gatedOutput(OSObject *          /* owner */,
                                     IOGatedOutputQueue * self,
                                     IOMbufQueue *        queue,
                                     UInt32 *             state)
{
    mbuf_t pkt;
    UInt32        status;

    do {
        pkt = IOMbufQueueDequeue(queue);
        assert(pkt);

        // Handoff the packet to the controller driver.

        status = ((self->_target)->*(self->_action))( pkt, 0 );

        if ( status == ( kIOOutputStatusAccepted | kIOOutputCommandNone ) )
        {
            // Fast-path the typical code path.
            self->_stats->outputCount++;
        }
        else
        {
            // Look at the return status and update statistics counters.

            switch (status & kIOOutputStatusMask)
            {
                default:
                case kIOOutputStatusAccepted:
                    self->_stats->outputCount++;
                    break;
    
                case kIOOutputStatusRetry:
                    IOMbufQueuePrepend(queue, pkt);
                    self->_stats->retryCount++;
                    break;
            }
    
            // Handle the requested action.
    
            switch (status & kIOOutputCommandMask)
            {
                case kIOOutputCommandStall:
                    *state = kStateOutputStalled;
                    self->_stats->stallCount++;
                    break;
    
                default:
                    break;
            }
        }
    }
	while ( IOMbufQueueGetSize(queue) && (*state == 0) );
}

//---------------------------------------------------------------------------
// Called by our superclass to output all packets in the packet queue given.

enum {
    kStateOutputDeferred = 0x100
};

void IOGatedOutputQueue::output(IOMbufQueue * queue, UInt32 * state)
{
    if ( _gate->attemptAction((IOCommandGate::Action)
                                &IOGatedOutputQueue::gatedOutput,
                              (void *) this,
                              (void *) queue,
                              (void *) state) == kIOReturnCannotLock )
    {
        *state = kStateOutputDeferred;
    }
}

bool IOGatedOutputQueue::scheduleServiceThread(void * param)
{
    if ( ((uintptr_t) param) & kStateOutputDeferred )
    {
        _interruptSrc->interruptOccurred(0, 0, 0);
        return true;
    }
    else
    {
        return super::scheduleServiceThread(param);
    }
}

void IOGatedOutputQueue::restartDeferredOutput(
                                        OSObject *               owner,
                                        IOInterruptEventSource * sender,
                                        int                      count)
{
    IOGatedOutputQueue * self = (IOGatedOutputQueue *) owner;
    self->serviceThread((void *) kStateOutputDeferred);
}
