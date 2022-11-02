
class NSURLCredentialStorage : NSObject {
  class func shared() -> NSURLCredentialStorage
  func credentials(for space: NSURLProtectionSpace) -> [String : NSURLCredential]?
  var allCredentials: [NSURLProtectionSpace : [String : NSURLCredential]] { get }
  func setCredential(_ credential: NSURLCredential, for space: NSURLProtectionSpace)
  func remove(_ credential: NSURLCredential, for space: NSURLProtectionSpace)
  @available(tvOS 7.0, *)
  func remove(_ credential: NSURLCredential, for space: NSURLProtectionSpace, options options: [String : AnyObject]? = [:])
  func defaultCredential(for space: NSURLProtectionSpace) -> NSURLCredential?
  func setDefaultCredential(_ credential: NSURLCredential, for space: NSURLProtectionSpace)
}
extension NSURLCredentialStorage {
  @available(tvOS 8.0, *)
  func getCredentialsFor(_ protectionSpace: NSURLProtectionSpace, task task: NSURLSessionTask, completionHandler completionHandler: ([String : NSURLCredential]?) -> Void)
  @available(tvOS 8.0, *)
  func setCredential(_ credential: NSURLCredential, for protectionSpace: NSURLProtectionSpace, task task: NSURLSessionTask)
  @available(tvOS 8.0, *)
  func remove(_ credential: NSURLCredential, for protectionSpace: NSURLProtectionSpace, options options: [String : AnyObject]? = [:], task task: NSURLSessionTask)
  @available(tvOS 8.0, *)
  func getDefaultCredential(for space: NSURLProtectionSpace, task task: NSURLSessionTask, completionHandler completionHandler: (NSURLCredential?) -> Void)
  @available(tvOS 8.0, *)
  func setDefaultCredential(_ credential: NSURLCredential, for protectionSpace: NSURLProtectionSpace, task task: NSURLSessionTask)
}
let NSURLCredentialStorageChangedNotification: String
@available(tvOS 7.0, *)
let NSURLCredentialStorageRemoveSynchronizableCredentials: String
