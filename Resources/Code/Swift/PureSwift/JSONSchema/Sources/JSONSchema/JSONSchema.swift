//
//  JSONSchema.swift
//  JSONSchema
//
//  Created by Alsey Coleman Miller on 12/16/17.
//

import Foundation

/// JSON Schema is a vocabulary that allows you to annotate and validate JSON documents.
///
/// - SeeAlso: [json-schema.org](http://json-schema.org)
public enum JSONSchema: String, Codable {
    
    case draft4 = "http://json-schema.org/draft-04/schema#"
}

// MARK: - URL Conversion

public extension JSONSchema {
    
    public init?(url: URL) {
        
        self.init(rawValue: url.absoluteString)
    }
    
    public var url: URL {
        
        return URL(string: rawValue)!
    }
}

// MARK: - Supporting Types

public extension JSONSchema {
    
    
}

/// Reference storage for value types.
public final class Indirect <T: Codable>: Codable {
    
    public let value: T
    
    public init(_ value: T) {
        
        self.value = value
    }
    
    public init(from decoder: Decoder) throws {
        
        let singleValue = try decoder.singleValueContainer()
        
        let rawValue = try singleValue.decode(T.self)
        
        self.value = rawValue
    }
    
    public func encode(to encoder: Encoder) throws {
        
        var singleValue = encoder.singleValueContainer()
        
        try singleValue.encode(value)
    }
}
