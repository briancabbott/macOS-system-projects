//
//  toliteral.swift
//  toliteral
//
//  Created by Dan Kogai on 6/17/14.
//  Copyright (c) 2014 Dan Kogai. All rights reserved.
//

import Foundation

func toLiteral(obj:NilType) -> String? { return "nil" }
func toLiteral(obj:AnyObject, escapeUnicode eu:Bool = false) -> String? {
    switch obj {
    case let o as NSNumber:
        switch o.objCType {
        case "c", "C":
            return o.boolValue.description
        case "q", "l", "i", "s":
            return o.longLongValue.description
        case "Q", "L", "I", "S":
            return o.unsignedLongLongValue.description
        default:
            switch o.doubleValue {
            case 0.0/0.0:   return "0.0/0.0"    // NaN
            case -1.0/0.0:  return "-1.0/0.0"   // -infinity
            case +1.0/0.0:  return "+1.0/0.0"   //  infinity
            default:
                return NSString(format:"%a", o.doubleValue)
            }
        }
    case let o as NSString:
        var s = ""
        for u in String(o).unicodeScalars {
            s += eu || u.isASCII() ? u.escape() : String(u)
        }
        return "\"" + s + "\""
    case let o as NSArray:
        if o.count == 0 { return "[]" }
        var a:String[] = []
        for v:AnyObject in o {
            if let s = toLiteral(v, escapeUnicode:eu) {
                a.append(s)
            } else {
                return nil
            }
        }
        return "[" + Swift.join(", ", a) + "]"
    case let o as NSDictionary:
        if o.count == 0 { return "[:]" }
        var a:String[] = []
        for (k:AnyObject, v:AnyObject) in o {
            var kv = ""
            if let s = toLiteral(k, escapeUnicode:eu) {
                kv += s
            } else {
                return nil
            }
            if let s = toLiteral(v, escapeUnicode:eu) {
                kv += ":" + s
            } else {
                return nil
            }
            a.append(kv)
        }
        return "[" + Swift.join(", ", a) + "]"
    default:
        return nil
    }
}
