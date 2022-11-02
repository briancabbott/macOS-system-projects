//
//  IVGenerate.swift
//  
//
//  Created by Alsey Coleman Miller on 5/7/22.
//

/// Linux Crypto IV Generate
public struct IVGenerate: RawRepresentable, Equatable, Hashable, Codable {
    
    public let rawValue: String
    
    public init(rawValue: String) {
        self.rawValue = rawValue
    }
}

// MARK: - ExpressibleByStringLiteral

extension IVGenerate: ExpressibleByStringLiteral {
    
    public init(stringLiteral value: String) {
        self.init(rawValue: value)
    }
}

// MARK: - CustomStringConvertible

extension IVGenerate: CustomStringConvertible, CustomDebugStringConvertible {
    
    public var description: String {
        rawValue
    }
    
    public var debugDescription: String {
        description
    }
}

// MARK: - Definitions

public extension IVGenerate {
    
    @_alwaysEmitIntoClient
    static var none: IVGenerate { "<none>" }
    
    @_alwaysEmitIntoClient
    static var `default`: IVGenerate { "<default>" }
}
