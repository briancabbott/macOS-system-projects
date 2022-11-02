
class CFNumberFormatter {
}
func CFNumberFormatterGetTypeID() -> CFTypeID
enum CFNumberFormatterStyle : CFIndex {
  init?(rawValue rawValue: CFIndex)
  var rawValue: CFIndex { get }
  case noStyle
  case decimalStyle
  case currencyStyle
  case percentStyle
  case scientificStyle
  case spellOutStyle
  @available(watchOS 2.0, *)
  case ordinalStyle
  @available(watchOS 2.0, *)
  case currencyISOCodeStyle
  @available(watchOS 2.0, *)
  case currencyPluralStyle
  @available(watchOS 2.0, *)
  case currencyAccountingStyle
}
func CFNumberFormatterCreate(_ allocator: CFAllocator!, _ locale: CFLocale!, _ style: CFNumberFormatterStyle) -> CFNumberFormatter!
func CFNumberFormatterGetLocale(_ formatter: CFNumberFormatter!) -> CFLocale!
func CFNumberFormatterGetStyle(_ formatter: CFNumberFormatter!) -> CFNumberFormatterStyle
func CFNumberFormatterGetFormat(_ formatter: CFNumberFormatter!) -> CFString!
func CFNumberFormatterSetFormat(_ formatter: CFNumberFormatter!, _ formatString: CFString!)
func CFNumberFormatterCreateStringWithNumber(_ allocator: CFAllocator!, _ formatter: CFNumberFormatter!, _ number: CFNumber!) -> CFString!
func CFNumberFormatterCreateStringWithValue(_ allocator: CFAllocator!, _ formatter: CFNumberFormatter!, _ numberType: CFNumberType, _ valuePtr: UnsafePointer<Void>) -> CFString!
struct CFNumberFormatterOptionFlags : OptionSetType {
  init(rawValue rawValue: CFOptionFlags)
  let rawValue: CFOptionFlags
  static var parseIntegersOnly: CFNumberFormatterOptionFlags { get }
}
func CFNumberFormatterCreateNumberFromString(_ allocator: CFAllocator!, _ formatter: CFNumberFormatter!, _ string: CFString!, _ rangep: UnsafeMutablePointer<CFRange>, _ options: CFOptionFlags) -> CFNumber!
func CFNumberFormatterGetValueFromString(_ formatter: CFNumberFormatter!, _ string: CFString!, _ rangep: UnsafeMutablePointer<CFRange>, _ numberType: CFNumberType, _ valuePtr: UnsafeMutablePointer<Void>) -> Bool
func CFNumberFormatterSetProperty(_ formatter: CFNumberFormatter!, _ key: CFString!, _ value: CFTypeRef!)
func CFNumberFormatterCopyProperty(_ formatter: CFNumberFormatter!, _ key: CFString!) -> CFTypeRef!
let kCFNumberFormatterCurrencyCode: CFString!
let kCFNumberFormatterDecimalSeparator: CFString!
let kCFNumberFormatterCurrencyDecimalSeparator: CFString!
let kCFNumberFormatterAlwaysShowDecimalSeparator: CFString!
let kCFNumberFormatterGroupingSeparator: CFString!
let kCFNumberFormatterUseGroupingSeparator: CFString!
let kCFNumberFormatterPercentSymbol: CFString!
let kCFNumberFormatterZeroSymbol: CFString!
let kCFNumberFormatterNaNSymbol: CFString!
let kCFNumberFormatterInfinitySymbol: CFString!
let kCFNumberFormatterMinusSign: CFString!
let kCFNumberFormatterPlusSign: CFString!
let kCFNumberFormatterCurrencySymbol: CFString!
let kCFNumberFormatterExponentSymbol: CFString!
let kCFNumberFormatterMinIntegerDigits: CFString!
let kCFNumberFormatterMaxIntegerDigits: CFString!
let kCFNumberFormatterMinFractionDigits: CFString!
let kCFNumberFormatterMaxFractionDigits: CFString!
let kCFNumberFormatterGroupingSize: CFString!
let kCFNumberFormatterSecondaryGroupingSize: CFString!
let kCFNumberFormatterRoundingMode: CFString!
let kCFNumberFormatterRoundingIncrement: CFString!
let kCFNumberFormatterFormatWidth: CFString!
let kCFNumberFormatterPaddingPosition: CFString!
let kCFNumberFormatterPaddingCharacter: CFString!
let kCFNumberFormatterDefaultFormat: CFString!
let kCFNumberFormatterMultiplier: CFString!
let kCFNumberFormatterPositivePrefix: CFString!
let kCFNumberFormatterPositiveSuffix: CFString!
let kCFNumberFormatterNegativePrefix: CFString!
let kCFNumberFormatterNegativeSuffix: CFString!
let kCFNumberFormatterPerMillSymbol: CFString!
let kCFNumberFormatterInternationalCurrencySymbol: CFString!
@available(watchOS 2.0, *)
let kCFNumberFormatterCurrencyGroupingSeparator: CFString!
@available(watchOS 2.0, *)
let kCFNumberFormatterIsLenient: CFString!
@available(watchOS 2.0, *)
let kCFNumberFormatterUseSignificantDigits: CFString!
@available(watchOS 2.0, *)
let kCFNumberFormatterMinSignificantDigits: CFString!
@available(watchOS 2.0, *)
let kCFNumberFormatterMaxSignificantDigits: CFString!
enum CFNumberFormatterRoundingMode : CFIndex {
  init?(rawValue rawValue: CFIndex)
  var rawValue: CFIndex { get }
  case roundCeiling
  case roundFloor
  case roundDown
  case roundUp
  case roundHalfEven
  case roundHalfDown
  case roundHalfUp
}
enum CFNumberFormatterPadPosition : CFIndex {
  init?(rawValue rawValue: CFIndex)
  var rawValue: CFIndex { get }
  case beforePrefix
  case afterPrefix
  case beforeSuffix
  case afterSuffix
}
func CFNumberFormatterGetDecimalInfoForCurrencyCode(_ currencyCode: CFString!, _ defaultFractionDigits: UnsafeMutablePointer<Int32>, _ roundingIncrement: UnsafeMutablePointer<Double>) -> Bool
