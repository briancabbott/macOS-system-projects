
@available(iOS 8.0, *)
class UITraitCollection : NSObject, NSCopying, NSSecureCoding {
  init?(coder aDecoder: NSCoder)
  func containsTraits(in trait: UITraitCollection?) -> Bool
  /*not inherited*/ init(traitsFrom traitCollections: [UITraitCollection])
  /*not inherited*/ init(userInterfaceIdiom idiom: UIUserInterfaceIdiom)
  var userInterfaceIdiom: UIUserInterfaceIdiom { get }
  /*not inherited*/ init(displayScale scale: CGFloat)
  var displayScale: CGFloat { get }
  /*not inherited*/ init(horizontalSizeClass horizontalSizeClass: UIUserInterfaceSizeClass)
  var horizontalSizeClass: UIUserInterfaceSizeClass { get }
  /*not inherited*/ init(verticalSizeClass verticalSizeClass: UIUserInterfaceSizeClass)
  var verticalSizeClass: UIUserInterfaceSizeClass { get }
  @available(iOS 9.0, *)
  /*not inherited*/ init(forceTouchCapability capability: UIForceTouchCapability)
  @available(iOS 9.0, *)
  var forceTouchCapability: UIForceTouchCapability { get }
  @available(iOS 8.0, *)
  func copy(with zone: NSZone = nil) -> AnyObject
  @available(iOS 8.0, *)
  class func supportsSecureCoding() -> Bool
  @available(iOS 8.0, *)
  func encode(with aCoder: NSCoder)
}
protocol UITraitEnvironment : NSObjectProtocol {
  @available(iOS 8.0, *)
  var traitCollection: UITraitCollection { get }
  @available(iOS 8.0, *)
  func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?)
}
