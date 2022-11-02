
enum UIImageOrientation : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case up
  case down
  case left
  case right
  case upMirrored
  case downMirrored
  case leftMirrored
  case rightMirrored
}
enum UIImageResizingMode : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case tile
  case stretch
}
@available(iOS 7.0, *)
enum UIImageRenderingMode : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case automatic
  case alwaysOriginal
  case alwaysTemplate
}
@available(iOS 2.0, *)
class UIImage : NSObject, NSSecureCoding {
  /*not inherited*/ init?(named name: String)
  @available(iOS 8.0, *)
  /*not inherited*/ init?(named name: String, in bundle: NSBundle?, compatibleWith traitCollection: UITraitCollection?)
  init?(contentsOfFile path: String)
  init?(data data: NSData)
  @available(iOS 6.0, *)
  init?(data data: NSData, scale scale: CGFloat)
  init(cgImage cgImage: CGImage)
  @available(iOS 4.0, *)
  init(cgImage cgImage: CGImage, scale scale: CGFloat, orientation orientation: UIImageOrientation)
  @available(iOS 5.0, *)
  init(ciImage ciImage: CIImage)
  @available(iOS 6.0, *)
  init(ciImage ciImage: CIImage, scale scale: CGFloat, orientation orientation: UIImageOrientation)
  var size: CGSize { get }
  var cgImage: CGImage? { get }
  @available(iOS 5.0, *)
  var ciImage: CIImage? { get }
  var imageOrientation: UIImageOrientation { get }
  @available(iOS 4.0, *)
  var scale: CGFloat { get }
  @available(iOS 5.0, *)
  class func animatedImageNamed(_ name: String, duration duration: NSTimeInterval) -> UIImage?
  @available(iOS 5.0, *)
  class func animatedResizableImageNamed(_ name: String, capInsets capInsets: UIEdgeInsets, duration duration: NSTimeInterval) -> UIImage?
  @available(iOS 6.0, *)
  class func animatedResizableImageNamed(_ name: String, capInsets capInsets: UIEdgeInsets, resizingMode resizingMode: UIImageResizingMode, duration duration: NSTimeInterval) -> UIImage?
  @available(iOS 5.0, *)
  class func animatedImage(with images: [UIImage], duration duration: NSTimeInterval) -> UIImage?
  @available(iOS 5.0, *)
  var images: [UIImage]? { get }
  @available(iOS 5.0, *)
  var duration: NSTimeInterval { get }
  func draw(at point: CGPoint)
  func draw(at point: CGPoint, blendMode blendMode: CGBlendMode, alpha alpha: CGFloat)
  func draw(in rect: CGRect)
  func draw(in rect: CGRect, blendMode blendMode: CGBlendMode, alpha alpha: CGFloat)
  func drawAsPattern(in rect: CGRect)
  @available(iOS 5.0, *)
  func resizableImage(withCapInsets capInsets: UIEdgeInsets) -> UIImage
  @available(iOS 6.0, *)
  func resizableImage(withCapInsets capInsets: UIEdgeInsets, resizingMode resizingMode: UIImageResizingMode) -> UIImage
  @available(iOS 5.0, *)
  var capInsets: UIEdgeInsets { get }
  @available(iOS 6.0, *)
  var resizingMode: UIImageResizingMode { get }
  @available(iOS 6.0, *)
  func withAlignmentRectInsets(_ alignmentInsets: UIEdgeInsets) -> UIImage
  @available(iOS 6.0, *)
  var alignmentRectInsets: UIEdgeInsets { get }
  @available(iOS 7.0, *)
  func withRenderingMode(_ renderingMode: UIImageRenderingMode) -> UIImage
  @available(iOS 7.0, *)
  var renderingMode: UIImageRenderingMode { get }
  @available(iOS 8.0, *)
  @NSCopying var traitCollection: UITraitCollection { get }
  @available(iOS 8.0, *)
  var imageAsset: UIImageAsset? { get }
  @available(iOS 9.0, *)
  func imageFlippedForRightToLeftLayoutDirection() -> UIImage
  @available(iOS 9.0, *)
  var flipsForRightToLeftLayoutDirection: Bool { get }
  @available(iOS 2.0, *)
  class func supportsSecureCoding() -> Bool
  @available(iOS 2.0, *)
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
}

extension UIImage : _ImageLiteralConvertible {
  convenience init!(failableImageLiteral name: String)
}
extension UIImage {
  func stretchableImage(withLeftCapWidth leftCapWidth: Int, topCapHeight topCapHeight: Int) -> UIImage
  var leftCapWidth: Int { get }
  var topCapHeight: Int { get }
}
extension CIImage {
  @available(iOS 5.0, *)
  init?(image image: UIImage)
  @available(iOS 5.0, *)
  init?(image image: UIImage, options options: [NSObject : AnyObject]? = [:])
}
func UIImagePNGRepresentation(_ image: UIImage) -> NSData?
func UIImageJPEGRepresentation(_ image: UIImage, _ compressionQuality: CGFloat) -> NSData?
