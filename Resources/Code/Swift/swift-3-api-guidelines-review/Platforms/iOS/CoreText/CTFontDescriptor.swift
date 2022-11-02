
class CTFontDescriptor {
}
@available(iOS 3.2, *)
func CTFontDescriptorGetTypeID() -> CFTypeID
@available(iOS 3.2, *)
let kCTFontURLAttribute: CFString
@available(iOS 3.2, *)
let kCTFontNameAttribute: CFString
@available(iOS 3.2, *)
let kCTFontDisplayNameAttribute: CFString
@available(iOS 3.2, *)
let kCTFontFamilyNameAttribute: CFString
@available(iOS 3.2, *)
let kCTFontStyleNameAttribute: CFString
@available(iOS 3.2, *)
let kCTFontTraitsAttribute: CFString
@available(iOS 3.2, *)
let kCTFontVariationAttribute: CFString
@available(iOS 3.2, *)
let kCTFontSizeAttribute: CFString
@available(iOS 3.2, *)
let kCTFontMatrixAttribute: CFString
@available(iOS 3.2, *)
let kCTFontCascadeListAttribute: CFString
@available(iOS 3.2, *)
let kCTFontCharacterSetAttribute: CFString
@available(iOS 3.2, *)
let kCTFontLanguagesAttribute: CFString
@available(iOS 3.2, *)
let kCTFontBaselineAdjustAttribute: CFString
@available(iOS 3.2, *)
let kCTFontMacintoshEncodingsAttribute: CFString
@available(iOS 3.2, *)
let kCTFontFeaturesAttribute: CFString
@available(iOS 3.2, *)
let kCTFontFeatureSettingsAttribute: CFString
@available(iOS 3.2, *)
let kCTFontFixedAdvanceAttribute: CFString
@available(iOS 3.2, *)
let kCTFontOrientationAttribute: CFString
enum CTFontOrientation : UInt32 {
  init?(rawValue rawValue: UInt32)
  var rawValue: UInt32 { get }
  @available(iOS 6.0, *)
  case `default`
  @available(iOS 6.0, *)
  case horizontal
  @available(iOS 6.0, *)
  case vertical
  @available(iOS, introduced=3.2, deprecated=9.0)
  static var kCTFontDefaultOrientation: CTFontOrientation { get }
  @available(iOS, introduced=3.2, deprecated=9.0)
  static var kCTFontHorizontalOrientation: CTFontOrientation { get }
  @available(iOS, introduced=3.2, deprecated=9.0)
  static var kCTFontVerticalOrientation: CTFontOrientation { get }
}
@available(iOS 3.2, *)
let kCTFontFormatAttribute: CFString
enum CTFontFormat : UInt32 {
  init?(rawValue rawValue: UInt32)
  var rawValue: UInt32 { get }
  case unrecognized
  case openTypePostScript
  case openTypeTrueType
  case trueType
  case postScript
  case bitmap
}
@available(iOS 3.2, *)
let kCTFontRegistrationScopeAttribute: CFString
@available(iOS 3.2, *)
let kCTFontPriorityAttribute: CFString
var kCTFontPrioritySystem: Int { get }
var kCTFontPriorityNetwork: Int { get }
var kCTFontPriorityComputer: Int { get }
var kCTFontPriorityUser: Int { get }
var kCTFontPriorityDynamic: Int { get }
var kCTFontPriorityProcess: Int { get }
typealias CTFontPriority = UInt32
@available(iOS 3.2, *)
let kCTFontEnabledAttribute: CFString
@available(iOS 6.0, *)
let kCTFontDownloadableAttribute: CFString
@available(iOS 7.0, *)
let kCTFontDownloadedAttribute: CFString
@available(iOS 3.2, *)
func CTFontDescriptorCreateWithNameAndSize(_ name: CFString, _ size: CGFloat) -> CTFontDescriptor
@available(iOS 3.2, *)
func CTFontDescriptorCreateWithAttributes(_ attributes: CFDictionary) -> CTFontDescriptor
@available(iOS 3.2, *)
func CTFontDescriptorCreateCopyWithAttributes(_ original: CTFontDescriptor, _ attributes: CFDictionary) -> CTFontDescriptor
@available(iOS 7.0, *)
func CTFontDescriptorCreateCopyWithFamily(_ original: CTFontDescriptor, _ family: CFString) -> CTFontDescriptor?
@available(iOS 7.0, *)
func CTFontDescriptorCreateCopyWithSymbolicTraits(_ original: CTFontDescriptor, _ symTraitValue: CTFontSymbolicTraits, _ symTraitMask: CTFontSymbolicTraits) -> CTFontDescriptor?
@available(iOS 3.2, *)
func CTFontDescriptorCreateCopyWithVariation(_ original: CTFontDescriptor, _ variationIdentifier: CFNumber, _ variationValue: CGFloat) -> CTFontDescriptor
@available(iOS 3.2, *)
func CTFontDescriptorCreateCopyWithFeature(_ original: CTFontDescriptor, _ featureTypeIdentifier: CFNumber, _ featureSelectorIdentifier: CFNumber) -> CTFontDescriptor
@available(iOS 3.2, *)
func CTFontDescriptorCreateMatchingFontDescriptors(_ descriptor: CTFontDescriptor, _ mandatoryAttributes: CFSet?) -> CFArray?
@available(iOS 3.2, *)
func CTFontDescriptorCreateMatchingFontDescriptor(_ descriptor: CTFontDescriptor, _ mandatoryAttributes: CFSet?) -> CTFontDescriptor?
enum CTFontDescriptorMatchingState : UInt32 {
  init?(rawValue rawValue: UInt32)
  var rawValue: UInt32 { get }
  case didBegin
  case didFinish
  case willBeginQuerying
  case stalled
  case willBeginDownloading
  case downloading
  case didFinishDownloading
  case didMatch
  case didFailWithError
}
@available(iOS 6.0, *)
let kCTFontDescriptorMatchingSourceDescriptor: CFString
@available(iOS 6.0, *)
let kCTFontDescriptorMatchingDescriptors: CFString
@available(iOS 6.0, *)
let kCTFontDescriptorMatchingResult: CFString
@available(iOS 6.0, *)
let kCTFontDescriptorMatchingPercentage: CFString
@available(iOS 6.0, *)
let kCTFontDescriptorMatchingCurrentAssetSize: CFString
@available(iOS 6.0, *)
let kCTFontDescriptorMatchingTotalDownloadedSize: CFString
@available(iOS 6.0, *)
let kCTFontDescriptorMatchingTotalAssetSize: CFString
@available(iOS 6.0, *)
let kCTFontDescriptorMatchingError: CFString
typealias CTFontDescriptorProgressHandler = (CTFontDescriptorMatchingState, CFDictionary) -> Bool
@available(iOS 6.0, *)
func CTFontDescriptorMatchFontDescriptorsWithProgressHandler(_ descriptors: CFArray, _ mandatoryAttributes: CFSet?, _ progressBlock: CTFontDescriptorProgressHandler) -> Bool
@available(iOS 3.2, *)
func CTFontDescriptorCopyAttributes(_ descriptor: CTFontDescriptor) -> CFDictionary
@available(iOS 3.2, *)
func CTFontDescriptorCopyAttribute(_ descriptor: CTFontDescriptor, _ attribute: CFString) -> CFTypeRef?
@available(iOS 3.2, *)
func CTFontDescriptorCopyLocalizedAttribute(_ descriptor: CTFontDescriptor, _ attribute: CFString, _ language: UnsafeMutablePointer<Unmanaged<CFString>?>) -> CFTypeRef?
