
@available(tvOS 4.0, *)
let NSCalendarIdentifierGregorian: String
@available(tvOS 4.0, *)
let NSCalendarIdentifierBuddhist: String
@available(tvOS 4.0, *)
let NSCalendarIdentifierChinese: String
@available(tvOS 4.0, *)
let NSCalendarIdentifierCoptic: String
@available(tvOS 4.0, *)
let NSCalendarIdentifierEthiopicAmeteMihret: String
@available(tvOS 4.0, *)
let NSCalendarIdentifierEthiopicAmeteAlem: String
@available(tvOS 4.0, *)
let NSCalendarIdentifierHebrew: String
@available(tvOS 4.0, *)
let NSCalendarIdentifierISO8601: String
@available(tvOS 4.0, *)
let NSCalendarIdentifierIndian: String
@available(tvOS 4.0, *)
let NSCalendarIdentifierIslamic: String
@available(tvOS 4.0, *)
let NSCalendarIdentifierIslamicCivil: String
@available(tvOS 4.0, *)
let NSCalendarIdentifierJapanese: String
@available(tvOS 4.0, *)
let NSCalendarIdentifierPersian: String
@available(tvOS 4.0, *)
let NSCalendarIdentifierRepublicOfChina: String
@available(tvOS 8.0, *)
let NSCalendarIdentifierIslamicTabular: String
@available(tvOS 8.0, *)
let NSCalendarIdentifierIslamicUmmAlQura: String
struct NSCalendarUnit : OptionSetType {
  init(rawValue rawValue: UInt)
  let rawValue: UInt
  static var era: NSCalendarUnit { get }
  static var year: NSCalendarUnit { get }
  static var month: NSCalendarUnit { get }
  static var day: NSCalendarUnit { get }
  static var hour: NSCalendarUnit { get }
  static var minute: NSCalendarUnit { get }
  static var second: NSCalendarUnit { get }
  static var weekday: NSCalendarUnit { get }
  static var weekdayOrdinal: NSCalendarUnit { get }
  @available(tvOS 4.0, *)
  static var quarter: NSCalendarUnit { get }
  @available(tvOS 5.0, *)
  static var weekOfMonth: NSCalendarUnit { get }
  @available(tvOS 5.0, *)
  static var weekOfYear: NSCalendarUnit { get }
  @available(tvOS 5.0, *)
  static var yearForWeekOfYear: NSCalendarUnit { get }
  @available(tvOS 5.0, *)
  static var nanosecond: NSCalendarUnit { get }
  @available(tvOS 4.0, *)
  static var calendar: NSCalendarUnit { get }
  @available(tvOS 4.0, *)
  static var timeZone: NSCalendarUnit { get }
  @available(tvOS, introduced=2.0, deprecated=8.0, message="Use NSCalendarUnitEra instead")
  static var NSEraCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=2.0, deprecated=8.0, message="Use NSCalendarUnitYear instead")
  static var NSYearCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=2.0, deprecated=8.0, message="Use NSCalendarUnitMonth instead")
  static var NSMonthCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=2.0, deprecated=8.0, message="Use NSCalendarUnitDay instead")
  static var NSDayCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=2.0, deprecated=8.0, message="Use NSCalendarUnitHour instead")
  static var NSHourCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=2.0, deprecated=8.0, message="Use NSCalendarUnitMinute instead")
  static var NSMinuteCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=2.0, deprecated=8.0, message="Use NSCalendarUnitSecond instead")
  static var NSSecondCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=2.0, deprecated=8.0, message="Use NSCalendarUnitWeekOfMonth or NSCalendarUnitWeekOfYear, depending on which you mean")
  static var NSWeekCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=2.0, deprecated=8.0, message="Use NSCalendarUnitWeekday instead")
  static var NSWeekdayCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=2.0, deprecated=8.0, message="Use NSCalendarUnitWeekdayOrdinal instead")
  static var NSWeekdayOrdinalCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=4.0, deprecated=8.0, message="Use NSCalendarUnitQuarter instead")
  static var NSQuarterCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=5.0, deprecated=8.0, message="Use NSCalendarUnitWeekOfMonth instead")
  static var NSWeekOfMonthCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=5.0, deprecated=8.0, message="Use NSCalendarUnitWeekOfYear instead")
  static var NSWeekOfYearCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=5.0, deprecated=8.0, message="Use NSCalendarUnitYearForWeekOfYear instead")
  static var NSYearForWeekOfYearCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=4.0, deprecated=8.0, message="Use NSCalendarUnitCalendar instead")
  static var NSCalendarCalendarUnit: NSCalendarUnit { get }
  @available(tvOS, introduced=4.0, deprecated=8.0, message="Use NSCalendarUnitTimeZone instead")
  static var NSTimeZoneCalendarUnit: NSCalendarUnit { get }
}
struct NSCalendarOptions : OptionSetType {
  init(rawValue rawValue: UInt)
  let rawValue: UInt
  static var wrapComponents: NSCalendarOptions { get }
  @available(tvOS 7.0, *)
  static var matchStrictly: NSCalendarOptions { get }
  @available(tvOS 7.0, *)
  static var searchBackwards: NSCalendarOptions { get }
  @available(tvOS 7.0, *)
  static var matchPreviousTimePreservingSmallerUnits: NSCalendarOptions { get }
  @available(tvOS 7.0, *)
  static var matchNextTimePreservingSmallerUnits: NSCalendarOptions { get }
  @available(tvOS 7.0, *)
  static var matchNextTime: NSCalendarOptions { get }
  @available(tvOS 7.0, *)
  static var matchFirst: NSCalendarOptions { get }
  @available(tvOS 7.0, *)
  static var matchLast: NSCalendarOptions { get }
}
@available(tvOS, introduced=2.0, deprecated=8.0, message="Use NSCalendarWrapComponents instead")
var NSWrapCalendarComponents: Int { get }
class NSCalendar : NSObject, NSCopying, NSSecureCoding {
  class func current() -> NSCalendar
  @available(tvOS 2.0, *)
  class func autoupdatingCurrent() -> NSCalendar
  @available(tvOS 8.0, *)
  /*not inherited*/ init?(identifier calendarIdentifierConstant: String)
  init?(calendarIdentifier ident: String)
  var calendarIdentifier: String { get }
  @NSCopying var locale: NSLocale?
  @NSCopying var timeZone: NSTimeZone
  var firstWeekday: Int
  var minimumDaysInFirstWeek: Int
  @available(tvOS 5.0, *)
  var eraSymbols: [String] { get }
  @available(tvOS 5.0, *)
  var longEraSymbols: [String] { get }
  @available(tvOS 5.0, *)
  var monthSymbols: [String] { get }
  @available(tvOS 5.0, *)
  var shortMonthSymbols: [String] { get }
  @available(tvOS 5.0, *)
  var veryShortMonthSymbols: [String] { get }
  @available(tvOS 5.0, *)
  var standaloneMonthSymbols: [String] { get }
  @available(tvOS 5.0, *)
  var shortStandaloneMonthSymbols: [String] { get }
  @available(tvOS 5.0, *)
  var veryShortStandaloneMonthSymbols: [String] { get }
  @available(tvOS 5.0, *)
  var weekdaySymbols: [String] { get }
  @available(tvOS 5.0, *)
  var shortWeekdaySymbols: [String] { get }
  @available(tvOS 5.0, *)
  var veryShortWeekdaySymbols: [String] { get }
  @available(tvOS 5.0, *)
  var standaloneWeekdaySymbols: [String] { get }
  @available(tvOS 5.0, *)
  var shortStandaloneWeekdaySymbols: [String] { get }
  @available(tvOS 5.0, *)
  var veryShortStandaloneWeekdaySymbols: [String] { get }
  @available(tvOS 5.0, *)
  var quarterSymbols: [String] { get }
  @available(tvOS 5.0, *)
  var shortQuarterSymbols: [String] { get }
  @available(tvOS 5.0, *)
  var standaloneQuarterSymbols: [String] { get }
  @available(tvOS 5.0, *)
  var shortStandaloneQuarterSymbols: [String] { get }
  @available(tvOS 5.0, *)
  var amSymbol: String { get }
  @available(tvOS 5.0, *)
  var pmSymbol: String { get }
  func minimumRange(of unit: NSCalendarUnit) -> NSRange
  func maximumRange(of unit: NSCalendarUnit) -> NSRange
  func range(of smaller: NSCalendarUnit, in larger: NSCalendarUnit, for date: NSDate) -> NSRange
  func ordinality(of smaller: NSCalendarUnit, in larger: NSCalendarUnit, for date: NSDate) -> Int
  @available(tvOS 2.0, *)
  func range(of unit: NSCalendarUnit, start datep: AutoreleasingUnsafeMutablePointer<NSDate?>, interval tip: UnsafeMutablePointer<NSTimeInterval>, for date: NSDate) -> Bool
  func date(from comps: NSDateComponents) -> NSDate?
  func components(_ unitFlags: NSCalendarUnit, from date: NSDate) -> NSDateComponents
  func date(byAdding comps: NSDateComponents, to date: NSDate, options opts: NSCalendarOptions = []) -> NSDate?
  func components(_ unitFlags: NSCalendarUnit, from startingDate: NSDate, to resultDate: NSDate, options opts: NSCalendarOptions = []) -> NSDateComponents
  @available(tvOS 8.0, *)
  func getEra(_ eraValuePointer: UnsafeMutablePointer<Int>, year yearValuePointer: UnsafeMutablePointer<Int>, month monthValuePointer: UnsafeMutablePointer<Int>, day dayValuePointer: UnsafeMutablePointer<Int>, from date: NSDate)
  @available(tvOS 8.0, *)
  func getEra(_ eraValuePointer: UnsafeMutablePointer<Int>, yearForWeekOfYear yearValuePointer: UnsafeMutablePointer<Int>, weekOfYear weekValuePointer: UnsafeMutablePointer<Int>, weekday weekdayValuePointer: UnsafeMutablePointer<Int>, from date: NSDate)
  @available(tvOS 8.0, *)
  func getHour(_ hourValuePointer: UnsafeMutablePointer<Int>, minute minuteValuePointer: UnsafeMutablePointer<Int>, second secondValuePointer: UnsafeMutablePointer<Int>, nanosecond nanosecondValuePointer: UnsafeMutablePointer<Int>, from date: NSDate)
  @available(tvOS 8.0, *)
  func component(_ unit: NSCalendarUnit, from date: NSDate) -> Int
  @available(tvOS 8.0, *)
  func date(withEra eraValue: Int, year yearValue: Int, month monthValue: Int, day dayValue: Int, hour hourValue: Int, minute minuteValue: Int, second secondValue: Int, nanosecond nanosecondValue: Int) -> NSDate?
  @available(tvOS 8.0, *)
  func date(withEra eraValue: Int, yearForWeekOfYear yearValue: Int, weekOfYear weekValue: Int, weekday weekdayValue: Int, hour hourValue: Int, minute minuteValue: Int, second secondValue: Int, nanosecond nanosecondValue: Int) -> NSDate?
  @available(tvOS 8.0, *)
  func startOfDay(for date: NSDate) -> NSDate
  @available(tvOS 8.0, *)
  func components(in timezone: NSTimeZone, from date: NSDate) -> NSDateComponents
  @available(tvOS 8.0, *)
  func compare(_ date1: NSDate, to date2: NSDate, toUnitGranularity unit: NSCalendarUnit) -> NSComparisonResult
  @available(tvOS 8.0, *)
  func isDate(_ date1: NSDate, equalTo date2: NSDate, toUnitGranularity unit: NSCalendarUnit) -> Bool
  @available(tvOS 8.0, *)
  func isDate(_ date1: NSDate, inSameDayAs date2: NSDate) -> Bool
  @available(tvOS 8.0, *)
  func isDate(inToday date: NSDate) -> Bool
  @available(tvOS 8.0, *)
  func isDate(inYesterday date: NSDate) -> Bool
  @available(tvOS 8.0, *)
  func isDate(inTomorrow date: NSDate) -> Bool
  @available(tvOS 8.0, *)
  func isDate(inWeekend date: NSDate) -> Bool
  @available(tvOS 8.0, *)
  func range(ofWeekendStart datep: AutoreleasingUnsafeMutablePointer<NSDate?>, interval tip: UnsafeMutablePointer<NSTimeInterval>, containing date: NSDate) -> Bool
  @available(tvOS 8.0, *)
  func nextWeekendStart(_ datep: AutoreleasingUnsafeMutablePointer<NSDate?>, interval tip: UnsafeMutablePointer<NSTimeInterval>, options options: NSCalendarOptions = [], after date: NSDate) -> Bool
  @available(tvOS 8.0, *)
  func components(_ unitFlags: NSCalendarUnit, from startingDateComp: NSDateComponents, to resultDateComp: NSDateComponents, options options: NSCalendarOptions = []) -> NSDateComponents
  @available(tvOS 8.0, *)
  func date(byAdding unit: NSCalendarUnit, value value: Int, to date: NSDate, options options: NSCalendarOptions = []) -> NSDate?
  @available(tvOS 8.0, *)
  func enumerateDatesStarting(after start: NSDate, matching comps: NSDateComponents, options opts: NSCalendarOptions = [], using block: (NSDate?, Bool, UnsafeMutablePointer<ObjCBool>) -> Void)
  @available(tvOS 8.0, *)
  func nextDate(after date: NSDate, matching comps: NSDateComponents, options options: NSCalendarOptions = []) -> NSDate?
  @available(tvOS 8.0, *)
  func nextDate(after date: NSDate, matching unit: NSCalendarUnit, value value: Int, options options: NSCalendarOptions = []) -> NSDate?
  @available(tvOS 8.0, *)
  func nextDate(after date: NSDate, matchingHour hourValue: Int, minute minuteValue: Int, second secondValue: Int, options options: NSCalendarOptions = []) -> NSDate?
  @available(tvOS 8.0, *)
  func date(bySettingUnit unit: NSCalendarUnit, value v: Int, of date: NSDate, options opts: NSCalendarOptions = []) -> NSDate?
  @available(tvOS 8.0, *)
  func date(bySettingHour h: Int, minute m: Int, second s: Int, of date: NSDate, options opts: NSCalendarOptions = []) -> NSDate?
  @available(tvOS 8.0, *)
  func date(_ date: NSDate, matchesComponents components: NSDateComponents) -> Bool
  func copy(with zone: NSZone = nil) -> AnyObject
  class func supportsSecureCoding() -> Bool
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
}
@available(tvOS 8.0, *)
let NSCalendarDayChangedNotification: String
var NSDateComponentUndefined: Int { get }
@available(tvOS, introduced=2.0, deprecated=8.0, message="Use NSDateComponentUndefined instead")
var NSUndefinedDateComponent: Int { get }
class NSDateComponents : NSObject, NSCopying, NSSecureCoding {
  @available(tvOS 4.0, *)
  @NSCopying var calendar: NSCalendar?
  @available(tvOS 4.0, *)
  @NSCopying var timeZone: NSTimeZone?
  var era: Int
  var year: Int
  var month: Int
  var day: Int
  var hour: Int
  var minute: Int
  var second: Int
  @available(tvOS 5.0, *)
  var nanosecond: Int
  var weekday: Int
  var weekdayOrdinal: Int
  @available(tvOS 4.0, *)
  var quarter: Int
  @available(tvOS 5.0, *)
  var weekOfMonth: Int
  @available(tvOS 5.0, *)
  var weekOfYear: Int
  @available(tvOS 5.0, *)
  var yearForWeekOfYear: Int
  @available(tvOS 6.0, *)
  var isLeapMonth: Bool
  @available(tvOS 4.0, *)
  @NSCopying var date: NSDate? { get }
  @available(tvOS 8.0, *)
  func setValue(_ value: Int, forComponent unit: NSCalendarUnit)
  @available(tvOS 8.0, *)
  func value(forComponent unit: NSCalendarUnit) -> Int
  @available(tvOS 8.0, *)
  var isValidDate: Bool { get }
  @available(tvOS 8.0, *)
  func isValidDate(in calendar: NSCalendar) -> Bool
  func copy(with zone: NSZone = nil) -> AnyObject
  class func supportsSecureCoding() -> Bool
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
}
