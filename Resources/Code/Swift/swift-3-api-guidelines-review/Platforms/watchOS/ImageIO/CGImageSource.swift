
class CGImageSource {
}
enum CGImageSourceStatus : Int32 {
  init?(rawValue rawValue: Int32)
  var rawValue: Int32 { get }
  case statusUnexpectedEOF
  case statusInvalidData
  case statusUnknownType
  case statusReadingHeader
  case statusIncomplete
  case statusComplete
}
@available(watchOS 2.0, *)
let kCGImageSourceTypeIdentifierHint: CFString
@available(watchOS 2.0, *)
let kCGImageSourceShouldCache: CFString
@available(watchOS 2.0, *)
let kCGImageSourceShouldCacheImmediately: CFString
@available(watchOS 2.0, *)
let kCGImageSourceShouldAllowFloat: CFString
@available(watchOS 2.0, *)
let kCGImageSourceCreateThumbnailFromImageIfAbsent: CFString
@available(watchOS 2.0, *)
let kCGImageSourceCreateThumbnailFromImageAlways: CFString
@available(watchOS 2.0, *)
let kCGImageSourceThumbnailMaxPixelSize: CFString
@available(watchOS 2.0, *)
let kCGImageSourceCreateThumbnailWithTransform: CFString
@available(watchOS 2.0, *)
let kCGImageSourceSubsampleFactor: CFString
@available(watchOS 2.0, *)
func CGImageSourceGetTypeID() -> CFTypeID
@available(watchOS 2.0, *)
func CGImageSourceCopyTypeIdentifiers() -> CFArray
@available(watchOS 2.0, *)
func CGImageSourceCreateWithDataProvider(_ provider: CGDataProvider, _ options: CFDictionary?) -> CGImageSource?
@available(watchOS 2.0, *)
func CGImageSourceCreateWithData(_ data: CFData, _ options: CFDictionary?) -> CGImageSource?
@available(watchOS 2.0, *)
func CGImageSourceCreateWithURL(_ url: CFURL, _ options: CFDictionary?) -> CGImageSource?
@available(watchOS 2.0, *)
func CGImageSourceGetType(_ isrc: CGImageSource) -> CFString?
@available(watchOS 2.0, *)
func CGImageSourceGetCount(_ isrc: CGImageSource) -> Int
@available(watchOS 2.0, *)
func CGImageSourceCopyProperties(_ isrc: CGImageSource, _ options: CFDictionary?) -> CFDictionary?
@available(watchOS 2.0, *)
func CGImageSourceCopyPropertiesAtIndex(_ isrc: CGImageSource, _ index: Int, _ options: CFDictionary?) -> CFDictionary?
@available(watchOS 2.0, *)
func CGImageSourceCopyMetadataAtIndex(_ isrc: CGImageSource, _ index: Int, _ options: CFDictionary?) -> CGImageMetadata?
@available(watchOS 2.0, *)
func CGImageSourceCreateImageAtIndex(_ isrc: CGImageSource, _ index: Int, _ options: CFDictionary?) -> CGImage?
@available(watchOS 2.0, *)
func CGImageSourceRemoveCacheAtIndex(_ isrc: CGImageSource, _ index: Int)
@available(watchOS 2.0, *)
func CGImageSourceCreateThumbnailAtIndex(_ isrc: CGImageSource, _ index: Int, _ options: CFDictionary?) -> CGImage?
@available(watchOS 2.0, *)
func CGImageSourceCreateIncremental(_ options: CFDictionary?) -> CGImageSource
@available(watchOS 2.0, *)
func CGImageSourceUpdateData(_ isrc: CGImageSource, _ data: CFData, _ final: Bool)
@available(watchOS 2.0, *)
func CGImageSourceUpdateDataProvider(_ isrc: CGImageSource, _ provider: CGDataProvider, _ final: Bool)
@available(watchOS 2.0, *)
func CGImageSourceGetStatus(_ isrc: CGImageSource) -> CGImageSourceStatus
@available(watchOS 2.0, *)
func CGImageSourceGetStatusAtIndex(_ isrc: CGImageSource, _ index: Int) -> CGImageSourceStatus
