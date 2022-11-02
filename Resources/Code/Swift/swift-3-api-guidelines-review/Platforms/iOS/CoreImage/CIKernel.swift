
typealias CIKernelROICallback = (Int32, CGRect) -> CGRect
@available(iOS 8.0, *)
class CIKernel : NSObject {
  @available(iOS 8.0, *)
  class func kernels(with string: String) -> [CIKernel]?
  @available(iOS 8.0, *)
  convenience init?(string string: String)
  @available(iOS 8.0, *)
  var name: String { get }
  @available(iOS 9.0, *)
  func setROISelector(_ method: Selector)
  @available(iOS 8.0, *)
  func apply(withExtent extent: CGRect, roiCallback callback: CIKernelROICallback, arguments args: [AnyObject]?) -> CIImage?
}
@available(iOS 8.0, *)
class CIColorKernel : CIKernel {
  @available(iOS 8.0, *)
  func apply(withExtent extent: CGRect, arguments args: [AnyObject]?) -> CIImage?
}
@available(iOS 8.0, *)
class CIWarpKernel : CIKernel {
  @available(iOS 8.0, *)
  func apply(withExtent extent: CGRect, roiCallback callback: CIKernelROICallback, inputImage image: CIImage, arguments args: [AnyObject]?) -> CIImage?
}
