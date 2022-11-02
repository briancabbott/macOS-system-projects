
class CTParagraphStyle {
}
@available(tvOS 3.2, *)
func CTParagraphStyleGetTypeID() -> CFTypeID
enum CTTextAlignment : UInt8 {
  init?(rawValue rawValue: UInt8)
  var rawValue: UInt8 { get }
  @available(tvOS 6.0, *)
  case left
  @available(tvOS 6.0, *)
  case right
  @available(tvOS 6.0, *)
  case center
  @available(tvOS 6.0, *)
  case justified
  @available(tvOS 6.0, *)
  case natural
  @available(tvOS, introduced=3.2, deprecated=9.0)
  static var kCTLeftTextAlignment: CTTextAlignment { get }
  @available(tvOS, introduced=3.2, deprecated=9.0)
  static var kCTRightTextAlignment: CTTextAlignment { get }
  @available(tvOS, introduced=3.2, deprecated=9.0)
  static var kCTCenterTextAlignment: CTTextAlignment { get }
  @available(tvOS, introduced=3.2, deprecated=9.0)
  static var kCTJustifiedTextAlignment: CTTextAlignment { get }
  @available(tvOS, introduced=3.2, deprecated=9.0)
  static var kCTNaturalTextAlignment: CTTextAlignment { get }
}
enum CTLineBreakMode : UInt8 {
  init?(rawValue rawValue: UInt8)
  var rawValue: UInt8 { get }
  case byWordWrapping
  case byCharWrapping
  case byClipping
  case byTruncatingHead
  case byTruncatingTail
  case byTruncatingMiddle
}
enum CTWritingDirection : Int8 {
  init?(rawValue rawValue: Int8)
  var rawValue: Int8 { get }
  case natural
  case leftToRight
  case rightToLeft
}
enum CTParagraphStyleSpecifier : UInt32 {
  init?(rawValue rawValue: UInt32)
  var rawValue: UInt32 { get }
  case alignment
  case firstLineHeadIndent
  case headIndent
  case tailIndent
  case tabStops
  case defaultTabInterval
  case lineBreakMode
  case lineHeightMultiple
  case maximumLineHeight
  case minimumLineHeight
  case lineSpacing
  case paragraphSpacing
  case paragraphSpacingBefore
  case baseWritingDirection
  case maximumLineSpacing
  case minimumLineSpacing
  case lineSpacingAdjustment
  case lineBoundsOptions
  case count
}
struct CTParagraphStyleSetting {
  var spec: CTParagraphStyleSpecifier
  var valueSize: Int
  var value: UnsafePointer<Void>
}
@available(tvOS 3.2, *)
func CTParagraphStyleCreate(_ settings: UnsafePointer<CTParagraphStyleSetting>, _ settingCount: Int) -> CTParagraphStyle
@available(tvOS 3.2, *)
func CTParagraphStyleCreateCopy(_ paragraphStyle: CTParagraphStyle) -> CTParagraphStyle
@available(tvOS 3.2, *)
func CTParagraphStyleGetValueForSpecifier(_ paragraphStyle: CTParagraphStyle, _ spec: CTParagraphStyleSpecifier, _ valueBufferSize: Int, _ valueBuffer: UnsafeMutablePointer<Void>) -> Bool
