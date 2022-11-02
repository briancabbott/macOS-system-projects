//
//  main.swift
//  toliteral
//
//  Created by Dan Kogai on 6/18/14.
//  Copyright (c) 2014 Dan Kogai. All rights reserved.
//

println(     "\(nil) => "
    + toLiteral(nil)!)
println(     "\([]) => "
    + toLiteral([])!)
println(     "\([:]) => "
    + toLiteral([:])!)
println(     "\(false) => "
    + toLiteral(false)!)
println(     "\([false,true]) => "
    + toLiteral([false,true])!)
println(     "\([false:true]) => "
    + toLiteral([false:true])!)
println(     "\(0) => "
    + toLiteral(0)!)
println(     "\(-1/0.0) => "
    + toLiteral(-1/0.0)!)
println(     "\(0.1) => "
    + toLiteral(0.1)!)
for s in ["a α\tあ\n🐣", "a \u03B1\t\u3042\n\U0001F423"] {
    for e in [false, true] {
        println(     "\(s) => "
            + toLiteral(s, escapeUnicode:e)!)
        println(     "\([s]) => "
            + toLiteral([s], escapeUnicode:e)!)
        println(     "\([s:s]) => "
            + toLiteral([s:s], escapeUnicode:e)!)
    }
}
