//: Playground - noun: a place where people can play

(Int128(1)...Int128(32)).reduce(1,combine:*)
Float128(1.0/3.0)
Float128(0.0).isPowerOf2
Float128(0.5).isPowerOf2
Float128(1.0).isPowerOf2
Float128(1.5).isPowerOf2
Float128.infinity.isPowerOf2
Float128.NaN.isPowerOf2
UInt128.max.msbAt
UInt64.max.msbAt
(UInt64.max >> 1).msbAt
UInt64(1).msbAt
UInt64(0).msbAt
UInt32.max.msbAt
UInt16.max.msbAt
UInt8.max.msbAt
UInt.max.msbAt


var m = Float128(2.0/3.0) * Float128(1.0/3.0)
m.asDouble
m._toBitPattern().toString(16)
m = Float128(0.1)*Float128(0.1)*Float128(0.1)
m = Float128(UInt64.max)
m.asUInt64 == UInt64.max
