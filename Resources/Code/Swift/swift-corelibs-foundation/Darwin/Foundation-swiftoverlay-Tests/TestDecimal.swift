//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import Foundation
import XCTest

class TestDecimal : XCTestCase {
    func test_AdditionWithNormalization() {
        
        let biggie = Decimal(65536)
        let smallee = Decimal(65536)
        let answer = biggie/smallee
        XCTAssertEqual(Decimal(1),answer)
        
        var one = Decimal(1)
        var addend = Decimal(1)
        var expected = Decimal()
        var result = Decimal()
        
        expected._isNegative = 0;
        expected._isCompact = 0;
        
        // 2 digits -- certain to work
        addend._exponent = -1;
        XCTAssertEqual(.noError, NSDecimalAdd(&result, &one, &addend, .plain), "1 + 0.1")
        expected._exponent = -1;
        expected._length = 1;
        expected._mantissa.0 = 11;
        XCTAssertEqual(.orderedSame, NSDecimalCompare(&expected, &result), "1.1 == 1 + 0.1")
        
        // 38 digits -- guaranteed by NSDecimal to work
        addend._exponent = -37;
        XCTAssertEqual(.noError, NSDecimalAdd(&result, &one, &addend, .plain), "1 + 1e-37")
        expected._exponent = -37;
        expected._length = 8;
        expected._mantissa.0 = 0x0001;
        expected._mantissa.1 = 0x0000;
        expected._mantissa.2 = 0x36a0;
        expected._mantissa.3 = 0x00f4;
        expected._mantissa.4 = 0x46d9;
        expected._mantissa.5 = 0xd5da;
        expected._mantissa.6 = 0xee10;
        expected._mantissa.7 = 0x0785;
        XCTAssertEqual(.orderedSame, NSDecimalCompare(&expected, &result), "1 + 1e-37")
        
        // 39 digits -- not guaranteed to work but it happens to, so we make the test work either way
        addend._exponent = -38;
        let error = NSDecimalAdd(&result, &one, &addend, .plain)
        XCTAssertTrue(error == .noError || error == .lossOfPrecision, "1 + 1e-38")
        if error == .noError {
            expected._exponent = -38;
            expected._length = 8;
            expected._mantissa.0 = 0x0001;
            expected._mantissa.1 = 0x0000;
            expected._mantissa.2 = 0x2240;
            expected._mantissa.3 = 0x098a;
            expected._mantissa.4 = 0xc47a;
            expected._mantissa.5 = 0x5a86;
            expected._mantissa.6 = 0x4ca8;
            expected._mantissa.7 = 0x4b3b;
            XCTAssertEqual(.orderedSame, NSDecimalCompare(&expected, &result), "1 + 1e-38")
        } else {
            XCTAssertEqual(.orderedSame, NSDecimalCompare(&one, &result), "1 + 1e-38")
        }
        
        // 40 digits -- doesn't work; need to make sure it's rounding for us
        addend._exponent = -39;
        XCTAssertEqual(.lossOfPrecision, NSDecimalAdd(&result, &one, &addend, .plain), "1 + 1e-39")
        XCTAssertEqual("1", result.description)
        XCTAssertEqual(.orderedSame, NSDecimalCompare(&one, &result), "1 + 1e-39")
    }

