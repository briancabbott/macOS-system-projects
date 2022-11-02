
@available(iOS 3.2, *)
let kCTFontAttributeName: CFString
@available(iOS 3.2, *)
let kCTForegroundColorFromContextAttributeName: CFString
@available(iOS 3.2, *)
let kCTKernAttributeName: CFString
@available(iOS 3.2, *)
let kCTLigatureAttributeName: CFString
@available(iOS 3.2, *)
let kCTForegroundColorAttributeName: CFString
@available(iOS 3.2, *)
let kCTParagraphStyleAttributeName: CFString
@available(iOS 3.2, *)
let kCTStrokeWidthAttributeName: CFString
@available(iOS 3.2, *)
let kCTStrokeColorAttributeName: CFString
@available(iOS 3.2, *)
let kCTUnderlineStyleAttributeName: CFString
@available(iOS 3.2, *)
let kCTSuperscriptAttributeName: CFString
@available(iOS 3.2, *)
let kCTUnderlineColorAttributeName: CFString
@available(iOS 4.3, *)
let kCTVerticalFormsAttributeName: CFString
@available(iOS 3.2, *)
let kCTGlyphInfoAttributeName: CFString
@available(iOS, introduced=3.2, deprecated=9.0)
let kCTCharacterShapeAttributeName: CFString
@available(iOS 7.0, *)
let kCTLanguageAttributeName: CFString
@available(iOS 3.2, *)
let kCTRunDelegateAttributeName: CFString
struct CTUnderlineStyle : OptionSetType {
  init(rawValue rawValue: Int32)
  let rawValue: Int32
  static var none: CTUnderlineStyle { get }
  static var single: CTUnderlineStyle { get }
  static var thick: CTUnderlineStyle { get }
  static var double: CTUnderlineStyle { get }
}
struct CTUnderlineStyleModifiers : OptionSetType {
  init(rawValue rawValue: Int32)
  let rawValue: Int32
  static var patternSolid: CTUnderlineStyleModifiers { get }
  static var patternDot: CTUnderlineStyleModifiers { get }
  static var patternDash: CTUnderlineStyleModifiers { get }
  static var patternDashDot: CTUnderlineStyleModifiers { get }
  static var patternDashDotDot: CTUnderlineStyleModifiers { get }
}
@available(iOS 6.0, *)
let kCTBaselineClassAttributeName: CFString
@available(iOS 6.0, *)
let kCTBaselineInfoAttributeName: CFString
@available(iOS 6.0, *)
let kCTBaselineReferenceInfoAttributeName: CFString
@available(iOS 6.0, *)
let kCTWritingDirectionAttributeName: CFString
var kCTWritingDirectionEmbedding: Int { get }
var kCTWritingDirectionOverride: Int { get }
@available(iOS 8.0, *)
let kCTRubyAnnotationAttributeName: CFString
