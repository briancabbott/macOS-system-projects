
@available(OSX 10.6, *)
let kCTFontManagerErrorDomain: CFString
@available(OSX 10.6, *)
let kCTFontManagerErrorFontURLsKey: CFString
enum CTFontManagerError : CFIndex {
  init?(rawValue rawValue: CFIndex)
  var rawValue: CFIndex { get }
  case fileNotFound
  case insufficientPermissions
  case unrecognizedFormat
  case invalidFontData
  case alreadyRegistered
  case notRegistered
  case inUse
  case systemRequired
}
