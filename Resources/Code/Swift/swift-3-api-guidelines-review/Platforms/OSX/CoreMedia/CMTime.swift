
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
@available(OSX 10.7, *)
let kCMTimeInvalid: CMTime
@available(OSX 10.7, *)
let kCMTimeIndefinite: CMTime
@available(OSX 10.7, *)
let kCMTimePositiveInfinity: CMTime
@available(OSX 10.7, *)
let kCMTimeNegativeInfinity: CMTime
@available(OSX 10.7, *)
let kCMTimeZero: CMTime
@available(OSX 10.7, *)
func CMTimeMake(_ value: Int64, _ timescale: Int32) -> CMTime
@available(OSX 10.7, *)
func CMTimeMakeWithEpoch(_ value: Int64, _ timescale: Int32, _ epoch: Int64) -> CMTime
@available(OSX 10.7, *)
func CMTimeMakeWithSeconds(_ seconds: Float64, _ preferredTimeScale: Int32) -> CMTime
@available(OSX 10.7, *)
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
@available(OSX 10.7, *)
func CMTimeConvertScale(_ time: CMTime, _ newTimescale: Int32, _ method: CMTimeRoundingMethod) -> CMTime
@available(OSX 10.7, *)
func CMTimeAdd(_ addend1: CMTime, _ addend2: CMTime) -> CMTime
@available(OSX 10.7, *)
func CMTimeSubtract(_ minuend: CMTime, _ subtrahend: CMTime) -> CMTime
@available(OSX 10.7, *)
func CMTimeMultiply(_ time: CMTime, _ multiplier: Int32) -> CMTime
@available(OSX 10.7, *)
func CMTimeMultiplyByFloat64(_ time: CMTime, _ multiplier: Float64) -> CMTime
@available(OSX 10.10, *)
func CMTimeMultiplyByRatio(_ time: CMTime, _ multiplier: Int32, _ divisor: Int32) -> CMTime
@available(OSX 10.7, *)
func CMTimeCompare(_ time1: CMTime, _ time2: CMTime) -> Int32
@available(OSX 10.7, *)
func CMTimeMinimum(_ time1: CMTime, _ time2: CMTime) -> CMTime
@available(OSX 10.7, *)
func CMTimeMaximum(_ time1: CMTime, _ time2: CMTime) -> CMTime
@available(OSX 10.7, *)
func CMTimeAbsoluteValue(_ time: CMTime) -> CMTime
@available(OSX 10.7, *)
func CMTimeCopyAsDictionary(_ time: CMTime, _ allocator: CFAllocator?) -> CFDictionary?
@available(OSX 10.7, *)
func CMTimeMakeFromDictionary(_ dict: CFDictionary?) -> CMTime
@available(OSX 10.7, *)
let kCMTimeValueKey: CFString
@available(OSX 10.7, *)
let kCMTimeScaleKey: CFString
@available(OSX 10.7, *)
let kCMTimeEpochKey: CFString
@available(OSX 10.7, *)
let kCMTimeFlagsKey: CFString
@available(OSX 10.7, *)
func CMTimeCopyDescription(_ allocator: CFAllocator?, _ time: CMTime) -> CFString?
@available(OSX 10.7, *)
func CMTimeShow(_ time: CMTime)
