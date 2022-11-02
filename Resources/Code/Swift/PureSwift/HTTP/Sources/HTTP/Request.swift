//
//  HTTPRequest.swift
//  SwiftFoundation
//
//  Created by Alsey Coleman Miller on 6/29/15.
//  Copyright © 2015 PureSwift. All rights reserved.
//

import Foundation

/// HTTP URL Request.
public struct HTTPRequest: Equatable, Hashable {
    
    public var method: HTTPMethod
    
    public var uri: String
    
    public var version: HTTPVersion
    
    public var headers: [HTTPHeader: String]
    
    public var body: Data
}

public extension HTTPRequest {
    
    init?(data: Data) {
        guard let message = HTTPMessage(data: data) else {
            return nil
        }
        self.init(message: message)
    }
    
    var data: Data {
        let message = HTTPMessage(request: self)
        return message.data
    }
}

internal extension HTTPRequest {
    
    init?(message: HTTPMessage) {
        guard case let .request(header) = message.head else {
            return nil
        }
        self.init(
            method: header.method,
            uri: header.uri,
            version: header.version,
            headers: message.headers,
            body: message.body
        )
    }
}

internal extension HTTPMessage {
    
    init(request: HTTPRequest) {
        self.init(
            head: .request(
                .init(
                    method: request.method,
                    uri: request.uri,
                    version: request.version
                )
            ),
            headers: request.headers,
            body: request.body
        )
    }
}
