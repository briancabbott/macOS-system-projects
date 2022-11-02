//: [Previous](@previous)

import Int2X

typealias U16 = UInt2X<UInt8>
typealias I16 = Int2X<UInt8>

var i16:I16 = 0
i16 == I16.min

U16.min == 0
I16.min
I16.min == 0 // FIXME: wrongly true
0 == I16.min // so is this one
I16.min == I16(0) // but this one is correctly false
I16.max == 0x7fff // similar symptom
I16.max == I16(0x7fff)

+I16(1) * +I16(1)
-I16(1) * +I16(1)
+I16(1) * -I16(1)
-I16(1) * -I16(1)

+I16(42).quotientAndRemainder(dividingBy: +I16(5)).quotient
-I16(42).quotientAndRemainder(dividingBy: +I16(5)).quotient
+I16(42).quotientAndRemainder(dividingBy: -I16(5)).quotient
-I16(42).quotientAndRemainder(dividingBy: -I16(5)).quotient
+I16(42).quotientAndRemainder(dividingBy: +I16(5)).remainder
-I16(42).quotientAndRemainder(dividingBy: +I16(5)).remainder
+I16(42).quotientAndRemainder(dividingBy: -I16(5)).remainder
-I16(42).quotientAndRemainder(dividingBy: -I16(5)).remainder

(I16(0)..<I16(8))[4]

Int128.min.toString(radix:16)
Int256.min.toString(radix:16)
Int512.min.toString(radix:16)

func fact<T:FixedWidthInteger>(_ n:T)->T {
    return n == 0 ? 1 : (1...Int(n)).map{ T($0) }.reduce(1, *)
}

fact(Int128(34)) // wrongly negative
fact(Int256(34))
var i256:Int256 = "-0x1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"

//: [Next](@next)
