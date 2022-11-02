
@available(iOS 4.0, *)
class EKRecurrenceEnd : NSObject, NSCopying {
  convenience init(end endDate: NSDate)
  convenience init(occurrenceCount occurrenceCount: Int)
  var endDate: NSDate? { get }
  var occurrenceCount: Int { get }
  @available(iOS 4.0, *)
  func copy(with zone: NSZone = nil) -> AnyObject
}
