
let SCNSceneSourceAssetContributorsKey: String
let SCNSceneSourceAssetCreatedDateKey: String
let SCNSceneSourceAssetModifiedDateKey: String
let SCNSceneSourceAssetUpAxisKey: String
let SCNSceneSourceAssetUnitKey: String
let SCNSceneSourceAssetAuthoringToolKey: String
let SCNSceneSourceAssetAuthorKey: String
let SCNSceneSourceAssetUnitNameKey: String
let SCNSceneSourceAssetUnitMeterKey: String
let SCNSceneSourceCreateNormalsIfAbsentKey: String
let SCNSceneSourceCheckConsistencyKey: String
let SCNSceneSourceFlattenSceneKey: String
let SCNSceneSourceUseSafeModeKey: String
let SCNSceneSourceAssetDirectoryURLsKey: String
let SCNSceneSourceOverrideAssetURLsKey: String
let SCNSceneSourceStrictConformanceKey: String
@available(tvOS 8.0, *)
let SCNSceneSourceAnimationImportPolicyKey: String
@available(tvOS 8.0, *)
let SCNSceneSourceAnimationImportPolicyPlay: String
@available(tvOS 8.0, *)
let SCNSceneSourceAnimationImportPolicyPlayRepeatedly: String
@available(tvOS 8.0, *)
let SCNSceneSourceAnimationImportPolicyDoNotPlay: String
@available(tvOS 8.0, *)
let SCNSceneSourceAnimationImportPolicyPlayUsingSceneTimeBase: String
let SCNDetailedErrorsKey: String
let SCNConsistencyElementIDErrorKey: String
let SCNConsistencyElementTypeErrorKey: String
let SCNConsistencyLineNumberErrorKey: String
var SCNConsistencyInvalidURIError: Int { get }
var SCNConsistencyInvalidCountError: Int { get }
var SCNConsistencyInvalidArgumentError: Int { get }
var SCNConsistencyMissingElementError: Int { get }
var SCNConsistencyMissingAttributeError: Int { get }
var SCNConsistencyXMLSchemaValidationError: Int { get }
enum SCNSceneSourceStatus : Int {
  init?(rawValue rawValue: Int)
  var rawValue: Int { get }
  case error
  case parsing
  case validating
  case processing
  case complete
}
typealias SCNSceneSourceStatusHandler = (Float, SCNSceneSourceStatus, NSError?, UnsafeMutablePointer<ObjCBool>) -> Void
@available(tvOS 8.0, *)
class SCNSceneSource : NSObject {
  init?(url url: NSURL, options options: [String : AnyObject]? = [:])
  init?(data data: NSData, options options: [String : AnyObject]? = [:])
  var url: NSURL? { get }
  var data: NSData? { get }
  func scene(options options: [String : AnyObject]? = [:], statusHandler statusHandler: SCNSceneSourceStatusHandler? = nil) -> SCNScene?
  func scene(options options: [String : AnyObject]? = [:]) throws -> SCNScene
  func property(forKey key: String) -> AnyObject?
  func __entry(withIdentifier uid: String, with entryClass: AnyClass) -> AnyObject?
  func identifiersOfEntries(with entryClass: AnyClass) -> [String]
  @available(tvOS 8.0, *)
  func entries(passingTest predicate: (AnyObject, String, UnsafeMutablePointer<ObjCBool>) -> Bool) -> [AnyObject]
}

@available(iOS 8.0, OSX 10.8, *)
extension SCNSceneSource {
  @warn_unused_result
  func entryWithIdentifier<T>(_ uid: String, withClass entryClass: T.Type) -> T?
}
