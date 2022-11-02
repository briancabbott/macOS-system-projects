
enum UITouchPhase : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case began
  case moved
  case stationary
  case ended
  case cancelled
}
enum UIForceTouchCapability : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case unknown
  case unavailable
  case available
}
@available(tvOS 9.0, *)
enum UITouchType : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case direct
  case indirect
  @available(tvOS 9.1, *)
  case stylus
}
@available(tvOS 9.1, *)
struct UITouchProperties : OptionSetType {
  init(rawValue rawValue: Int)
  let rawValue: Int
  static var force: UITouchProperties { get }
  static var azimuth: UITouchProperties { get }
  static var altitude: UITouchProperties { get }
  static var location: UITouchProperties { get }
}
@available(tvOS 2.0, *)
class UITouch : NSObject {
  var timestamp: NSTimeInterval { get }
  var phase: UITouchPhase { get }
  var tapCount: Int { get }
  @available(tvOS 9.0, *)
  var type: UITouchType { get }
  @available(tvOS 8.0, *)
  var majorRadius: CGFloat { get }
  @available(tvOS 8.0, *)
  var majorRadiusTolerance: CGFloat { get }
  var window: UIWindow? { get }
  var view: UIView? { get }
  @available(tvOS 3.2, *)
  var gestureRecognizers: [UIGestureRecognizer]? { get }
  func location(in view: UIView?) -> CGPoint
  func previousLocation(in view: UIView?) -> CGPoint
  @available(tvOS 9.1, *)
  func preciseLocation(in view: UIView?) -> CGPoint
  @available(tvOS 9.1, *)
  func precisePreviousLocation(in view: UIView?) -> CGPoint
  @available(tvOS 9.0, *)
  var force: CGFloat { get }
  @available(tvOS 9.0, *)
  var maximumPossibleForce: CGFloat { get }
  @available(tvOS 9.1, *)
  func azimuthAngle(in view: UIView?) -> CGFloat
  @available(tvOS 9.1, *)
  func azimuthUnitVector(in view: UIView?) -> CGVector
  @available(tvOS 9.1, *)
  var altitudeAngle: CGFloat { get }
  @available(tvOS 9.1, *)
  var estimationUpdateIndex: NSNumber? { get }
  @available(tvOS 9.1, *)
  var estimatedProperties: UITouchProperties { get }
  @available(tvOS 9.1, *)
  var estimatedPropertiesExpectingUpdates: UITouchProperties { get }
}
