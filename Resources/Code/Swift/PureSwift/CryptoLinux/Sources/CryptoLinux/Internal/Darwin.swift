//
//  Darwin.swift
//  BluetoothLinux
//
//  Created by Alsey Coleman Miller on 2/28/16.
//  Copyright © 2016 PureSwift. All rights reserved.
//

import SystemPackage
import Socket

#if !os(Linux)
#warning("This module will only run on Linux")

@usableFromInline
internal func stub(function: StaticString = #function) -> Never {
    fatalError("\(function) not implemented. This code only runs on Linux.")
}

public var AF_ALG: CInt { 38 }
public var SOL_ALG: CInt { 279 }

/* Socket options */
public var ALG_SET_KEY: CInt            { 1 }
public var ALG_SET_IV: CInt             { 2 }
public var ALG_SET_OP: CInt             { 3 }

/* Operations */
public var ALG_OP_DECRYPT: CInt         { 0 }
public var ALG_OP_ENCRYPT: CInt         { 1 }

public struct sockaddr_alg {
    public var salg_family: UInt16
    public var salg_type: (UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8)
    public var salg_feat: UInt32
    public var salg_mask: UInt32
    public var salg_name: (UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
                           UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
                           UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
                           UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8)
    public init() {
        stub()
    }
}

internal extension MessageFlags {
    static var more: MessageFlags { stub() }
}

public extension SocketOptionLevel {
    static var crypto: SocketOptionLevel { SocketOptionLevel(rawValue: SOL_ALG) }
}

public extension SocketAddressFamily {
    static var crypto: SocketAddressFamily { SocketAddressFamily(rawValue: AF_ALG) }
}

internal extension SocketDescriptor {
    
    init<T: SocketProtocol>(
        _ protocolID: T,
        flags: SocketFlags,
        retryOnInterrupt: Bool = true
    ) throws {
        stub()
    }
    
    init<Address: SocketAddress>(
        _ protocolID: Address.ProtocolID,
        bind address: Address,
        flags: SocketFlags,
        retryOnInterrupt: Bool = true
    ) throws {
        stub()
    }
}

@usableFromInline
internal struct SocketFlags: OptionSet, Hashable, Codable {
    
    /// The raw C file events.
    @_alwaysEmitIntoClient
    public let rawValue: CInt

    /// Create a strongly-typed file events from a raw C value.
    @_alwaysEmitIntoClient
    public init(rawValue: CInt) { self.rawValue = rawValue }

    @_alwaysEmitIntoClient
    private init(_ raw: CInt) { self.init(rawValue: numericCast(raw)) }
}

extension SocketFlags {
    
    /// Set the `O_NONBLOCK` file status flag on the open file description referred to by the new file
    /// descriptor.  Using this flag saves extra calls to `fcntl()` to achieve the same result.
    public static var nonBlocking: SocketFlags { stub() }
    
    /// Set the close-on-exec (`FD_CLOEXEC`) flag on the new file descriptor.
    public static var closeOnExec: SocketFlags { stub() }
}

#endif
