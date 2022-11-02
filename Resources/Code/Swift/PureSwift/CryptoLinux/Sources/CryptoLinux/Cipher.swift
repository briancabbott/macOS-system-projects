//
//  Cipher.swift
//  
//
//  Created by Alsey Coleman Miller on 5/6/22.
//

/// Linux Crypto Cipher
public struct Cipher: Equatable, Hashable, Codable {
    
    enum CodingKeys: String, CodingKey {
        case name
        case driver
        case module
        case priority
        case referenceCount = "refcnt"
        case selfTest = "selftest"
        case isInternal = "internal"
        case type
        case async
        case blockSize = "blocksize"
        case minKeysize = "min keysize"
        case maxKeysize = "max keysize"
        case chunkSize = "chunksize"
        case walkSize = "walksize"
        case digestSize = "digestsize"
        case seedSize = "seedsize"
        case ivSize = "ivsize"
        case ivGenerate = "geniv"
        case maxAuthsize = "maxauthsize"
    }
    
    public let name: Name
    
    public let driver: String
    
    public let module: Module
    
    public let priority: Int
    
    public let referenceCount: UInt
    
    public let selfTest: SelfTest
    
    public let isInternal: Bool
    
    public let type: CipherType
    
    public let async: Bool?
    
    public let ivGenerate: IVGenerate?
    
    public let blockSize: UInt?
    
    public let minKeysize: UInt?
    
    public let maxKeysize: UInt?
    
    public let ivSize: UInt?
    
    public let chunkSize: UInt?
    
    public let walkSize: UInt?
    
    public let digestSize: UInt?
    
    public let seedSize: UInt?
    
    public let maxAuthsize: UInt?
}
