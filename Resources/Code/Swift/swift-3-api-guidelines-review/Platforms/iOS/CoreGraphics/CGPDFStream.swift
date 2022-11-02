
typealias CGPDFStreamRef = COpaquePointer
enum CGPDFDataFormat : Int32 {
  init?(rawValue rawValue: Int32)
  var rawValue: Int32 { get }
  case raw
  case jpegEncoded
  case JPEG2000
}
@available(iOS 2.0, *)
func CGPDFStreamGetDictionary(_ stream: CGPDFStreamRef) -> CGPDFDictionaryRef
@available(iOS 2.0, *)
func CGPDFStreamCopyData(_ stream: CGPDFStreamRef, _ format: UnsafeMutablePointer<CGPDFDataFormat>) -> CFData?