    func test_BasicConstruction() {
        let zero = Decimal()
        XCTAssertEqual(20, MemoryLayout<Decimal>.size)
        XCTAssertEqual(0, zero._exponent)
        XCTAssertEqual(0, zero._length)
        XCTAssertEqual(0, zero._isNegative)
        XCTAssertEqual(0, zero._isCompact)
        XCTAssertEqual(0, zero._reserved)
        let (m0, m1, m2, m3, m4, m5, m6, m7) = zero._mantissa
        XCTAssertEqual(0, m0)
        XCTAssertEqual(0, m1)
        XCTAssertEqual(0, m2)
        XCTAssertEqual(0, m3)
        XCTAssertEqual(0, m4)
        XCTAssertEqual(0, m5)
        XCTAssertEqual(0, m6)
        XCTAssertEqual(0, m7)
        XCTAssertEqual(8, NSDecimalMaxSize)
        XCTAssertEqual(32767, NSDecimalNoScale)
        XCTAssertFalse(zero.isNormal)
        XCTAssertTrue(zero.isFinite)
        XCTAssertTrue(zero.isZero)
        XCTAssertFalse(zero.isSubnormal)
        XCTAssertFalse(zero.isInfinite)
        XCTAssertFalse(zero.isNaN)
        XCTAssertFalse(zero.isSignaling)

        if #available(macOS 10.15, iOS 13, tvOS 13, watchOS 6, *) {
            let d1 = Decimal(1234567890123456789 as UInt64)
            XCTAssertEqual(d1._exponent, 0)
            XCTAssertEqual(d1._length, 4)
        }
    }
    func test_Constants() {
        XCTAssertEqual(8, NSDecimalMaxSize)
        XCTAssertEqual(32767, NSDecimalNoScale)
        let smallest = Decimal(_exponent: 127, _length: 8, _isNegative: 1, _isCompact: 1, _reserved: 0, _mantissa: (UInt16.max, UInt16.max, UInt16.max, UInt16.max, UInt16.max, UInt16.max, UInt16.max, UInt16.max))
        XCTAssertEqual(smallest, -Decimal.greatestFiniteMagnitude)
        let biggest = Decimal(_exponent: 127, _length: 8, _isNegative: 0, _isCompact: 1, _reserved: 0, _mantissa: (UInt16.max, UInt16.max, UInt16.max, UInt16.max, UInt16.max, UInt16.max, UInt16.max, UInt16.max))
        XCTAssertEqual(biggest, Decimal.greatestFiniteMagnitude)
        let leastNormal = Decimal(_exponent: -128, _length: 1, _isNegative: 0, _isCompact: 1, _reserved: 0, _mantissa: (1, 0, 0, 0, 0, 0, 0, 0))
        XCTAssertEqual(leastNormal, Decimal.leastNormalMagnitude)
        let leastNonzero = Decimal(_exponent: -128, _length: 1, _isNegative: 0, _isCompact: 1, _reserved: 0, _mantissa: (1, 0, 0, 0, 0, 0, 0, 0))
        XCTAssertEqual(leastNonzero, Decimal.leastNonzeroMagnitude)
        let pi = Decimal(_exponent: -38, _length: 8, _isNegative: 0, _isCompact: 1, _reserved: 0, _mantissa: (0x6623, 0x7d57, 0x16e7, 0xad0d, 0xaf52, 0x4641, 0xdfa7, 0xec58))
        XCTAssertEqual(pi, Decimal.pi)
        XCTAssertEqual(10, Decimal.radix)
        XCTAssertTrue(Decimal().isCanonical)
        XCTAssertFalse(Decimal().isSignalingNaN)
        XCTAssertFalse(Decimal.nan.isSignalingNaN)
        XCTAssertTrue(Decimal.nan.isNaN)
        XCTAssertEqual(.quietNaN, Decimal.nan.floatingPointClass)
        XCTAssertEqual(.positiveZero, Decimal().floatingPointClass)
        XCTAssertEqual(.negativeNormal, smallest.floatingPointClass)
        XCTAssertEqual(.positiveNormal, biggest.floatingPointClass)
        XCTAssertFalse(Double.nan.isFinite)
        XCTAssertFalse(Double.nan.isInfinite)
    }

    func test_Description() {
        XCTAssertEqual("0", Decimal().description)
        XCTAssertEqual("0", Decimal(0).description)
        XCTAssertEqual("10", Decimal(_exponent: 1, _length: 1, _isNegative: 0, _isCompact: 1, _reserved: 0, _mantissa: (1, 0, 0, 0, 0, 0, 0, 0)).description)
        XCTAssertEqual("10", Decimal(10).description)
        XCTAssertEqual("123.458", Decimal(_exponent: -3, _length: 2, _isNegative: 0, _isCompact:1, _reserved: 0, _mantissa: (57922, 1, 0, 0, 0, 0, 0, 0)).description)
        XCTAssertEqual("123.458", Decimal(123.458).description)
        XCTAssertEqual("123", Decimal(UInt8(123)).description)
        XCTAssertEqual("45", Decimal(Int8(45)).description)
        XCTAssertEqual("3.14159265358979323846264338327950288419", Decimal.pi.description)
        XCTAssertEqual("-30000000000", Decimal(sign: .minus, exponent: 10, significand: Decimal(3)).description)
        XCTAssertEqual("300000", Decimal(sign: .plus, exponent: 5, significand: Decimal(3)).description)
        XCTAssertEqual("5", Decimal(signOf: Decimal(3), magnitudeOf: Decimal(5)).description)
        XCTAssertEqual("-5", Decimal(signOf: Decimal(-3), magnitudeOf: Decimal(5)).description)
        XCTAssertEqual("5", Decimal(signOf: Decimal(3), magnitudeOf: Decimal(-5)).description)
        XCTAssertEqual("-5", Decimal(signOf: Decimal(-3), magnitudeOf: Decimal(-5)).description)
    }

    func test_ExplicitConstruction() {
        var explicit = Decimal(
            _exponent: 0x17f,
            _length: 0xff,
            _isNegative: 3,
            _isCompact: 4,
            _reserved: UInt32(1<<18 + 1<<17 + 1),
            _mantissa: (6, 7, 8, 9, 10, 11, 12, 13)
        )
        XCTAssertEqual(0x7f, explicit._exponent)
        XCTAssertEqual(0x7f, explicit.exponent)
        XCTAssertEqual(0x0f, explicit._length)
        XCTAssertEqual(1, explicit._isNegative)
        XCTAssertEqual(FloatingPointSign.minus, explicit.sign)
        XCTAssertTrue(explicit.isSignMinus)
        XCTAssertEqual(0, explicit._isCompact)
        XCTAssertEqual(UInt32(1<<17 + 1), explicit._reserved)
        let (m0, m1, m2, m3, m4, m5, m6, m7) = explicit._mantissa
        XCTAssertEqual(6, m0)
        XCTAssertEqual(7, m1)
        XCTAssertEqual(8, m2)
        XCTAssertEqual(9, m3)
        XCTAssertEqual(10, m4)
        XCTAssertEqual(11, m5)
        XCTAssertEqual(12, m6)
        XCTAssertEqual(13, m7)
        explicit._isCompact = 5
        explicit._isNegative = 6
        XCTAssertEqual(0, explicit._isNegative)
        XCTAssertEqual(1, explicit._isCompact)
        XCTAssertEqual(FloatingPointSign.plus, explicit.sign)
        XCTAssertFalse(explicit.isSignMinus)
        XCTAssertTrue(explicit.isNormal)
        
        let significand = explicit.significand
        XCTAssertEqual(0, significand._exponent)
        XCTAssertEqual(0, significand.exponent)
        XCTAssertEqual(0x0f, significand._length)
        XCTAssertEqual(0, significand._isNegative)
        XCTAssertEqual(1, significand._isCompact)
        XCTAssertEqual(0, significand._reserved)
        let (sm0, sm1, sm2, sm3, sm4, sm5, sm6, sm7) = significand._mantissa
        XCTAssertEqual(6, sm0)
        XCTAssertEqual(7, sm1)
        XCTAssertEqual(8, sm2)
        XCTAssertEqual(9, sm3)
        XCTAssertEqual(10, sm4)
        XCTAssertEqual(11, sm5)
        XCTAssertEqual(12, sm6)
        XCTAssertEqual(13, sm7)
    }

    func test_Maths() {
        for i in -2...10 {
            for j in 0...5 {
                XCTAssertEqual(Decimal(i*j), Decimal(i) * Decimal(j), "\(Decimal(i*j)) == \(i) * \(j)")
                XCTAssertEqual(Decimal(i+j), Decimal(i) + Decimal(j), "\(Decimal(i+j)) == \(i)+\(j)")
                XCTAssertEqual(Decimal(i-j), Decimal(i) - Decimal(j), "\(Decimal(i-j)) == \(i)-\(j)")
                if j != 0 {
                    let approximation = Decimal(Double(i)/Double(j))
                    let answer = Decimal(i) / Decimal(j)
                    let answerDescription = answer.description
                    let approximationDescription = approximation.description
                    var failed: Bool = false
                    var count = 0
                    let SIG_FIG = 14
                    for (a, b) in zip(answerDescription, approximationDescription) {
                        if a != b {
                            failed = true
                            break
                        }
                        if count == 0 && (a == "-" || a == "0" || a == ".") {
                            continue // don't count these as significant figures
                        }
                        if count >= SIG_FIG {
                            break
                        }
                        count += 1
                    }
                    XCTAssertFalse(failed, "\(Decimal(i/j)) == \(i)/\(j)")
                }
            }
        }
    }

    func test_Misc() {
        XCTAssertEqual(.minus, Decimal(-5.2).sign)
        XCTAssertEqual(.plus, Decimal(5.2).sign)
        var d = Decimal(5.2)
        XCTAssertEqual(.plus, d.sign)
        d.negate()
        XCTAssertEqual(.minus, d.sign)
        d.negate()
        XCTAssertEqual(.plus, d.sign)
        var e = Decimal(0)
        e.negate()
        XCTAssertEqual(e, 0)
        XCTAssertTrue(Decimal(3.5).isEqual(to: Decimal(3.5)))
        XCTAssertTrue(Decimal.nan.isEqual(to: Decimal.nan))
        XCTAssertTrue(Decimal(1.28).isLess(than: Decimal(2.24)))
        XCTAssertFalse(Decimal(2.28).isLess(than: Decimal(2.24)))
        XCTAssertTrue(Decimal(1.28).isTotallyOrdered(belowOrEqualTo: Decimal(2.24)))
        XCTAssertFalse(Decimal(2.28).isTotallyOrdered(belowOrEqualTo: Decimal(2.24)))
        XCTAssertTrue(Decimal(1.2).isTotallyOrdered(belowOrEqualTo: Decimal(1.2)))
        XCTAssertTrue(Decimal.nan.isEqual(to: Decimal.nan))
        XCTAssertTrue(Decimal.nan.isLess(than: Decimal(0)))
        XCTAssertFalse(Decimal.nan.isLess(than: Decimal.nan))
        XCTAssertTrue(Decimal.nan.isLessThanOrEqualTo(Decimal(0)))
        XCTAssertTrue(Decimal.nan.isLessThanOrEqualTo(Decimal.nan))
        XCTAssertFalse(Decimal.nan.isTotallyOrdered(belowOrEqualTo: Decimal.nan))
        XCTAssertFalse(Decimal.nan.isTotallyOrdered(belowOrEqualTo: Decimal(2.3)))
        XCTAssertTrue(Decimal(2) < Decimal(3))
        XCTAssertTrue(Decimal(3) > Decimal(2))
        XCTAssertEqual(Decimal(-9), Decimal(1) - Decimal(10))
        XCTAssertEqual(Decimal(1.234), abs(Decimal(1.234)))
        XCTAssertEqual(Decimal(1.234), abs(Decimal(-1.234)))
        if #available(macOS 10.15, iOS 13, tvOS 13, watchOS 6, *) {
            XCTAssertTrue(Decimal.nan.magnitude.isNaN)
        }
        var a = Decimal(1234)
        var r = a
        XCTAssertEqual(.noError, NSDecimalMultiplyByPowerOf10(&r, &a, 1, .plain))
        XCTAssertEqual(Decimal(12340), r)
        a = Decimal(1234)
        XCTAssertEqual(.noError, NSDecimalMultiplyByPowerOf10(&r, &a, 2, .plain))
        XCTAssertEqual(Decimal(123400), r)
        XCTAssertEqual(.overflow, NSDecimalMultiplyByPowerOf10(&r, &a, 128, .plain))
        XCTAssertTrue(r.isNaN)
        a = Decimal(1234)
        XCTAssertEqual(.noError, NSDecimalMultiplyByPowerOf10(&r, &a, -2, .plain))
        XCTAssertEqual(Decimal(12.34), r)
        var ur = r
        XCTAssertEqual(.underflow, NSDecimalMultiplyByPowerOf10(&ur, &r, -128, .plain))
        XCTAssertTrue(ur.isNaN)
        a = Decimal(1234)
        XCTAssertEqual(.noError, NSDecimalPower(&r, &a, 0, .plain))
        XCTAssertEqual(Decimal(1), r)
        a = Decimal(8)
        XCTAssertEqual(.noError, NSDecimalPower(&r, &a, 2, .plain))
        XCTAssertEqual(Decimal(64), r)
        a = Decimal(-2)
        XCTAssertEqual(.noError, NSDecimalPower(&r, &a, 3, .plain))
        XCTAssertEqual(Decimal(-8), r)
        for i in -2...10 {
            for j in 0...5 {
                var actual = Decimal(i)
                var result = actual
                XCTAssertEqual(.noError, NSDecimalPower(&result, &actual, j, .plain))
                let expected = Decimal(pow(Double(i), Double(j)))
                XCTAssertEqual(expected, result, "\(result) == \(i)^\(j)")
                if #available(macOS 10.15, iOS 13, tvOS 13, watchOS 6, *) {
                    XCTAssertEqual(expected, pow(actual, j))
                }
            }
        }
    }

    func test_MultiplicationOverflow() {
        var multiplicand = Decimal(_exponent: 0, _length: 8, _isNegative: 0, _isCompact: 0, _reserved: 0, _mantissa: ( 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff ))
        
        var result = Decimal()
        var multiplier = Decimal(1)
        
        multiplier._mantissa.0 = 2
        
        XCTAssertEqual(.noError, NSDecimalMultiply(&result, &multiplicand, &multiplier, .plain), "2 * max mantissa")
        XCTAssertEqual(.noError, NSDecimalMultiply(&result, &multiplier, &multiplicand, .plain), "max mantissa * 2")
        
        multiplier._exponent = 0x7f
        XCTAssertEqual(.overflow, NSDecimalMultiply(&result, &multiplicand, &multiplier, .plain), "2e127 * max mantissa")
        XCTAssertEqual(.overflow, NSDecimalMultiply(&result, &multiplier, &multiplicand, .plain), "max mantissa * 2e127")
    }

    func test_NaNInput() {
        var NaN = Decimal.nan
        var one = Decimal(1)
        var result = Decimal()
        
        XCTAssertNotEqual(.noError, NSDecimalAdd(&result, &NaN, &one, .plain))
        XCTAssertTrue(NSDecimalIsNotANumber(&result), "NaN + 1")
        XCTAssertNotEqual(.noError, NSDecimalAdd(&result, &one, &NaN, .plain))
        XCTAssertTrue(NSDecimalIsNotANumber(&result), "1 + NaN")
        
        XCTAssertNotEqual(.noError, NSDecimalSubtract(&result, &NaN, &one, .plain))
        XCTAssertTrue(NSDecimalIsNotANumber(&result), "NaN - 1")
        XCTAssertNotEqual(.noError, NSDecimalSubtract(&result, &one, &NaN, .plain))
        XCTAssertTrue(NSDecimalIsNotANumber(&result), "1 - NaN")
        
        XCTAssertNotEqual(.noError, NSDecimalMultiply(&result, &NaN, &one, .plain))
        XCTAssertTrue(NSDecimalIsNotANumber(&result), "NaN * 1")
        XCTAssertNotEqual(.noError, NSDecimalMultiply(&result, &one, &NaN, .plain))
        XCTAssertTrue(NSDecimalIsNotANumber(&result), "1 * NaN")
        
        XCTAssertNotEqual(.noError, NSDecimalDivide(&result, &NaN, &one, .plain))
        XCTAssertTrue(NSDecimalIsNotANumber(&result), "NaN / 1")
        XCTAssertNotEqual(.noError, NSDecimalDivide(&result, &one, &NaN, .plain))
        XCTAssertTrue(NSDecimalIsNotANumber(&result), "1 / NaN")
        
        XCTAssertNotEqual(.noError, NSDecimalPower(&result, &NaN, 0, .plain))
        XCTAssertTrue(NSDecimalIsNotANumber(&result), "NaN ^ 0")
        XCTAssertNotEqual(.noError, NSDecimalPower(&result, &NaN, 4, .plain))
        XCTAssertTrue(NSDecimalIsNotANumber(&result), "NaN ^ 4")
        XCTAssertNotEqual(.noError, NSDecimalPower(&result, &NaN, 5, .plain))
        XCTAssertTrue(NSDecimalIsNotANumber(&result), "NaN ^ 5")
        
        XCTAssertNotEqual(.noError, NSDecimalMultiplyByPowerOf10(&result, &NaN, 0, .plain))
        XCTAssertTrue(NSDecimalIsNotANumber(&result), "NaN e0")
        XCTAssertNotEqual(.noError, NSDecimalMultiplyByPowerOf10(&result, &NaN, 4, .plain))
        XCTAssertTrue(NSDecimalIsNotANumber(&result), "NaN e4")
        XCTAssertNotEqual(.noError, NSDecimalMultiplyByPowerOf10(&result, &NaN, 5, .plain))
        XCTAssertTrue(NSDecimalIsNotANumber(&result), "NaN e5")

        XCTAssertFalse(Double(truncating: NSDecimalNumber(decimal: Decimal(0))).isNaN)
        XCTAssertTrue(Decimal(Double.leastNonzeroMagnitude).isNaN)
        XCTAssertTrue(Decimal(Double.leastNormalMagnitude).isNaN)
        XCTAssertTrue(Decimal(Double.greatestFiniteMagnitude).isNaN)
        XCTAssertTrue(Decimal(Double("1e-129")!).isNaN)
        XCTAssertTrue(Decimal(Double("0.1e-128")!).isNaN)
    }

    func test_NegativeAndZeroMultiplication() {
        var one = Decimal(1)
        var zero = Decimal(0)
        var negativeOne = Decimal(-1)
        
        var result = Decimal()
        
        XCTAssertEqual(.noError, NSDecimalMultiply(&result, &one, &one, .plain), "1 * 1")
        XCTAssertEqual(.orderedSame, NSDecimalCompare(&one, &result), "1 * 1")
        
        XCTAssertEqual(.noError, NSDecimalMultiply(&result, &one, &negativeOne, .plain), "1 * -1")
        XCTAssertEqual(.orderedSame, NSDecimalCompare(&negativeOne, &result), "1 * -1")
        
        XCTAssertEqual(.noError, NSDecimalMultiply(&result, &negativeOne, &one, .plain), "-1 * 1")
        XCTAssertEqual(.orderedSame, NSDecimalCompare(&negativeOne, &result), "-1 * 1")
        
        XCTAssertEqual(.noError, NSDecimalMultiply(&result, &negativeOne, &negativeOne, .plain), "-1 * -1")
        XCTAssertEqual(.orderedSame, NSDecimalCompare(&one, &result), "-1 * -1")
        
        XCTAssertEqual(.noError, NSDecimalMultiply(&result, &one, &zero, .plain), "1 * 0")
        XCTAssertEqual(.orderedSame, NSDecimalCompare(&zero, &result), "1 * 0")
        XCTAssertEqual(0, result._isNegative, "1 * 0")
        
        XCTAssertEqual(.noError, NSDecimalMultiply(&result, &zero, &one, .plain), "0 * 1")
        XCTAssertEqual(.orderedSame, NSDecimalCompare(&zero, &result), "0 * 1")
        XCTAssertEqual(0, result._isNegative, "0 * 1")
        
        XCTAssertEqual(.noError, NSDecimalMultiply(&result, &negativeOne, &zero, .plain), "-1 * 0")
        XCTAssertEqual(.orderedSame, NSDecimalCompare(&zero, &result), "-1 * 0")
        XCTAssertEqual(0, result._isNegative, "-1 * 0")
        
        XCTAssertEqual(.noError, NSDecimalMultiply(&result, &zero, &negativeOne, .plain), "0 * -1")
        XCTAssertEqual(.orderedSame, NSDecimalCompare(&zero, &result), "0 * -1")
        XCTAssertEqual(0, result._isNegative, "0 * -1")
    }

    func test_Normalize() {
        var one = Decimal(1)
        var ten = Decimal(-10)
        XCTAssertEqual(.noError, NSDecimalNormalize(&one, &ten, .plain))
        XCTAssertEqual(Decimal(1), one)
        XCTAssertEqual(Decimal(-10), ten)
        XCTAssertEqual(1, one._length)
        XCTAssertEqual(1, ten._length)
        one = Decimal(1)
        ten = Decimal(10)
        XCTAssertEqual(.noError, NSDecimalNormalize(&one, &ten, .plain))
        XCTAssertEqual(Decimal(1), one)
        XCTAssertEqual(Decimal(10), ten)
        XCTAssertEqual(1, one._length)
        XCTAssertEqual(1, ten._length)
    }

    func test_NSDecimal() {
        var nan = Decimal.nan
        XCTAssertTrue(NSDecimalIsNotANumber(&nan))
        var zero = Decimal()
        XCTAssertFalse(NSDecimalIsNotANumber(&zero))
        var three = Decimal(3)
        var guess = Decimal()
        NSDecimalCopy(&guess, &three)
        XCTAssertEqual(three, guess)
        
        var f = Decimal(_exponent: 0, _length: 2, _isNegative: 0, _isCompact: 0, _reserved: 0, _mantissa: (0x0000, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000))
        let before = f.description
        XCTAssertEqual(0, f._isCompact)
        NSDecimalCompact(&f)
        XCTAssertEqual(1, f._isCompact)
        let after = f.description
        XCTAssertEqual(before, after)
    }

    func test_RepeatingDivision()  {
        let repeatingNumerator = Decimal(16)
        let repeatingDenominator = Decimal(9)
        let repeating = repeatingNumerator / repeatingDenominator
        
        let numerator = Decimal(1010)
        var result = numerator / repeating
        
        var expected = Decimal()
        expected._exponent = -35;
        expected._length = 8;
        expected._isNegative = 0;
        expected._isCompact = 1;
        expected._reserved = 0;
        expected._mantissa.0 = 51946;
        expected._mantissa.1 = 3;
        expected._mantissa.2 = 15549;
        expected._mantissa.3 = 55864;
        expected._mantissa.4 = 57984;
        expected._mantissa.5 = 55436;
        expected._mantissa.6 = 45186;
        expected._mantissa.7 = 10941;
        
        XCTAssertEqual(.orderedSame, NSDecimalCompare(&expected, &result), "568.12500000000000000000000000000248554: \(expected.description) != \(result.description)");
    }

    func test_Round() {
        var testCases = [
            // expected, start, scale, round
            ( 0, 0.5, 0, Decimal.RoundingMode.down ),
            ( 1, 0.5, 0, Decimal.RoundingMode.up ),
            ( 2, 2.5, 0, Decimal.RoundingMode.bankers ),
            ( 4, 3.5, 0, Decimal.RoundingMode.bankers ),
            ( 5, 5.2, 0, Decimal.RoundingMode.plain ),
            ( 4.5, 4.5, 1, Decimal.RoundingMode.down ),
            ( 5.5, 5.5, 1, Decimal.RoundingMode.up ),
            ( 6.5, 6.5, 1, Decimal.RoundingMode.plain ),
            ( 7.5, 7.5, 1, Decimal.RoundingMode.bankers ),
            
            ( -1, -0.5, 0, Decimal.RoundingMode.down ),
            ( -2, -2.5, 0, Decimal.RoundingMode.up ),
            ( -5, -5.2, 0, Decimal.RoundingMode.plain ),
            ( -4.5, -4.5, 1, Decimal.RoundingMode.down ),
            ( -5.5, -5.5, 1, Decimal.RoundingMode.up ),
            ( -6.5, -6.5, 1, Decimal.RoundingMode.plain ),
            ( -7.5, -7.5, 1, Decimal.RoundingMode.bankers ),
        ]
        if #available(macOS 10.16, iOS 14, watchOS 7, tvOS 14, *) {
            testCases += [
                ( -2, -2.5, 0, Decimal.RoundingMode.bankers ),
                ( -4, -3.5, 0, Decimal.RoundingMode.bankers ),
            ]
        }
        for testCase in testCases {
            let (expected, start, scale, mode) = testCase
            var num = Decimal(start)
            var r = num
            NSDecimalRound(&r, &num, scale, mode)
            XCTAssertEqual(Decimal(expected), r)
            let numnum = NSDecimalNumber(decimal:Decimal(start))
            let behavior = NSDecimalNumberHandler(roundingMode: mode, scale: Int16(scale), raiseOnExactness: false, raiseOnOverflow: true, raiseOnUnderflow: true, raiseOnDivideByZero: true)
            let result = numnum.rounding(accordingToBehavior:behavior)
            XCTAssertEqual(Double(expected), result.doubleValue)
        }
    }

    func test_ScanDecimal() {
        let testCases = [
            // expected, value
            ( 123.456e78, "123.456e78" ),
            ( -123.456e78, "-123.456e78" ),
            ( 123.456, " 123.456 " ),
            ( 3.14159, " 3.14159e0" ),
            ( 3.14159, " 3.14159e-0" ),
            ( 0.314159, " 3.14159e-1" ),
            ( 3.14159, " 3.14159e+0" ),
            ( 31.4159, " 3.14159e+1" ),
            ( 12.34, " 01234e-02"),
            ]
        for testCase in testCases {
            let (expected, string) = testCase
            let decimal = Decimal(string:string)!
            let aboutOne = Decimal(expected) / decimal
            let approximatelyRight = aboutOne >= Decimal(0.99999) && aboutOne <= Decimal(1.00001)
            XCTAssertTrue(approximatelyRight, "\(expected) ~= \(decimal) : \(aboutOne) \(aboutOne >= Decimal(0.99999)) \(aboutOne <= Decimal(1.00001))" )
        }
        guard let ones = Decimal(string:"111111111111111111111111111111111111111") else {
            XCTFail("Unable to parse Decimal(string:'111111111111111111111111111111111111111')")
            return
        }
        let num = ones / Decimal(9)
        guard let answer = Decimal(string:"12345679012345679012345679012345679012.3") else {
            XCTFail("Unable to parse Decimal(string:'12345679012345679012345679012345679012.3')")
            return
        }
        XCTAssertEqual(answer,num,"\(ones) / 9 = \(answer) \(num)")
    }

    func test_Significand() {
        var x = -42 as Decimal
        XCTAssertEqual(x.significand.sign, .plus)
        var y = Decimal(sign: .plus, exponent: 0, significand: x)
        XCTAssertEqual(y, -42)
        y = Decimal(sign: .minus, exponent: 0, significand: x)
        XCTAssertEqual(y, 42)

        x = 42 as Decimal
        XCTAssertEqual(x.significand.sign, .plus)
        y = Decimal(sign: .plus, exponent: 0, significand: x)
        XCTAssertEqual(y, 42)
        y = Decimal(sign: .minus, exponent: 0, significand: x)
        XCTAssertEqual(y, -42)

        let a = Decimal.leastNonzeroMagnitude
        XCTAssertEqual(Decimal(sign: .plus, exponent: -10, significand: a), 0)
        XCTAssertEqual(Decimal(sign: .plus, exponent: .min, significand: a), 0)
        let b = Decimal.greatestFiniteMagnitude
        XCTAssertTrue(Decimal(sign: .plus, exponent: 10, significand: b).isNaN)
        XCTAssertTrue(Decimal(sign: .plus, exponent: .max, significand: b).isNaN)
    }

    func test_SimpleMultiplication() {
        var multiplicand = Decimal()
        multiplicand._isNegative = 0
        multiplicand._isCompact = 0
        multiplicand._length = 1
        multiplicand._exponent = 1
        
        var multiplier = multiplicand
        multiplier._exponent = 2
        
        var expected = multiplicand
        expected._isNegative = 0
        expected._isCompact = 0
        expected._exponent = 3
        expected._length = 1
        
        var result = Decimal()
        
        for i in 1..<UInt8.max {
            multiplicand._mantissa.0 = UInt16(i)
            
            for j in 1..<UInt8.max {
                multiplier._mantissa.0 = UInt16(j)
                expected._mantissa.0 = UInt16(i) * UInt16(j)
                
                XCTAssertEqual(.noError, NSDecimalMultiply(&result, &multiplicand, &multiplier, .plain), "\(i) * \(j)")
                XCTAssertEqual(.orderedSame, NSDecimalCompare(&expected, &result), "\(expected._mantissa.0) == \(i) * \(j)");
            }
        }
    }

    func test_Strideable() {
        XCTAssertEqual(Decimal(476), Decimal(1024).distance(to: Decimal(1500)))
        XCTAssertEqual(Decimal(68040), Decimal(386).advanced(by: Decimal(67654)))

        let x = 42 as Decimal
        XCTAssertEqual(x.distance(to: 43), 1)
        XCTAssertEqual(x.advanced(by: 1), 43)
        XCTAssertEqual(x.distance(to: 41), -1)
        XCTAssertEqual(x.advanced(by: -1), 41)
    }
    
    func test_ULP() {
        var x = 0.1 as Decimal
        XCTAssertFalse(x.ulp > x)

        x = .nan
        XCTAssertTrue(x.ulp.isNaN)
        XCTAssertTrue(x.nextDown.isNaN)
        XCTAssertTrue(x.nextUp.isNaN)

        x = .greatestFiniteMagnitude
        XCTAssertEqual(x.ulp, Decimal(string: "1e127")!)
        XCTAssertEqual(x.nextDown, x - Decimal(string: "1e127")!)
        XCTAssertTrue(x.nextUp.isNaN)

        // '4' is an important value to test because the max supported
        // significand of this type is not 10 ** 38 - 1 but rather 2 ** 128 - 1,
        // for which reason '4.ulp' is not equal to '1.ulp' despite having the
        // same decimal exponent.
        x = 4
        XCTAssertEqual(x.ulp, Decimal(string: "1e-37")!)
        XCTAssertEqual(x.nextDown, x - Decimal(string: "1e-37")!)
        XCTAssertEqual(x.nextUp, x + Decimal(string: "1e-37")!)
        XCTAssertEqual(x.nextDown.nextUp, x)
        XCTAssertEqual(x.nextUp.nextDown, x)
        XCTAssertNotEqual(x.nextDown, x)
        XCTAssertNotEqual(x.nextUp, x)

        // For similar reasons, '3.40282366920938463463374607431768211455',
        // which has the same significand as 'Decimal.greatestFiniteMagnitude',
        // is an important value to test because the distance to the next
        // representable value is more than 'ulp' and instead requires
        // incrementing '_exponent'.
        x = Decimal(string: "3.40282366920938463463374607431768211455")!
        XCTAssertEqual(x.ulp, Decimal(string: "0.00000000000000000000000000000000000001")!)
        XCTAssertEqual(x.nextUp, Decimal(string: "3.4028236692093846346337460743176821146")!)
        x = Decimal(string: "3.4028236692093846346337460743176821146")!
        XCTAssertEqual(x.ulp, Decimal(string: "0.0000000000000000000000000000000000001")!)
        XCTAssertEqual(x.nextDown, Decimal(string: "3.40282366920938463463374607431768211455")!)

        x = 1
        XCTAssertEqual(x.ulp, Decimal(string: "1e-38")!)
        XCTAssertEqual(x.nextDown, x - Decimal(string: "1e-38")!)
        XCTAssertEqual(x.nextUp, x + Decimal(string: "1e-38")!)
        XCTAssertEqual(x.nextDown.nextUp, x)
        XCTAssertEqual(x.nextUp.nextDown, x)
        XCTAssertNotEqual(x.nextDown, x)
        XCTAssertNotEqual(x.nextUp, x)

        x = .leastNonzeroMagnitude
        XCTAssertEqual(x.ulp, x)
        XCTAssertEqual(x.nextDown, 0)
        XCTAssertEqual(x.nextUp, x + x)
        XCTAssertEqual(x.nextDown.nextUp, x)
        XCTAssertEqual(x.nextUp.nextDown, x)
        XCTAssertNotEqual(x.nextDown, x)
        XCTAssertNotEqual(x.nextUp, x)

        x = 0
        XCTAssertEqual(x.ulp, Decimal(string: "1e-128")!)
        XCTAssertEqual(x.nextDown, -Decimal(string: "1e-128")!)
        XCTAssertEqual(x.nextUp, Decimal(string: "1e-128")!)
        XCTAssertEqual(x.nextDown.nextUp, x)
        XCTAssertEqual(x.nextUp.nextDown, x)
        XCTAssertNotEqual(x.nextDown, x)
        XCTAssertNotEqual(x.nextUp, x)

        x = -1
        XCTAssertEqual(x.ulp, Decimal(string: "1e-38")!)
        XCTAssertEqual(x.nextDown, x - Decimal(string: "1e-38")!)
        XCTAssertEqual(x.nextUp, x + Decimal(string: "1e-38")!)
        XCTAssertEqual(x.nextDown.nextUp, x)
        XCTAssertEqual(x.nextUp.nextDown, x)
        XCTAssertNotEqual(x.nextDown, x)
        XCTAssertNotEqual(x.nextUp, x)
    }

    func test_unconditionallyBridgeFromObjectiveC() {
        XCTAssertEqual(Decimal(), Decimal._unconditionallyBridgeFromObjectiveC(nil))
    }

    func test_parseDouble() throws {
        XCTAssertEqual(Decimal(Double(0.0)), Decimal(Int.zero))
        XCTAssertEqual(Decimal(Double(-0.0)), Decimal(Int.zero))

        // These values can only be represented as Decimal.nan
        XCTAssertEqual(Decimal(Double.nan), Decimal.nan)
        XCTAssertEqual(Decimal(Double.signalingNaN), Decimal.nan)

        // These values are out out range for Decimal
        XCTAssertEqual(Decimal(-Double.leastNonzeroMagnitude), Decimal.nan)
        XCTAssertEqual(Decimal(Double.leastNonzeroMagnitude), Decimal.nan)
        XCTAssertEqual(Decimal(-Double.leastNormalMagnitude), Decimal.nan)
        XCTAssertEqual(Decimal(Double.leastNormalMagnitude), Decimal.nan)
        XCTAssertEqual(Decimal(-Double.greatestFiniteMagnitude), Decimal.nan)
        XCTAssertEqual(Decimal(Double.greatestFiniteMagnitude), Decimal.nan)

        // SR-13837
        let testDoubles: [(Double, String)] = [
            (1.8446744073709550E18, "1844674407370954752"),
            (1.8446744073709551E18, "1844674407370954752"),
            (1.8446744073709552E18, "1844674407370955264"),
            (1.8446744073709553E18, "1844674407370955264"),
            (1.8446744073709554E18, "1844674407370955520"),
            (1.8446744073709555E18, "1844674407370955520"),

            (1.8446744073709550E19, "18446744073709547520"),
            (1.8446744073709551E19, "18446744073709552640"),
            (1.8446744073709552E19, "18446744073709552640"),
            (1.8446744073709553E19, "18446744073709552640"),
            (1.8446744073709554E19, "18446744073709555200"),
            (1.8446744073709555E19, "18446744073709555200"),

            (1.8446744073709550E20, "184467440737095526400"),
            (1.8446744073709551E20, "184467440737095526400"),
            (1.8446744073709552E20, "184467440737095526400"),
            (1.8446744073709553E20, "184467440737095526400"),
            (1.8446744073709554E20, "184467440737095552000"),
            (1.8446744073709555E20, "184467440737095552000"),
        ]

        for (d, s) in testDoubles {
            XCTAssertEqual(Decimal(d), Decimal(string: s))
            XCTAssertEqual(Decimal(d).description, try XCTUnwrap(Decimal(string: s)).description)
        }
    }

    func test_initExactly() {
        // This really requires some tests using a BinaryInteger of bitwidth > 128 to test failures.
        let d1 = Decimal(exactly: UInt64.max)
        XCTAssertNotNil(d1)
        XCTAssertEqual(d1?.description, UInt64.max.description)
        XCTAssertEqual(d1?._length, 4)

        let d2 = Decimal(exactly: Int64.min)
        XCTAssertNotNil(d2)
        XCTAssertEqual(d2?.description, Int64.min.description)
        XCTAssertEqual(d2?._length, 4)

        let d3 = Decimal(exactly: Int64.max)
        XCTAssertNotNil(d3)
        XCTAssertEqual(d3?.description, Int64.max.description)
        XCTAssertEqual(d3?._length, 4)

        let d4 = Decimal(exactly: Int32.min)
        XCTAssertNotNil(d4)
        XCTAssertEqual(d4?.description, Int32.min.description)
        XCTAssertEqual(d4?._length, 2)

        let d5 = Decimal(exactly: Int32.max)
        XCTAssertNotNil(d5)
        XCTAssertEqual(d5?.description, Int32.max.description)
        XCTAssertEqual(d5?._length, 2)

        let d6 = Decimal(exactly: 0)
        XCTAssertNotNil(d6)
        XCTAssertEqual(d6, Decimal.zero)
        XCTAssertEqual(d6?.description, "0")
        XCTAssertEqual(d6?._length, 0)

        let d7 = Decimal(exactly: 1)
        XCTAssertNotNil(d7)
        XCTAssertEqual(d7?.description, "1")
        XCTAssertEqual(d7?._length, 1)

        let d8 = Decimal(exactly: -1)
        XCTAssertNotNil(d8)
        XCTAssertEqual(d8?.description, "-1")
        XCTAssertEqual(d8?._length, 1)
    }

}
