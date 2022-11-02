
@available(tvOS 8.0, *)
enum MTLSamplerMinMagFilter : UInt {
  init?(rawValue rawValue: UInt)
  var rawValue: UInt { get }
  case nearest
  case linear
}
@available(tvOS 8.0, *)
enum MTLSamplerMipFilter : UInt {
  init?(rawValue rawValue: UInt)
  var rawValue: UInt { get }
  case notMipmapped
  case nearest
  case linear
}
@available(tvOS 8.0, *)
enum MTLSamplerAddressMode : UInt {
  init?(rawValue rawValue: UInt)
  var rawValue: UInt { get }
  case clampToEdge
  case `repeat`
  case mirrorRepeat
  case clampToZero
}
@available(tvOS 8.0, *)
class MTLSamplerDescriptor : NSObject, NSCopying {
  var minFilter: MTLSamplerMinMagFilter
  var magFilter: MTLSamplerMinMagFilter
  var mipFilter: MTLSamplerMipFilter
  var maxAnisotropy: Int
  var sAddressMode: MTLSamplerAddressMode
  var tAddressMode: MTLSamplerAddressMode
  var rAddressMode: MTLSamplerAddressMode
  var normalizedCoordinates: Bool
  var lodMinClamp: Float
  var lodMaxClamp: Float
  @available(tvOS 9.0, *)
  var lodAverage: Bool
  @available(tvOS 9.0, *)
  var compareFunction: MTLCompareFunction
  var label: String?
  @available(tvOS 8.0, *)
  func copy(with zone: NSZone = nil) -> AnyObject
}
@available(tvOS 8.0, *)
protocol MTLSamplerState : NSObjectProtocol {
  var label: String? { get }
  var device: MTLDevice { get }
}
