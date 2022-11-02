
@available(OSX 10.11, *)
protocol MTLBuffer : MTLResource {
  var length: Int { get }
  func contents() -> UnsafeMutablePointer<Void>
  @available(OSX 10.11, *)
  func didModifyRange(_ range: NSRange)
}
