
var kAuthorizationRightRule: String { get }
var kAuthorizationRuleIsAdmin: String { get }
var kAuthorizationRuleAuthenticateAsSessionUser: String { get }
var kAuthorizationRuleAuthenticateAsAdmin: String { get }
var kAuthorizationRuleClassAllow: String { get }
var kAuthorizationRuleClassDeny: String { get }
var kAuthorizationComment: String { get }
func AuthorizationRightGet(_ rightName: UnsafePointer<Int8>, _ rightDefinition: UnsafeMutablePointer<CFDictionary?>) -> OSStatus
func AuthorizationRightSet(_ authRef: AuthorizationRef, _ rightName: UnsafePointer<Int8>, _ rightDefinition: CFTypeRef, _ descriptionKey: CFString?, _ bundle: CFBundle?, _ localeTableName: CFString?) -> OSStatus
func AuthorizationRightRemove(_ authRef: AuthorizationRef, _ rightName: UnsafePointer<Int8>) -> OSStatus
