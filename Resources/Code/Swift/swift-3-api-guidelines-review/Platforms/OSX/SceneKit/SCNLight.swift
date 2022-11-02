
let SCNLightTypeAmbient: String
let SCNLightTypeOmni: String
let SCNLightTypeDirectional: String
let SCNLightTypeSpot: String
@available(OSX 10.10, *)
enum SCNShadowMode : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case forward
  case deferred
  case modulated
}
@available(OSX 10.8, *)
class SCNLight : NSObject, SCNAnimatable, SCNTechniqueSupport, NSCopying, NSSecureCoding {
  var type: String
  var color: AnyObject
  var name: String?
  var castsShadow: Bool
  var shadowColor: AnyObject
  var shadowRadius: CGFloat
  @available(OSX 10.10, *)
  var shadowMapSize: CGSize
  @available(OSX 10.10, *)
  var shadowSampleCount: Int
  @available(OSX 10.10, *)
  var shadowMode: SCNShadowMode
  @available(OSX 10.10, *)
  var shadowBias: CGFloat
  @available(OSX 10.10, *)
  var orthographicScale: CGFloat
  @available(OSX 10.10, *)
  var zNear: CGFloat
  @available(OSX 10.10, *)
  var zFar: CGFloat
  @available(OSX 10.10, *)
  var attenuationStartDistance: CGFloat
  @available(OSX 10.10, *)
  var attenuationEndDistance: CGFloat
  @available(OSX 10.10, *)
  var attenuationFalloffExponent: CGFloat
  @available(OSX 10.10, *)
  var spotInnerAngle: CGFloat
  @available(OSX 10.10, *)
  var spotOuterAngle: CGFloat
  @available(OSX 10.9, *)
  var gobo: SCNMaterialProperty? { get }
  @available(OSX 10.10, *)
  var categoryBitMask: Int
  @available(OSX, introduced=10.8, deprecated=10.10)
  func attribute(forKey key: String) -> AnyObject?
  @available(OSX, introduced=10.8, deprecated=10.10)
  func setAttribute(_ attribute: AnyObject?, forKey key: String)
  @available(OSX 10.8, *)
  func add(_ animation: CAAnimation, forKey key: String?)
  @available(OSX 10.8, *)
  func removeAllAnimations()
  @available(OSX 10.8, *)
  func removeAnimation(forKey key: String)
  @available(OSX 10.8, *)
  var animationKeys: [String] { get }
  @available(OSX 10.8, *)
  func animation(forKey key: String) -> CAAnimation?
  @available(OSX 10.9, *)
  func pauseAnimation(forKey key: String)
  @available(OSX 10.9, *)
  func resumeAnimation(forKey key: String)
  @available(OSX 10.9, *)
  func isAnimation(forKeyPaused key: String) -> Bool
  @available(OSX 10.10, *)
  func removeAnimation(forKey key: String, fadeOutDuration duration: CGFloat)
  @available(OSX 10.10, *)
  @NSCopying var technique: SCNTechnique?
  @available(OSX 10.8, *)
  func copy(with zone: NSZone = nil) -> AnyObject
  @available(OSX 10.8, *)
  class func supportsSecureCoding() -> Bool
  @available(OSX 10.8, *)
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
}
@available(OSX, introduced=10.8, deprecated=10.10)
let SCNLightAttenuationStartKey: String
@available(OSX, introduced=10.8, deprecated=10.10)
let SCNLightAttenuationEndKey: String
@available(OSX, introduced=10.8, deprecated=10.10)
let SCNLightAttenuationFalloffExponentKey: String
@available(OSX, introduced=10.8, deprecated=10.10)
let SCNLightSpotInnerAngleKey: String
@available(OSX, introduced=10.8, deprecated=10.10)
let SCNLightSpotOuterAngleKey: String
@available(OSX, introduced=10.8, deprecated=10.10)
let SCNLightShadowNearClippingKey: String
@available(OSX, introduced=10.8, deprecated=10.10)
let SCNLightShadowFarClippingKey: String
