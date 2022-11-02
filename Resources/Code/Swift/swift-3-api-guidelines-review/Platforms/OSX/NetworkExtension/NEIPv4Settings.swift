
@available(OSX 10.11, *)
class NEIPv4Settings : NSObject, NSSecureCoding, NSCopying {
  @available(OSX 10.11, *)
  init(addresses addresses: [String], subnetMasks subnetMasks: [String])
  @available(OSX 10.11, *)
  var addresses: [String] { get }
  @available(OSX 10.11, *)
  var subnetMasks: [String] { get }
  @available(OSX 10.11, *)
  var includedRoutes: [NEIPv4Route]?
  @available(OSX 10.11, *)
  var excludedRoutes: [NEIPv4Route]?
  @available(OSX 10.11, *)
  class func supportsSecureCoding() -> Bool
  @available(OSX 10.11, *)
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
  @available(OSX 10.11, *)
  func copy(with zone: NSZone = nil) -> AnyObject
}
@available(OSX 10.11, *)
class NEIPv4Route : NSObject, NSSecureCoding, NSCopying {
  @available(OSX 10.11, *)
  init(destinationAddress address: String, subnetMask subnetMask: String)
  @available(OSX 10.11, *)
  var destinationAddress: String { get }
  @available(OSX 10.11, *)
  var destinationSubnetMask: String { get }
  @available(OSX 10.11, *)
  var gatewayAddress: String?
  @available(OSX 10.11, *)
  class func defaultRoute() -> NEIPv4Route
  @available(OSX 10.11, *)
  class func supportsSecureCoding() -> Bool
  @available(OSX 10.11, *)
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
  @available(OSX 10.11, *)
  func copy(with zone: NSZone = nil) -> AnyObject
}
