
@available(iOS 8.0, *)
enum UIBlurEffectStyle : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case extraLight
  case light
  case dark
}
@available(iOS 8.0, *)
class UIVisualEffect : NSObject, NSCopying, NSSecureCoding {
  @available(iOS 8.0, *)
  func copy(with zone: NSZone = nil) -> AnyObject
  @available(iOS 8.0, *)
  class func supportsSecureCoding() -> Bool
  @available(iOS 8.0, *)
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
}
@available(iOS 8.0, *)
class UIBlurEffect : UIVisualEffect {
  /*not inherited*/ init(style style: UIBlurEffectStyle)
}
@available(iOS 8.0, *)
class UIVibrancyEffect : UIVisualEffect {
  /*not inherited*/ init(for blurEffect: UIBlurEffect)
}
@available(iOS 8.0, *)
class UIVisualEffectView : UIView, NSSecureCoding {
  var contentView: UIView { get }
  @NSCopying var effect: UIVisualEffect?
  init(effect effect: UIVisualEffect?)
  @available(iOS 8.0, *)
  class func supportsSecureCoding() -> Bool
}
