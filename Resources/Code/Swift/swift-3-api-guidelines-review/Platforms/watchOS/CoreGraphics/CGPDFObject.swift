
typealias CGPDFBoolean = UInt8
typealias CGPDFInteger = Int
typealias CGPDFReal = CGFloat
typealias CGPDFObjectRef = COpaquePointer
enum CGPDFObjectType : Int32 {
  init?(rawValue rawValue: Int32)
  var rawValue: Int32 { get }
  case null
  case boolean
  case integer
  case real
  case name
  case string
  case array
  case dictionary
  case stream
}
@available(watchOS 2.0, *)
func CGPDFObjectGetType(_ object: CGPDFObjectRef) -> CGPDFObjectType
@available(watchOS 2.0, *)
func CGPDFObjectGetValue(_ object: CGPDFObjectRef, _ type: CGPDFObjectType, _ value: UnsafeMutablePointer<Void>) -> Bool
