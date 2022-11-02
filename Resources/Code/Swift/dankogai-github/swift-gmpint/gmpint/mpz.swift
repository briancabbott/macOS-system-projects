//
//  mpz.swift
//  gmpint
//
//  Created by Dan Kogai on 7/5/14.
//  Copyright (c) 2014 Dan Kogai. All rights reserved.
//

import Darwin
/// Big Integer by GMP
class GMPInt {
    private var mpz = mpz_t()
    init(){ gmpint_seti(&mpz, 0)}
    init(_ mpz:mpz_t) { self.mpz = mpz }
    init(_ s:String, base:Int=10){
        s.withCString {
            gmpint_sets(&self.mpz, $0, Int32(base))
        }
    }
    // to work around the difference between
    // GMP's 32-bit int and OS X's 64-bit int,
    // we use string even for ints
    convenience init(_ i:Int) { self.init(String(i)) }
    deinit {
        gmpint_unset(&mpz)
    }
    func toInt() -> Int? {
        return gmpint_fits_int(&mpz) == 0 ?
            nil : Int(gmpint2int(&mpz))
    }
    var asInt: Int? {
        return self.toInt()
    }
}
extension GMPInt: Printable {
    func toString(base:Int=10)->String {
        let cstr = gmpint2str(&mpz, Int32(base))
        let result = String.fromCString(cstr)
        free(cstr)
        return result!
    }
    var description:String { return toString() }
}
extension GMPInt: Equatable, Comparable {}
func <(lhs:GMPInt, rhs:GMPInt)->Bool {
    return gmpint_cmp(&lhs.mpz, &rhs.mpz) < 0
}
func ==(lhs:GMPInt, rhs:GMPInt)->Bool {
    return gmpint_cmp(&lhs.mpz, &rhs.mpz) == 0
}
/// unary +
prefix func +(op:GMPInt) -> GMPInt { return op }
/// unary -
prefix func -(op:GMPInt) -> GMPInt {
    var rop = GMPInt()
    gmpint_negz(&rop.mpz, &op.mpz)
    return rop
}
/// abs
func abs(op:GMPInt)->GMPInt {
    var rop = GMPInt()
    gmpint_absz(&rop.mpz, &op.mpz)
    return rop
}
/// <<, left bit shift
func <<(lhs:GMPInt, bits:UInt) -> GMPInt {
    var rop = GMPInt()
    gmpint_lshift(&rop.mpz, &lhs.mpz, bits)
    return rop
}
/// <<=
func <<=(inout lhs:GMPInt, bits:UInt) -> GMPInt {
    gmpint_lshift(&lhs.mpz, &lhs.mpz, bits)
    return lhs
}
/// >>, right bit shift
func >>(lhs:GMPInt, bits:UInt) -> GMPInt {
    var rop = GMPInt()
    gmpint_rshift(&rop.mpz, &lhs.mpz, bits)
    return rop
}
/// >>=
func >>=(inout lhs:GMPInt, bits:UInt) -> GMPInt {
    gmpint_rshift(&lhs.mpz, &lhs.mpz, bits)
    return lhs
}
/// binary +
func +(lhs:GMPInt, rhs:GMPInt) -> GMPInt {
    var rop = GMPInt()
    gmpint_addz(&rop.mpz, &lhs.mpz, &rhs.mpz)
    return rop
}
func +(lhs:GMPInt, rhs:Int) -> GMPInt {
    return lhs + GMPInt(rhs)
}
func +(lhs:Int, rhs:GMPInt) -> GMPInt {
    return GMPInt(lhs) + rhs
}
/// +=
func +=(inout lhs:GMPInt, rhs:GMPInt) -> GMPInt {
    gmpint_addz(&lhs.mpz, &lhs.mpz, &rhs.mpz)
    return lhs
}
func +=(inout lhs:GMPInt, rhs:Int) -> GMPInt {
    lhs += GMPInt(rhs)
    return lhs
}
/// binary -
func -(lhs:GMPInt, rhs:GMPInt) -> GMPInt {
    var rop = GMPInt()
    gmpint_subz(&rop.mpz, &lhs.mpz, &rhs.mpz)
    return rop
}
func -(lhs:GMPInt, rhs:Int) -> GMPInt {
    return lhs - GMPInt(rhs)
}
func -(lhs:Int, rhs:GMPInt) -> GMPInt {
    return GMPInt(lhs) - rhs
}
/// -=
func -=(inout lhs:GMPInt, rhs:GMPInt) -> GMPInt {
    gmpint_subz(&lhs.mpz, &lhs.mpz, &rhs.mpz)
    return lhs
}
func -=(inout lhs:GMPInt, rhs:Int) -> GMPInt {
    lhs -= GMPInt(rhs)
    return lhs
}
/// binary *
func *(lhs:GMPInt, rhs:GMPInt) -> GMPInt {
    var rop = GMPInt()
    gmpint_mulz(&rop.mpz, &lhs.mpz, &rhs.mpz)
    return rop
}
func *(lhs:GMPInt, rhs:Int) -> GMPInt {
    return lhs * GMPInt(rhs)
}
func *(lhs:Int, rhs:GMPInt) -> GMPInt {
    return GMPInt(lhs) * rhs
}
/// *=
func *=(inout lhs:GMPInt, rhs:GMPInt) -> GMPInt {
    gmpint_mulz(&lhs.mpz, &lhs.mpz, &rhs.mpz)
    return lhs
}
func *=(inout lhs:GMPInt, rhs:Int) -> GMPInt {
    lhs *= GMPInt(rhs)
    return lhs
}
/// /%, the divmod operator
infix operator /% { precedence 150 associativity left }
func /%(lhs:GMPInt, rhs:GMPInt) -> (GMPInt, GMPInt) {
    var r = GMPInt(), q = GMPInt()
    // GMP 6.0.0 + MacPorts + Yosemite had a bug that
    //   libdyld.dylib`stack_not_16_byte_aligned_error:
    // when rhs fits uint.
    // to work around it, we left-shift both sides with 64
    // then right shift the remainder w/ 64
    // which got fixed in GMP 6.0.0_1
    /*
    var n = lhs << 64
    var d = rhs << 64
    gmpint_divmodz(&r.mpz, &q.mpz, &n.mpz, &d.mpz)
    return (r, q >> 64)
    */
    gmpint_divmodz(&r.mpz, &q.mpz, &lhs.mpz, &rhs.mpz)
    return (r, q)
}
func /%(lhs:GMPInt, rhs:Int) -> (GMPInt, GMPInt) {
    return lhs /% GMPInt(rhs)
}
func /%(lhs:Int, rhs:GMPInt) -> (GMPInt, GMPInt) {
    return GMPInt(lhs) /% rhs
}
/// binary /
func /(lhs:GMPInt, rhs:GMPInt) -> GMPInt {
    return (lhs /% rhs).0
}
func /(lhs:GMPInt, rhs:Int) -> GMPInt {
    return (lhs /% rhs).0
}
func /(lhs:Int, rhs:GMPInt) -> GMPInt {
    return (lhs /% rhs).0
}
/// /=
func /=(inout lhs:GMPInt, rhs:GMPInt) -> GMPInt {
    lhs = lhs / rhs
    return lhs
}
func /=(inout lhs:GMPInt, rhs:Int) -> GMPInt {
    lhs = lhs / rhs
    return lhs
}
/// binary %
func %(lhs:GMPInt, rhs:GMPInt) -> GMPInt {
    return (lhs /% rhs).1
}
func %(lhs:GMPInt, rhs:Int) -> GMPInt {
    return (lhs /% rhs).1
}
func %(lhs:Int, rhs:GMPInt) -> GMPInt {
    return (lhs /% rhs).1
}
/// /=
func %=(inout lhs:GMPInt, rhs:GMPInt) -> GMPInt {
    lhs = lhs % rhs
    return lhs
}
func %=(inout lhs:GMPInt, rhs:Int) -> GMPInt {
    lhs = lhs % rhs
    return lhs
}
/// ** pow
infix operator ** { associativity right precedence 170 }
func **(lhs:GMPInt, rhs:UInt) -> GMPInt {
    var rop = GMPInt()
    gmpint_powui(&rop.mpz, &lhs.mpz, rhs)
    return rop
}
/// **=
infix operator **= { associativity right precedence 90 }
func **=(inout lhs:GMPInt, rhs:UInt) -> GMPInt {
    gmpint_powui(&lhs.mpz, &lhs.mpz, rhs)
    return lhs
}
/// other methods
extension GMPInt {
    func powmod(ext:GMPInt, mod:GMPInt) -> GMPInt {
        var rop = GMPInt()
        gmpint_powmodz(&rop.mpz, &self.mpz, &ext.mpz, &mod.mpz)
        return rop
    }
}
