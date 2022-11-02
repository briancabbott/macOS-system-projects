//
//  SocketAddress.swift
//  
//
//  Created by Alsey Coleman Miller on 5/6/22.
//

import Socket

public struct CryptoSocketAddress: SocketAddress {
    
    public typealias ProtocolID = CryptoSocketProtocol
    
    public var name: Cipher.Name
    
    public var type: CipherType
    
    public init(
        name: Cipher.Name,
        type: CipherType
    ) {
        self.name = name
        self.type = type
    }
    
    public func withUnsafePointer<Result>(
      _ body: (UnsafePointer<CInterop.SocketAddress>, UInt32) throws -> Result
    ) rethrows -> Result {
        var socketAddress = CInterop.CryptoSocketAddress()
        socketAddress.salg_family = .init(SocketAddressFamily.crypto.rawValue)
        name.rawValue.withCString { (cString) in
            cString.withMemoryRebound(to: CInterop.CipherName.self, capacity: 1, {
                socketAddress.salg_name = $0.pointee
            })
        }
        type.rawValue.withCString { (cString) in
            cString.withMemoryRebound(to: CInterop.CipherType.self, capacity: 1, {
                socketAddress.salg_type = $0.pointee
            })
        }
        return try socketAddress.withUnsafePointer(body)
    }
    
    public static func withUnsafePointer(
        _ body: (UnsafeMutablePointer<CInterop.SocketAddress>, UInt32) throws -> ()
    ) rethrows -> Self {
        var bytes = CInterop.CryptoSocketAddress()
        try bytes.withUnsafeMutablePointer(body)
        #if !os(Linux)
        guard #available(macOS 11.0, *) else {
            stub()
        }
        #endif
        let nameString = withUnsafeBytes(of: bytes.salg_name) { namePointer in
            String(unsafeUninitializedCapacity: 64, initializingUTF8With: { buffer in
                buffer.initialize(from: namePointer).1
            })
        }
        
        let typeString = withUnsafeBytes(of: bytes.salg_type) { typePointer in
            String(unsafeUninitializedCapacity: 14, initializingUTF8With: { buffer in
                buffer.initialize(from: typePointer).1
            })
        }
        return Self.init(
            name: .init(rawValue: nameString),
            type: .init(rawValue: typeString)
        )
    }
}
