
class NSScreen : NSObject {
  class func screens() -> [NSScreen]?
  class func main() -> NSScreen?
  class func deepest() -> NSScreen?
  @available(OSX 10.9, *)
  class func screensHaveSeparateSpaces() -> Bool
  var depth: NSWindowDepth { get }
  var frame: NSRect { get }
  var visibleFrame: NSRect { get }
  var deviceDescription: [String : AnyObject] { get }
  @available(OSX 10.6, *)
  var colorSpace: NSColorSpace? { get }
  var supportedWindowDepths: UnsafePointer<NSWindowDepth> { get }
  @available(OSX 10.7, *)
  func convertRectToBacking(_ aRect: NSRect) -> NSRect
  @available(OSX 10.7, *)
  func convertRectFromBacking(_ aRect: NSRect) -> NSRect
  @available(OSX 10.7, *)
  func backingAlignedRect(_ aRect: NSRect, options options: NSAlignmentOptions = []) -> NSRect
  @available(OSX 10.7, *)
  var backingScaleFactor: CGFloat { get }
}
@available(OSX 10.6, *)
let NSScreenColorSpaceDidChangeNotification: String
extension NSScreen {
  @available(OSX 10.11, *)
  var maximumExtendedDynamicRangeColorComponentValue: CGFloat { get }
}
extension NSScreen {
}
