
struct CAAutoresizingMask : OptionSetType {
  init(rawValue rawValue: UInt32)
  let rawValue: UInt32
  static var layerNotSizable: CAAutoresizingMask { get }
  static var layerMinXMargin: CAAutoresizingMask { get }
  static var layerWidthSizable: CAAutoresizingMask { get }
  static var layerMaxXMargin: CAAutoresizingMask { get }
  static var layerMinYMargin: CAAutoresizingMask { get }
  static var layerHeightSizable: CAAutoresizingMask { get }
  static var layerMaxYMargin: CAAutoresizingMask { get }
}
struct CAEdgeAntialiasingMask : OptionSetType {
  init(rawValue rawValue: UInt32)
  let rawValue: UInt32
  static var layerLeftEdge: CAEdgeAntialiasingMask { get }
  static var layerRightEdge: CAEdgeAntialiasingMask { get }
  static var layerBottomEdge: CAEdgeAntialiasingMask { get }
  static var layerTopEdge: CAEdgeAntialiasingMask { get }
}
class CALayer : NSObject, NSCoding, CAMediaTiming {
  init(layer layer: AnyObject)
  func presentationLayer() -> AnyObject?
  func modelLayer() -> AnyObject
  class func defaultValue(forKey key: String) -> AnyObject?
  class func needsDisplay(forKey key: String) -> Bool
  func shouldArchiveValue(forKey key: String) -> Bool
  var bounds: CGRect
  var position: CGPoint
  var zPosition: CGFloat
  var anchorPoint: CGPoint
  var anchorPointZ: CGFloat
  var transform: CATransform3D
  func affineTransform() -> CGAffineTransform
  func setAffineTransform(_ m: CGAffineTransform)
  var frame: CGRect
  var isHidden: Bool
  var isDoubleSided: Bool
  var isGeometryFlipped: Bool
  func contentsAreFlipped() -> Bool
  var superlayer: CALayer? { get }
  func removeFromSuperlayer()
  var sublayers: [CALayer]?
  func addSublayer(_ layer: CALayer)
  func insertSublayer(_ layer: CALayer, at idx: UInt32)
  func insertSublayer(_ layer: CALayer, below sibling: CALayer?)
  func insertSublayer(_ layer: CALayer, above sibling: CALayer?)
  func replaceSublayer(_ layer: CALayer, with layer2: CALayer)
  var sublayerTransform: CATransform3D
  var mask: CALayer?
  var masksToBounds: Bool
  func convert(_ p: CGPoint, from l: CALayer?) -> CGPoint
  func convert(_ p: CGPoint, to l: CALayer?) -> CGPoint
  func convert(_ r: CGRect, from l: CALayer?) -> CGRect
  func convert(_ r: CGRect, to l: CALayer?) -> CGRect
  func convertTime(_ t: CFTimeInterval, from l: CALayer?) -> CFTimeInterval
  func convertTime(_ t: CFTimeInterval, to l: CALayer?) -> CFTimeInterval
  func hitTest(_ p: CGPoint) -> CALayer?
  func contains(_ p: CGPoint) -> Bool
  var contents: AnyObject?
  var contentsRect: CGRect
  var contentsGravity: String
  @available(OSX 10.7, *)
  var contentsScale: CGFloat
  var contentsCenter: CGRect
  var minificationFilter: String
  var magnificationFilter: String
  var minificationFilterBias: Float
  var isOpaque: Bool
  func display()
  func setNeedsDisplay()
  func setNeedsDisplayIn(_ r: CGRect)
  func needsDisplay() -> Bool
  func displayIfNeeded()
  var needsDisplayOnBoundsChange: Bool
  @available(OSX 10.8, *)
  var drawsAsynchronously: Bool
  func draw(in ctx: CGContext)
  func render(in ctx: CGContext)
  var edgeAntialiasingMask: CAEdgeAntialiasingMask
  var backgroundColor: CGColor?
  var cornerRadius: CGFloat
  var borderWidth: CGFloat
  var borderColor: CGColor?
  var opacity: Float
  var compositingFilter: AnyObject?
  var filters: [AnyObject]?
  var backgroundFilters: [AnyObject]?
  var shouldRasterize: Bool
  var rasterizationScale: CGFloat
  var shadowColor: CGColor?
  var shadowOpacity: Float
  var shadowOffset: CGSize
  var shadowRadius: CGFloat
  var shadowPath: CGPath?
  var autoresizingMask: CAAutoresizingMask
  var layoutManager: AnyObject?
  func preferredFrameSize() -> CGSize
  func setNeedsLayout()
  func needsLayout() -> Bool
  func layoutIfNeeded()
  func layoutSublayers()
  func resizeSublayers(withOldSize size: CGSize)
  func resize(withOldSuperlayerSize size: CGSize)
  class func defaultAction(forKey event: String) -> CAAction?
  func action(forKey event: String) -> CAAction?
  var actions: [String : CAAction]?
  func add(_ anim: CAAnimation, forKey key: String?)
  func removeAllAnimations()
  func removeAnimation(forKey key: String)
  func animationKeys() -> [String]?
  func animation(forKey key: String) -> CAAnimation?
  var name: String?
  weak var delegate: @sil_weak AnyObject?
  var style: [NSObject : AnyObject]?
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
  var beginTime: CFTimeInterval
  var duration: CFTimeInterval
  var speed: Float
  var timeOffset: CFTimeInterval
  var repeatCount: Float
  var repeatDuration: CFTimeInterval
  var autoreverses: Bool
  var fillMode: String
}
struct _CALayerIvars {
  var refcount: Int32
  var magic: UInt32
  var layer: UnsafeMutablePointer<Void>
}
extension NSObject {
  class func preferredSize(of layer: CALayer) -> CGSize
  func preferredSize(of layer: CALayer) -> CGSize
  class func invalidateLayout(of layer: CALayer)
  func invalidateLayout(of layer: CALayer)
  class func layoutSublayers(of layer: CALayer)
  func layoutSublayers(of layer: CALayer)
}
protocol CAAction {
  @available(OSX 10.0, *)
  func run(forKey event: String, object anObject: AnyObject, arguments dict: [NSObject : AnyObject]?)
}
extension NSNull : CAAction {
  @available(OSX 10.0, *)
  func run(forKey event: String, object anObject: AnyObject, arguments dict: [NSObject : AnyObject]?)
}
extension NSObject {
  class func display(_ layer: CALayer)
  func display(_ layer: CALayer)
  class func draw(_ layer: CALayer, in ctx: CGContext)
  func draw(_ layer: CALayer, in ctx: CGContext)
  class func action(for layer: CALayer, forKey event: String) -> CAAction?
  func action(for layer: CALayer, forKey event: String) -> CAAction?
}
@available(OSX 10.5, *)
let kCAGravityCenter: String
@available(OSX 10.5, *)
let kCAGravityTop: String
@available(OSX 10.5, *)
let kCAGravityBottom: String
@available(OSX 10.5, *)
let kCAGravityLeft: String
@available(OSX 10.5, *)
let kCAGravityRight: String
@available(OSX 10.5, *)
let kCAGravityTopLeft: String
@available(OSX 10.5, *)
let kCAGravityTopRight: String
@available(OSX 10.5, *)
let kCAGravityBottomLeft: String
@available(OSX 10.5, *)
let kCAGravityBottomRight: String
@available(OSX 10.5, *)
let kCAGravityResize: String
@available(OSX 10.5, *)
let kCAGravityResizeAspect: String
@available(OSX 10.5, *)
let kCAGravityResizeAspectFill: String
@available(OSX 10.5, *)
let kCAFilterNearest: String
@available(OSX 10.5, *)
let kCAFilterLinear: String
@available(OSX 10.6, *)
let kCAFilterTrilinear: String
@available(OSX 10.5, *)
let kCAOnOrderIn: String
@available(OSX 10.5, *)
let kCAOnOrderOut: String
@available(OSX 10.5, *)
let kCATransition: String
