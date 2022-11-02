//
//  Version.swift
//  
//
//  Created by Alsey Coleman Miller on 10/2/22.
//

/// The version of HTTP protocol.
///
/// HTTP uses a "major.minor" numbering scheme to indicate versions of the protocol.
public struct HTTPVersion: Equatable, Hashable, Codable, Sendable {
    
    /// The "major" part of the protocol version.
    public let major: UInt16

    /// The "minor" part of the protocol version.
    public let minor: UInt16
    
    /// Initialize with major and minor values.
    public init(major: UInt16, minor: UInt16) {
        self.major = major
        self.minor = minor
    }
}

// MARK: - RawRepresentable

extension HTTPVersion: RawRepresentable {
    
    public init?(rawValue: String) {
        self.init(rawValue)
    }
    
    internal init?<S>(_ string: S) where S: StringProtocol {
        guard string.hasPrefix(Self.prefix),
              string.count >= Self.minLength else {
            return nil
        }
        let components = string
            .suffix(from: string.index(string.startIndex, offsetBy: Self.prefix.count))
            .split(separator: Self.separator, maxSplits: 2)
        guard components.count == 2,
              let major = UInt16(components[0]),
              let minor = UInt16(components[1]) else {
            return nil
        }
        self.init(major: major, minor: minor)
    }
    
    public var rawValue: String {
        return Self.prefix
            + major.description
            + String(Self.separator)
            + minor.description
    }
}

// MARK: - Constants

internal extension HTTPVersion {
    
    static var minLength: Int { 8 } //prefix.count + 3 }
    
    static var prefix: String { "HTTP/" }
    
    static var separator: Character { "." }
}

// MARK: - Definitions

public extension HTTPVersion {
    
    /// HTTP v1.0
    static var v1: HTTPVersion      { .init(major: 1, minor: 0) }
    
    /// HTTP v1.1
    static var v1_1: HTTPVersion    { .init(major: 1, minor: 1) }
    
    /// HTTP v2.0
    static var v2: HTTPVersion      { .init(major: 2, minor: 0) }
}
