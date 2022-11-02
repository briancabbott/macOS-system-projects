
class NSURLConnection : NSObject {
  @available(tvOS, introduced=2.0, deprecated=9.0, message="Use NSURLSession (see NSURLSession.h)")
  init?(request request: NSURLRequest, delegate delegate: AnyObject?, startImmediately startImmediately: Bool)
  @available(tvOS, introduced=2.0, deprecated=9.0, message="Use NSURLSession (see NSURLSession.h)")
  init?(request request: NSURLRequest, delegate delegate: AnyObject?)
  @available(tvOS 5.0, *)
  @NSCopying var originalRequest: NSURLRequest { get }
  @available(tvOS 5.0, *)
  @NSCopying var currentRequest: NSURLRequest { get }
  @available(tvOS 2.0, *)
  func start()
  func cancel()
  @available(tvOS 2.0, *)
  func schedule(in aRunLoop: NSRunLoop, forMode mode: String)
  @available(tvOS 2.0, *)
  func unschedule(from aRunLoop: NSRunLoop, forMode mode: String)
  @available(tvOS 5.0, *)
  func setDelegateQueue(_ queue: NSOperationQueue?)
  class func canHandle(_ request: NSURLRequest) -> Bool
}
protocol NSURLConnectionDelegate : NSObjectProtocol {
  optional func connection(_ connection: NSURLConnection, didFailWithError error: NSError)
  optional func connectionShouldUseCredentialStorage(_ connection: NSURLConnection) -> Bool
  optional func connection(_ connection: NSURLConnection, willSendRequestFor challenge: NSURLAuthenticationChallenge)
  @available(tvOS, introduced=3.0, deprecated=8.0, message="Use -connection:willSendRequestForAuthenticationChallenge: instead.")
  optional func connection(_ connection: NSURLConnection, canAuthenticateAgainstProtectionSpace protectionSpace: NSURLProtectionSpace) -> Bool
  @available(tvOS, introduced=2.0, deprecated=8.0, message="Use -connection:willSendRequestForAuthenticationChallenge: instead.")
  optional func connection(_ connection: NSURLConnection, didReceive challenge: NSURLAuthenticationChallenge)
  @available(tvOS, introduced=2.0, deprecated=8.0, message="Use -connection:willSendRequestForAuthenticationChallenge: instead.")
  optional func connection(_ connection: NSURLConnection, didCancel challenge: NSURLAuthenticationChallenge)
}
protocol NSURLConnectionDataDelegate : NSURLConnectionDelegate {
  optional func connection(_ connection: NSURLConnection, willSend request: NSURLRequest, redirectResponse response: NSURLResponse?) -> NSURLRequest?
  optional func connection(_ connection: NSURLConnection, didReceive response: NSURLResponse)
  optional func connection(_ connection: NSURLConnection, didReceive data: NSData)
  optional func connection(_ connection: NSURLConnection, needNewBodyStream request: NSURLRequest) -> NSInputStream?
  optional func connection(_ connection: NSURLConnection, didSendBodyData bytesWritten: Int, totalBytesWritten totalBytesWritten: Int, totalBytesExpectedToWrite totalBytesExpectedToWrite: Int)
  optional func connection(_ connection: NSURLConnection, willCacheResponse cachedResponse: NSCachedURLResponse) -> NSCachedURLResponse?
  optional func connectionDidFinishLoading(_ connection: NSURLConnection)
}
protocol NSURLConnectionDownloadDelegate : NSURLConnectionDelegate {
  optional func connection(_ connection: NSURLConnection, didWriteData bytesWritten: Int64, totalBytesWritten totalBytesWritten: Int64, expectedTotalBytes expectedTotalBytes: Int64)
  optional func connectionDidResumeDownloading(_ connection: NSURLConnection, totalBytesWritten totalBytesWritten: Int64, expectedTotalBytes expectedTotalBytes: Int64)
  func connectionDidFinishDownloading(_ connection: NSURLConnection, destinationURL destinationURL: NSURL)
}
extension NSURLConnection {
  @available(tvOS, introduced=2.0, deprecated=9.0, message="Use [NSURLSession dataTaskWithRequest:completionHandler:] (see NSURLSession.h")
  class func sendSynchronousRequest(_ request: NSURLRequest, returning response: AutoreleasingUnsafeMutablePointer<NSURLResponse?>) throws -> NSData
}
extension NSURLConnection {
  @available(tvOS, introduced=5.0, deprecated=9.0, message="Use [NSURLSession dataTaskWithRequest:completionHandler:] (see NSURLSession.h")
  class func sendAsynchronousRequest(_ request: NSURLRequest, queue queue: NSOperationQueue, completionHandler handler: (NSURLResponse?, NSData?, NSError?) -> Void)
}
