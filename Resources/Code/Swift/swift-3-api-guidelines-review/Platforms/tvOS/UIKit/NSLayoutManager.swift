
@available(tvOS 7.0, *)
enum NSTextLayoutOrientation : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case horizontal
  case vertical
}
@available(tvOS 7.0, *)
enum NSGlyphProperty : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case null
  case controlCharacter
  case elastic
  case nonBaseCharacter
}
@available(tvOS 7.0, *)
enum NSControlCharacterAction : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case zeroAdvancement
  case whitespace
  case horizontalTab
  case lineBreak
  case paragraphBreak
  case containerBreak
}
protocol NSTextLayoutOrientationProvider {
  @available(tvOS 7.0, *)
  var layoutOrientation: NSTextLayoutOrientation { get }
}
@available(tvOS 7.0, *)
class NSLayoutManager : NSObject, NSCoding {
  init?(coder coder: NSCoder)
  unowned(unsafe) var textStorage: @sil_unmanaged NSTextStorage?
  var textContainers: [NSTextContainer] { get }
  func addTextContainer(_ container: NSTextContainer)
  func insertTextContainer(_ container: NSTextContainer, at index: Int)
  func removeTextContainer(at index: Int)
  func textContainerChangedGeometry(_ container: NSTextContainer)
  unowned(unsafe) var delegate: @sil_unmanaged NSLayoutManagerDelegate?
  var showsInvisibleCharacters: Bool
  var showsControlCharacters: Bool
  var hyphenationFactor: CGFloat
  var usesFontLeading: Bool
  @available(tvOS 7.0, *)
  var allowsNonContiguousLayout: Bool
  @available(tvOS 7.0, *)
  var hasNonContiguousLayout: Bool { get }
  func invalidateGlyphs(forCharacterRange charRange: NSRange, changeInLength delta: Int, actualCharacterRange actualCharRange: NSRangePointer)
  @available(tvOS 7.0, *)
  func invalidateLayout(forCharacterRange charRange: NSRange, actualCharacterRange actualCharRange: NSRangePointer)
  func invalidateDisplay(forCharacterRange charRange: NSRange)
  func invalidateDisplay(forGlyphRange glyphRange: NSRange)
  @available(tvOS 7.0, *)
  func processEditing(for textStorage: NSTextStorage, edited editMask: NSTextStorageEditActions, range newCharRange: NSRange, changeInLength delta: Int, invalidatedRange invalidatedCharRange: NSRange)
  func ensureGlyphs(forCharacterRange charRange: NSRange)
  func ensureGlyphs(forGlyphRange glyphRange: NSRange)
  func ensureLayout(forCharacterRange charRange: NSRange)
  func ensureLayout(forGlyphRange glyphRange: NSRange)
  func ensureLayout(for container: NSTextContainer)
  func ensureLayout(forBoundingRect bounds: CGRect, in container: NSTextContainer)
  @available(tvOS 7.0, *)
  func setGlyphs(_ glyphs: UnsafePointer<CGGlyph>, properties props: UnsafePointer<NSGlyphProperty>, characterIndexes charIndexes: UnsafePointer<Int>, font aFont: UIFont, forGlyphRange glyphRange: NSRange)
  var numberOfGlyphs: Int { get }
  func cgGlyph(at glyphIndex: Int, isValidIndex isValidIndex: UnsafeMutablePointer<ObjCBool>) -> CGGlyph
  func cgGlyph(at glyphIndex: Int) -> CGGlyph
  func isValidGlyphIndex(_ glyphIndex: Int) -> Bool
  @available(tvOS 7.0, *)
  func propertyForGlyph(at glyphIndex: Int) -> NSGlyphProperty
  func characterIndexForGlyph(at glyphIndex: Int) -> Int
  func glyphIndexForCharacter(at charIndex: Int) -> Int
  @available(tvOS 7.0, *)
  func getGlyphsIn(_ glyphRange: NSRange, glyphs glyphBuffer: UnsafeMutablePointer<CGGlyph>, properties props: UnsafeMutablePointer<NSGlyphProperty>, characterIndexes charIndexBuffer: UnsafeMutablePointer<Int>, bidiLevels bidiLevelBuffer: UnsafeMutablePointer<UInt8>) -> Int
  func setTextContainer(_ container: NSTextContainer, forGlyphRange glyphRange: NSRange)
  func setLineFragmentRect(_ fragmentRect: CGRect, forGlyphRange glyphRange: NSRange, usedRect usedRect: CGRect)
  func setExtraLineFragmentRect(_ fragmentRect: CGRect, usedRect usedRect: CGRect, textContainer container: NSTextContainer)
  func setLocation(_ location: CGPoint, forStartOfGlyphRange glyphRange: NSRange)
  func setNotShownAttribute(_ flag: Bool, forGlyphAt glyphIndex: Int)
  func setDrawsOutsideLineFragment(_ flag: Bool, forGlyphAt glyphIndex: Int)
  func setAttachmentSize(_ attachmentSize: CGSize, forGlyphRange glyphRange: NSRange)
  func getFirstUnlaidCharacterIndex(_ charIndex: UnsafeMutablePointer<Int>, glyphIndex glyphIndex: UnsafeMutablePointer<Int>)
  func firstUnlaidCharacterIndex() -> Int
  func firstUnlaidGlyphIndex() -> Int
  func textContainerForGlyph(at glyphIndex: Int, effectiveRange effectiveGlyphRange: NSRangePointer) -> NSTextContainer?
  @available(tvOS 9.0, *)
  func textContainerForGlyph(at glyphIndex: Int, effectiveRange effectiveGlyphRange: NSRangePointer, withoutAdditionalLayout flag: Bool) -> NSTextContainer?
  func usedRect(for container: NSTextContainer) -> CGRect
  func lineFragmentRectForGlyph(at glyphIndex: Int, effectiveRange effectiveGlyphRange: NSRangePointer) -> CGRect
  @available(tvOS 9.0, *)
  func lineFragmentRectForGlyph(at glyphIndex: Int, effectiveRange effectiveGlyphRange: NSRangePointer, withoutAdditionalLayout flag: Bool) -> CGRect
  func lineFragmentUsedRectForGlyph(at glyphIndex: Int, effectiveRange effectiveGlyphRange: NSRangePointer) -> CGRect
  @available(tvOS 9.0, *)
  func lineFragmentUsedRectForGlyph(at glyphIndex: Int, effectiveRange effectiveGlyphRange: NSRangePointer, withoutAdditionalLayout flag: Bool) -> CGRect
  var extraLineFragmentRect: CGRect { get }
  var extraLineFragmentUsedRect: CGRect { get }
  var extraLineFragmentTextContainer: NSTextContainer? { get }
  func locationForGlyph(at glyphIndex: Int) -> CGPoint
  func notShownAttributeForGlyph(at glyphIndex: Int) -> Bool
  func drawsOutsideLineFragmentForGlyph(at glyphIndex: Int) -> Bool
  func attachmentSizeForGlyph(at glyphIndex: Int) -> CGSize
  @available(tvOS 7.0, *)
  func truncatedGlyphRangeInLineFragmentForGlyph(at glyphIndex: Int) -> NSRange
  func glyphRange(forCharacterRange charRange: NSRange, actualCharacterRange actualCharRange: NSRangePointer) -> NSRange
  func characterRange(forGlyphRange glyphRange: NSRange, actualGlyphRange actualGlyphRange: NSRangePointer) -> NSRange
  func glyphRange(for container: NSTextContainer) -> NSRange
  func range(ofNominallySpacedGlyphsContaining glyphIndex: Int) -> NSRange
  func boundingRect(forGlyphRange glyphRange: NSRange, in container: NSTextContainer) -> CGRect
  func glyphRange(forBoundingRect bounds: CGRect, in container: NSTextContainer) -> NSRange
  func glyphRange(forBoundingRectWithoutAdditionalLayout bounds: CGRect, in container: NSTextContainer) -> NSRange
  func glyphIndex(for point: CGPoint, in container: NSTextContainer, fractionOfDistanceThroughGlyph partialFraction: UnsafeMutablePointer<CGFloat>) -> Int
  func glyphIndex(for point: CGPoint, in container: NSTextContainer) -> Int
  func fractionOfDistanceThroughGlyph(for point: CGPoint, in container: NSTextContainer) -> CGFloat
  func characterIndex(for point: CGPoint, in container: NSTextContainer, fractionOfDistanceBetweenInsertionPoints partialFraction: UnsafeMutablePointer<CGFloat>) -> Int
  func getLineFragmentInsertionPointsForCharacter(at charIndex: Int, alternatePositions aFlag: Bool, inDisplayOrder dFlag: Bool, positions positions: UnsafeMutablePointer<CGFloat>, characterIndexes charIndexes: UnsafeMutablePointer<Int>) -> Int
  @available(tvOS 7.0, *)
  func enumerateLineFragments(forGlyphRange glyphRange: NSRange, using block: (CGRect, CGRect, NSTextContainer, NSRange, UnsafeMutablePointer<ObjCBool>) -> Void)
  @available(tvOS 7.0, *)
  func enumerateEnclosingRects(forGlyphRange glyphRange: NSRange, withinSelectedGlyphRange selectedRange: NSRange, in textContainer: NSTextContainer, using block: (CGRect, UnsafeMutablePointer<ObjCBool>) -> Void)
  func drawBackground(forGlyphRange glyphsToShow: NSRange, at origin: CGPoint)
  func drawGlyphs(forGlyphRange glyphsToShow: NSRange, at origin: CGPoint)
  @available(tvOS 7.0, *)
  func showCGGlyphs(_ glyphs: UnsafePointer<CGGlyph>, positions positions: UnsafePointer<CGPoint>, count glyphCount: Int, font font: UIFont, matrix textMatrix: CGAffineTransform, attributes attributes: [String : AnyObject] = [:], in graphicsContext: CGContext)
  @available(tvOS 7.0, *)
  func fillBackgroundRectArray(_ rectArray: UnsafePointer<CGRect>, count rectCount: Int, forCharacterRange charRange: NSRange, color color: UIColor)
  func drawUnderline(forGlyphRange glyphRange: NSRange, underlineType underlineVal: NSUnderlineStyle, baselineOffset baselineOffset: CGFloat, lineFragmentRect lineRect: CGRect, lineFragmentGlyphRange lineGlyphRange: NSRange, containerOrigin containerOrigin: CGPoint)
  func underlineGlyphRange(_ glyphRange: NSRange, underlineType underlineVal: NSUnderlineStyle, lineFragmentRect lineRect: CGRect, lineFragmentGlyphRange lineGlyphRange: NSRange, containerOrigin containerOrigin: CGPoint)
  func drawStrikethrough(forGlyphRange glyphRange: NSRange, strikethroughType strikethroughVal: NSUnderlineStyle, baselineOffset baselineOffset: CGFloat, lineFragmentRect lineRect: CGRect, lineFragmentGlyphRange lineGlyphRange: NSRange, containerOrigin containerOrigin: CGPoint)
  func strikethroughGlyphRange(_ glyphRange: NSRange, strikethroughType strikethroughVal: NSUnderlineStyle, lineFragmentRect lineRect: CGRect, lineFragmentGlyphRange lineGlyphRange: NSRange, containerOrigin containerOrigin: CGPoint)
  @available(tvOS 7.0, *)
  func encode(with aCoder: NSCoder)
}
protocol NSLayoutManagerDelegate : NSObjectProtocol {
  @available(tvOS 7.0, *)
  optional func layoutManager(_ layoutManager: NSLayoutManager, shouldGenerateGlyphs glyphs: UnsafePointer<CGGlyph>, properties props: UnsafePointer<NSGlyphProperty>, characterIndexes charIndexes: UnsafePointer<Int>, font aFont: UIFont, forGlyphRange glyphRange: NSRange) -> Int
  @available(tvOS 7.0, *)
  optional func layoutManager(_ layoutManager: NSLayoutManager, lineSpacingAfterGlyphAt glyphIndex: Int, withProposedLineFragmentRect rect: CGRect) -> CGFloat
  @available(tvOS 7.0, *)
  optional func layoutManager(_ layoutManager: NSLayoutManager, paragraphSpacingBeforeGlyphAt glyphIndex: Int, withProposedLineFragmentRect rect: CGRect) -> CGFloat
  @available(tvOS 7.0, *)
  optional func layoutManager(_ layoutManager: NSLayoutManager, paragraphSpacingAfterGlyphAt glyphIndex: Int, withProposedLineFragmentRect rect: CGRect) -> CGFloat
  @available(tvOS 7.0, *)
  optional func layoutManager(_ layoutManager: NSLayoutManager, shouldUse action: NSControlCharacterAction, forControlCharacterAt charIndex: Int) -> NSControlCharacterAction
  @available(tvOS 7.0, *)
  optional func layoutManager(_ layoutManager: NSLayoutManager, shouldBreakLineByWordBeforeCharacterAt charIndex: Int) -> Bool
  @available(tvOS 7.0, *)
  optional func layoutManager(_ layoutManager: NSLayoutManager, shouldBreakLineByHyphenatingBeforeCharacterAt charIndex: Int) -> Bool
  @available(tvOS 7.0, *)
  optional func layoutManager(_ layoutManager: NSLayoutManager, boundingBoxForControlGlyphAt glyphIndex: Int, for textContainer: NSTextContainer, proposedLineFragment proposedRect: CGRect, glyphPosition glyphPosition: CGPoint, characterIndex charIndex: Int) -> CGRect
  @available(tvOS 9.0, *)
  optional func layoutManager(_ layoutManager: NSLayoutManager, shouldSetLineFragmentRect lineFragmentRect: UnsafeMutablePointer<CGRect>, lineFragmentUsedRect lineFragmentUsedRect: UnsafeMutablePointer<CGRect>, baselineOffset baselineOffset: UnsafeMutablePointer<CGFloat>, in textContainer: NSTextContainer, forGlyphRange glyphRange: NSRange) -> Bool
  @available(tvOS 7.0, *)
  optional func layoutManagerDidInvalidateLayout(_ sender: NSLayoutManager)
  @available(tvOS 7.0, *)
  optional func layoutManager(_ layoutManager: NSLayoutManager, didCompleteLayoutFor textContainer: NSTextContainer?, atEnd layoutFinishedFlag: Bool)
  @available(tvOS 7.0, *)
  optional func layoutManager(_ layoutManager: NSLayoutManager, textContainer textContainer: NSTextContainer, didChangeGeometryFrom oldSize: CGSize)
}
@available(tvOS, introduced=7.0, deprecated=9.0, message="Use NSControlCharacterActionZeroAdvancement instead")
var NSControlCharacterZeroAdvancementAction: Int { get }
@available(tvOS, introduced=7.0, deprecated=9.0, message="Use NSControlCharacterActionWhitespace instead")
var NSControlCharacterWhitespaceAction: Int { get }
@available(tvOS, introduced=7.0, deprecated=9.0, message="Use NSControlCharacterActionHorizontalTab instead")
var NSControlCharacterHorizontalTabAction: Int { get }
@available(tvOS, introduced=7.0, deprecated=9.0, message="Use NSControlCharacterActionLineBreak instead")
var NSControlCharacterLineBreakAction: Int { get }
@available(tvOS, introduced=7.0, deprecated=9.0, message="Use NSControlCharacterActionParagraphBreak instead")
var NSControlCharacterParagraphBreakAction: Int { get }
@available(tvOS, introduced=7.0, deprecated=9.0, message="Use NSControlCharacterActionContainerBreak instead")
var NSControlCharacterContainerBreakAction: Int { get }
extension NSLayoutManager {
  func glyph(at glyphIndex: Int, isValidIndex isValidIndex: UnsafeMutablePointer<ObjCBool>) -> CGGlyph
  func glyph(at glyphIndex: Int) -> CGGlyph
}
