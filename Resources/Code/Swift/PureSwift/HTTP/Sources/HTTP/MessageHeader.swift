//
//  File.swift
//  
//
//  Created by Alsey Coleman Miller on 10/3/22.
//

import Foundation

// MARK: - Header

extension HTTPMessage {
    
    enum Header: Equatable, Hashable, Sendable {
        case request(Request)
        case response(Response)
    }
}

extension HTTPMessage.Header: RawRepresentable {
    
    public init?(rawValue: String) {
        self.init(rawValue)
    }
    
    internal init?<S>(_ string: S) where S: StringProtocol {
        if string.hasPrefix(HTTPVersion.prefix),
            let response = Response(string) {
            self = .response(response)
        } else if let request = Request(string) {
            self = .request(request)
        } else {
            return nil
        }
    }
    
    public var rawValue: String {
        switch self {
        case .request(let request):
            return request.rawValue
        case .response(let response):
            return response.rawValue
        }
    }
}

extension HTTPMessage.Header {
    
    static var separator: String { " " }
}

// MARK: - Request

extension HTTPMessage.Header {
    
    struct Request: Equatable, Hashable, Sendable {
        
        public var method: HTTPMethod
        
        public var uri: String
        
        public var version: HTTPVersion
    }
}

extension HTTPMessage.Header.Request: RawRepresentable {
    
    public init?(rawValue: String) {
        self.init(rawValue)
    }
    
    internal init?<S>(_ string: S) where S: StringProtocol {
        let components = string.split(separator: " ", maxSplits: 3, omittingEmptySubsequences: false)
        guard components.count == 3,
              let version = HTTPVersion(components[2])
            else { return nil }
        let method = HTTPMethod(rawValue: String(components[0]))
        let uri = String(components[1])
        self.init(method: method, uri: uri, version: version)
    }
    
    public var rawValue: String {
        method.rawValue
            + HTTPMessage.Header.separator
            + uri
            + HTTPMessage.Header.separator
            + version.rawValue
    }
}

extension HTTPMessage.Header.Request: CustomStringConvertible, CustomDebugStringConvertible {
    
    public var description: String {
        rawValue
    }
    
    public var debugDescription: String {
        rawValue
    }
}

// MARK: - Response

extension HTTPMessage.Header {
    
    struct Response: Equatable, Hashable, Sendable {
        
        public var version: HTTPVersion
        
        public var code: HTTPStatusCode
        
        public var status: String
        
        public init(version: HTTPVersion, code: HTTPStatusCode, status: String? = nil) {
            self.version = version
            self.code = code
            self.status = status ?? code.reasonPhrase ?? ""
        }
    }
}

extension HTTPMessage.Header.Response: RawRepresentable {
    
    public init?(rawValue: String) {
        self.init(rawValue)
    }
    
    internal init?<S>(_ string: S) where S: StringProtocol {
        let components = string.split(separator: " ", maxSplits: 3, omittingEmptySubsequences: false)
        guard components.count == 3,
              let version = HTTPVersion(components[0]),
              let code = HTTPStatusCode.RawValue(components[1])
            else { return nil }
        let status = String(components[2])
        self.init(
            version: version,
            code: .init(rawValue: code),
            status: status
        )
    }
    
    public var rawValue: String {
        version.rawValue
            + HTTPMessage.Header.separator
            + code.description
            + HTTPMessage.Header.separator
            + status
    }
}

extension HTTPMessage.Header.Response: CustomStringConvertible, CustomDebugStringConvertible {
    
    public var description: String {
        rawValue
    }
    
    public var debugDescription: String {
        rawValue
    }
}
