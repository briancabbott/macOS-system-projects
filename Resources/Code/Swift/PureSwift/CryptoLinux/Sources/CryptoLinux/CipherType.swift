//
//  CipherType.swift
//  
//
//  Created by Alsey Coleman Miller on 5/6/22.
//

/// Linux Crypto Cipher Type
public struct CipherType: RawRepresentable, Equatable, Hashable, Codable {
    
    public let rawValue: String
    
    public init(rawValue: String) {
        assert(rawValue.utf8.count <= 14)
        self.rawValue = rawValue
    }
}

// MARK: - ExpressibleByStringLiteral

extension CipherType: ExpressibleByStringLiteral {
    
    public init(stringLiteral value: String) {
        self.init(rawValue: value)
    }
}

// MARK: - CustomStringConvertible

extension CipherType: CustomStringConvertible, CustomDebugStringConvertible {
    
    public var description: String {
        rawValue
    }
    
    public var debugDescription: String {
        description
    }
}

// MARK: - Definitions

public extension CipherType {
    
    @_alwaysEmitIntoClient
    static var randomNumberGenerator: CipherType { "rng" }
    
    @_alwaysEmitIntoClient
    static var compression: CipherType { "compression" }
    
    @_alwaysEmitIntoClient
    static var sCompression: CipherType { "scomp" }
    
    @_alwaysEmitIntoClient
    static var aead: CipherType { "aead" }
    
    @_alwaysEmitIntoClient
    static var cipher: CipherType { "cipher" }
    
    @_alwaysEmitIntoClient
    static var akCipher: CipherType { "akcipher" }
    
    @_alwaysEmitIntoClient
    static var ablkcipher: CipherType { "ablkcipher" }
    
    @_alwaysEmitIntoClient
    static var skCipher: CipherType { "skcipher" }
    
    @_alwaysEmitIntoClient
    static var sHash: CipherType { "shash" }
    
    @_alwaysEmitIntoClient
    static var kpp: CipherType { "kpp" }
}
