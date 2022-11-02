
@available(iOS 7.0, *)
class MKMapCamera : NSObject, NSSecureCoding, NSCopying {
  var centerCoordinate: CLLocationCoordinate2D
  var heading: CLLocationDirection
  var pitch: CGFloat
  var altitude: CLLocationDistance
  convenience init(lookingAtCenter centerCoordinate: CLLocationCoordinate2D, fromEyeCoordinate eyeCoordinate: CLLocationCoordinate2D, eyeAltitude eyeAltitude: CLLocationDistance)
  @available(iOS 9.0, *)
  convenience init(lookingAtCenter centerCoordinate: CLLocationCoordinate2D, fromDistance distance: CLLocationDistance, pitch pitch: CGFloat, heading heading: CLLocationDirection)
  @available(iOS 7.0, *)
  class func supportsSecureCoding() -> Bool
  @available(iOS 7.0, *)
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
  @available(iOS 7.0, *)
  func copy(with zone: NSZone = nil) -> AnyObject
}
