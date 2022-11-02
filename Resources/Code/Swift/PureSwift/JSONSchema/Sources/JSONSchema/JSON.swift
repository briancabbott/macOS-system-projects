//
//  JSON.swift
//  SwiftFoundation
//
//  Created by Alsey Coleman Miller on 6/28/15.
//  Copyright © 2015 PureSwift. All rights reserved.
//

/// [JavaScript Object Notation](json.org)
public struct JSON {
    
    public typealias Array = [JSON.Value]
    
    public typealias Object = [String: JSON.Value]
    
    /// JSON value type.
    ///
    /// - Note: Guarenteed to be valid JSON if root value is array or object.
    public enum Value {
        
        /// JSON value is a null placeholder.
        case null
        
        /// JSON value is an array of other JSON values.
        case array(JSON.Array)
        
        /// JSON value a JSON object.
        case object(JSON.Object)
        
        /// JSON value is a `String`.
        case string(Swift.String)
        
        /// JSON value is a `Bool`.
        case boolean(Bool)
        
        /// JSON value is a `Int`.
        case integer(Int64)
        
        /// JSON value is a `Double`.
        case double(Swift.Double)
    }
}

// MARK: Extract Values

public extension JSON.Value {
    
    public var arrayValue: JSON.Array? {
        
        guard case let .array(value) = self else { return nil }
        
        return value
    }
    
    public var objectValue: JSON.Object? {
        
        guard case let .object(value) = self else { return nil }
        
        return value
    }
    
    public var stringValue: String? {
        
        guard case let .string(value) = self else { return nil }
        
        return value
    }
    
    public var booleanValue: Bool? {
        
        guard case let .boolean(value) = self else { return nil }
        
        return value
    }
    
    public var integerValue: Int64? {
        
        guard case let .integer(value) = self else { return nil }
        
        return value
    }
    
    public var doubleValue: Double? {
        
        guard case let .double(value) = self else { return nil }
        
        return value
    }
}

// MARK: - Equatable

extension JSON.Value: Equatable {
    
    public static func == (lhs: JSON.Value, rhs: JSON.Value) -> Bool {
        
        switch (lhs, rhs) {
            
        case (.null, .null): return true
            
        case let (.string(leftValue), .string(rightValue)): return leftValue == rightValue
            
        case let (.boolean(leftValue), .boolean(rightValue)): return leftValue == rightValue
            
        case let (.integer(leftValue), .integer(rightValue)): return leftValue == rightValue
            
        case let (.double(leftValue), .double(rightValue)): return leftValue == rightValue
            
        case let (.array(leftValue), .array(rightValue)): return leftValue == rightValue
            
        case let (.object(leftValue), .object(rightValue)): return leftValue == rightValue
            
        default: return false
        }
    }
}

// MARK: - Codable
/*
extension JSON.Value: Decodable {
    
    public init(from decoder: Decoder) throws {
        
        do {
            
            if try decoder.singleValueContainer().decodeNil() {
                
                self = .null
                return
            }
            
        } catch { }
        
        do {
            
            if decoder.container(keyedBy: <#T##CodingKey.Protocol#>)
        }
    }
}
*/
