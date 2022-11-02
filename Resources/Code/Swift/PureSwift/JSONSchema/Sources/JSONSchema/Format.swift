//
//  Format.swift
//  JSONSchema
//
//  Created by Alsey Coleman Miller on 12/17/17.
//

/**
 Structural validation alone may be insufficient to validate that an instance meets all the requirements of an application. The "format" keyword is defined to allow interoperable semantic validation for a fixed subset of values which are accurately described by authoritative resources, be they RFCs or other external specifications.
 
 The value of this keyword is called a format attribute. It MUST be a string. A format attribute can generally only validate a given set of instance types. If the type of the instance to validate is not in this set, validation for this format attribute and instance SHOULD succeed.
 */
public struct Format: RawRepresentable, Codable {
    
    public var rawValue: String
    
    public init(rawValue: String) {
        
        self.rawValue = rawValue
    }
}

// MARK: - CustomStringConvertible

extension Format: CustomStringConvertible {
    
    public var description: String {
        
        return rawValue
    }
}

// MARK: - ExpressibleByStringLiteral

extension Format: ExpressibleByStringLiteral {
    
    public init(stringLiteral value: String) {
        
        self.rawValue = value
    }
}

// MARK: - Equatable

extension Format: Equatable {
    
    public static func == (lhs: Format, rhs: Format) -> Bool {
        
        return lhs.rawValue == rhs.rawValue
    }
}

// MARK: - Hashable

extension Format: Hashable {
    
    public var hashValue: Int {
        
        return rawValue.hashValue
    }
}
