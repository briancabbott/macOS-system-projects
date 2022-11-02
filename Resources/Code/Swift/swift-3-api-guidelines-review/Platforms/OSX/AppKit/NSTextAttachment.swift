
@available(OSX 10.0, *)
var NSAttachmentCharacter: Int { get }
protocol NSTextAttachmentContainer : NSObjectProtocol {
  @available(OSX 10.11, *)
  func image(forBounds imageBounds: NSRect, textContainer textContainer: NSTextContainer?, characterIndex charIndex: Int) -> NSImage?
  @available(OSX 10.11, *)
  func attachmentBounds(for textContainer: NSTextContainer?, proposedLineFragment lineFrag: NSRect, glyphPosition position: CGPoint, characterIndex charIndex: Int) -> NSRect
}
@available(OSX 10.0, *)
class NSTextAttachment : NSObject, NSTextAttachmentContainer, NSCoding {
  @available(OSX 10.11, *)
  init(data contentData: NSData?, ofType uti: String?)
  convenience init(fileWrapper fileWrapper: NSFileWrapper?)
  @available(OSX 10.11, *)
  @NSCopying var contents: NSData?
  @available(OSX 10.11, *)
  var fileType: String?
  @available(OSX 10.11, *)
  var image: NSImage?
  @available(OSX 10.11, *)
  var bounds: NSRect
  var fileWrapper: NSFileWrapper?
  var attachmentCell: NSTextAttachmentCellProtocol?
  @available(OSX 10.11, *)
  func image(forBounds imageBounds: NSRect, textContainer textContainer: NSTextContainer?, characterIndex charIndex: Int) -> NSImage?
  @available(OSX 10.11, *)
  func attachmentBounds(for textContainer: NSTextContainer?, proposedLineFragment lineFrag: NSRect, glyphPosition position: CGPoint, characterIndex charIndex: Int) -> NSRect
  @available(OSX 10.0, *)
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
}
extension NSAttributedString {
  @available(OSX 10.0, *)
  /*not inherited*/ init(attachment attachment: NSTextAttachment)
}
extension NSMutableAttributedString {
  func updateAttachments(fromPath path: String)
}
protocol NSTextAttachmentCellProtocol : NSObjectProtocol {
  func draw(withFrame cellFrame: NSRect, in controlView: NSView?)
  func wantsToTrackMouse() -> Bool
  func highlight(_ flag: Bool, withFrame cellFrame: NSRect, in controlView: NSView?)
  func trackMouse(_ theEvent: NSEvent, in cellFrame: NSRect, of controlView: NSView?, untilMouseUp flag: Bool) -> Bool
  func cellSize() -> NSSize
  func cellBaselineOffset() -> NSPoint
  @available(OSX 10.0, *)
  unowned(unsafe) var attachment: @sil_unmanaged NSTextAttachment? { get set }
  func draw(withFrame cellFrame: NSRect, in controlView: NSView?, characterIndex charIndex: Int)
  @available(OSX 10.0, *)
  func draw(withFrame cellFrame: NSRect, in controlView: NSView?, characterIndex charIndex: Int, layoutManager layoutManager: NSLayoutManager)
  func wantsToTrackMouse(for theEvent: NSEvent, in cellFrame: NSRect, of controlView: NSView?, atCharacterIndex charIndex: Int) -> Bool
  func trackMouse(_ theEvent: NSEvent, in cellFrame: NSRect, of controlView: NSView?, atCharacterIndex charIndex: Int, untilMouseUp flag: Bool) -> Bool
  @available(OSX 10.0, *)
  func cellFrame(for textContainer: NSTextContainer, proposedLineFragment lineFrag: NSRect, glyphPosition position: NSPoint, characterIndex charIndex: Int) -> NSRect
}
class NSTextAttachmentCell : NSCell, NSTextAttachmentCellProtocol {
  func wantsToTrackMouse() -> Bool
  func cellSize() -> NSSize
  func cellBaselineOffset() -> NSPoint
  @available(OSX 10.0, *)
  unowned(unsafe) var attachment: @sil_unmanaged NSTextAttachment?
  func draw(withFrame cellFrame: NSRect, in controlView: NSView?, characterIndex charIndex: Int)
  @available(OSX 10.0, *)
  func draw(withFrame cellFrame: NSRect, in controlView: NSView?, characterIndex charIndex: Int, layoutManager layoutManager: NSLayoutManager)
  func wantsToTrackMouse(for theEvent: NSEvent, in cellFrame: NSRect, of controlView: NSView?, atCharacterIndex charIndex: Int) -> Bool
  func trackMouse(_ theEvent: NSEvent, in cellFrame: NSRect, of controlView: NSView?, atCharacterIndex charIndex: Int, untilMouseUp flag: Bool) -> Bool
  @available(OSX 10.0, *)
  func cellFrame(for textContainer: NSTextContainer, proposedLineFragment lineFrag: NSRect, glyphPosition position: NSPoint, characterIndex charIndex: Int) -> NSRect
}
