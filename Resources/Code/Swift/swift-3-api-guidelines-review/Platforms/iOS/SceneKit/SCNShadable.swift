
@available(iOS 9.0, *)
enum SCNBufferFrequency : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case perFrame
  case perNode
  case perShadable
}
protocol SCNBufferStream : NSObjectProtocol {
  func writeBytes(_ bytes: UnsafeMutablePointer<Void>, length length: Int)
}
typealias SCNBufferBindingBlock = (SCNBufferStream, SCNNode, SCNShadable, SCNRenderer) -> Void
typealias SCNBindingBlock = (UInt32, UInt32, SCNNode, SCNRenderer) -> Void
protocol SCNShadable : NSObjectProtocol {
  @available(iOS 8.0, *)
  optional var program: SCNProgram? { get set }
  @available(iOS 8.0, *)
  optional func handleBinding(ofSymbol symbol: String, using block: SCNBindingBlock? = nil)
  @available(iOS 8.0, *)
  optional func handleUnbinding(ofSymbol symbol: String, using block: SCNBindingBlock? = nil)
  @available(iOS 8.0, *)
  optional var shaderModifiers: [String : String]? { get set }
}
let SCNProgramMappingChannelKey: String
@available(iOS 8.0, *)
class SCNProgram : NSObject, NSCopying, NSSecureCoding {
  var vertexShader: String?
  var fragmentShader: String?
  @available(iOS 9.0, *)
  var vertexFunctionName: String?
  @available(iOS 9.0, *)
  var fragmentFunctionName: String?
  @available(iOS 9.0, *)
  func handleBinding(ofBufferNamed name: String, frequency frequency: SCNBufferFrequency, using block: SCNBufferBindingBlock)
  @available(iOS 8.0, *)
  var isOpaque: Bool
  func setSemantic(_ semantic: String?, forSymbol symbol: String, options options: [String : AnyObject]? = [:])
  func semantic(forSymbol symbol: String) -> String?
  unowned(unsafe) var delegate: @sil_unmanaged SCNProgramDelegate?
  @available(iOS 9.0, *)
  var library: MTLLibrary?
  @available(iOS 8.0, *)
  func copy(with zone: NSZone = nil) -> AnyObject
  @available(iOS 8.0, *)
  class func supportsSecureCoding() -> Bool
  @available(iOS 8.0, *)
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
}
protocol SCNProgramDelegate : NSObjectProtocol {
  @available(iOS 8.0, *)
  optional func program(_ program: SCNProgram, handleError error: NSError)
}
@available(iOS 8.0, *)
let SCNShaderModifierEntryPointGeometry: String
@available(iOS 8.0, *)
let SCNShaderModifierEntryPointSurface: String
@available(iOS 8.0, *)
let SCNShaderModifierEntryPointLightingModel: String
@available(iOS 8.0, *)
let SCNShaderModifierEntryPointFragment: String
