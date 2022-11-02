
@available(OSX 10.6, *)
class NSOpenGLLayer : CAOpenGLLayer {
  unowned(unsafe) var view: @sil_unmanaged NSView?
  var openGLPixelFormat: NSOpenGLPixelFormat?
  var openGLContext: NSOpenGLContext?
  func openGLPixelFormat(forDisplayMask mask: UInt32) -> NSOpenGLPixelFormat
  func openGLContext(for pixelFormat: NSOpenGLPixelFormat) -> NSOpenGLContext
  func canDraw(in context: NSOpenGLContext, pixelFormat pixelFormat: NSOpenGLPixelFormat, forLayerTime t: CFTimeInterval, displayTime ts: UnsafePointer<CVTimeStamp>) -> Bool
  func draw(in context: NSOpenGLContext, pixelFormat pixelFormat: NSOpenGLPixelFormat, forLayerTime t: CFTimeInterval, displayTime ts: UnsafePointer<CVTimeStamp>)
}
