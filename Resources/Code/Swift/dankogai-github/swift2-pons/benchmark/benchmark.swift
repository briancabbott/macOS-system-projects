#!/usr/bin/env swift -I. -L. -lPONS
// compile: xcrun -sdk macosx swiftc -O pons/*.swift benchmark/benchmark.swift
//
// or "make module" then directly run as:
//
// ./benchmark/benchmark.swift
import PONS

#if os(Linux)
    import Glibc
#else
    import Foundation
#endif

func now()->Double {
    var tv = timeval()
    gettimeofday(&tv, nil)
    return Double(tv.tv_sec) + Double(tv.tv_usec)/1e6
}

func timeit(count:Int, task:()->())->Double {
    let started = now()
    for _ in 0..<count { task() }
    return now() - started
}

func fact<T:POInteger>(n:T)->T {
    return n < 2 ? 1 : (2...n).reduce(1, combine:*)
}

func ok(@autoclosure body:()->Bool) {
    if !body() { fatalError() }
}

print("fact(20 as Int) == ", fact(20 as Int))
print("fact(42 as BigInt) ==", fact(42 as BigInt))

({ count in
    let ti = timeit(count) {
     ok(fact(20 as Int) / fact(19 as Int) == 20)
    }
    let tb = timeit(count) {
     ok(fact(20 as BigInt) / fact(19 as BigInt) == 20)
    }
    let oi = Double(count)/ti
    let ob = Double(count)/tb
    print("fact(20)/fact(19) == 20")
    print("Int:    \(oi) ops/s (\(ti)s for \(count)ops)")
    print("BigInt: \(ob) ops/s (\(tb)s for \(count)ops)")
    print("Int/BigInt == \(oi/ob)")
})(1000)
({ count in
    if count == 0 { return }
    let t19 = timeit(count) {
     ok(fact(20 as BigInt) / fact(19 as BigInt) == 20)
    }
    let t99 = timeit(count) {
     ok(fact(100 as BigInt) / fact(99 as BigInt) == 100)
    }
    let t999 = timeit(count) {
     ok(fact(1000 as BigInt) / fact(999 as BigInt) == 1000)
    }
    let o19 = Double(count)/t19
    let o99 = Double(count)/t99
    let o999 = Double(count)/t999
    print("o19:20!/19!==19    \(o19) ops/s (\(t19)s for \(count)ops)")
    print("o99:100!/99!==100  \(o99) ops/s (\(t99)s for \(count)ops)")
    print("o999:100!/999!==1000  \(o999) ops/s (\(t999)s for \(count)ops)")
    print("o19/o999 == \(o99/o999)")
    let m20  = fact(20 as BigInt).unsignedValue.msbAt
    let m999 = fact(1000 as BigInt).unsignedValue.msbAt
    print("20!  == \(m20  + 1) bits")
    print("100! == \(m999 + 1) bits")
    print("bits(1000!)/bits(20!) == \( Double(m999+1)/Double(m20+1) )")
})(100)
print("fact(34 as UInt128) ==", fact(34 as UInt128))
({ count in
    if count == 0 { return }
    let tu128 = timeit(count) {
     ok(fact(34 as UInt128) / fact(33 as UInt128) == 34)
    }
    let tbu   = timeit(count) {
     ok(fact(34 as BigUInt) / fact(33 as BigUInt) == 34)
    }
    let ou128 = Double(count)/tu128
    let obu   = Double(count)/tbu
    print("UInt128: \(ou128) ops/s (\(tu128)s for \(count)ops)")
    print("BigUInt: \(obu) ops/s (\(tbu)s for \(count)ops)")
    print("UInt128/BigUInt == \(ou128/obu)")
})(1000)
print("fact(57 as UInt256) ==", fact(57 as UInt256))
({ count in
    if count == 0 { return }
    let tu256 = timeit(count) {
     ok(fact(57 as UInt256) / fact(56 as UInt256) == 57)
    }
    let tbu   = timeit(count) {
     ok(fact(57 as BigUInt) / fact(56 as BigUInt) == 57)
    }
    let ou256 = Double(count)/tu256
    let obu   = Double(count)/tbu
    print("UInt256: \(ou256) ops/s (\(tu256)s for \(count)ops)")
    print("BigUInt: \(obu) ops/s (\(tbu)s for \(count)ops)")
    print("UInt256/BigUInt == \(ou256/obu)")
})(100)
