//
//  CipherSelfTest.swift
//  
//
//  Created by Alsey Coleman Miller on 5/7/22.
//

public extension Cipher {
    
    /// Linux Crypto Cipher Self Text
    struct SelfTest: RawRepresentable, Equatable, Hashable, Codable {
        
        public let rawValue: String
        
        public init(rawValue: String) {
            self.rawValue = rawValue
        }
    }
}

// MARK: - ExpressibleByStringLiteral

extension Cipher.SelfTest: ExpressibleByStringLiteral {
    
    public init(stringLiteral value: String) {
        self.init(rawValue: value)
    }
}

// MARK: - CustomStringConvertible

extension Cipher.SelfTest: CustomStringConvertible, CustomDebugStringConvertible {
    
    public var description: String {
        rawValue
    }
    
    public var debugDescription: String {
        description
    }
}

// MARK: - Definitions

public extension Cipher.SelfTest {
    
    @_alwaysEmitIntoClient
    static var passed: Cipher.SelfTest { "passed" }
}
