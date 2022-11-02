
@available(watchOS 2.0, *)
class EKStructuredLocation : EKObject, NSCopying {
  convenience init(title title: String)
  var title: String
  var geoLocation: CLLocation?
  var radius: Double
  @available(watchOS 2.0, *)
  func copy(with zone: NSZone = nil) -> AnyObject
}
