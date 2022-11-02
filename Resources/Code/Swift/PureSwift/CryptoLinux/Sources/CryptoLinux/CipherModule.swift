//
//  CipherModule.swift
//  
//
//  Created by Alsey Coleman Miller on 5/6/22.
//

public extension Cipher {
    
    /// Linux Crypto Cipher Module
    struct Module: RawRepresentable, Equatable, Hashable, Codable {
        
        public let rawValue: String
        
        public init(rawValue: String) {
            self.rawValue = rawValue
        }
    }
}

// MARK: - ExpressibleByStringLiteral

extension Cipher.Module: ExpressibleByStringLiteral {
    
    public init(stringLiteral value: String) {
        self.init(rawValue: value)
    }
}

// MARK: - CustomStringConvertible

extension Cipher.Module: CustomStringConvertible, CustomDebugStringConvertible {
    
    public var description: String {
        rawValue
    }
    
    public var debugDescription: String {
        description
    }
}
