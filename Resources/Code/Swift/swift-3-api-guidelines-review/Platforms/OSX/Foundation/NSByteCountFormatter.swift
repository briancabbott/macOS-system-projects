
struct NSByteCountFormatterUnits : OptionSetType {
  init(rawValue rawValue: UInt)
  let rawValue: UInt
  static var useDefault: NSByteCountFormatterUnits { get }
  static var useBytes: NSByteCountFormatterUnits { get }
  static var useKB: NSByteCountFormatterUnits { get }
  static var useMB: NSByteCountFormatterUnits { get }
  static var useGB: NSByteCountFormatterUnits { get }
  static var useTB: NSByteCountFormatterUnits { get }
  static var usePB: NSByteCountFormatterUnits { get }
  static var useEB: NSByteCountFormatterUnits { get }
  static var useZB: NSByteCountFormatterUnits { get }
  static var useYBOrHigher: NSByteCountFormatterUnits { get }
  static var useAll: NSByteCountFormatterUnits { get }
}
enum NSByteCountFormatterCountStyle : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case file
  case memory
  case decimal
  case binary
}
@available(OSX 10.8, *)
class NSByteCountFormatter : NSFormatter {
  class func string(fromByteCount byteCount: Int64, countStyle countStyle: NSByteCountFormatterCountStyle) -> String
  func string(fromByteCount byteCount: Int64) -> String
  var allowedUnits: NSByteCountFormatterUnits
  var countStyle: NSByteCountFormatterCountStyle
  var allowsNonnumericFormatting: Bool
  var includesUnit: Bool
  var includesCount: Bool
  var includesActualByteCount: Bool
  var isAdaptive: Bool
  var zeroPadsFractionDigits: Bool
  @available(OSX 10.10, *)
  var formattingContext: NSFormattingContext
}
