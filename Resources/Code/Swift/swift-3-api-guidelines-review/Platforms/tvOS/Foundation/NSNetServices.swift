
let NSNetServicesErrorCode: String
let NSNetServicesErrorDomain: String
enum NSNetServicesError : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case unknownError
  case collisionError
  case notFoundError
  case activityInProgress
  case badArgumentError
  case cancelledError
  case invalidError
  case timeoutError
}
struct NSNetServiceOptions : OptionSetType {
  init(rawValue rawValue: UInt)
  let rawValue: UInt
  static var noAutoRename: NSNetServiceOptions { get }
  @available(tvOS 7.0, *)
  static var listenForConnections: NSNetServiceOptions { get }
}
class NSNetService : NSObject {
  init(domain domain: String, type type: String, name name: String, port port: Int32)
  convenience init(domain domain: String, type type: String, name name: String)
  func schedule(in aRunLoop: NSRunLoop, forMode mode: String)
  func remove(from aRunLoop: NSRunLoop, forMode mode: String)
  unowned(unsafe) var delegate: @sil_unmanaged NSNetServiceDelegate?
  @available(tvOS 7.0, *)
  var includesPeerToPeer: Bool
  var name: String { get }
  var type: String { get }
  var domain: String { get }
  var hostName: String? { get }
  var addresses: [NSData]? { get }
  @available(tvOS 2.0, *)
  var port: Int { get }
  func publish()
  @available(tvOS 2.0, *)
  func publish(_ options: NSNetServiceOptions = [])
  func stop()
  class func dictionary(fromTXTRecord txtData: NSData) -> [String : NSData]
  class func data(fromTXTRecord txtDictionary: [String : NSData]) -> NSData
  func resolve(withTimeout timeout: NSTimeInterval)
  func getInputStream(_ inputStream: UnsafeMutablePointer<NSInputStream?>, outputStream outputStream: UnsafeMutablePointer<NSOutputStream?>) -> Bool
  func setTXTRecord(_ recordData: NSData?) -> Bool
  func txtRecordData() -> NSData?
  func startMonitoring()
  func stopMonitoring()
}
class NSNetServiceBrowser : NSObject {
  unowned(unsafe) var delegate: @sil_unmanaged NSNetServiceBrowserDelegate?
  @available(tvOS 7.0, *)
  var includesPeerToPeer: Bool
  func schedule(in aRunLoop: NSRunLoop, forMode mode: String)
  func remove(from aRunLoop: NSRunLoop, forMode mode: String)
  func searchForBrowsableDomains()
  func searchForRegistrationDomains()
  func searchForServices(ofType type: String, inDomain domainString: String)
  func stop()
}
protocol NSNetServiceDelegate : NSObjectProtocol {
  optional func netServiceWillPublish(_ sender: NSNetService)
  optional func netServiceDidPublish(_ sender: NSNetService)
  optional func netService(_ sender: NSNetService, didNotPublish errorDict: [String : NSNumber])
  optional func netServiceWillResolve(_ sender: NSNetService)
  optional func netServiceDidResolveAddress(_ sender: NSNetService)
  optional func netService(_ sender: NSNetService, didNotResolve errorDict: [String : NSNumber])
  optional func netServiceDidStop(_ sender: NSNetService)
  optional func netService(_ sender: NSNetService, didUpdateTXTRecord data: NSData)
  @available(tvOS 7.0, *)
  optional func netService(_ sender: NSNetService, didAcceptConnectionWith inputStream: NSInputStream, outputStream outputStream: NSOutputStream)
}
protocol NSNetServiceBrowserDelegate : NSObjectProtocol {
  optional func netServiceBrowserWillSearch(_ browser: NSNetServiceBrowser)
  optional func netServiceBrowserDidStopSearch(_ browser: NSNetServiceBrowser)
  optional func netServiceBrowser(_ browser: NSNetServiceBrowser, didNotSearch errorDict: [String : NSNumber])
  optional func netServiceBrowser(_ browser: NSNetServiceBrowser, didFindDomain domainString: String, moreComing moreComing: Bool)
  optional func netServiceBrowser(_ browser: NSNetServiceBrowser, didFind service: NSNetService, moreComing moreComing: Bool)
  optional func netServiceBrowser(_ browser: NSNetServiceBrowser, didRemoveDomain domainString: String, moreComing moreComing: Bool)
  optional func netServiceBrowser(_ browser: NSNetServiceBrowser, didRemove service: NSNetService, moreComing moreComing: Bool)
}
