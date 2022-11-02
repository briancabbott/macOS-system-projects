
@available(tvOS 6.0, *)
enum NSTextAlignment : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case left
  case center
  case right
  case justified
  case natural
}
@available(tvOS 6.0, *)
func NSTextAlignmentToCTTextAlignment(_ nsTextAlignment: NSTextAlignment) -> CTTextAlignment
@available(tvOS 6.0, *)
func NSTextAlignmentFromCTTextAlignment(_ ctTextAlignment: CTTextAlignment) -> NSTextAlignment
@available(tvOS 6.0, *)
enum NSWritingDirection : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case natural
  case leftToRight
  case rightToLeft
}
