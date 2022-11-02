//
//  File.swift
//  
//
//  Created by Carson Katri on 5/28/20.
//

import JavaScriptKit

// This is just a wrapper around the JavaScript JSON object that converts the JSObjectRef to a Decodable type.
public class JSONDecoder {
    public init() {
        
    }
    
    // This should be a minor hurdle if I can leverage the JavaScript JSON type
    public func decode<T: Decodable>(_ type: T.Type, from jsonString: String) throws -> T {
        guard let json = JSObjectRef.global.JSON.object?.parse?(jsonString) else {
            throw DecodingError.valueNotFound(type, DecodingError.Context(codingPath: [], debugDescription: "The given data did not contain a top-level value."))
        }
        return try JSValueDecoder().decode(from: json)
    }
    
    public func decode<T: Decodable>(_ type: T.Type, from data: Data) throws -> T {
        guard let json = JSObjectRef.global.JSON.object?.parse?(data.stringValue) else {
            throw DecodingError.valueNotFound(type, DecodingError.Context(codingPath: [], debugDescription: "The given data did not contain a top-level value."))
        }
        do {
            return try JSValueDecoder().decode(from: json)
        } catch {
            throw DecodingError.valueNotFound(type, DecodingError.Context(codingPath: [], debugDescription: "The given data did not contain a top-level value."))
        }
    }
}
