//
//  main.swift
//  tap
//
//  Created by Dan Kogai on 1/21/16.
//  Copyright © 2016 Dan Kogai. All rights reserved.
//
import TAP
let test = TAP(tests:12)
test.ok(42+0.195 == 42.195,     "42 + 0.195 == 42.195")
test.eq(42+0.195,   42.195,     "42 + 0.195 is 42.195")
test.eq([42,0.195],[42,0.195],  "[42,0.195] is [42,0.195]")
test.eq([42:0.195],[42:0.195],  "[42:0.195] is [42,0:195]")
test.ne(42+0.195,   42.0,       "42 + 0.195 is not 42")
test.ne([42,0.195],[42,0.0],    "[42,0.195] is not [42,0.0]")
test.ne([42:0.195],[42:0.0],    "[42:0.195] is not [42:0.0]")
let optionalNum: Int? = 1
let num: Int = 2
test.eq(optionalNum, num-1,       "Optional(1) == 2 - 1")
test.eq(num-1, optionalNum,       "2 - 1 == Optional(1)")
test.eq(Optional<Int>.none, Optional<Int>.none, "Optional(nil) == Optional(nil)")
test.ne(nil, num-1,               "Optional(nil) is not 2 - 1")
test.ne(num-1, nil,               "2 - 1 is not Optional(nil)")
test.done()
