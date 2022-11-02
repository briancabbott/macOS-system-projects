
class SFAuthorization : NSObject, NSCoding {
  class func authorization() -> AnyObject!
  func authorizationRef() -> AuthorizationRef
  class func authorization(with flags: AuthorizationFlags, rights rights: UnsafePointer<AuthorizationRights>, environment environment: UnsafePointer<AuthorizationEnvironment>) -> AnyObject!
  init!(flags flags: AuthorizationFlags, rights rights: UnsafePointer<AuthorizationRights>, environment environment: UnsafePointer<AuthorizationEnvironment>)
  func invalidateCredentials()
  func obtain(withRight rightName: AuthorizationString, flags flags: AuthorizationFlags) throws
  func obtain(withRights rights: UnsafePointer<AuthorizationRights>, flags flags: AuthorizationFlags, environment environment: UnsafePointer<AuthorizationEnvironment>, authorizedRights authorizedRights: UnsafeMutablePointer<UnsafeMutablePointer<AuthorizationRights>>) throws
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
}
extension SFAuthorization {
}
