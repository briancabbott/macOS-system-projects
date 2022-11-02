
@available(iOS 4.0, *)
class AVAsset : NSObject, NSCopying, AVAsynchronousKeyValueLoading {
  convenience init(url URL: NSURL)
  var duration: CMTime { get }
  var preferredRate: Float { get }
  var preferredVolume: Float { get }
  var preferredTransform: CGAffineTransform { get }
  @available(iOS 4.0, *)
  func copy(with zone: NSZone = nil) -> AnyObject
  @available(iOS 4.0, *)
  func statusOfValue(forKey key: String, error outError: NSErrorPointer) -> AVKeyValueStatus
  @available(iOS 4.0, *)
  func loadValuesAsynchronously(forKeys keys: [String], completionHandler handler: (() -> Void)? = nil)
}
extension AVAsset {
  var providesPreciseDurationAndTiming: Bool { get }
  func cancelLoading()
}
extension AVAsset {
  @available(iOS 5.0, *)
  var referenceRestrictions: AVAssetReferenceRestrictions { get }
}
struct AVAssetReferenceRestrictions : OptionSetType {
  init(rawValue rawValue: UInt)
  let rawValue: UInt
  static var forbidNone: AVAssetReferenceRestrictions { get }
  static var forbidRemoteReferenceToLocal: AVAssetReferenceRestrictions { get }
  static var forbidLocalReferenceToRemote: AVAssetReferenceRestrictions { get }
  static var forbidCrossSiteReference: AVAssetReferenceRestrictions { get }
  static var forbidLocalReferenceToLocal: AVAssetReferenceRestrictions { get }
  static var forbidAll: AVAssetReferenceRestrictions { get }
}
extension AVAsset {
  var tracks: [AVAssetTrack] { get }
  func track(withTrackID trackID: CMPersistentTrackID) -> AVAssetTrack?
  func tracks(withMediaType mediaType: String) -> [AVAssetTrack]
  func tracks(withMediaCharacteristic mediaCharacteristic: String) -> [AVAssetTrack]
  @available(iOS 7.0, *)
  var trackGroups: [AVAssetTrackGroup] { get }
}
extension AVAsset {
  @available(iOS 5.0, *)
  var creationDate: AVMetadataItem? { get }
  var lyrics: String? { get }
  var commonMetadata: [AVMetadataItem] { get }
  @available(iOS 8.0, *)
  var metadata: [AVMetadataItem] { get }
  var availableMetadataFormats: [String] { get }
  func metadata(forFormat format: String) -> [AVMetadataItem]
}
extension AVAsset {
  @available(iOS 4.3, *)
  var availableChapterLocales: [NSLocale] { get }
  @available(iOS 4.3, *)
  func chapterMetadataGroups(withTitleLocale locale: NSLocale, containingItemsWithCommonKeys commonKeys: [String]?) -> [AVTimedMetadataGroup]
  @available(iOS 6.0, *)
  func chapterMetadataGroups(bestMatchingPreferredLanguages preferredLanguages: [String]) -> [AVTimedMetadataGroup]
}
extension AVAsset {
  @available(iOS 5.0, *)
  var availableMediaCharacteristicsWithMediaSelectionOptions: [String] { get }
  @available(iOS 5.0, *)
  func mediaSelectionGroup(forMediaCharacteristic mediaCharacteristic: String) -> AVMediaSelectionGroup?
  @available(iOS 9.0, *)
  var preferredMediaSelection: AVMediaSelection { get }
}
extension AVAsset {
  @available(iOS 4.2, *)
  var hasProtectedContent: Bool { get }
}
extension AVAsset {
  @available(iOS 9.0, *)
  var canContainFragments: Bool { get }
  @available(iOS 9.0, *)
  var containsFragments: Bool { get }
}
extension AVAsset {
  @available(iOS 4.3, *)
  var isPlayable: Bool { get }
  @available(iOS 4.3, *)
  var isExportable: Bool { get }
  @available(iOS 4.3, *)
  var isReadable: Bool { get }
  @available(iOS 4.3, *)
  var isComposable: Bool { get }
  @available(iOS 5.0, *)
  var isCompatibleWithSavedPhotosAlbum: Bool { get }
  @available(iOS 9.0, *)
  var isCompatibleWithAirPlayVideo: Bool { get }
}
@available(iOS 4.0, *)
let AVURLAssetPreferPreciseDurationAndTimingKey: String
@available(iOS 5.0, *)
let AVURLAssetReferenceRestrictionsKey: String
@available(iOS 8.0, *)
let AVURLAssetHTTPCookiesKey: String
@available(iOS 4.0, *)
class AVURLAsset : AVAsset {
  @available(iOS 5.0, *)
  class func audiovisualTypes() -> [String]
  @available(iOS 5.0, *)
  class func audiovisualMIMETypes() -> [String]
  @available(iOS 5.0, *)
  class func isPlayableExtendedMIMEType(_ extendedMIMEType: String) -> Bool
  init(url URL: NSURL, options options: [String : AnyObject]? = [:])
  @NSCopying var url: NSURL { get }
}
extension AVURLAsset {
  @available(iOS 6.0, *)
  var resourceLoader: AVAssetResourceLoader { get }
}
extension AVURLAsset {
  func compatibleTrack(for compositionTrack: AVCompositionTrack) -> AVAssetTrack?
}
@available(iOS 9.0, *)
let AVAssetDurationDidChangeNotification: String
@available(iOS 9.0, *)
let AVAssetChapterMetadataGroupsDidChangeNotification: String
@available(iOS 9.0, *)
let AVAssetMediaSelectionGroupsDidChangeNotification: String
protocol AVFragmentMinding {
}
extension AVFragmentedAsset {
}
