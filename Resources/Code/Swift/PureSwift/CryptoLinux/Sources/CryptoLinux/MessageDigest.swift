//
//  MessageDigest.swift
//  
//
//  Created by Alsey Coleman Miller on 5/7/22.
//

import Foundation
import Socket

/// Linux Crypto Message Digest
///
/// - SeeAlso: [Linux Kernel Message Digest API](https://www.kernel.org/doc/html/v5.10/crypto/userspace-if.html#message-digest-api)
public struct MessageDigest {
    
    public let type: CipherType
    
    public let name: Cipher.Name
        
    public init(
        type: CipherType = "hash",
        name: Cipher.Name
    ) {
        self.type = type
        self.name = name
    }
    
    public func hash(_ data: Data) async throws -> Data {
        let socket = try await Socket.crypto(type: type, name: name)
        do {
            try await socket.write(data)
            let result = try await socket.read(Int(getpagesize()))
            // cleanup
            await socket.close()
            return result
        }
        catch {
            // cleanup
            await socket.close()
            throw error
        }
    }
}
