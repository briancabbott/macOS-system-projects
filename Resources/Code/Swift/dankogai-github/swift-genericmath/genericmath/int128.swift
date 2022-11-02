//
//  int128.swift
//  genericmath
//
//  Created by Dan Kogai on 1/30/16.
//  Copyright © 2016 Dan Kogai. All rights reserved.
//

public struct UInt128 {
    var value:(UInt32, UInt32, UInt32, UInt32)
    public init(_ a:UInt32, _ b:UInt32, _ c:UInt32, _ d:UInt32) {
        value = (a, b, c, d)
    }
    public init(_ h:UInt64, _ l:UInt64) {
        (value.0, value.1) = (UInt32(h >> 32), UInt32(h & 0xffffFFFF))
        (value.2, value.3) = (UInt32(l >> 32), UInt32(l & 0xffffFFFF))
    }
    public init(_ s:UInt128) {
        value = s.value
    }
    public init() {
        value = (0, 0, 0, 0)
    }
    public init(_ s:UInt8)  { self.init(0, 0, 0, UInt32(s)) }
    public init(_ s:UInt16) { self.init(0, 0, 0, UInt32(s)) }
    public init(_ s:UInt32) { self.init(0, 0, 0, s) }
    public init(_ s:UInt64) { self.init(0, s) }
    public init(_ s:UInt)   { self.init(0, UInt64(s)) }
    public init(_ s:Int8)   { self.init(0, 0, 0, UInt32(s)) }
    public init(_ s:Int16)  { self.init(0, 0, 0, UInt32(s)) }
    public init(_ s:Int32)  { self.init(0, 0, 0, UInt32(s)) }
    public init(_ s:Int64)  { self.init(0, UInt64(s)) }
    public init(_ s:Int)    { self.init(0, UInt64(s)) }
    public init(_ d:Double) {
        if d < Double(UInt64.max) {
            self.init(0, UInt64(d))
        } else {
            self.init(
                UInt64(d / 18446744073709551616.0),
                UInt64(d % 18446744073709551616.0)
            )
        }
    }
    public init(_ f:Float) {
        self.init(Double(f))
    }
    // utility
    public static let min = UInt128(0, 0, 0, 0)
    public static let max = UInt128(
        UInt32.max, UInt32.max, UInt32.max, UInt32.max
    )
    public var asUInt32s:(UInt32, UInt32, UInt32, UInt32) {
        return value
    }
    public var asUInt64s:(UInt64,UInt64) {
        return (
            (UInt64(value.0) << 32) | UInt64(value.1),
            (UInt64(value.2) << 32) | UInt64(value.3)
        )
    }
}
extension UInt128: IntegerLiteralConvertible, _BuiltinIntegerLiteralConvertible {
    public typealias IntegerLiteralType = UInt64.IntegerLiteralType
    public init(integerLiteral literal:IntegerLiteralType) {
        (value.0, value.1) = (0, 0)
        (value.2, value.3) = (UInt32(literal >> 32), UInt32(literal & 0xffffFFFF))
    }
    public init(_builtinIntegerLiteral value:_MaxBuiltinIntegerType) {
        self.init(UInt64(_builtinIntegerLiteral: value))
    }
}
public extension UInt32 {
    public init(_ u128:UInt128) {
        guard u128 <= UInt128(UInt32.max) else {
            fatalError("\(u128) > UInt32.max")
        }
        self = u128.value.3
    }
}
public extension UInt64 {
    public init(_ u128:UInt128) {
        guard u128 <= UInt128(UInt64.max) else {
            fatalError("\(u128) > UInt64.max")
        }
        self = UInt64(u128.value.2) << 32 + UInt64(u128.value.3)
    }
}
public extension Int64 {
    public init(_ u128:UInt128) {
        guard u128 <= UInt128(Int64.max) else {
            fatalError("\(u128) > Int64.max")
        }
        self.init(UInt64(u128))
    }
}
public extension UInt {
    public init(_ u128:UInt128) {
        self.init(UInt64(u128))
    }
}
public extension Int {
    public init(_ u128:UInt128) {
        self.init(Int64(u128))
    }
}
// let's make it equatable
extension UInt128: Equatable {}
public func ==(lhs:UInt128, rhs:UInt128)->Bool {
    if lhs.value.0 != rhs.value.0 { return false }
    if lhs.value.1 != rhs.value.1 { return false }
    if lhs.value.2 != rhs.value.2 { return false }
    if lhs.value.3 != rhs.value.3 { return false }
    return true
}
// addition
public extension UInt128 {
    public static func addWithOverflow(lhs:UInt128, _ rhs:UInt128)->(UInt128, overflow:Bool) {
        var r = UInt128()
        let r3 = UInt64(lhs.value.3) + UInt64(rhs.value.3)
        r.value.3 = UInt32(r3 & 0xffffFFFF)
        let r2 = UInt64(lhs.value.2) + UInt64(rhs.value.2) + (r3 >> 32)
        r.value.2 = UInt32(r2 & 0xffffFFFF)
        let r1 = UInt64(lhs.value.1) + UInt64(rhs.value.1) + (r2 >> 32)
        r.value.1 = UInt32(r1 & 0xffffFFFF)
        let r0 = UInt64(lhs.value.0) + UInt64(rhs.value.0) + (r1 >> 32)
        r.value.0 = UInt32(r0 & 0xffffFFFF)
        return (r, r0 > 0xffffFFFF)
    }
}
public func &+(lhs:UInt128, rhs:UInt128)->UInt128 {
    return UInt128.addWithOverflow(lhs, rhs).0
}
public func +(lhs:UInt128, rhs:UInt128)->UInt128 {
    let (r, overflow) = UInt128.addWithOverflow(lhs, rhs)
    if overflow {
        fatalError("overflow:\(lhs)+\(rhs)")
    }
    return r
}
// to define subtraction, we need to make it comparable
extension UInt128: Comparable {}
public func <(lhs:UInt128, rhs:UInt128)->Bool {
    if lhs.value.0 > rhs.value.0 { return false }
    if lhs.value.0 < rhs.value.0 { return true }
    if lhs.value.1 > rhs.value.1 { return false }
    if lhs.value.1 < rhs.value.1 { return true }
    if lhs.value.2 > rhs.value.2 { return false }
    if lhs.value.2 < rhs.value.2 { return true }
    if lhs.value.3 > rhs.value.3 { return false }
    if lhs.value.3 < rhs.value.3 { return true }
    return false
}
// then all we need for subtraction is two's complement
public prefix func ~(u:UInt128)->UInt128 {
    return UInt128(~u.value.0, ~u.value.1, ~u.value.2, ~u.value.3)
}
public func &-(lhs:UInt128, rhs:UInt128)->UInt128 {
    return lhs &+ ~rhs &+ UInt128(0, 0, 0, 1)
}
public extension UInt128 {
    public static func subtractWithOverflow(lhs:UInt128, _ rhs:UInt128)->(UInt128, overflow:Bool) {
        return (lhs &- rhs, lhs < rhs)
    }
}
// finally, we can define - with overflow check
public func -(lhs:UInt128, rhs:UInt128)->UInt128 {
    let (r, overflow) =  UInt128.subtractWithOverflow(lhs, rhs)
    if overflow {
        fatalError("overflow:\(lhs)-\(rhs)")
    }
    return r
}
// before we define multiplication, let's define shift operators
public func <<(lhs:UInt128, rhs:UInt32)->UInt128 {
    if rhs == 0 { return lhs }
    if rhs > 128 { return UInt128(0) }
    if rhs > 32 {
        return (lhs << 32) << (rhs - 32)
    }
    if rhs == 32 {
        return UInt128(lhs.value.1, lhs.value.2, lhs.value.3, 0)
    }
    var r = UInt128()
    let r3 = (UInt64(lhs.value.3) << UInt64(rhs))
    r.value.3 = UInt32(r3 & 0xffffFFFF)
    let r2 = (UInt64(lhs.value.2) << UInt64(rhs)) | (r3 >> 32)
    r.value.2 = UInt32(r2 & 0xffffFFFF)
    let r1 = (UInt64(lhs.value.1) << UInt64(rhs)) | (r2 >> 32)
    r.value.1 = UInt32(r1 & 0xffffFFFF)
    let r0 = (UInt64(lhs.value.0) << UInt64(rhs)) | (r1 >> 32)
    r.value.0 = UInt32(r0 & 0xffffFFFF)
    return r
}
public func <<(lhs:UInt128, rhs:UInt128)->UInt128 {
    // let s:UInt32 = UInt128(128) < rhs ? rhs.value.3 : 128
    return lhs << UInt32(rhs)
}
public func <<=(inout lhs:UInt128, rhs:UInt32) {
    lhs = lhs << rhs
}
public func <<=(inout lhs:UInt128, rhs:UInt128) {
    lhs = lhs << rhs
}
public func >>(lhs:UInt128, rhs:UInt32)->UInt128 {
    if rhs == 0  { return lhs }
    if rhs > 128 { return UInt128(0) }
    if rhs > 32 {
        return (lhs >> 32) >> (rhs - 32)
    }
    if rhs == 32 {
        return UInt128(0, lhs.value.0, lhs.value.1, lhs.value.2)
    }
    var r = UInt128()
    let rhs_32 = UInt32(32 - Int32(rhs))
    let mask = ~0 >> rhs_32
    r.value.0 = lhs.value.0 >> rhs
    let r1 = (lhs.value.0 & mask) << rhs_32
    r.value.1 = r1 | (lhs.value.1 >> rhs)
    let r2 = (lhs.value.1 & mask) << rhs_32
    r.value.2 = r2 | (lhs.value.2 >> rhs)
    let r3 = (lhs.value.2 & mask) << rhs_32
    r.value.3 = r3 | (lhs.value.3 >> rhs)
    return r
}
public func >>(lhs:UInt128, rhs:UInt128)->UInt128 {
    // let s = UInt128(128) < rhs ? rhs.value.3 : 128
    return lhs >> UInt32(rhs)
}
public func >>=(inout lhs:UInt128, rhs:UInt32) {
    lhs = lhs >> rhs
}
public func >>=(inout lhs:UInt128, rhs:UInt128) {
    lhs = lhs >> rhs
}
// and other bitwise ops
// let us define other binops as well
extension UInt128 : BitwiseOperationsType {
    public static var allZeros: UInt128 { return UInt128(0) }
}
public func & (lhs:UInt128, rhs:UInt128)->UInt128 {
    return UInt128(
            lhs.value.0 & rhs.value.0,
            lhs.value.1 & rhs.value.1,
            lhs.value.2 & rhs.value.2,
            lhs.value.3 & rhs.value.3
    )
}
public func &=(inout lhs:UInt128, rhs:UInt128) {
    lhs = lhs & rhs
}
public func | (lhs:UInt128, rhs:UInt128)->UInt128 {
    return UInt128(
        lhs.value.0 | rhs.value.0,
        lhs.value.1 | rhs.value.1,
        lhs.value.2 | rhs.value.2,
        lhs.value.3 | rhs.value.3
    )
}
public func |=(inout lhs:UInt128, rhs:UInt128) {
    lhs = lhs | rhs
}
public func ^ (lhs:UInt128, rhs:UInt128)->UInt128 {
    return UInt128(
        lhs.value.0 ^ rhs.value.0,
        lhs.value.1 ^ rhs.value.1,
        lhs.value.2 ^ rhs.value.2,
        lhs.value.3 ^ rhs.value.3
    )
}
public func ^=(inout lhs:UInt128, rhs:UInt128) {
    lhs = lhs ^ rhs
}
// multiplication at last!
public extension UInt128 {
    public static func multiplyWithOverflow(lhs:UInt128, _ rhs:UInt32)->(UInt128, overflow:Bool) {
        var r = UInt128()
        let rhs64 = UInt64(rhs)
        let r3 = UInt64(lhs.value.3) * rhs64
        r.value.3 = UInt32(r3 & 0xffffFFFF)
        let r2 = UInt64(lhs.value.2) * rhs64 + (r3 >> 32)
        r.value.2 = UInt32(r2 & 0xffffFFFF)
        let r1 = UInt64(lhs.value.1) * rhs64 + (r2 >> 32)
        r.value.1 = UInt32(r1 & 0xffffFFFF)
        let r0 = UInt64(lhs.value.0) * rhs64 + (r1 >> 32)
        r.value.0 = UInt32(r0 & 0xffffFFFF)
        return (r, r0 > 0xffffFFFF)
    }
    public static func multiplyWithOverflow(lhs:UInt128, _ rhs:UInt64)->(UInt128, overflow:Bool) {
        if rhs <= UInt64(UInt32.max) {
            return multiplyWithOverflow(lhs, UInt32(rhs))
        }
        var r = UInt128()
        var o = false
        let (r3, o3) = multiplyWithOverflow(lhs, rhs & 0xffffFFFF) // lower half
        if o3 {
            return (r3, o3)
        }
        let (r2, o2) = multiplyWithOverflow(lhs, rhs >> 32)         // upper half
        (r, o) = addWithOverflow(r2 << 32, r3)
        return (r,  o || o2 || r2.value.0 != 0)
    }
    public static func multiplyWithOverflow(lhs:UInt128, _ rhs:UInt128)->(UInt128, overflow:Bool) {
        if rhs <= UInt128(UInt32.max) {
            return multiplyWithOverflow(lhs, UInt32(rhs))
        }
        if rhs <= UInt128(UInt64.max) {
            return multiplyWithOverflow(lhs, UInt64(rhs))
        }
        var r = UInt128()
        var o = false
        let (r3, o3) = multiplyWithOverflow(lhs, rhs.value.3)
        if o3 {
            return (r3, o3)
        }
        let (r2, o2) = multiplyWithOverflow(lhs, rhs.value.2)
        (r, o) = addWithOverflow(r2 << 32, r3)
        if o || o2 || r2.value.0 != 0 {
            return (r, true)
        }
        let (r1, o1) = multiplyWithOverflow(lhs, rhs.value.1)
        (r, o) = addWithOverflow(r1 << 64, r)
        if o || o1 || r1.value.0 != 0 || r1.value.1 != 0 {
            return (r, true)
        }
        let (r0, o0) = multiplyWithOverflow(lhs, rhs.value.0)
        (r, o) = addWithOverflow(r0 << 96, r)
        return (r,
            o || o0
                || r0.value.0 != 0
                || r0.value.1 != 0
                || r0.value.2 != 0
        )
    }
}
public func &*(lhs:UInt128, rhs:UInt32)->UInt128 {
    return UInt128.multiplyWithOverflow(lhs, rhs).0
}
public func &*(lhs:UInt128, rhs:UInt64)->UInt128 {
    return UInt128.multiplyWithOverflow(lhs, rhs).0
}
public func &*(lhs:UInt128, rhs:UInt128)->UInt128 {
    return UInt128.multiplyWithOverflow(lhs, rhs).0
}
public func *(lhs:UInt128, rhs:UInt32)->UInt128 {
    let (r, overflow) = UInt128.multiplyWithOverflow(lhs, rhs)
    if overflow {
        fatalError("overflow:\(lhs)*\(rhs)")
    }
    return r
}
public func *(lhs:UInt128, rhs:UInt64)->UInt128 {
    let (r, overflow) = UInt128.multiplyWithOverflow(lhs, rhs)
    if overflow {
        fatalError("overflow:\(lhs)*\(rhs)")
    }
    return r
}
public func *(lhs:UInt128, rhs:UInt128)->UInt128 {
    let (r, overflow) = UInt128.multiplyWithOverflow(lhs, rhs)
    if overflow {
        fatalError("overflow:\(lhs)*\(rhs)")
    }
    return r
}
// now let's divide but start small
public func divmod(lhs:UInt128, _ rhs:UInt32)->(UInt128, UInt32) {
    var r = UInt128()
    let rhs64 = UInt64(rhs)
    r.value.0 = lhs.value.0 / rhs
    let r1 = (UInt64(lhs.value.0 % rhs) << 32) + UInt64(lhs.value.1)
    r.value.1 = UInt32(r1 / rhs64)
    let r2 = ((r1 % rhs64) << 32) + UInt64(lhs.value.2)
    r.value.2 = UInt32(r2 / rhs64)
    let r3 = ((r2 % rhs64) << 32) + UInt64(lhs.value.3)
    r.value.3 = UInt32(r3 / rhs64)
    return (r, UInt32(r3 % rhs64))
}
// with divmod(_:UInt128, _:UInt32)->(UInt128, UInt32) we can stringify
extension UInt128 : CustomStringConvertible, CustomDebugStringConvertible, Hashable {
    public static let int2char = Array("0123456789abcdefghijklmnopqrstuvwxyz".characters)
    public func toString(base:Int = 10)-> String {
        guard 2 <= base && base <= 36 else {
            fatalError("base out of range. \(base) is not within 2...36")
        }
        var u128 = self
        var digits = [Int]()
        repeat {
            var r:UInt32
            (u128, r) = divmod(u128, UInt32(base))
            digits.append(Int(r))
        } while u128 != UInt128.min
        return digits.reverse().map{"\(UInt128.int2char[$0])"}.joinWithSeparator("")

    }
    public var description:String {
        return self.toString()
    }
    public var debugDescription:String {
        return "UInt128(\"" + self.toString(16) + "\",base:16)"
    }
    public static let char2int:[Character:Int] = {
        var result = [Character:Int]()
        for i in 0..<int2char.count {
            result[int2char[i]] = i
        }
        return result
    }()
    public init(_ s:String, base:Int = 10) {
        self.init(0)
        for c in s.characters {
            if let d = UInt128.char2int[c] {
                self = self*UInt128(base) + UInt128(d)
            }
        }
    }
    public var hashValue : Int {    // slow but steady
        return self.description.hashValue
    }
}
// now let's divide by larger values
public func divmod(lhs:UInt128, _ rhs:UInt64)->(UInt128, UInt64) {
    if rhs <= UInt64(UInt32.max) {
        let (q, r) = divmod(lhs, UInt32(rhs))
        return (q, UInt64(r))
    }
    let (h, l) = lhs.asUInt64s
    let hq = h / rhs
    let hr = h % rhs
    // print("hq=\(hq), hr=\(hr)")
    let lqd = Double(UInt128(hr, l)) / Double(rhs)
    let lq = UInt64(lqd)
    let q = UInt128(hq, lq)
    let r = lhs - q * UInt128(rhs)
    // print("q=\(q), r=\(r)")
    return (q, UInt64(r))
}
public extension Double {
    public init(_ u128:UInt128) {
        var d = Double(u128.value.0)
        d = 0x1_0000_0000 * d + Double(u128.value.1)
        d = 0x1_0000_0000 * d + Double(u128.value.2)
        d = 0x1_0000_0000 * d + Double(u128.value.3)
        self = d
    }
}
public func divmod(lhs:UInt128, _ rhs:UInt128)->(UInt128, UInt128) {
    if rhs <= UInt128(UInt32.max) {
        let (q, r) = divmod(lhs, UInt32(rhs))
        return (q, UInt128(r))
    }
    if rhs <= UInt128(UInt64.max) {
        let (q, r) = divmod(lhs, UInt64(rhs))
        return (q, UInt128(r))
    }
    var q = divmod(lhs, UInt64(rhs >> UInt32(64))).0  >> UInt128(64) // could be larger
    var o = false
    var qr = UInt128()
    while true {
        (qr, o) = UInt128.multiplyWithOverflow(q, rhs)
        // print(qr, o)
        if o || qr > lhs {
            // print("\(q) - 1")
            q = q - UInt128(1)
        } else {
            return (q, lhs - qr)
        }
    }
}
// finally, / and %
public func /(lhs:UInt128, rhs:UInt32)->UInt128 {
    return divmod(lhs, rhs).0
}
public func /(lhs:UInt128, rhs:UInt64)->UInt128 {
    return divmod(lhs, rhs).0
}
public func /(lhs:UInt128, rhs:UInt128)->UInt128 {
    return divmod(lhs, rhs).0
}
public func %(lhs:UInt128, rhs:UInt32)->UInt32 {
    return divmod(lhs, rhs).1
}
public func %(lhs:UInt128, rhs:UInt64)->UInt64 {
    return divmod(lhs, rhs).1
}
public func %(lhs:UInt128, rhs:UInt128)->UInt128 {
    return divmod(lhs, rhs).1
}
// and [+-*/%]=
public func +=(inout lhs:UInt128, rhs:UInt128) {
    lhs = lhs + rhs
}
public func -=(inout lhs:UInt128, rhs:UInt128) {
    lhs = lhs - rhs
}
public func *=(inout lhs:UInt128, rhs:UInt128) {
    lhs = lhs * rhs
}
public func /=(inout lhs:UInt128, rhs:UInt128) {
    lhs = lhs / rhs
}
public func %=(inout lhs:UInt128, rhs:UInt128) {
    lhs = lhs + rhs
}
extension UInt128 : IntegerArithmeticType {
    public static func divideWithOverflow(lhs:UInt128, _ rhs:UInt128)->(UInt128, overflow:Bool) {
        return (divmod(lhs, rhs).0, false)
    }
    public static func remainderWithOverflow(lhs:UInt128, _ rhs:UInt128)->(UInt128, overflow:Bool) {
        return (divmod(lhs, rhs).1, false)
    }
    public func toIntMax()->IntMax {
        return IntMax(self)
    }
}
extension UInt128 : RandomAccessIndexType {
    public typealias Distance = Int.Distance
    public func successor() -> UInt128 {
        return self + UInt128(1)
    }
    public func predecessor() -> UInt128 {
        return self - UInt128(1)
    }
    public typealias Stride = Distance
    public func advancedBy(n: Stride) -> UInt128 {
        return self + UInt128(n)
    }
    public func distanceTo(end: UInt128) -> Stride {
        return Distance(self) - Distance(end)
    }
}
extension UInt128 : UnsignedIntegerType {
    public func toUIntMax()->UIntMax {
        return UIntMax(self)
    }
}
// aux. stuff
public extension UInt128 {
    /// give the location of the most significant bit
    /// -1 if none found (meaning zero)
    public var msbAt:Int {
        var msbAt = value.0.msbAt
        if msbAt != 0 { return msbAt + 96 }
        msbAt = value.1.msbAt
        if msbAt != 0 { return msbAt + 64 }
        msbAt = value.2.msbAt
        if msbAt != 0 { return msbAt + 32 }
        return value.3.msbAt
    }
}
//
// now let's go for signed Int128
//
public struct Int128 {
    var value:UInt128
    public init(_ a:UInt32, _ b:UInt32, _ c:UInt32, _ d:UInt32) {
        value = UInt128(a, b, c, d)
    }
    public init(_ h:UInt64, _ l:UInt64) {
        value = UInt128(h, l)
    }
    public init(_ s:UInt128) {
        value = s
    }
    public init(_ s:Int128) {
        value = s.value
    }
    public init() {
        value = UInt128(0, 0, 0, 0)
    }
    public init(_ s:UInt8)  { self.init(0, 0, 0, UInt32(s)) }
    public init(_ s:UInt16) { self.init(0, 0, 0, UInt32(s)) }
    public init(_ s:UInt32) { self.init(0, 0, 0, s) }
    public init(_ s:UInt64) { self.init(0, s) }
    public init(_ s:UInt)   { self.init(0, UInt64(s)) }
    public init(_ s:Int8)   { self = s < 0 ? -Int128(UInt32(-s)) : Int128(UInt32(s)) }
    public init(_ s:Int16)  { self = s < 0 ? -Int128(UInt32(-s)) : Int128(UInt32(s)) }
    public init(_ s:Int32)  { self = s < 0 ? -Int128(UInt32(-s)) : Int128(UInt32(s)) }
    public init(_ s:Int64)  { self = s < 0 ? -Int128(UInt64(-s)) : Int128(UInt64(s)) }
    public init(_ s:Int)    { self = s < 0 ? -Int128(UInt64(-s)) : Int128(UInt64(s)) }
    public init(_ d:Double) {
        self = d < 0 ? -Int128(UInt128(-d)) : Int128(UInt128(d))
    }
    public init(_ f:Float) {
        self.init(Double(f))
    }
    // utility
    public static let min = Int128(UInt128(0x8000_0000,0,0,0))
    public static let max = Int128(UInt128.max >> 1)
    public var isSignMinus:Bool {
        return value.value.0 & 0x8000_0000 != 0
    }
}
public extension Int64 {
    public init(_ i128:Int128) {
        guard i128.abs <= Int128(Int64.max) else {
            fatalError("\(i128) > Int64.max")
        }
        self.init(Int64(i128))
    }
    
}
extension Int128: IntegerLiteralConvertible, _BuiltinIntegerLiteralConvertible {
    public typealias IntegerLiteralType = Int.IntegerLiteralType
    public init(integerLiteral value:IntegerLiteralType) {
        self.init(value)
    }
    public init(_builtinIntegerLiteral value:_MaxBuiltinIntegerType) {
        self.init(UInt64(_builtinIntegerLiteral: value))
    }
}
extension Int128: Equatable {}
public func ==(lhs:Int128, rhs:Int128)->Bool {
    return lhs.value == rhs.value
}
extension Int128: SignedNumberType {}
public prefix func - (i:Int128)->Int128 {
    return Int128(~i.value &+ UInt128(1))
}
public prefix func + (i:Int128)->Int128 {
    return i
}
public extension Int128 {
    public static func addWithOverflow(lhs:Int128, _ rhs:Int128)->(Int128, overflow:Bool) {
        let (r, o) = UInt128.addWithOverflow(lhs.value, rhs.value)
        // if either side is -, overflow is okay
        if lhs.isSignMinus {
            if !rhs.isSignMinus { return (Int128(r), false) }
        } else {
            if  rhs.isSignMinus { return (Int128(r), false) }
        }
        return(Int128(r), o)
    }
    public static func subtractWithOverflow(lhs:Int128, _ rhs:Int128)->(Int128, overflow:Bool) {
        return addWithOverflow(rhs, -lhs)
    }
}
public func &+(lhs:Int128, rhs:Int128)->Int128 {
    return Int128.addWithOverflow(lhs, rhs).0
}
public func +(lhs:Int128, rhs:Int128)->Int128 {
    let (r, overflow) = Int128.addWithOverflow(lhs, rhs)
    if overflow {
        fatalError("overflow:\(lhs)+\(rhs)")
    }
    return Int128(r)
}
public func &-(lhs:Int128, rhs:Int128)->Int128 {
    return lhs &+ (-rhs)
}
public func -(lhs:Int128, rhs:Int128)->Int128 {
    return lhs + (-rhs)
}
extension Int128: AbsoluteValuable {
    public var abs:Int128 {
        return self.isSignMinus ? -self : self
    }
    public static func abs(x: Int128) -> Int128 {
        return x.abs
    }
}
public func abs(i:Int128)->Int128 { return i.abs }
extension Int128: Comparable {}
public func <(lhs:Int128, rhs:Int128)->Bool {
    if lhs.isSignMinus == rhs.isSignMinus {
        return lhs.abs.value < rhs.abs.value
    }
    return lhs.isSignMinus ? true : false
}
public extension Int128 {
    public static func multiplyWithOverflow(lhs:Int128, _ rhs:Int32)->(Int128, overflow:Bool) {
        let isSignMinus = lhs.isSignMinus ? rhs < 0 ? false : true : rhs < 0 ? true : false
        let (r, o) = UInt128.multiplyWithOverflow(lhs.abs.value, UInt32(Swift.abs(rhs)))
        return (isSignMinus ? -Int128(r) : Int128(r) , o)
    }
    public static func multiplyWithOverflow(lhs:Int128, _ rhs:Int64)->(Int128, overflow:Bool) {
        let isSignMinus = lhs.isSignMinus ? rhs < 0 ? false : true : rhs < 0 ? true : false
        let (r, o) = UInt128.multiplyWithOverflow(lhs.abs.value, UInt64(Swift.abs(rhs)))
        return (isSignMinus ? -Int128(r) : Int128(r) , o)
    }
    public static func multiplyWithOverflow(lhs:Int128, _ rhs:Int128)->(Int128, overflow:Bool) {
        let isSignMinus = lhs.isSignMinus ? rhs.isSignMinus ? false : true : rhs.isSignMinus ? true : false
        let (r, o) = UInt128.multiplyWithOverflow(lhs.abs.value, rhs.abs.value)
        return (isSignMinus ? -Int128(r) : Int128(r) , o)
    }
}
public func &*(lhs:Int128, rhs:Int32)->Int128 {
    return Int128.multiplyWithOverflow(lhs, rhs).0
}
public func &*(lhs:Int128, rhs:Int64)->Int128 {
    return Int128.multiplyWithOverflow(lhs, rhs).0
}
public func &*(lhs:Int128, rhs:Int128)->Int128 {
    return Int128.multiplyWithOverflow(lhs, rhs).0
}
public func *(lhs:Int128, rhs:Int32)->Int128 {
    let (r, overflow) = Int128.multiplyWithOverflow(lhs, rhs)
    if overflow {
        fatalError("overflow:\(lhs)*\(rhs)")
    }
    return r
}
public func *(lhs:Int128, rhs:Int64)->Int128 {
    let (r, overflow) = Int128.multiplyWithOverflow(lhs, rhs)
    if overflow {
        fatalError("overflow:\(lhs)*\(rhs)")
    }
    return r
}
public func *(lhs:Int128, rhs:Int128)->Int128 {
    let (r, overflow) = Int128.multiplyWithOverflow(lhs, rhs)
    if overflow {
        fatalError("overflow:\(lhs)*\(rhs)")
    }
    return r
}
extension Int128 : CustomStringConvertible, CustomDebugStringConvertible, Hashable {
    public func toString(base:Int = 10)-> String {
        if self.isSignMinus {
            return "-" + (-self).value.toString(base)
        } else {
            return self.value.toString(base)
        }
    }
    public var description:String {
        return self.toString()
    }
    public var debugDescription:String {
        return "Int128(\"" + self.toString(16) + "\",base:16)"
    }
    public init(_ s:String, base:Int = 10) {
        self.init(0)
        if s[s.startIndex] == "-" || s[s.startIndex] == "+" {
            value = UInt128(s[s.startIndex.successor()..<s.endIndex], base:base)
            if s[s.startIndex] == "-" { self = -self }
        } else {
            value = UInt128(s, base:base)
        }
    }
    public var hashValue : Int {    // slow but steady
        return self.description.hashValue
    }
}
public func divmod(lhs:Int128, _ rhs:Int32)->(Int128, Int32) {
    let isSignMinus = lhs.isSignMinus ? rhs < 0 ? false : true : rhs < 0 ? true : false
    let (q, r) = divmod(lhs.abs.value, UInt32(Swift.abs(rhs)))
    return (isSignMinus ? -Int128(q) : Int128(q) , lhs.isSignMinus ? -Int32(r) : Int32(r))
}
public func divmod(lhs:Int128, _ rhs:Int64)->(Int128, Int64) {
    let isSignMinus = lhs.isSignMinus ? rhs < 0 ? false : true : rhs < 0 ? true : false
    let (q, r) = divmod(lhs.abs.value, UInt64(Swift.abs(rhs)))
    return (isSignMinus ? -Int128(q) : Int128(q) , lhs.isSignMinus ? -Int64(r) : Int64(r))
}
public func divmod(lhs:Int128, _ rhs:Int128)->(Int128, Int128) {
    let isSignMinus = lhs.isSignMinus ? rhs.isSignMinus ? false : true : rhs.isSignMinus ? true : false
    let (q, r) = divmod(lhs.abs.value, rhs.abs.value)
    // print(q, r, isSignMinus, lhs.isSignMinus)
    return (isSignMinus ? -Int128(q) : Int128(q) , lhs.isSignMinus ? -Int128(r) : Int128(r))
}
public func /(lhs:Int128, rhs:Int32)->Int128 {
    return divmod(lhs, rhs).0
}
public func /(lhs:Int128, rhs:Int64)->Int128 {
    return divmod(lhs, rhs).0
}
public func /(lhs:Int128, rhs:Int128)->Int128 {
    return divmod(lhs, rhs).0
}
public func %(lhs:Int128, rhs:Int32)->Int32 {
    return divmod(lhs, rhs).1
}
public func %(lhs:Int128, rhs:Int64)->Int64 {
    return divmod(lhs, rhs).1
}
public func %(lhs:Int128, rhs:Int128)->Int128 {
    return divmod(lhs, rhs).1
}
extension Int128 : IntegerArithmeticType {
    public static func divideWithOverflow(lhs:Int128, _ rhs:Int128)->(Int128, overflow:Bool) {
        return (divmod(lhs, rhs).0, false)
    }
    public static func remainderWithOverflow(lhs:Int128, _ rhs:Int128)->(Int128, overflow:Bool) {
        return (divmod(lhs, rhs).1, false)
    }
    public func toIntMax()->IntMax {
        return IntMax(self)
    }
}
// bitwise ops
extension Int128 : BitwiseOperationsType {
    public static var allZeros: Int128 { return Int128(0) }
}
public prefix func ~(i:Int128)->Int128 {
    return Int128(~i.value)
}
public func & (lhs:Int128, rhs:Int128)->Int128 {
    return Int128(lhs.value & rhs.value)
}
public func &=(inout lhs:Int128, rhs:Int128) {
    lhs = lhs & rhs
}
public func | (lhs:Int128, rhs:Int128)->Int128 {
    return Int128(lhs.value | rhs.value)
}
public func |=(inout lhs:Int128, rhs:Int128) {
    lhs = lhs | rhs
}
public func ^ (lhs:Int128, rhs:Int128)->Int128 {
    return Int128(lhs.value ^ rhs.value)
}
public func ^=(inout lhs:Int128, rhs:Int128) {
    lhs = lhs ^ rhs
}
public func <<(lhs:Int128, rhs:Int128)->Int128 {
    let u = lhs.abs.value << rhs.abs.value
    return lhs.isSignMinus ? -Int128(u) : Int128(u)
}
public func <<=(inout lhs:Int128, rhs:Int128) {
    lhs = lhs << rhs
}
public func >>(lhs:Int128, rhs:Int128)->Int128 {
    let u = lhs.abs.value >> rhs.abs.value
    return lhs.isSignMinus ? -Int128(u) : Int128(u)
}
public func >>=(inout lhs:Int128, rhs:Int128) {
    lhs = lhs >> rhs
}
// and [+-*/%]=
public func +=(inout lhs:Int128, rhs:Int128) {
    lhs = lhs + rhs
}
public func -=(inout lhs:Int128, rhs:Int128) {
    lhs = lhs - rhs
}
public func *=(inout lhs:Int128, rhs:Int128) {
    lhs = lhs * rhs
}
public func /=(inout lhs:Int128, rhs:Int128) {
    lhs = lhs / rhs
}
public func %=(inout lhs:Int128, rhs:Int128) {
    lhs = lhs + rhs
}
extension Int128 : RandomAccessIndexType {
    public typealias Distance = Int.Distance
    public func successor() -> Int128 {
        return self + Int128(1)
    }
    public func predecessor() -> Int128 {
        return self - Int128(1)
    }
    public typealias Stride = Distance
    public func advancedBy(n: Stride) -> Int128 {
        return self + n
    }
    public func distanceTo(end: Int128) -> Stride {
        return self - end
    }
}
extension Int128: SignedIntegerType {}

