
@available(tvOS 9.0, *)
class GKBehavior : NSObject, NSFastEnumeration {
  var goalCount: Int { get }
  convenience init(goal goal: GKGoal, weight weight: Float)
  convenience init(goals goals: [GKGoal])
  convenience init(goals goals: [GKGoal], andWeights weights: [NSNumber])
  convenience init(weightedGoals weightedGoals: [GKGoal : NSNumber])
  func setWeight(_ weight: Float, for goal: GKGoal)
  func weight(for goal: GKGoal) -> Float
  func remove(_ goal: GKGoal)
  func removeAllGoals()
  subscript(_ idx: Int) -> GKGoal { get }
  subscript(_ goal: GKGoal) -> NSNumber
  @available(tvOS 9.0, *)
  func countByEnumerating(with state: UnsafeMutablePointer<NSFastEnumerationState>, objects buffer: AutoreleasingUnsafeMutablePointer<AnyObject?>, count len: Int) -> Int
}
