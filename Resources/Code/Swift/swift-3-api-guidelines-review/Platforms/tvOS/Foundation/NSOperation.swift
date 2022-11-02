
@available(tvOS 2.0, *)
class NSOperation : NSObject {
  func start()
  func main()
  var isCancelled: Bool { get }
  func cancel()
  var isExecuting: Bool { get }
  var isFinished: Bool { get }
  var isConcurrent: Bool { get }
  @available(tvOS 7.0, *)
  var isAsynchronous: Bool { get }
  var isReady: Bool { get }
  func addDependency(_ op: NSOperation)
  func removeDependency(_ op: NSOperation)
  var dependencies: [NSOperation] { get }
  var queuePriority: NSOperationQueuePriority
  @available(tvOS 4.0, *)
  var completionBlock: (() -> Void)?
  @available(tvOS 4.0, *)
  func waitUntilFinished()
  @available(tvOS, introduced=4.0, deprecated=8.0)
  var threadPriority: Double
  @available(tvOS 8.0, *)
  var qualityOfService: NSQualityOfService
  @available(tvOS 8.0, *)
  var name: String?
}
enum NSOperationQueuePriority : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case veryLow
  case low
  case normal
  case high
  case veryHigh
}
@available(tvOS 4.0, *)
class NSBlockOperation : NSOperation {
  convenience init(block block: () -> Void)
  func addExecutionBlock(_ block: () -> Void)
  var executionBlocks: [() -> Void] { get }
}
@available(tvOS 2.0, *)
let NSInvocationOperationVoidResultException: String
@available(tvOS 2.0, *)
let NSInvocationOperationCancelledException: String
let NSOperationQueueDefaultMaxConcurrentOperationCount: Int
@available(tvOS 2.0, *)
class NSOperationQueue : NSObject {
  func addOperation(_ op: NSOperation)
  @available(tvOS 4.0, *)
  func addOperations(_ ops: [NSOperation], waitUntilFinished wait: Bool)
  @available(tvOS 4.0, *)
  func addOperation(_ block: () -> Void)
  var operations: [NSOperation] { get }
  @available(tvOS 4.0, *)
  var operationCount: Int { get }
  var maxConcurrentOperationCount: Int
  var isSuspended: Bool
  @available(tvOS 4.0, *)
  var name: String?
  @available(tvOS 8.0, *)
  var qualityOfService: NSQualityOfService
  @available(tvOS 8.0, *)
  unowned(unsafe) var underlyingQueue: @sil_unmanaged dispatch_queue_t?
  func cancelAllOperations()
  func waitUntilAllOperationsAreFinished()
  @available(tvOS 4.0, *)
  class func current() -> NSOperationQueue?
  @available(tvOS 4.0, *)
  class func main() -> NSOperationQueue
}
