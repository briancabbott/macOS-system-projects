
typealias CMTimeValue = Int64
typealias CMTimeScale = Int32
var kCMTimeMaxTimescale: Int { get }
typealias CMTimeEpoch = Int64
struct CMTimeFlags : OptionSetType {
  init(rawValue rawValue: UInt32)
  let rawValue: UInt32
  static var valid: CMTimeFlags { get }
  static var hasBeenRounded: CMTimeFlags { get }
  static var positiveInfinity: CMTimeFlags { get }
  static var negativeInfinity: CMTimeFlags { get }
  static var indefinite: CMTimeFlags { get }
  static var impliedValueFlagsMask: CMTimeFlags { get }
}
struct CMTime {
  var value: CMTimeValue
  var timescale: CMTimeScale
  var flags: CMTimeFlags
  var epoch: CMTimeEpoch
  init()
  init(value value: CMTimeValue, timescale timescale: CMTimeScale, flags flags: CMTimeFlags, epoch epoch: CMTimeEpoch)
}

extension CMTime {
  init(seconds seconds: Double, preferredTimescale preferredTimescale: CMTimeScale)
  init(value value: CMTimeValue, timescale timescale: CMTimeScale)
}

extension CMTime {
  var isValid: Bool { get }
  var isPositiveInfinity: Bool { get }
  var isNegativeInfinity: Bool { get }
  var isIndefinite: Bool { get }
  var isNumeric: Bool { get }
  var hasBeenRounded: Bool { get }
  var seconds: Double { get }
  func convertScale(_ newTimescale: Int32, method method: CMTimeRoundingMethod) -> CMTime
}

extension CMTime : Equatable, Comparable {
}
@available(tvOS 4.0, *)
let kCMTimeInvalid: CMTime
@available(tvOS 4.0, *)
let kCMTimeIndefinite: CMTime
@available(tvOS 4.0, *)
let kCMTimePositiveInfinity: CMTime
@available(tvOS 4.0, *)
let kCMTimeNegativeInfinity: CMTime
@available(tvOS 4.0, *)
let kCMTimeZero: CMTime
@available(tvOS 4.0, *)
func CMTimeMake(_ value: Int64, _ timescale: Int32) -> CMTime
@available(tvOS 4.0, *)
func CMTimeMakeWithEpoch(_ value: Int64, _ timescale: Int32, _ epoch: Int64) -> CMTime
@available(tvOS 4.0, *)
func CMTimeMakeWithSeconds(_ seconds: Float64, _ preferredTimeScale: Int32) -> CMTime
@available(tvOS 4.0, *)
func CMTimeGetSeconds(_ time: CMTime) -> Float64
enum CMTimeRoundingMethod : UInt32 {
  init?(rawValue rawValue: UInt32)
  var rawValue: UInt32 { get }
  case roundHalfAwayFromZero
  case roundTowardZero
  case roundAwayFromZero
  case quickTime
  case roundTowardPositiveInfinity
  case roundTowardNegativeInfinity
  static var `default`: CMTimeRoundingMethod { get }
}
@available(tvOS 4.0, *)
func CMTimeConvertScale(_ time: CMTime, _ newTimescale: Int32, _ method: CMTimeRoundingMethod) -> CMTime
@available(tvOS 4.0, *)
func CMTimeAdd(_ addend1: CMTime, _ addend2: CMTime) -> CMTime
@available(tvOS 4.0, *)
func CMTimeSubtract(_ minuend: CMTime, _ subtrahend: CMTime) -> CMTime
@available(tvOS 4.0, *)
func CMTimeMultiply(_ time: CMTime, _ multiplier: Int32) -> CMTime
@available(tvOS 4.0, *)
func CMTimeMultiplyByFloat64(_ time: CMTime, _ multiplier: Float64) -> CMTime
@available(tvOS 7.1, *)
func CMTimeMultiplyByRatio(_ time: CMTime, _ multiplier: Int32, _ divisor: Int32) -> CMTime
@available(tvOS 4.0, *)
func CMTimeCompare(_ time1: CMTime, _ time2: CMTime) -> Int32
@available(tvOS 4.0, *)
func CMTimeMinimum(_ time1: CMTime, _ time2: CMTime) -> CMTime
@available(tvOS 4.0, *)
func CMTimeMaximum(_ time1: CMTime, _ time2: CMTime) -> CMTime
@available(tvOS 4.0, *)
func CMTimeAbsoluteValue(_ time: CMTime) -> CMTime
@available(tvOS 4.0, *)
func CMTimeCopyAsDictionary(_ time: CMTime, _ allocator: CFAllocator?) -> CFDictionary?
@available(tvOS 4.0, *)
func CMTimeMakeFromDictionary(_ dict: CFDictionary?) -> CMTime
@available(tvOS 4.0, *)
let kCMTimeValueKey: CFString
@available(tvOS 4.0, *)
let kCMTimeScaleKey: CFString
@available(tvOS 4.0, *)
let kCMTimeEpochKey: CFString
@available(tvOS 4.0, *)
let kCMTimeFlagsKey: CFString
@available(tvOS 4.0, *)
func CMTimeCopyDescription(_ allocator: CFAllocator?, _ time: CMTime) -> CFString?
@available(tvOS 4.0, *)
func CMTimeShow(_ time: CMTime)
