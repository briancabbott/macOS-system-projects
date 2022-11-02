
class NSURLResponse : NSObject, NSSecureCoding, NSCopying {
  init(url URL: NSURL, mimeType MIMEType: String?, expectedContentLength length: Int, textEncodingName name: String?)
  @NSCopying var url: NSURL? { get }
  var mimeType: String? { get }
  var expectedContentLength: Int64 { get }
  var textEncodingName: String? { get }
  var suggestedFilename: String? { get }
  class func supportsSecureCoding() -> Bool
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
  func copy(with zone: NSZone = nil) -> AnyObject
}
class NSHTTPURLResponse : NSURLResponse {
  @available(iOS 5.0, *)
  init?(url url: NSURL, statusCode statusCode: Int, httpVersion HTTPVersion: String?, headerFields headerFields: [String : String]?)
  var statusCode: Int { get }
  var allHeaderFields: [NSObject : AnyObject] { get }
  class func localizedString(forStatusCode statusCode: Int) -> String
}
