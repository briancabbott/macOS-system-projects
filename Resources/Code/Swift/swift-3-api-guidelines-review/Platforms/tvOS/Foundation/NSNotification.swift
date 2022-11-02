
class NSNotification : NSObject, NSCopying, NSCoding {
  var name: String { get }
  var object: AnyObject? { get }
  var userInfo: [NSObject : AnyObject]? { get }
  @available(tvOS 4.0, *)
  init(name name: String, object object: AnyObject?, userInfo userInfo: [NSObject : AnyObject]? = [:])
  init?(coder aDecoder: NSCoder)
  func copy(with zone: NSZone = nil) -> AnyObject
  func encode(with aCoder: NSCoder)
}
extension NSNotification {
  convenience init(name aName: String, object anObject: AnyObject?)
}
class NSNotificationCenter : NSObject {
  class func defaultCenter() -> NSNotificationCenter
  func addObserver(_ observer: AnyObject, selector aSelector: Selector, name aName: String?, object anObject: AnyObject?)
  func post(_ notification: NSNotification)
  func postNotificationName(_ aName: String, object anObject: AnyObject?)
  func postNotificationName(_ aName: String, object anObject: AnyObject?, userInfo aUserInfo: [NSObject : AnyObject]? = [:])
  func removeObserver(_ observer: AnyObject)
  func removeObserver(_ observer: AnyObject, name aName: String?, object anObject: AnyObject?)
  @available(tvOS 4.0, *)
  func addObserver(forName name: String?, object obj: AnyObject?, queue queue: NSOperationQueue?, using block: (NSNotification) -> Void) -> NSObjectProtocol
}
