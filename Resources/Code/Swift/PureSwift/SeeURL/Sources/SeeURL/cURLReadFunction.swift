//
//  cURLReadFunction.swift
//  SwiftFoundation
//
//  Created by Alsey Coleman Miller on 8/4/15.
//  Copyright © 2015 PureSwift. All rights reserved.
//

#if os(Linux)
    import CcURL
#endif

import SwiftFoundation

public extension cURL {
    
    public typealias ReadCallBack = curl_read_callback
    
    public static var ReadFunction: ReadCallBack { return curlReadFunction }
    
    public final class ReadFunctionStorage {
        
        public let data: Data
        
        public var currentIndex = 0
        
        public init(data: Data) {
            
            self.data = data
        }
    }
}

public func curlReadFunction(pointer: UnsafeMutablePointer<Int8>, size: Int, nmemb: Int, readData: UnsafeMutablePointer<Void>) -> Int {
    
    let storage = unsafeBitCast(readData, cURL.ReadFunctionStorage.self)
    
    let data = storage.data
    
    let currentIndex = storage.currentIndex
    
    guard (size * nmemb) > 0 else { return 0 }
    
    guard currentIndex < data.byteValue.count else { return 0 }
    
    let byte = data.byteValue[currentIndex]
    
    let char = CChar(byte)
    
    pointer.memory = char
    
    storage.currentIndex += 1
    
    return 1
}

