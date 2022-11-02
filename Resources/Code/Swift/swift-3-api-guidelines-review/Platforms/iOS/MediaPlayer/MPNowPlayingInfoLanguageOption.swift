
@available(iOS 9.0, *)
let MPLanguageOptionCharacteristicIsMainProgramContent: String
@available(iOS 9.0, *)
let MPLanguageOptionCharacteristicIsAuxiliaryContent: String
@available(iOS 9.0, *)
let MPLanguageOptionCharacteristicContainsOnlyForcedSubtitles: String
@available(iOS 9.0, *)
let MPLanguageOptionCharacteristicTranscribesSpokenDialog: String
@available(iOS 9.0, *)
let MPLanguageOptionCharacteristicDescribesMusicAndSound: String
@available(iOS 9.0, *)
let MPLanguageOptionCharacteristicEasyToRead: String
@available(iOS 9.0, *)
let MPLanguageOptionCharacteristicDescribesVideo: String
@available(iOS 9.0, *)
let MPLanguageOptionCharacteristicLanguageTranslation: String
@available(iOS 9.0, *)
let MPLanguageOptionCharacteristicDubbedTranslation: String
@available(iOS 9.0, *)
let MPLanguageOptionCharacteristicVoiceOverTranslation: String
@available(iOS 9.0, *)
enum MPNowPlayingInfoLanguageOptionType : UInt {
  init?(rawValue rawValue: UInt)
  var rawValue: UInt { get }
  case audible
  case legible
}
@available(iOS 9.0, *)
class MPNowPlayingInfoLanguageOption : NSObject {
  init(type languageOptionType: MPNowPlayingInfoLanguageOptionType, languageTag languageTag: String, characteristics languageOptionCharacteristics: [String]?, displayName displayName: String, identifier identifier: String)
  func isAutomaticLegibleLanguageOption() -> Bool
  func isAutomaticAudibleLanguageOption() -> Bool
  var languageOptionType: MPNowPlayingInfoLanguageOptionType { get }
  var languageTag: String? { get }
  var languageOptionCharacteristics: [String]? { get }
  var displayName: String? { get }
  var identifier: String? { get }
}
@available(iOS 9.0, *)
class MPNowPlayingInfoLanguageOptionGroup : NSObject {
  init(languageOptions languageOptions: [MPNowPlayingInfoLanguageOption], defaultLanguageOption defaultLanguageOption: MPNowPlayingInfoLanguageOption?, allowEmptySelection allowEmptySelection: Bool)
  var languageOptions: [MPNowPlayingInfoLanguageOption] { get }
  var defaultLanguageOption: MPNowPlayingInfoLanguageOption? { get }
  var allowEmptySelection: Bool { get }
}
