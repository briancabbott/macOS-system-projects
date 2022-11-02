
@available(OSX 10.7, *)
class AVVideoComposition : NSObject, NSCopying, NSMutableCopying {
  @available(OSX 10.9, *)
  /*not inherited*/ init(propertiesOf asset: AVAsset)
  @available(OSX 10.9, *)
  var customVideoCompositorClass: AnyObject.Type? { get }
  var frameDuration: CMTime { get }
  var renderSize: CGSize { get }
  var instructions: [AVVideoCompositionInstructionProtocol] { get }
  var animationTool: AVVideoCompositionCoreAnimationTool? { get }
  @available(OSX 10.7, *)
  func copy(with zone: NSZone = nil) -> AnyObject
  @available(OSX 10.7, *)
  func mutableCopy(with zone: NSZone = nil) -> AnyObject
}
extension AVVideoComposition {
  @available(OSX 10.11, *)
  /*not inherited*/ init(asset asset: AVAsset, applyingCIFiltersWithHandler applier: (AVAsynchronousCIImageFilteringRequest) -> Void)
}
@available(OSX 10.7, *)
class AVMutableVideoComposition : AVVideoComposition {
}
extension AVMutableVideoComposition {
}
@available(OSX 10.7, *)
class AVVideoCompositionInstruction : NSObject, NSSecureCoding, NSCopying, NSMutableCopying, AVVideoCompositionInstructionProtocol {
  var timeRange: CMTimeRange { get }
  var backgroundColor: CGColor? { get }
  var layerInstructions: [AVVideoCompositionLayerInstruction] { get }
  var enablePostProcessing: Bool { get }
  @available(OSX 10.9, *)
  var requiredSourceTrackIDs: [NSValue] { get }
  @available(OSX 10.9, *)
  var passthroughTrackID: CMPersistentTrackID { get }
  @available(OSX 10.7, *)
  class func supportsSecureCoding() -> Bool
  @available(OSX 10.7, *)
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
  @available(OSX 10.7, *)
  func copy(with zone: NSZone = nil) -> AnyObject
  @available(OSX 10.7, *)
  func mutableCopy(with zone: NSZone = nil) -> AnyObject
  @available(OSX 10.9, *)
  var containsTweening: Bool { get }
}
@available(OSX 10.7, *)
class AVMutableVideoCompositionInstruction : AVVideoCompositionInstruction {
}
@available(OSX 10.7, *)
class AVVideoCompositionLayerInstruction : NSObject, NSSecureCoding, NSCopying, NSMutableCopying {
  var trackID: CMPersistentTrackID { get }
  func getTransformRamp(for time: CMTime, start startTransform: UnsafeMutablePointer<CGAffineTransform>, end endTransform: UnsafeMutablePointer<CGAffineTransform>, timeRange timeRange: UnsafeMutablePointer<CMTimeRange>) -> Bool
  func getOpacityRamp(for time: CMTime, startOpacity startOpacity: UnsafeMutablePointer<Float>, endOpacity endOpacity: UnsafeMutablePointer<Float>, timeRange timeRange: UnsafeMutablePointer<CMTimeRange>) -> Bool
  @available(OSX 10.9, *)
  func getCropRectangleRamp(for time: CMTime, startCropRectangle startCropRectangle: UnsafeMutablePointer<CGRect>, endCropRectangle endCropRectangle: UnsafeMutablePointer<CGRect>, timeRange timeRange: UnsafeMutablePointer<CMTimeRange>) -> Bool
  @available(OSX 10.7, *)
  class func supportsSecureCoding() -> Bool
  @available(OSX 10.7, *)
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
  @available(OSX 10.7, *)
  func copy(with zone: NSZone = nil) -> AnyObject
  @available(OSX 10.7, *)
  func mutableCopy(with zone: NSZone = nil) -> AnyObject
}
@available(OSX 10.7, *)
class AVMutableVideoCompositionLayerInstruction : AVVideoCompositionLayerInstruction {
  convenience init(assetTrack track: AVAssetTrack)
  func setTransformRampFromStart(_ startTransform: CGAffineTransform, toEnd endTransform: CGAffineTransform, timeRange timeRange: CMTimeRange)
  func setTransform(_ transform: CGAffineTransform, at time: CMTime)
  func setOpacityRampFromStartOpacity(_ startOpacity: Float, toEndOpacity endOpacity: Float, timeRange timeRange: CMTimeRange)
  func setOpacity(_ opacity: Float, at time: CMTime)
  @available(OSX 10.9, *)
  func setCropRectangleRampFromStartCropRectangle(_ startCropRectangle: CGRect, toEndCropRectangle endCropRectangle: CGRect, timeRange timeRange: CMTimeRange)
  @available(OSX 10.9, *)
  func setCropRectangle(_ cropRectangle: CGRect, at time: CMTime)
}
@available(OSX 10.7, *)
class AVVideoCompositionCoreAnimationTool : NSObject {
  convenience init(additionalLayer layer: CALayer, asTrackID trackID: CMPersistentTrackID)
  convenience init(postProcessingAsVideoLayer videoLayer: CALayer, in animationLayer: CALayer)
  @available(OSX 10.9, *)
  convenience init(postProcessingAsVideoLayers videoLayers: [CALayer], in animationLayer: CALayer)
}
extension AVAsset {
  func unusedTrackID() -> CMPersistentTrackID
}
extension AVVideoComposition {
  @available(OSX 10.8, *)
  func isValid(for asset: AVAsset?, timeRange timeRange: CMTimeRange, validationDelegate validationDelegate: AVVideoCompositionValidationHandling?) -> Bool
}
protocol AVVideoCompositionValidationHandling : NSObjectProtocol {
  @available(OSX 10.8, *)
  optional func videoComposition(_ videoComposition: AVVideoComposition, shouldContinueValidatingAfterFindingInvalidValueForKey key: String) -> Bool
  @available(OSX 10.8, *)
  optional func videoComposition(_ videoComposition: AVVideoComposition, shouldContinueValidatingAfterFindingEmpty timeRange: CMTimeRange) -> Bool
  @available(OSX 10.8, *)
  optional func videoComposition(_ videoComposition: AVVideoComposition, shouldContinueValidatingAfterFindingInvalidTimeRangeIn videoCompositionInstruction: AVVideoCompositionInstructionProtocol) -> Bool
  @available(OSX 10.8, *)
  optional func videoComposition(_ videoComposition: AVVideoComposition, shouldContinueValidatingAfterFindingInvalidTrackIDIn videoCompositionInstruction: AVVideoCompositionInstructionProtocol, layerInstruction layerInstruction: AVVideoCompositionLayerInstruction, asset asset: AVAsset) -> Bool
}
