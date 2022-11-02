
class AMWorkflow : NSObject, NSCopying {
  class func run(at fileURL: NSURL, withInput input: AnyObject?) throws -> AnyObject
  convenience init(contentsOf fileURL: NSURL) throws
  func write(to fileURL: NSURL) throws
  func setValue(_ value: AnyObject?, forVariableWithName variableName: String) -> Bool
  func valueForVariable(withName variableName: String) -> AnyObject
  func addAction(_ action: AMAction)
  func removeAction(_ action: AMAction)
  func insertAction(_ action: AMAction, at index: Int)
  func moveAction(at startIndex: Int, to endIndex: Int)
  @NSCopying var fileURL: NSURL? { get }
  var actions: [AMAction] { get }
  var input: AnyObject?
  @available(OSX 10.6, *)
  var output: AnyObject? { get }
  func copy(with zone: NSZone = nil) -> AnyObject
}
