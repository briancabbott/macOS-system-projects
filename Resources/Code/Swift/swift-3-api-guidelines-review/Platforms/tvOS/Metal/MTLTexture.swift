
@available(tvOS 8.0, *)
enum MTLTextureType : UInt {
  init?(rawValue rawValue: UInt)
  var rawValue: UInt { get }
  case type1D
  case type1DArray
  case type2D
  case type2DArray
  case type2DMultisample
  case typeCube
  case type3D
}
@available(tvOS 9.0, *)
struct MTLTextureUsage : OptionSetType {
  init(rawValue rawValue: UInt)
  let rawValue: UInt
  static var unknown: MTLTextureUsage { get }
  static var shaderRead: MTLTextureUsage { get }
  static var shaderWrite: MTLTextureUsage { get }
  static var renderTarget: MTLTextureUsage { get }
  static var pixelFormatView: MTLTextureUsage { get }
}
@available(tvOS 8.0, *)
class MTLTextureDescriptor : NSObject, NSCopying {
  class func texture2DDescriptor(with pixelFormat: MTLPixelFormat, width width: Int, height height: Int, mipmapped mipmapped: Bool) -> MTLTextureDescriptor
  class func textureCubeDescriptor(with pixelFormat: MTLPixelFormat, size size: Int, mipmapped mipmapped: Bool) -> MTLTextureDescriptor
  var textureType: MTLTextureType
  var pixelFormat: MTLPixelFormat
  var width: Int
  var height: Int
  var depth: Int
  var mipmapLevelCount: Int
  var sampleCount: Int
  var arrayLength: Int
  var resourceOptions: MTLResourceOptions
  @available(tvOS 9.0, *)
  var cpuCacheMode: MTLCPUCacheMode
  @available(tvOS 9.0, *)
  var storageMode: MTLStorageMode
  @available(tvOS 9.0, *)
  var usage: MTLTextureUsage
  @available(tvOS 8.0, *)
  func copy(with zone: NSZone = nil) -> AnyObject
}
@available(tvOS 8.0, *)
protocol MTLTexture : MTLResource {
  var rootResource: MTLResource? { get }
  @available(tvOS 9.0, *)
  var parent: MTLTexture? { get }
  @available(tvOS 9.0, *)
  var parentRelativeLevel: Int { get }
  @available(tvOS 9.0, *)
  var parentRelativeSlice: Int { get }
  @available(tvOS 9.0, *)
  var buffer: MTLBuffer? { get }
  @available(tvOS 9.0, *)
  var bufferOffset: Int { get }
  @available(tvOS 9.0, *)
  var bufferBytesPerRow: Int { get }
  var textureType: MTLTextureType { get }
  var pixelFormat: MTLPixelFormat { get }
  var width: Int { get }
  var height: Int { get }
  var depth: Int { get }
  var mipmapLevelCount: Int { get }
  var sampleCount: Int { get }
  var arrayLength: Int { get }
  var usage: MTLTextureUsage { get }
  var isFramebufferOnly: Bool { get }
  func getBytes(_ pixelBytes: UnsafeMutablePointer<Void>, bytesPerRow bytesPerRow: Int, bytesPerImage bytesPerImage: Int, from region: MTLRegion, mipmapLevel level: Int, slice slice: Int)
  func replace(_ region: MTLRegion, mipmapLevel level: Int, slice slice: Int, withBytes pixelBytes: UnsafePointer<Void>, bytesPerRow bytesPerRow: Int, bytesPerImage bytesPerImage: Int)
  func getBytes(_ pixelBytes: UnsafeMutablePointer<Void>, bytesPerRow bytesPerRow: Int, from region: MTLRegion, mipmapLevel level: Int)
  func replace(_ region: MTLRegion, mipmapLevel level: Int, withBytes pixelBytes: UnsafePointer<Void>, bytesPerRow bytesPerRow: Int)
  func newTextureView(with pixelFormat: MTLPixelFormat) -> MTLTexture
  func newTextureView(with pixelFormat: MTLPixelFormat, textureType textureType: MTLTextureType, levels levelRange: NSRange, slices sliceRange: NSRange) -> MTLTexture
}
