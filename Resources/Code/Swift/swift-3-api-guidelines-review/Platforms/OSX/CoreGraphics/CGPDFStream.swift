
typealias CGPDFStreamRef = COpaquePointer
enum CGPDFDataFormat : Int32 {
  init?(rawValue rawValue: Int32)
  var rawValue: Int32 { get }
  case raw
  case jpegEncoded
  case JPEG2000
}
@available(OSX 10.3, *)
func CGPDFStreamGetDictionary(_ stream: CGPDFStreamRef) -> CGPDFDictionaryRef
@available(OSX 10.3, *)
func CGPDFStreamCopyData(_ stream: CGPDFStreamRef, _ format: UnsafeMutablePointer<CGPDFDataFormat>) -> CFData?
