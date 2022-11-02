
@available(iOS 7.0, *)
class MKMapSnapshotOptions : NSObject, NSCopying {
  @NSCopying var camera: MKMapCamera
  var mapRect: MKMapRect
  var region: MKCoordinateRegion
  var mapType: MKMapType
  var showsPointsOfInterest: Bool
  var showsBuildings: Bool
  var size: CGSize
  var scale: CGFloat
  @available(iOS 7.0, *)
  func copy(with zone: NSZone = nil) -> AnyObject
}
