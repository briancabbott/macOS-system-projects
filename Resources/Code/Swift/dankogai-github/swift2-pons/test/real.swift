//
//  rational.swift
//  test
//
//  Created by Dan Kogai on 2/17/16.
//  Copyright © 2016 Dan Kogai. All rights reserved.
//

func testReal(test:TAP) {
    func equiv<T:Equatable>(lhs:T, _ rhs:T)->Bool {
        return unsafeBitCast(lhs, UIntMax.self) == unsafeBitCast(rhs, UIntMax.self)
    }
    for d in [-0.0, +0.0, -1.0/0.0, +1.0/0.0, 0.0/0.0] {
        test.eq(equiv(BigRat(d).toDouble(), d), true, "BigRat(\(d)).toDouble() === \(d)")
        test.eq(equiv(BigFloat(d).toDouble(), d), true, "BigFloat(\(d)).toDouble() === \(d)")
    }
    //
    // Rational
    //
    test.eq("\(+2.over(4))", "(1/2)",  "\"\\(+2.over(4))\" == \"(1/2)\"")
    test.eq("\(-2.over(4))", "-(1/2)", "\"\\(-2.over(4))\" == \"-(1/2)\"")
    test.eq("\(2.over(4)+2.over(4).i)", "((1/2)+(1/2).i)",
        "\"\\(2.over(4)+2.over(4).i)\" == \"((1/2)+(1/2).i)\"")
    test.eq("\(2.over(4)-2.over(4).i)", "((1/2)-(1/2).i)",
        "\"\\(2.over(4)-2.over(4).i)\" == \"((1/2)+(1/2).i)\"")
    test.eq(+2.over(+4), +1.over(2), "+2/+4 == +1/2")
    test.eq(+2.over(-4), -1.over(2), "-2/+4 == -1/2")
    test.eq(-2.over(+4), -1.over(2), "+2/-4 == -1/2")
    test.eq(-2.over(-4), +1.over(2), "-2/-4 == +1/2")
    test.ok((+42.over(0)).isInfinite, "\(+42.over(0)) is infinite")
    test.ok((-42.over(0)).isInfinite, "\(-42.over(0)) is infinite")
    test.ok((0.over(0)).isNaN, "\(0.over(0)) is NaN")
    test.ne(0.over(0), 0.over(0), "NaN != NaN")
    test.eq(1.over(2)  <  1.over(3),  false, "+1/2 > +1/3")
    test.eq(1.over(2)  <  1.over(1),   true, "+1/2 < +1/1")
    test.eq(1.over(-2) < 1.over(-3),   true, "-1/2 < -1/3")
    test.eq(1.over(-2) < 1.over(-1),  false, "-1/2 > -1/1")
    test.eq(BigRat(42.195).toFPString(),    "42.19500000000000028",   "42.195 is really 42.19500000000000028")
    test.eq(BigRat(42.195).toFPString(16),  "2a.31eb851eb852",         "42.195 is also 2a.31eb851eb852")
    test.eq(BigRat(42.195).toFPString(10,places:4), "42.1950",  "42.195 to 4 dicimal places")
    test.eq(BigRat(42.195).toFPString(10,places:3), "42.195",   "42.195 to 3 dicimal places")
    test.eq(BigRat(42.195).toFPString(10,places:2), "42.20",    "42.195 to 2 dicimal places")
    test.eq(BigRat(42.195).toFPString(10,places:1), "42.2",     "42.195 to 1 dicimal place")
    test.eq( 1.999999999999.toFPString(10,places:12) , "1.999999999999",
        "1.999999999999.toFPString(10,places:12) is 1.999999999999")
    test.eq(1.999999999999.toFPString(10,places:11),  "2.0",
        "1.999999999999.toFPString(10,places:12) is 2.0")
    ({ q in
        test.eq(q.toMixed().0, -2,            "-14/6 = -2-1/3")
        test.eq(q.toMixed().1, (-1).over(3),  "-14/6 = -2-1/3")
    })((-14).over(6))
    //
    // BigFloat
    //
    test.eq(BigFloat(+0.0).isSignMinus, (+0.0).isSignMinus, "(BigFloat(+0.0).isSignMinus is \(BigFloat(+0.0).isSignMinus)")
    test.eq(BigFloat(-0.0).isSignMinus, (-0.0).isSignMinus, "(BigFloat(-0.0).isSignMinus is \(BigFloat(-0.0).isSignMinus)")
    for d in [0.5, Double.LN2, 1.0, Double.LN10, Double.PI] {
        test.eq(BigFloat(+d).toDouble(), +d, "BigFloat(+\(d)).toDouble() == +\(d))")
        test.eq(BigFloat(-d).toDouble(), -d, "BigFloat(-\(d)).toDouble() == -\(d))")
    }
}