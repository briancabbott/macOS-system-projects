//: [Previous](@previous)

import Int2X

typealias U16 = UInt2X<UInt8>

var u16 = U16()
u16 == u16
u16.hashValue

u16 = 0xfedc

u16 + U16(1)
U16(0xFFFF).multipliedHalfWidth(by: 0xff)
U16(0xFFFF).multipliedFullWidth(by: 0xffff)
(U16(0xff) * U16(0xff))

U16(0xffff).quotientAndRemainder(dividingBy: U16(0x77e)).remainder

UInt16(0xffff).dividingFullWidth((high:UInt16(0xfffe), low:UInt16(0xffff)))
U16(0xffff).dividingFullWidth((high:U16(0xfffe), low:U16(0xffff)))

U16(255) + U16(1)
U16(0) - U16(0)
////

func fact<T:FixedWidthInteger>(_ n:T)->T {
    return n == 0 ? 1 : (1...Int(n)).map{ T($0) }.reduce(1, *)
}
var u128  = fact(UInt128(34))
var u256  = fact(UInt256(57))
var u512  = fact(UInt512(98))
var u1024 = fact(UInt1024(170))
print(u1024.description)

////: [Next](@next)
