//
//  prime.swift
//  test
//
//  Created by Dan Kogai on 2/17/16.
//  Copyright © 2016 Dan Kogai. All rights reserved.
//

func testPrime(test:TAP) {
    //
    // Primarity
    //
    test.eq(Int32.max.isPrime,  true,   "2**31-1 is prime")
    test.eq(IntMax.max.isPrime, false,  "2**63-1 is not prime")
    test.eq((BigInt(1)<<127 - 1).isPrime, true,  "2**127-1 is prime")
    test.eq(UInt64.min.prevPrime, nil, "UInt64.min.prevPrime is nil")
    test.eq(UInt64.min.nextPrime,   2, "UInt64.min.nextPrime is 2")
    test.eq(UInt64.max.nextPrime, nil, "UInt64.max.prevPrime is nil")
    test.eq(UInt32.min.prevPrime, nil, "UInt32.min.prevPrime is nil")
    test.eq(UInt32.min.nextPrime,   2, "UInt32.min.nextPrime is 2")
    test.eq(UInt32.max.nextPrime, nil, "UInt32.max.prevPrime is nil")
    test.eq(UInt16.min.prevPrime, nil, "UInt16.min.prevPrime is nil")
    test.eq(UInt16.min.nextPrime,   2, "UInt16.min.nextPrime is 2")
    test.eq(UInt16.max.nextPrime, nil, "UInt16.max.prevPrime is nil")
    test.eq(UInt8.min.prevPrime,  nil, "UInt8.min.prevPrime is nil")
    test.eq(UInt8.min.nextPrime,   2,  "UInt8.min.nextPrime is 2")
    test.eq(UInt8.max.nextPrime,  nil, "UInt8.max.prevPrime is nil")
    test.eq(UInt.min.prevPrime,   nil, "UInt.min.prevPrime is nil")
    test.eq(UInt.min.nextPrime,    2,  "UInt.min.nextPrime is 2")
    test.eq(UInt.max.nextPrime,   nil, "UInt.max.prevPrime is nil")
    test.eq(Int64.min.prevPrime,  nil, "Int64.min.prevPrime is nil")
    test.eq(Int64.min.nextPrime,    2, "Int64.min.nextPrime is 2")
    test.eq(Int64.max.nextPrime,  nil, "Int64.max.prevPrime is nil")
    test.eq(Int32.min.prevPrime,  nil, "Int32.min.prevPrime is nil")
    test.eq(Int32.min.nextPrime,    2, "Int32.min.nextPrime is 2")
    test.eq(Int32.max.nextPrime,  nil, "Int32.max.prevPrime is nil")
    test.eq(Int16.min.prevPrime,  nil, "Int16.min.prevPrime is nil")
    test.eq(Int16.min.nextPrime,    2, "Int16.min.nextPrime is 2")
    test.eq(Int16.max.nextPrime,  nil, "Int16.max.prevPrime is nil")
    test.eq(Int8.min.prevPrime,   nil, "Int8.min.prevPrime is nil")
    test.eq(Int8.min.nextPrime,     2, "Int8.min.nextPrime is 2")
    test.eq(Int8.max.nextPrime,   nil, "Int8.max.prevPrime is nil")
    test.eq(Int.min.prevPrime,    nil, "Int.min.prevPrime is nil")
    test.eq(Int.min.nextPrime,      2, "Int.min.nextPrime is 2")
    test.eq(Int.max.nextPrime,    nil, "Int.max.prevPrime is nil")
    test.eq(BigUInt(2).prevPrime, nil, "BigUInt(2).prevPrime is nil")
    test.eq(BigUInt(0).nextPrime,   2, "BigUInt(0).nextPrime is 2")
    test.ne(UIntMax.max.asBigUInt!.nextPrime, nil, "UIntMax.max.asBigUInt!.nextPrime is not nil")
    test.eq(BigInt(2).prevPrime,  nil, "BigInt(2).prevPrime is nil")
    test.eq(BigInt(0).nextPrime,    2, "BigInt(≅0).prevPrime is 2")
    test.ne(IntMax.max.asBigInt!.nextPrime,   nil, "IntMax.max.asBigInt!.nextPrime is not nil")
    // Strong pseudoprimes to base 2
    // https://oeis.org/A001262
    [
        2047,3277,4033,4681,8321,15841,29341,42799,49141,
        52633,65281,74665,80581,85489,88357,90751,104653,
        130561,196093,220729,233017,252601,253241,256999,
        271951,280601,314821,357761,390937,458989,476971,
        486737
    ].forEach {
        test.eq($0.isPrime, false, "\($0) is not prime")

    }
    // Strong Lucas pseudoprimes.
    // https://oeis.org/A217255
    [
        5459,5777,10877,16109,18971,22499,24569,25199,
        40309,58519,75077,97439,100127,113573,115639,
        130139,155819,158399,161027,162133,176399,176471,
        189419,192509,197801,224369,230691,231703,243629,
        253259,268349,288919,313499,324899
    ].forEach {
        test.eq($0.isPrime, false, "\($0) is not prime")
    }
    let bigc = BigInt("4547337172376300111955330758342147474062293202868155909393")
    test.eq(bigc.isPrime, false, "\(bigc) is not prime")
    let bigp = BigInt("4547337172376300111955330758342147474062293202868155909489")
    test.eq(bigp.isPrime, true , "\(bigp) is prime")
    [
        BigUInt("3317044064679887385961981"):(false, true),
        BigUInt("3317044064679887385962123"):(true, false)
    ].forEach {
        let sp = $0.0.isSurelyPrime
        test.eq(sp.0, $0.1.0, "\($0.0) is prime? \($0.1.0)")
        test.eq(sp.1, $0.1.1, "for sure ? \($0.1.1)")
    }
    // factorization
    let two63 = [UInt](count:63, repeatedValue:2)
    let i32max = UInt(Int32.max)
    let i32pmax0  = UInt(Int32.max).prevPrime!
    let i32pmax1  = i32pmax0.prevPrime!
    let u32pmax0  = UInt(UInt32.max).prevPrime!
    let u32pmax1  = u32pmax0.prevPrime!
    var composites:[UInt:[UInt]] = [
        i32max*i32max           : [i32max, i32max],
        i32pmax0*i32pmax1       : [i32pmax1, i32pmax0],
        u32pmax0*u32pmax1       : [u32pmax1, u32pmax0],
        UInt(Int.max)           : [7,7,73,127,337,92737,649657],
        11111111111111111       : [2071723, 5363222357],
        614889782588491410      : [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47],
        3369738766071892021     : [204518747, 16476429743],
        10023859281455311421    : [1308520867, 7660450463]
    ]
    composites[two63.reduce(1,combine:*)] = two63
    for (k, v) in composites {
        test.eq(k.primeFactors, v, "\(k).primeFactors == \(v)")
    }
}
