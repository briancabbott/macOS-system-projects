
@available(iOS 2.0, *)
let NSURLProtectionSpaceHTTP: String
@available(iOS 2.0, *)
let NSURLProtectionSpaceHTTPS: String
@available(iOS 2.0, *)
let NSURLProtectionSpaceFTP: String
let NSURLProtectionSpaceHTTPProxy: String
let NSURLProtectionSpaceHTTPSProxy: String
let NSURLProtectionSpaceFTPProxy: String
let NSURLProtectionSpaceSOCKSProxy: String
let NSURLAuthenticationMethodDefault: String
let NSURLAuthenticationMethodHTTPBasic: String
let NSURLAuthenticationMethodHTTPDigest: String
let NSURLAuthenticationMethodHTMLForm: String
@available(iOS 2.0, *)
let NSURLAuthenticationMethodNTLM: String
@available(iOS 2.0, *)
let NSURLAuthenticationMethodNegotiate: String
@available(iOS 3.0, *)
let NSURLAuthenticationMethodClientCertificate: String
@available(iOS 3.0, *)
let NSURLAuthenticationMethodServerTrust: String
class NSURLProtectionSpace : NSObject, NSSecureCoding, NSCopying {
  init(host host: String, port port: Int, protocol protocol: String?, realm realm: String?, authenticationMethod authenticationMethod: String?)
  init(proxyHost host: String, port port: Int, type type: String?, realm realm: String?, authenticationMethod authenticationMethod: String?)
  var realm: String? { get }
  var receivesCredentialSecurely: Bool { get }
  var host: String { get }
  var port: Int { get }
  var proxyType: String? { get }
  var `protocol`: String? { get }
  var authenticationMethod: String { get }
  class func supportsSecureCoding() -> Bool
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
  func copy(with zone: NSZone = nil) -> AnyObject
}
extension NSURLProtectionSpace {
  @available(iOS 3.0, *)
  var distinguishedNames: [NSData]? { get }
}
extension NSURLProtectionSpace {
  @available(iOS 3.0, *)
  var serverTrust: SecTrust? { get }
}
