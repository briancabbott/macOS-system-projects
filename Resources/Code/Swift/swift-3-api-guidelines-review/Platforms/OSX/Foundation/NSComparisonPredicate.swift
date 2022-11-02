
struct NSComparisonPredicateOptions : OptionSetType {
  init(rawValue rawValue: UInt)
  let rawValue: UInt
  static var caseInsensitivePredicateOption: NSComparisonPredicateOptions { get }
  static var diacriticInsensitivePredicateOption: NSComparisonPredicateOptions { get }
  @available(OSX 10.6, *)
  static var normalizedPredicateOption: NSComparisonPredicateOptions { get }
}
enum NSComparisonPredicateModifier : UInt {
  init?(rawValue rawValue: UInt)
  var rawValue: UInt { get }
  case directPredicateModifier
  case allPredicateModifier
  case anyPredicateModifier
}
enum NSPredicateOperatorType : UInt {
  init?(rawValue rawValue: UInt)
  var rawValue: UInt { get }
  case lessThanPredicateOperatorType
  case lessThanOrEqualToPredicateOperatorType
  case greaterThanPredicateOperatorType
  case greaterThanOrEqualToPredicateOperatorType
  case equalToPredicateOperatorType
  case notEqualToPredicateOperatorType
  case matchesPredicateOperatorType
  case likePredicateOperatorType
  case beginsWithPredicateOperatorType
  case endsWithPredicateOperatorType
  case inPredicateOperatorType
  case customSelectorPredicateOperatorType
  @available(OSX 10.5, *)
  case containsPredicateOperatorType
  @available(OSX 10.5, *)
  case betweenPredicateOperatorType
}
@available(OSX 10.4, *)
class NSComparisonPredicate : NSPredicate {
  init(leftExpression lhs: NSExpression, rightExpression rhs: NSExpression, modifier modifier: NSComparisonPredicateModifier, type type: NSPredicateOperatorType, options options: NSComparisonPredicateOptions = [])
  init(leftExpression lhs: NSExpression, rightExpression rhs: NSExpression, customSelector selector: Selector)
  var predicateOperatorType: NSPredicateOperatorType { get }
  var comparisonPredicateModifier: NSComparisonPredicateModifier { get }
  var leftExpression: NSExpression { get }
  var rightExpression: NSExpression { get }
  var customSelector: Selector { get }
  var options: NSComparisonPredicateOptions { get }
}
