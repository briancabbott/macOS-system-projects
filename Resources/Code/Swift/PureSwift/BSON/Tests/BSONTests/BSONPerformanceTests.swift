//
//  BSONPerformanceTests.swift
//  BSON
//
//  Created by Joannis Orlandos on 16/07/16.
//
//

import Foundation
import XCTest
@testable import BSON

#if os(Linux)
    import Glibc
#endif

class BSONPerformanceTests: XCTestCase {
    
    static var allTests : [(String, (BSONPerformanceTests) -> () throws -> Void)] {
        return [
            ("testSerializationPerformance", testSerializationPerformance),
            ("testDeserializationPerformance", testDeserializationPerformance),
            ("testLargeDocumentPerformance", testLargeDocumentPerformance),
            ("testLargeDocumentPerformance2", testLargeDocumentPerformance),
            ("testObjectidPerformance", testObjectidPerformance),
        ]
    }
    
    func testSerializationPerformance() {
        var total = 0
        
        measure {
            for _ in 0..<1_000 {
                let kittenDocument: Document = [
                    "doubleTest": 0.04,
                    "stringTest": "foo",
                    "documentTest": [
                        "documentSubDoubleTest": 13.37,
                        "subArray": ["henk", "fred", "kaas", "goudvis"] as Document
                        ] as Document,
                    "nonRandomObjectId": try! ObjectId("0123456789ABCDEF01234567"),
                    "currentTime": Date(timeIntervalSince1970: Double(1453589266)),
                    "cool32bitNumber": Int32(9001),
                    "cool64bitNumber": 21312153544,
                    "code": JavascriptCode(code: "console.log(\"Hello there\");"),
                    "codeWithScope": JavascriptCode(code: "console.log(\"Hello there\");", withScope: ["hey": "hello"]),
                    "nothing": NSNull(),
                    "data": Binary(data: [34,34,34,34,34], withSubtype: .generic),
                    "boolFalse": false,
                    "boolTrue": true,
                    "timestamp": Timestamp(increment: 2000, timestamp: 8),
                    "regex": RegularExpression(pattern: "[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,}", options: []),
                    "minKey": MinKey(),
                    "maxKey": MaxKey()
                ]
                
                total += kittenDocument.bytes.count
            }
        }
        
        print("total = \(total)")
    }
    
    func testDeserializationPerformance() throws {
        let kittenDocument: Document = [
            "doubleTest": 0.04,
            "stringTest": "foo",
            "documentTest": [
                "documentSubDoubleTest": 13.37,
                "subArray": ["henk", "fred", "kaas", "goudvis"] as Document
            ] as Document,
            "nonRandomObjectId": try! ObjectId("0123456789ABCDEF01234567"),
            "currentTime": Date(timeIntervalSince1970: Double(1453589266)),
            "cool32bitNumber": Int32(9001),
            "cool64bitNumber": 21312153544,
            "code": JavascriptCode(code: "console.log(\"Hello there\");"),
            "codeWithScope": JavascriptCode(code: "console.log(\"Hello there\");", withScope: ["hey": "hello"]),
            "nothing": NSNull(),
            "data": Binary(data: [34,34,34,34,34], withSubtype: .generic),
            "boolFalse": false,
            "boolTrue": true,
            "timestamp": Timestamp(increment: 2000, timestamp: 8),
            "regex": RegularExpression(pattern: "[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,}", options: []),
            "minKey": MinKey(),
            "maxKey": MaxKey()
        ]

        var total = 0
        
        measure {
            var hash = 0
            
            for (k, v) in kittenDocument {
                hash += k.characters.count
                hash += v.makeBinary().count
            }
            
            total += hash
        }
        
        print("total = \(total)")
    }
    
    func testLargeDocumentPerformance() {
        var document: Document = [:]
        
        for i in 0..<999999 {
            document.append(Int32(i), forKey: "test\(i)")
        }
        
        measure {
            _ = document[765123]
        }
    }
    
    func testLargeDocumentPerformance2() {
        var document: Document = [:]
        
        for i in 0..<999999 {
            document.append(Int32(i), forKey: "test\(i)")
        }
        
        measure {
            _ = document["test765123"]
        }
    }
    
    func testObjectidPerformance() {
        measure {
            for _ in 0..<10_000 {
                _ = ObjectId()
            }
        }
    }
}
