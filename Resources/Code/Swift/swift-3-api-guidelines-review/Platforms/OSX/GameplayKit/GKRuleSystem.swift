
@available(OSX 10.11, *)
class GKRuleSystem : NSObject {
  func evaluate()
  var state: NSMutableDictionary { get }
  var rules: [GKRule] { get }
  func addRule(_ rule: GKRule)
  func addRules(from rules: [GKRule])
  func removeAllRules()
  var agenda: [GKRule] { get }
  var executed: [GKRule] { get }
  var facts: [AnyObject] { get }
  func grade(forFact fact: NSObjectProtocol) -> Float
  func minimumGrade(forFacts facts: [AnyObject]) -> Float
  func maximumGrade(forFacts facts: [AnyObject]) -> Float
  func assertFact(_ fact: NSObjectProtocol)
  func assertFact(_ fact: NSObjectProtocol, grade grade: Float)
  func retractFact(_ fact: NSObjectProtocol)
  func retractFact(_ fact: NSObjectProtocol, grade grade: Float)
  func reset()
}
@available(OSX 10.11, *)
class GKRule : NSObject {
  var salience: Int
  func evaluatePredicate(with system: GKRuleSystem) -> Bool
  func performAction(with system: GKRuleSystem)
  convenience init(predicate predicate: NSPredicate, assertingFact fact: NSObjectProtocol, grade grade: Float)
  convenience init(predicate predicate: NSPredicate, retractingFact fact: NSObjectProtocol, grade grade: Float)
  convenience init(blockPredicate predicate: (GKRuleSystem) -> Bool, action action: (GKRuleSystem) -> Void)
}
@available(OSX 10.11, *)
class GKNSPredicateRule : GKRule {
  var predicate: NSPredicate { get }
  init(predicate predicate: NSPredicate)
}
