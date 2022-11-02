//
//  main.swift
//  genericmath
//
//  Created by Dan Kogai on 1/30/16.
//  Copyright © 2016 Dan Kogai. All rights reserved.
//
let test = TAP()
print("#### UInt128")
test.eq(UInt128.min.description, "0", "UInt128.min == 0")
test.eq(UInt128("0"), UInt128.min,    "Unt128(\"0\") == UInt128.min")
let u128maxS = "340282366920938463463374607431768211455"
test.eq(UInt128.max.description, u128maxS, "UInt128.max == \(u128maxS)")
test.eq(UInt128(u128maxS), UInt128.max, "UInt128(\"\(u128maxS)\") == UInt128.max")
let palindromeI  = UInt128(0x0123456789abcdef, 0xfedcba9876543210)
let palindromeS =  "123456789abcdeffedcba9876543210"
test.eq(palindromeI.toString(16), palindromeS, "0x\"\(palindromeS)\"")
test.eq(UInt128(palindromeS, base:16), palindromeI, "UInt128(\"\(palindromeS)\",base:16)")
let u128maxDS = "UInt128(\"ffffffffffffffffffffffffffffffff\",base:16)"
test.eq(UInt128.max.debugDescription, u128maxDS, u128maxDS)
/*
for i:UInt32 in 0...128 {
    test.eq((UInt128.max >> i).msb, Int(128 - i), "(UInt128.max >> \(i)).msb = \(128-i)")
}
*/
print("#### Int128")
let i128minS = "-170141183460469231731687303715884105728"
let i128maxS = "170141183460469231731687303715884105727"
test.eq(Int128.min.description, i128minS, "Int128.min == \(i128minS)")
test.eq(Int128(i128minS), Int128.min, "Int128(\"\(i128minS)\") == Int128.min")
test.eq(Int128.max.description, i128maxS, "Int128.max == \(i128maxS)")
test.eq(Int128(i128maxS), Int128.max, "Int128(\"\(i128maxS)\") == Int128.max")
test.eq(Int128("+"+i128maxS), Int128.max, "Int128(\"+\(i128maxS)\") == Int128.max")
let i128minDS = "Int128(\"-80000000000000000000000000000000\",base:16)"
let i128maxDS = "Int128(\"7fffffffffffffffffffffffffffffff\",base:16)"
test.eq(Int128.min.debugDescription, i128minDS, i128minDS)
test.eq(Int128.max.debugDescription, i128maxDS, i128maxDS)
test.eq(Int128.min + Int128.max, Int128(-1),    "Int128.min + Int128.max == -1")
test.ok(Int128.min < Int128.max, "Int128.min < Int128.max")
test.ok(abs(Int128.min+Int128(1)) > abs(Int128.max-Int128(1)),   "abs(Int128.min+1) > abs(Int128.max-1)")
let int64maxSQ = Int128("85070591730234615847396907784232501249")
test.eq(+Int128(Int64.max) * +Int128(Int64.max), +int64maxSQ, "+ * + == +")
test.eq(+Int128(Int64.max) * -Int128(Int64.max), -int64maxSQ, "+ * - == -")
test.eq(-Int128(Int64.max) * +Int128(Int64.max), -int64maxSQ, "- * + == -")
test.eq(-Int128(Int64.max) * -Int128(Int64.max), +int64maxSQ, "- * - == +")
let m31 = Int64(Int32.max)
let f5 = Int64(UInt16.max) + Int64(2)
test.eq(+Int128(m31) / +Int128(f5), Int128(+m31 / +f5), "+m31 / +f5 == \(+m31 / +f5)")
test.eq(+Int128(m31) % +Int128(f5), Int128(+m31 % +f5), "+m31 % +f5 == \(+m31 % +f5)")
test.eq(+Int128(m31) / -Int128(f5), Int128(+m31 / -f5), "+m31 / -f5 == \(+m31 / -f5)")
test.eq(+Int128(m31) % -Int128(f5), Int128(+m31 % -f5), "+m31 % -f5 == \(+m31 % -f5)")
test.eq(-Int128(m31) / +Int128(f5), Int128(-m31 / +f5), "-m31 / +f5 == \(-m31 / +f5)")
test.eq(-Int128(m31) % +Int128(f5), Int128(-m31 % +f5), "-m31 % +f5 == \(-m31 % +f5)")
test.eq(-Int128(m31) / -Int128(f5), Int128(-m31 / -f5), "-m31 / -f5 == \(-m31 / -f5)")
test.eq(-Int128(m31) % -Int128(f5), Int128(-m31 % -f5), "-m31 % -f5 == \(-m31 % -f5)")
let pp127 = Int128("170141183460469231731687303715884105703")
let pp95  = Int128("39614081257132168796771975153")
let aqo127_95 = Int128(4294967296)
let aro127_95 = Int128(64424509415)
test.eq(+Int128(pp127) / +Int128(pp95), +Int128(aqo127_95), "+\(pp127) / \(pp95) +\(aqo127_95)")
test.eq(+Int128(pp127) % +Int128(pp95), +Int128(aro127_95), "+\(pp127) % \(pp95) +\(aro127_95)")
test.eq(+Int128(pp127) / -Int128(pp95), -Int128(aqo127_95), "+\(pp127) / \(pp95) -\(aqo127_95)")
test.eq(+Int128(pp127) % -Int128(pp95), +Int128(aro127_95), "+\(pp127) % \(pp95) +\(aro127_95)")
test.eq(-Int128(pp127) / +Int128(pp95), -Int128(aqo127_95), "-\(pp127) / \(pp95) -\(aqo127_95)")
test.eq(-Int128(pp127) % +Int128(pp95), -Int128(aro127_95), "-\(pp127) % \(pp95) +\(aro127_95)")
test.eq(-Int128(pp127) / -Int128(pp95), +Int128(aqo127_95), "-\(pp127) / \(pp95) +\(aqo127_95)")
test.eq(-Int128(pp127) % -Int128(pp95), -Int128(aro127_95), "-\(pp127) % \(pp95) -\(aro127_95)")
// check generics
protocol Integer: IntegerArithmeticType, SignedIntegerType {
    init(_:Self)
}
extension Int: Integer {}
extension Int128: Integer {}
func genericSum<N:Integer>(b:N, _ e:N)->N {
    if b > e { return genericSum(e, b) }
    return (b...e).reduce(N(0), combine:+)
    
}
func genericProduct<N:Integer>(b:N, _ e:N)->N {
    if b > e { return genericProduct(e, b) }
    return (b...e).reduce(N(1), combine:*)

}
test.eq(Int128(genericSum(1,100)), genericSum(Int128(1),Int128(100)),
        "Int128(genericSum(1,100)) == genericSum(Int128(1),Int128(100))")
