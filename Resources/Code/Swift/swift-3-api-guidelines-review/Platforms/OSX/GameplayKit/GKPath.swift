
@available(OSX 10.11, *)
class GKPath : NSObject {
  var radius: Float
  var isCyclical: Bool
  var numPoints: Int { get }
  init(points points: UnsafeMutablePointer<vector_float2>, count count: Int, radius radius: Float, cyclical cyclical: Bool)
  convenience init(graphNodes graphNodes: [GKGraphNode2D], radius radius: Float)
  func point(at index: Int) -> vector_float2
}
