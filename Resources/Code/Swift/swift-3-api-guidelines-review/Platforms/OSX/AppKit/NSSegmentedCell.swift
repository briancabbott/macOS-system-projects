
class NSSegmentedCell : NSActionCell {
  var segmentCount: Int
  var selectedSegment: Int
  func selectSegment(withTag tag: Int) -> Bool
  func makeNextSegmentKey()
  func makePreviousSegmentKey()
  var trackingMode: NSSegmentSwitchTracking
  func setWidth(_ width: CGFloat, forSegment segment: Int)
  func width(forSegment segment: Int) -> CGFloat
  func setImage(_ image: NSImage?, forSegment segment: Int)
  func image(forSegment segment: Int) -> NSImage?
  @available(OSX 10.5, *)
  func setImageScaling(_ scaling: NSImageScaling, forSegment segment: Int)
  @available(OSX 10.5, *)
  func imageScaling(forSegment segment: Int) -> NSImageScaling
  func setLabel(_ label: String, forSegment segment: Int)
  func label(forSegment segment: Int) -> String?
  func setSelected(_ selected: Bool, forSegment segment: Int)
  func isSelected(forSegment segment: Int) -> Bool
  func setEnabled(_ enabled: Bool, forSegment segment: Int)
  func isEnabled(forSegment segment: Int) -> Bool
  func setMenu(_ menu: NSMenu?, forSegment segment: Int)
  func menu(forSegment segment: Int) -> NSMenu?
  func setToolTip(_ toolTip: String?, forSegment segment: Int)
  func toolTip(forSegment segment: Int) -> String?
  func setTag(_ tag: Int, forSegment segment: Int)
  func tag(forSegment segment: Int) -> Int
  @available(OSX 10.5, *)
  var segmentStyle: NSSegmentStyle
  func drawSegment(_ segment: Int, inFrame frame: NSRect, with controlView: NSView)
}
extension NSSegmentedCell {
  @available(OSX 10.5, *)
  func interiorBackgroundStyle(forSegment segment: Int) -> NSBackgroundStyle
}
