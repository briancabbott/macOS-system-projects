//
//  PerformanceTests.swift
//  JSONC
//
//  Created by Alsey Coleman Miller on 12/19/15.
//  Copyright © 2015 PureSwift. All rights reserved.
//

#if os(OSX) || os(iOS)

import XCTest
import SwiftFoundation
import JSONC

class JSONPerformanceTests: XCTestCase {
    
    let performanceJSON: JSON.Value = {
        
        var jsonArray = JSON.Array([
            .String("value1"),
            .String("value2"),
            .Null,
            .Number(.Boolean(true)),
            .Number(.Integer(10)),
            .Number(.Double(10.10)),
            .Object(["Key": .String("Value")])
            ])
        
        for _ in 0...10 {
            
            jsonArray += jsonArray
        }
        
        return JSON.Value.Array(jsonArray)
    }()
    
    func testWritingPerformance() {
        
        let jsonValue = performanceJSON
        
        measureBlock {
            
            let _ = jsonValue.toString([], JSONC.self)!
        }
    }
    
    func testFoundationWritingPerformance() {
        
        let jsonValue = performanceJSON
        
        measureBlock {
            
            let _ = jsonValue.toData()
        }
        
    }
    
    lazy var performanceJSONString: String = self.performanceJSON.toString([], JSONC.self)!
    
    func testParsePerformance() {
        
        let jsonString = performanceJSONString
        
        measureBlock {
            
            let _ = JSON.Value(string: jsonString, JSONC.self)!
        }
    }
    
    func testFoundationParsePerformance() {
        
        let jsonString = performanceJSONString
        
        let jsonData = jsonString.toUTF8Data().toFoundation()
        
        measureBlock {
            
            let _ = JSON.Value(data: jsonData)!
        }
    }
}

#endif
