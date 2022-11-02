
enum NSURLCredentialPersistence : UInt {
  init?(rawValue rawValue: UInt)
  var rawValue: UInt { get }
  case none
  case forSession
  case permanent
  @available(tvOS 6.0, *)
  case synchronizable
}
class NSURLCredential : NSObject, NSSecureCoding, NSCopying {
  var persistence: NSURLCredentialPersistence { get }
  class func supportsSecureCoding() -> Bool
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
  func copy(with zone: NSZone = nil) -> AnyObject
}
extension NSURLCredential {
  init(user user: String, password password: String, persistence persistence: NSURLCredentialPersistence)
  var user: String? { get }
  var password: String? { get }
  var hasPassword: Bool { get }
}
extension NSURLCredential {
  @available(tvOS 3.0, *)
  init(identity identity: SecIdentity, certificates certArray: [AnyObject]?, persistence persistence: NSURLCredentialPersistence)
  var identity: SecIdentity? { get }
  @available(tvOS 3.0, *)
  var certificates: [AnyObject] { get }
}
extension NSURLCredential {
  @available(tvOS 3.0, *)
  init(trust trust: SecTrust)
  @available(tvOS 3.0, *)
  /*not inherited*/ init(for trust: SecTrust)
}
