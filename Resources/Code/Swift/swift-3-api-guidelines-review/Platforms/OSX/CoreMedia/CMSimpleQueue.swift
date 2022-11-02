
var kCMSimpleQueueError_AllocationFailed: OSStatus { get }
var kCMSimpleQueueError_RequiredParameterMissing: OSStatus { get }
var kCMSimpleQueueError_ParameterOutOfRange: OSStatus { get }
var kCMSimpleQueueError_QueueIsFull: OSStatus { get }
class CMSimpleQueue {
}
@available(OSX 10.7, *)
func CMSimpleQueueGetTypeID() -> CFTypeID
@available(OSX 10.7, *)
func CMSimpleQueueCreate(_ allocator: CFAllocator?, _ capacity: Int32, _ queueOut: UnsafeMutablePointer<CMSimpleQueue?>) -> OSStatus
@available(OSX 10.7, *)
func CMSimpleQueueEnqueue(_ queue: CMSimpleQueue, _ element: UnsafePointer<Void>) -> OSStatus
@available(OSX 10.7, *)
func CMSimpleQueueDequeue(_ queue: CMSimpleQueue) -> UnsafePointer<Void>
@available(OSX 10.7, *)
func CMSimpleQueueGetHead(_ queue: CMSimpleQueue) -> UnsafePointer<Void>
@available(OSX 10.7, *)
func CMSimpleQueueReset(_ queue: CMSimpleQueue) -> OSStatus
@available(OSX 10.7, *)
func CMSimpleQueueGetCapacity(_ queue: CMSimpleQueue) -> Int32
@available(OSX 10.7, *)
func CMSimpleQueueGetCount(_ queue: CMSimpleQueue) -> Int32
