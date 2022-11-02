
@available(OSX 10.10, *)
enum NSLengthFormatterUnit : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case millimeter
  case centimeter
  case meter
  case kilometer
  case inch
  case foot
  case yard
  case mile
}
@available(OSX 10.10, *)
class NSLengthFormatter : NSFormatter {
  @NSCopying var numberFormatter: NSNumberFormatter!
  var unitStyle: NSFormattingUnitStyle
  var isForPersonHeightUse: Bool
  func string(fromValue value: Double, unit unit: NSLengthFormatterUnit) -> String
  func string(fromMeters numberInMeters: Double) -> String
  func unitString(fromValue value: Double, unit unit: NSLengthFormatterUnit) -> String
  func unitString(fromMeters numberInMeters: Double, usedUnit unitp: UnsafeMutablePointer<NSLengthFormatterUnit>) -> String
}
