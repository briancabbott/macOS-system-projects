//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

@_exported import CoreMedia // Clang module

extension CMTime {
  public init(seconds: Double, preferredTimescale: CMTimeScale) {
    self = CMTimeMakeWithSeconds(seconds, preferredTimescale)
  }

  public init(value: CMTimeValue, timescale: CMTimeScale) {
    self = CMTimeMake(value, timescale)
  }
}

// CMTIME_IS_VALID
// CMTIME_IS_INVALID
// CMTIME_IS_POSITIVEINFINITY
// CMTIME_IS_NEGATIVEINFINITY
// CMTIME_IS_INDEFINITE
// CMTIME_IS_NUMERIC
// CMTIME_HAS_BEEN_ROUNDED
// CMTimeGetSeconds
// CMTimeConvertScale
extension CMTime {
  public var isValid: Bool {
    return self.flags.contains(.Valid)
  }

  public var isPositiveInfinity: Bool {
    return self.isValid &&
      self.flags.contains(.PositiveInfinity)
  }

  public var isNegativeInfinity: Bool {
    return self.isValid &&
      self.flags.contains(.NegativeInfinity)
  }

  public var isIndefinite: Bool {
    return self.isValid &&
      self.flags.contains(.Indefinite)
  }

  public var isNumeric: Bool {
    return
      self.flags.intersect([.Valid, .ImpliedValueFlagsMask]) == .Valid
  }

  public var hasBeenRounded: Bool {
    return self.isNumeric &&
      self.flags.contains(.HasBeenRounded)
  }

  public var seconds: Double {
    return CMTimeGetSeconds(self) as Double
  }

  public func convertScale(newTimescale: Int32, method: CMTimeRoundingMethod)
    -> CMTime {

    return CMTimeConvertScale(self, newTimescale, method)
  }
}

@warn_unused_result
public func CMTIME_IS_VALID(time: CMTime) -> Bool {
  return time.isValid
}

@warn_unused_result
public func CMTIME_IS_INVALID(time: CMTime) -> Bool {
  return !time.isValid
}

@warn_unused_result
public func CMTIME_IS_POSITIVEINFINITY(time: CMTime) -> Bool {
  return time.isPositiveInfinity
}

@warn_unused_result
public func CMTIME_IS_NEGATIVEINFINITY(time: CMTime) -> Bool {
  return time.isNegativeInfinity
}

@warn_unused_result
public func CMTIME_IS_INDEFINITE(time: CMTime) -> Bool {
  return time.isIndefinite
}

@warn_unused_result
public func CMTIME_IS_NUMERIC(time: CMTime) -> Bool {
  return time.isNumeric
}

@warn_unused_result
public func CMTIME_HAS_BEEN_ROUNDED(time: CMTime) -> Bool {
  return time.hasBeenRounded
}

// CMTimeAdd
@warn_unused_result
public func + (addend1: CMTime, addend2: CMTime) -> CMTime {
  return CMTimeAdd(addend1, addend2)
}

// CMTimeSubtract
@warn_unused_result
public func - (minuend: CMTime, subtrahend: CMTime) -> CMTime {
  return CMTimeSubtract(minuend, subtrahend)
}

extension CMTime : Equatable, Comparable {}

// CMTimeCompare
@warn_unused_result
public func < (time1: CMTime, time2: CMTime) -> Bool {
  return CMTimeCompare(time1, time2) < 0
}
@warn_unused_result
public func <= (time1: CMTime, time2: CMTime) -> Bool {
  return CMTimeCompare(time1, time2) <= 0
}
@warn_unused_result
public func > (time1: CMTime, time2: CMTime) -> Bool {
  return CMTimeCompare(time1, time2) > 0
}
@warn_unused_result
public func >= (time1: CMTime, time2: CMTime) -> Bool {
  return CMTimeCompare(time1, time2) >= 0
}
@warn_unused_result
public func == (time1: CMTime, time2: CMTime) -> Bool {
  return CMTimeCompare(time1, time2) == 0
}
@warn_unused_result
public func != (time1: CMTime, time2: CMTime) -> Bool {
  return CMTimeCompare(time1, time2) != 0
}
