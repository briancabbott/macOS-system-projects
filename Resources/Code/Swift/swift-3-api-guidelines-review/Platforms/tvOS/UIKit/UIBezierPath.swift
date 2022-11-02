
struct UIRectCorner : OptionSetType {
  init(rawValue rawValue: UInt)
  let rawValue: UInt
  static var topLeft: UIRectCorner { get }
  static var topRight: UIRectCorner { get }
  static var bottomLeft: UIRectCorner { get }
  static var bottomRight: UIRectCorner { get }
  static var allCorners: UIRectCorner { get }
}
@available(tvOS 3.2, *)
class UIBezierPath : NSObject, NSCopying, NSCoding {
  convenience init(rect rect: CGRect)
  convenience init(ovalIn rect: CGRect)
  convenience init(roundedRect rect: CGRect, cornerRadius cornerRadius: CGFloat)
  convenience init(roundedRect rect: CGRect, byRoundingCorners corners: UIRectCorner, cornerRadii cornerRadii: CGSize)
  convenience init(arcCenter center: CGPoint, radius radius: CGFloat, startAngle startAngle: CGFloat, endAngle endAngle: CGFloat, clockwise clockwise: Bool)
  convenience init(cgPath CGPath: CGPath)
  init?(coder aDecoder: NSCoder)
  var cgPath: CGPath
  func move(to point: CGPoint)
  func addLine(to point: CGPoint)
  func addCurve(to endPoint: CGPoint, controlPoint1 controlPoint1: CGPoint, controlPoint2 controlPoint2: CGPoint)
  func addQuadCurve(to endPoint: CGPoint, controlPoint controlPoint: CGPoint)
  @available(tvOS 4.0, *)
  func addArc(withCenter center: CGPoint, radius radius: CGFloat, startAngle startAngle: CGFloat, endAngle endAngle: CGFloat, clockwise clockwise: Bool)
  func close()
  func removeAllPoints()
  func append(_ bezierPath: UIBezierPath)
  @available(tvOS 6.0, *)
  func reversing() -> UIBezierPath
  func apply(_ transform: CGAffineTransform)
  var isEmpty: Bool { get }
  var bounds: CGRect { get }
  var currentPoint: CGPoint { get }
  func contains(_ point: CGPoint) -> Bool
  var lineWidth: CGFloat
  var lineCapStyle: CGLineCap
  var lineJoinStyle: CGLineJoin
  var miterLimit: CGFloat
  var flatness: CGFloat
  var usesEvenOddFillRule: Bool
  func setLineDash(_ pattern: UnsafePointer<CGFloat>, count count: Int, phase phase: CGFloat)
  func getLineDash(_ pattern: UnsafeMutablePointer<CGFloat>, count count: UnsafeMutablePointer<Int>, phase phase: UnsafeMutablePointer<CGFloat>)
  func fill()
  func stroke()
  func fill(with blendMode: CGBlendMode, alpha alpha: CGFloat)
  func stroke(with blendMode: CGBlendMode, alpha alpha: CGFloat)
  func addClip()
  @available(tvOS 3.2, *)
  func copy(with zone: NSZone = nil) -> AnyObject
  @available(tvOS 3.2, *)
  func encode(with aCoder: NSCoder)
}
