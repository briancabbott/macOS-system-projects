//
//  Arch.swift
//  
//
//  Created by Alsey Coleman Miller on 3/8/22.
//

public struct Arch: RawRepresentable, Equatable, Hashable, Codable {

    public let rawValue: String

    public init(rawValue: String) {
        self.rawValue = rawValue
    }
}

// MARK: - ExpressibleByStringLiteral

extension Arch: ExpressibleByStringLiteral {
    
    public init(stringLiteral value: String) {
        self.init(rawValue: value)
    }
}

// MARK: - CustomStringConvertible

extension Arch: CustomStringConvertible, CustomDebugStringConvertible {
    
    public var description: String {
        rawValue
    }
    
    public var debugDescription: String {
        rawValue
    }
}

// MARK: - Definitions

public extension Arch {
    
    static var armv5: Arch { "armv5" }
    
    static var armv6: Arch { "armv6" }
    
    static var armv7: Arch { "armv7" }
    
    static var arm64: Arch { "arm64" }
    
    static var x86_64: Arch { "x86_64" }
}
