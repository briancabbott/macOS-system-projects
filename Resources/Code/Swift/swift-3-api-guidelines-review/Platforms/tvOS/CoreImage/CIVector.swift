
@available(tvOS 5.0, *)
class CIVector : NSObject, NSCopying, NSSecureCoding {
  init(values values: UnsafePointer<CGFloat>, count count: Int)
  convenience init(x x: CGFloat)
  convenience init(x x: CGFloat, y y: CGFloat)
  convenience init(x x: CGFloat, y y: CGFloat, z z: CGFloat)
  convenience init(x x: CGFloat, y y: CGFloat, z z: CGFloat, w w: CGFloat)
  @available(tvOS 5.0, *)
  convenience init(cgPoint p: CGPoint)
  @available(tvOS 5.0, *)
  convenience init(cgRect r: CGRect)
  @available(tvOS 5.0, *)
  convenience init(cgAffineTransform r: CGAffineTransform)
  convenience init(string representation: String)
  func value(at index: Int) -> CGFloat
  var count: Int { get }
  var x: CGFloat { get }
  var y: CGFloat { get }
  var z: CGFloat { get }
  var w: CGFloat { get }
  @available(tvOS 5.0, *)
  var cgPointValue: CGPoint { get }
  @available(tvOS 5.0, *)
  var cgRectValue: CGRect { get }
  @available(tvOS 5.0, *)
  var cgAffineTransformValue: CGAffineTransform { get }
  var stringRepresentation: String { get }
  @available(tvOS 5.0, *)
  func copy(with zone: NSZone = nil) -> AnyObject
  @available(tvOS 5.0, *)
  class func supportsSecureCoding() -> Bool
  @available(tvOS 5.0, *)
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
}
