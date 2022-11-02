//
//  CipherName.swift
//  
//
//  Created by Alsey Coleman Miller on 5/6/22.
//

public extension Cipher {
    
    /// Linux Crypto Cipher Name
    struct Name: RawRepresentable, Equatable, Hashable, Codable {
        
        public let rawValue: String
        
        public init(rawValue: String) {
            assert(rawValue.utf8.count <= 64)
            self.rawValue = rawValue
        }
    }
}

// MARK: - ExpressibleByStringLiteral

extension Cipher.Name: ExpressibleByStringLiteral {
    
    public init(stringLiteral value: String) {
        self.init(rawValue: value)
    }
}

// MARK: - CustomStringConvertible

extension Cipher.Name: CustomStringConvertible, CustomDebugStringConvertible {
    
    public var description: String {
        rawValue
    }
    
    public var debugDescription: String {
        description
    }
}

// MARK: - Definitions

public extension Cipher.Name {
    
    @_alwaysEmitIntoClient
    static var sha1: Cipher.Name { "sha1" }
    
    @_alwaysEmitIntoClient
    static var sha256: Cipher.Name { "sha256" }
    
    @_alwaysEmitIntoClient
    static var sha224: Cipher.Name { "sha224" }
    
    @_alwaysEmitIntoClient
    static var sha384: Cipher.Name { "sha384" }
    
    @_alwaysEmitIntoClient
    static var sha512: Cipher.Name { "sha512" }
    
    @_alwaysEmitIntoClient
    static var sha3_224: Cipher.Name { "sha3-224" }
    
    @_alwaysEmitIntoClient
    static var sha3_256: Cipher.Name { "sha3-256" }
    
    @_alwaysEmitIntoClient
    static var sha3_384: Cipher.Name { "sha3-384" }
    
    @_alwaysEmitIntoClient
    static var sha3_512: Cipher.Name { "sha3-512" }
}
