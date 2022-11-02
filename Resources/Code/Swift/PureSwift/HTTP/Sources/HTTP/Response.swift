//
//  HTTPResponse.swift
//  SwiftFoundation
//
//  Created by Alsey Coleman Miller on 6/29/15.
//  Copyright © 2015 PureSwift. All rights reserved.
//

import Foundation

/// HTTP URL response.
public struct HTTPResponse {
    
    /// HTTP version
    public var version: HTTPVersion = .v1_1
    
    /// Returns the HTTP status code for the response.
    public var code: HTTPStatusCode
    
    public var status: String
    
    /// Returns a dictionary containing all the HTTP header fields.
    public var headers: [HTTPHeader: String]
    
    /// The HTTP response body.
    public var body: Data
    
    public init(
        version: HTTPVersion = .v1_1,
        code: HTTPStatusCode,
        status: String? = nil,
        headers: [HTTPHeader : String] = [:],
        body: Data = Data()
    ) {
        self.version = version
        self.code = code
        self.status = status ?? code.reasonPhrase ?? ""
        self.headers = headers
        self.body = body
    }
}

public extension HTTPResponse {
    
    init?(data: Data) {
        guard let message = HTTPMessage(data: data) else {
            return nil
        }
        self.init(message: message)
    }
    
    var data: Data {
        let message = HTTPMessage(response: self)
        return message.data
    }
}

internal extension HTTPResponse {
    
    init?(message: HTTPMessage) {
        guard case let .response(header) = message.head else {
            return nil
        }
        self.init(
            version: header.version,
            code: header.code,
            status: header.status,
            headers: message.headers,
            body: message.body
        )
    }
}

internal extension HTTPMessage {
    
    init(response: HTTPResponse) {
        self.init(
            head: .response(
                .init(
                    version: response.version,
                    code: response.code,
                    status: response.status
                )
            ),
            headers: response.headers,
            body: response.body
        )
    }
}

