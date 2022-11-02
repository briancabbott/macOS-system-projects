//
//  Header.swift
//  
//
//  Created by Alsey Coleman Miller on 10/2/22.
//

/// HTTP Method
public struct HTTPHeader: RawRepresentable, Codable, Equatable, Hashable, Sendable {
    
    public let rawValue: String
    
    public init(rawValue: String) {
        self.rawValue = rawValue
    }
}

// MARK: - ExpressibleByStringLiteral

extension HTTPHeader: ExpressibleByStringLiteral {
    
    public init(stringLiteral value: String) {
        self.init(rawValue: value)
    }
}

// MARK: - CustomStringConvertible

extension HTTPHeader: CustomStringConvertible, CustomDebugStringConvertible {
    
    public var description: String {
        rawValue
    }
    
    public var debugDescription: String {
        rawValue
    }
}

// MARK: - Definitions

public extension HTTPHeader {
    
    /// `Authorization` HTTP header
    static var authorization: HTTPHeader    { "Authorization" }
    
    /// `Date` HTTP header
    static var date: HTTPHeader             { "Date" }
    
    /// `Server` HTTP header
    static var server: HTTPHeader           { "Server" }
    
    /// `Content-Type` HTTP header
    static var contentType: HTTPHeader      { "Content-Type" }
    
    /// `Content-Length` HTTP header
    static var contentLength: HTTPHeader    { "Content-Length" }
}
