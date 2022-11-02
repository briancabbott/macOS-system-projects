
@available(OSX 10.7, *)
class AVAssetTrack : NSObject, NSCopying, AVAsynchronousKeyValueLoading {
  weak var asset: @sil_weak AVAsset? { get }
  var trackID: CMPersistentTrackID { get }
  @available(OSX 10.7, *)
  func copy(with zone: NSZone = nil) -> AnyObject
  @available(OSX 10.7, *)
  func statusOfValue(forKey key: String, error outError: NSErrorPointer) -> AVKeyValueStatus
  @available(OSX 10.7, *)
  func loadValuesAsynchronously(forKeys keys: [String], completionHandler handler: (() -> Void)? = nil)
}
extension AVAssetTrack {
  var mediaType: String { get }
  var formatDescriptions: [AnyObject] { get }
  @available(OSX 10.8, *)
  var isPlayable: Bool { get }
  var isEnabled: Bool { get }
  var isSelfContained: Bool { get }
  var totalSampleDataLength: Int64 { get }
  func hasMediaCharacteristic(_ mediaCharacteristic: String) -> Bool
}
extension AVAssetTrack {
  var timeRange: CMTimeRange { get }
  var naturalTimeScale: CMTimeScale { get }
  var estimatedDataRate: Float { get }
}
extension AVAssetTrack {
  var languageCode: String { get }
  var extendedLanguageTag: String { get }
}
extension AVAssetTrack {
  var naturalSize: CGSize { get }
  var preferredTransform: CGAffineTransform { get }
}
extension AVAssetTrack {
  var preferredVolume: Float { get }
}
extension AVAssetTrack {
  var nominalFrameRate: Float { get }
  @available(OSX 10.10, *)
  var minFrameDuration: CMTime { get }
  @available(OSX 10.10, *)
  var requiresFrameReordering: Bool { get }
}
extension AVAssetTrack {
  var segments: [AVAssetTrackSegment] { get }
  func segment(forTrackTime trackTime: CMTime) -> AVAssetTrackSegment?
  func samplePresentationTime(forTrackTime trackTime: CMTime) -> CMTime
}
extension AVAssetTrack {
  var commonMetadata: [AVMetadataItem] { get }
  @available(OSX 10.10, *)
  var metadata: [AVMetadataItem] { get }
  var availableMetadataFormats: [String] { get }
  func metadata(forFormat format: String) -> [AVMetadataItem]
}
extension AVAssetTrack {
  @available(OSX 10.9, *)
  var availableTrackAssociationTypes: [String] { get }
  @available(OSX 10.9, *)
  func associatedTracks(ofType trackAssociationType: String) -> [AVAssetTrack]
}
@available(OSX 10.9, *)
let AVTrackAssociationTypeAudioFallback: String
@available(OSX 10.9, *)
let AVTrackAssociationTypeChapterList: String
@available(OSX 10.9, *)
let AVTrackAssociationTypeForcedSubtitlesOnly: String
@available(OSX 10.9, *)
let AVTrackAssociationTypeSelectionFollower: String
@available(OSX 10.9, *)
let AVTrackAssociationTypeTimecode: String
@available(OSX 10.10, *)
let AVTrackAssociationTypeMetadataReferent: String
extension AVAssetTrack {
  @available(OSX 10.10, *)
  var canProvideSampleCursors: Bool { get }
  @available(OSX 10.10, *)
  func makeSampleCursor(withPresentationTimeStamp presentationTimeStamp: CMTime) -> AVSampleCursor?
  @available(OSX 10.10, *)
  func makeSampleCursorAtFirstSampleInDecodeOrder() -> AVSampleCursor?
  @available(OSX 10.10, *)
  func makeSampleCursorAtLastSampleInDecodeOrder() -> AVSampleCursor?
}
@available(OSX 10.11, *)
let AVAssetTrackTimeRangeDidChangeNotification: String
@available(OSX 10.11, *)
let AVAssetTrackSegmentsDidChangeNotification: String
@available(OSX 10.11, *)
let AVAssetTrackTrackAssociationsDidChangeNotification: String
@available(OSX 10.11, *)
class AVFragmentedAssetTrack : AVAssetTrack {
}
