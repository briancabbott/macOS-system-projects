//
//  main.swift
//  gmpint
//
//  Created by Dan Kogai on 7/5/14.
//  Copyright (c) 2014 Dan Kogai. All rights reserved.
//

println (
    GMPInt("1234567890123456789012345678901234567890")
    /%
    GMPInt(10_000_000_000)
)
var gi0 = GMPInt(2)
println(gi0.toInt())
println(gi0.asInt == gi0.toInt())
println(gi0.asInt! == 2)
gi0 **= 1024
println(gi0)
println(gi0.toInt())
println(gi0.asInt == gi0.toInt())
println(gi0.powmod(GMPInt(10_000_000_000), mod:GMPInt(10_000_000_000)))

var n = GMPInt(1)
for i in 1...42 {
    n *= i
    println("fact(\(i)) = \(n)")
}
for i in Array(1...42).reverse() {
    n /= i
    println("fact(\(i - 1)) = \(n)")
}
var f0 = GMPInt(0), f1 = GMPInt(1)
for i in 0...255 {
    println("fib(\(i)) = \(f1)")
    (f0, f1) = (f1, f0+f1)
}
for i in Array(1...255).reverse() {
    (f0, f1) = (f1-f0, f0)
    println("fib(\(i - 1)) = \(f0)")
}