test.eq(Int128(genericProduct(1,16)), genericProduct(Int128(1),Int128(16)),
        "Int128(genericProduct(1,16)) == genericProduct(Int128(1),Int128(16))")
({
    func P(start:Int128, _ end:Int128)->Int128 {
        return genericProduct(start,end)
    }
    func F(n:Int128)->Int128 {
        return n < 2 ? 1 : (2...n).reduce(1,combine:*)
    }
    for i in 1...16 {
        let (b, e) = (Int128(i), Int128(i*2))
        test.eq(F(e)/F(b), P(b+1,e),"\(e)!/\(b)! == \(b+1)P\(e)")
    }
})()
// check if they are correctly arithmetic-shifting
test.eq(Int128(-1)<<1, Int128(-2), "Int128(-1)<<1 == Int128(-2)")
test.eq(Int128(-2)>>1, Int128(-1), "Int128(-2)>>1 == Int128(-1)")
//
print("#### Float128")
#if os(Linux)
    import Glibc
#else
    import Darwin
#endif
[
    +0.0, -0.0, +Double.infinity, -Double.infinity, +1.0/7.0, -1.0/7.0,
    M_PI, M_E, M_LN2, M_LN10, M_SQRT2, M_SQRT1_2,
    DBL_MAX, DBL_MIN
].forEach {
    test.eq(Float128($0).asDouble, $0,   "Float128(\($0)).asDouble == \($0)")
}
test.eq((+Float128(0.0)).isZero,            true,   "+Float128(0.0) is Zero")
test.eq((-Float128(0.0)).isZero,            true,   "-Float128(0.0) is Zero")
test.eq((+Float128(0.0)).isSignMinus,       false,  "+Float128(0.0) is positive")
test.eq((-Float128(0.0)).isSignMinus,       true,   "-Float128(0.0) is negative")
test.eq((+Float128.infinity).isInfinite,    true,   "+Float128.infinity is Infinite")
test.eq((-Float128.infinity).isInfinite,    true,   "-Float128.infinity is Infinite")
test.eq((+Float128.infinity).isSignMinus,   false,  "+Float128.infinity is positive")
test.eq((-Float128.infinity).isSignMinus,   true,   "-Float128.infinity is negative")
test.eq(Float128.NaN.isNaN,                 true,   "Float128.NaN is NaN")
test.eq(Float128.quietNaN.isNaN,            true,   "Float128.quietNaN is NaN")
test.ne(Float128.NaN, Float128.NaN,                 "Float128.NaN != itself")
test.ne(Float128.quietNaN, Float128.quietNaN,       "Float128.quetNaN != itself")
test.eq(Float128(-Double.infinity) < Float128(+Double.infinity), true, "-infinity < +infinity")
test.eq(Float128(-Double.infinity) < Float128(0), true, "-infinity < 0")
test.eq(Float128(0) < Float128(+Double.infinity), true, "0 < +infinity")
test.eq(Float128(-2.0) < Float128(+1.0), true, "-2.0 < +1.0")
test.eq(Float128(-2.5) < Float128(-2.0), true, "-2.5 < -2.0")
test.eq(Float128(+2.0) < Float128(+2.5), true, "+2.0 < +2.5")
/*
for i:UInt64 in 0...63 {
    let v = UInt64.max >> i
    test.eq(Float128(v).asUInt64, v, "Float128(UInt64.max >> \(i)) == \(v)")
}
for i:Int64 in 0...62 {
    let v = Int64.max >> i
    test.eq(Float128(+v).asInt64,  v, "Float128(+(Int64.max >> \(i))) == +\(v)")
    test.eq(Float128(-v).asInt64, -v, "Float128(-(Int64.max >> \(i))) == -\(v)")
}
*/
test.done()
