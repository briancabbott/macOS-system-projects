//
//  main.swift
//  immutablearray
//
//  Created by Dan Kogai on 6/29/14.
//  Copyright (c) 2014 Dan Kogai. All rights reserved.
//

let a  = [0,1,2,3]
let ia = ImmutableArray(a)
assert(!ia.isEmpty)
assert(ia.startIndex == a.startIndex)
assert(ia.endIndex == a.endIndex)
assert(ia.count == 4)
for i in 0..ia.count {
    assert(ia[i] == i)
}
for (i, v) in enumerate(ia) {
    assert(i == v)
}
a[3] += 1
assert(a  == [0,1,2,4])
assert(ia == [0,1,2,3])
assert([0,1,2,3].immutable() == [0,1,2,3])
assert(ia != [0,1,2,4])
assert(ia == ImmutableArray(0,1,2,3))
assert(ia.map{ $0 * $0 } == [0,1,4,9])
assert(ia.filter{ $0 % 2 == 0 } == [0,2])
assert(ia.reduce(0){ $0 + $1 } == 6)
assert(ia.reverse() == [3,2,1,0])

import Darwin
func timeit(f:()->()) -> Double { // by ms
    let started = mach_absolute_time()
    f()
    return Double(mach_absolute_time() - started) / 1e6
}
if C_ARGC > 1 {
    let a = Array(0..Int(1e3))
    let ia = ImmutableArray(a)
    println("a.copy()\t\(a.count) times: " + timeit {
        for _ in 0..a.count { let v = a.copy() }
    }.description + "ms")
    println("a.immutable()\t\(a.count) times: " + timeit {
        for _ in 0..a.count { let v = a.immutable() }
    }.description + "ms")
    println("iterate a\t\(a.count) times: " + timeit {
        for _ in 0..a.count { var l = 0; for v in a { l = v } }
    }.description + "ms")
    println("iterate ia\t\(ia.count) times: " + timeit {
        for _ in 0..ia.count { var l = 0; for v in ia { l = v } }
    }.description + "ms")
    println("access a[i]\t\(a.count) times: " + timeit {
        for _ in 0..a.count {
            var l = 0; for i in 0..a.count { l = a[i] }
        }
    }.description + "ms")
    println("access ia[i]\t\(ia.count) times: " + timeit {
        for _ in 0..ia.count {
            var l = 0; for i in 0..ia.count { l = ia[i] }
        }
    }.description + "ms")

}