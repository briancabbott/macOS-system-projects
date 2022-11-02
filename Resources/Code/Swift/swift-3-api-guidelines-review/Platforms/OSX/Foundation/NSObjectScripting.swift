
extension NSObject {
  @available(OSX 10.5, *)
  class func scriptingValue(for objectSpecifier: NSScriptObjectSpecifier) -> AnyObject?
  @available(OSX 10.5, *)
  func scriptingValue(for objectSpecifier: NSScriptObjectSpecifier) -> AnyObject?
  var scriptingProperties: [String : AnyObject]?
  @available(OSX 10.5, *)
  class func copyScriptingValue(_ value: AnyObject, forKey key: String, withProperties properties: [String : AnyObject]) -> AnyObject?
  @available(OSX 10.5, *)
  func copyScriptingValue(_ value: AnyObject, forKey key: String, withProperties properties: [String : AnyObject]) -> AnyObject?
  @available(OSX 10.5, *)
  class func newScriptingObject(of objectClass: AnyClass, forValueForKey key: String, withContentsValue contentsValue: AnyObject?, properties properties: [String : AnyObject]) -> AnyObject?
  @available(OSX 10.5, *)
  func newScriptingObject(of objectClass: AnyClass, forValueForKey key: String, withContentsValue contentsValue: AnyObject?, properties properties: [String : AnyObject]) -> AnyObject?
  class func scriptingProperties() -> [String : AnyObject]?
  class func setScriptingProperties(_ scriptingProperties: [String : AnyObject]?)
}
