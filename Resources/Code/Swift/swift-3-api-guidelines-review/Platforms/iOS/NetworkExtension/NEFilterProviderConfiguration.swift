
@available(iOS 9.0, *)
class NEFilterProviderConfiguration : NSObject, NSSecureCoding, NSCopying {
  @available(iOS 9.0, *)
  var filterBrowsers: Bool
  @available(iOS 9.0, *)
  var filterSockets: Bool
  @available(iOS 9.0, *)
  var vendorConfiguration: [String : AnyObject]?
  @available(iOS 9.0, *)
  var serverAddress: String?
  @available(iOS 9.0, *)
  var username: String?
  @available(iOS 9.0, *)
  var organization: String?
  @available(iOS 9.0, *)
  @NSCopying var passwordReference: NSData?
  @available(iOS 9.0, *)
  @NSCopying var identityReference: NSData?
  @available(iOS 9.0, *)
  class func supportsSecureCoding() -> Bool
  @available(iOS 9.0, *)
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
  @available(iOS 9.0, *)
  func copy(with zone: NSZone = nil) -> AnyObject
}
