
class NSDateFormatter : NSFormatter {
  @available(iOS 8.0, *)
  var formattingContext: NSFormattingContext
  func getObjectValue(_ obj: AutoreleasingUnsafeMutablePointer<AnyObject?>, for string: String, range rangep: UnsafeMutablePointer<NSRange>) throws
  func string(from date: NSDate) -> String
  func date(from string: String) -> NSDate?
  @available(iOS 4.0, *)
  class func localizedString(from date: NSDate, dateStyle dstyle: NSDateFormatterStyle, time tstyle: NSDateFormatterStyle) -> String
  @available(iOS 4.0, *)
  class func dateFormat(fromTemplate tmplate: String, options opts: Int, locale locale: NSLocale?) -> String?
  class func defaultFormatterBehavior() -> NSDateFormatterBehavior
  class func setDefaultFormatterBehavior(_ behavior: NSDateFormatterBehavior)
  @available(iOS 8.0, *)
  func setLocalizedDateFormatFromTemplate(_ dateFormatTemplate: String)
  var dateFormat: String!
  var dateStyle: NSDateFormatterStyle
  var timeStyle: NSDateFormatterStyle
  @NSCopying var locale: NSLocale!
  var generatesCalendarDates: Bool
  var formatterBehavior: NSDateFormatterBehavior
  @NSCopying var timeZone: NSTimeZone!
  @NSCopying var calendar: NSCalendar!
  var isLenient: Bool
  @NSCopying var twoDigitStartDate: NSDate?
  @NSCopying var defaultDate: NSDate?
  var eraSymbols: [String]!
  var monthSymbols: [String]!
  var shortMonthSymbols: [String]!
  var weekdaySymbols: [String]!
  var shortWeekdaySymbols: [String]!
  var amSymbol: String!
  var pmSymbol: String!
  @available(iOS 2.0, *)
  var longEraSymbols: [String]!
  @available(iOS 2.0, *)
  var veryShortMonthSymbols: [String]!
  @available(iOS 2.0, *)
  var standaloneMonthSymbols: [String]!
  @available(iOS 2.0, *)
  var shortStandaloneMonthSymbols: [String]!
  @available(iOS 2.0, *)
  var veryShortStandaloneMonthSymbols: [String]!
  @available(iOS 2.0, *)
  var veryShortWeekdaySymbols: [String]!
  @available(iOS 2.0, *)
  var standaloneWeekdaySymbols: [String]!
  @available(iOS 2.0, *)
  var shortStandaloneWeekdaySymbols: [String]!
  @available(iOS 2.0, *)
  var veryShortStandaloneWeekdaySymbols: [String]!
  @available(iOS 2.0, *)
  var quarterSymbols: [String]!
  @available(iOS 2.0, *)
  var shortQuarterSymbols: [String]!
  @available(iOS 2.0, *)
  var standaloneQuarterSymbols: [String]!
  @available(iOS 2.0, *)
  var shortStandaloneQuarterSymbols: [String]!
  @available(iOS 2.0, *)
  @NSCopying var gregorianStartDate: NSDate?
  @available(iOS 4.0, *)
  var doesRelativeDateFormatting: Bool
}
enum NSDateFormatterStyle : UInt {
  init?(rawValue rawValue: UInt)
  var rawValue: UInt { get }
  case noStyle
  case shortStyle
  case mediumStyle
  case longStyle
  case fullStyle
}
enum NSDateFormatterBehavior : UInt {
  init?(rawValue rawValue: UInt)
  var rawValue: UInt { get }
  case behaviorDefault
  case behavior10_4
}
