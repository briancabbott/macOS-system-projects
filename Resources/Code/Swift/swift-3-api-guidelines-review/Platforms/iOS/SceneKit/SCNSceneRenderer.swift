
let SCNHitTestFirstFoundOnlyKey: String
let SCNHitTestSortResultsKey: String
let SCNHitTestClipToZRangeKey: String
let SCNHitTestBackFaceCullingKey: String
let SCNHitTestBoundingBoxOnlyKey: String
let SCNHitTestIgnoreChildNodesKey: String
let SCNHitTestRootNodeKey: String
@available(iOS 8.0, *)
let SCNHitTestIgnoreHiddenNodesKey: String
@available(iOS 9.0, *)
enum SCNRenderingAPI : UInt {
  init?(rawValue rawValue: UInt)
  var rawValue: UInt { get }
  case metal
  case openGLES2
}
@available(iOS 9.0, *)
struct SCNDebugOptions : OptionSetType {
  init(rawValue rawValue: UInt)
  let rawValue: UInt
  static var none: SCNDebugOptions { get }
  static var showPhysicsShapes: SCNDebugOptions { get }
  static var showBoundingBoxes: SCNDebugOptions { get }
  static var showLightInfluences: SCNDebugOptions { get }
  static var showLightExtents: SCNDebugOptions { get }
  static var showPhysicsFields: SCNDebugOptions { get }
  static var showWireframe: SCNDebugOptions { get }
}
@available(iOS 8.0, *)
class SCNHitTestResult : NSObject {
  var node: SCNNode { get }
  var geometryIndex: Int { get }
  var faceIndex: Int { get }
  var localCoordinates: SCNVector3 { get }
  var worldCoordinates: SCNVector3 { get }
  var localNormal: SCNVector3 { get }
  var worldNormal: SCNVector3 { get }
  var modelTransform: SCNMatrix4 { get }
  func textureCoordinates(withMappingChannel channel: Int) -> CGPoint
}
protocol SCNSceneRenderer : NSObjectProtocol {
  @available(iOS 8.0, *)
  var scene: SCNScene? { get set }
  @available(iOS 8.0, *)
  var sceneTime: NSTimeInterval { get set }
  unowned(unsafe) var delegate: @sil_unmanaged SCNSceneRendererDelegate? { get set }
  @available(iOS 8.0, *)
  func hitTest(_ point: CGPoint, options options: [String : AnyObject]? = [:]) -> [SCNHitTestResult]
  @available(iOS 8.0, *)
  func isNode(insideFrustum node: SCNNode, withPointOfView pointOfView: SCNNode) -> Bool
  @available(iOS 9.0, *)
  func nodesInsideFrustum(withPointOfView pointOfView: SCNNode) -> [SCNNode]
  @available(iOS 8.0, *)
  func projectPoint(_ point: SCNVector3) -> SCNVector3
  @available(iOS 8.0, *)
  func unprojectPoint(_ point: SCNVector3) -> SCNVector3
  var isPlaying: Bool { get set }
  var loops: Bool { get set }
  @available(iOS 8.0, *)
  var pointOfView: SCNNode? { get set }
  var autoenablesDefaultLighting: Bool { get set }
  var isJitteringEnabled: Bool { get set }
  @available(iOS 8.0, *)
  func prepare(_ object: AnyObject, shouldAbortBlock block: (() -> Bool)? = nil) -> Bool
  @available(iOS 8.0, *)
  func prepare(_ objects: [AnyObject], withCompletionHandler completionHandler: ((Bool) -> Void)? = nil)
  @available(iOS 8.0, *)
  var showsStatistics: Bool { get set }
  @available(iOS 9.0, *)
  var debugOptions: SCNDebugOptions { get set }
  @available(iOS 9.0, *)
  var renderingAPI: SCNRenderingAPI { get }
  var context: UnsafeMutablePointer<Void> { get }
  @available(iOS 9.0, *)
  var currentRenderCommandEncoder: MTLRenderCommandEncoder? { get }
  @available(iOS 9.0, *)
  var device: MTLDevice? { get }
  @available(iOS 9.0, *)
  var colorPixelFormat: MTLPixelFormat { get }
  @available(iOS 9.0, *)
  var depthPixelFormat: MTLPixelFormat { get }
  @available(iOS 9.0, *)
  var stencilPixelFormat: MTLPixelFormat { get }
  @available(iOS 9.0, *)
  var commandQueue: MTLCommandQueue? { get }
  @available(iOS 9.0, *)
  var audioListener: SCNNode? { get set }
}
protocol SCNSceneRendererDelegate : NSObjectProtocol {
  @available(iOS 8.0, *)
  optional func renderer(_ renderer: SCNSceneRenderer, updateAtTime time: NSTimeInterval)
  @available(iOS 8.0, *)
  optional func renderer(_ renderer: SCNSceneRenderer, didApplyAnimationsAtTime time: NSTimeInterval)
  @available(iOS 8.0, *)
  optional func renderer(_ renderer: SCNSceneRenderer, didSimulatePhysicsAtTime time: NSTimeInterval)
  @available(iOS 8.0, *)
  optional func renderer(_ renderer: SCNSceneRenderer, willRenderScene scene: SCNScene, atTime time: NSTimeInterval)
  @available(iOS 8.0, *)
  optional func renderer(_ renderer: SCNSceneRenderer, didRenderScene scene: SCNScene, atTime time: NSTimeInterval)
}
