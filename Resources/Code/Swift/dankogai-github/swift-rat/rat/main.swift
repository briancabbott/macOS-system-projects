//
//  main.swift
//  rat
//
//  Created by Dan Kogai on 1/19/16.
//  Copyright © 2016 Dan Kogai. All rights reserved.
//
var tests = 0
func okay(p:Bool, _ message:String = "") {
    tests += 1
    let result = (p ? "" : "not ") + "ok"
    print("\(result) \(tests) # \(message)")
}
func same<T:Equatable>(actual:T, _ expected:T, _ message:String) {
    tests += 1
    if actual == expected {
        print("ok \(tests) # \(message)")
    } else {
        print("not ok \(tests)")
        print("#   message: \(message)")
        print("#       got: \(actual)")
        print("#  expected: \(expected)")
    }
}
func done_testing(){ print("1..\(tests)") }
////
#if os(Linux)
    import Glibc
#else
    import Foundation
#endif
////
same(Rat(8,-12).numerator,      -2, "Rat(8,-12).numerator == -2")
same(Rat(8,-12).denominator,    +3, "Rat(8,-12).denominator == +3")
same(Rat(8,-12), Rat(-2,3),         "Rat(8,-12) == Rat(-2,3)")
same(Rat(8,-12).inv, Rat(-3,2)  ,   "Rat(8,-12).inv == Rat(-3,2)")
same("\(Rat(8,-12))", "(-2/3)"  ,   "Rat(8.-12).description == \"(-2/3)\"")
same(Rat(2,3)*Rat(3,4), Rat(1,2),   "Rat(2,3)*Rat(3,4) == Rat(1,2)")
same(Rat(-2,3)/Rat(-4,3), Rat(1,2), "Rat(-2,3)/Rat(-4,3) == Rat(1,2)")
same(Rat(1,3)+Rat(2,3),Rat(1,1),    "Rat(1,3)+Rat(2,3) == Rat(1,1)")
same(Rat(1,1)-Rat(2,3),Rat(1,3),    "Rat(1,1)+Rat(2,3) == Rat(1,3)")
same(Rat(1,12)+Rat(1,8),Rat(5,24),  "Rat(1,12)+Rat(1,8) == Rat(5,24)")
same(Rat(5,24)-Rat(1,8),Rat(1,12),  "Rat(5,24)-Rat(1,8) == Rat(1,12)")
same(Double(Rat<Int>(M_PI)), M_PI,  "\(Rat<Int>(M_PI)) == \(M_PI)")
done_testing()
