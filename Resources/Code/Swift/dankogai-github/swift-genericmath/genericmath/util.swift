//
//  util.swift
//  genericmath
//
//  Created by Dan Kogai on 2/1/16.
//  Copyright © 2016 Dan Kogai. All rights reserved.
//

#if os(Linux)
    import Glibc
#else
    import Darwin
#endif
public extension Double {
    #if os(Linux)
    public static func frexp(d:Double)->(Double, Int) { return Glibc.frexp(d) }
    #else
    public static func frexp(d:Double)->(Double, Int)   { return Darwin.frexp(d) }
    public static func ldexp(m:Double, _ e:Int)->Double { return Darwin.ldexp(m, e) }
    #endif
}
public extension UInt32 {
    public var msbAt:Int {  // good only up to UInt22
        return Double.frexp(Double(self.toUIntMax())).1 - 1
    }
}
public extension UInt16 {
    public var msbAt:Int { return UInt32(self).msbAt }
}
public extension UInt8 {
    public var msbAt:Int { return UInt32(self).msbAt }
}
public extension UInt64 {
    public var msbAt:Int {  // so we split
        let m = UInt32(self >> 32).msbAt
        return m != -1 ? m + 32 : UInt32(self & 0xffff_ffff).msbAt
    }
}
public extension UnsignedIntegerType {
    public var msbAt:Int {
        return self.toUIntMax().msbAt
    }
}
