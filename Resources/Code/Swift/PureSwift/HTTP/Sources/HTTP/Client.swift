//
//  HTTPClient.swift
//  SwiftFoundation
//
//  Created by Alsey Coleman Miller on 9/02/15.
//  Copyright © 2015 PureSwift. All rights reserved.
//

import Foundation
#if canImport(FoundationNetworking)
import FoundationNetworking
#endif

/// HTTP Client
public protocol HTTPClient {
    
    func data(for request: URLRequest) async throws -> (Data, URLResponse)
}

extension URLSession: HTTPClient {
    
    public func data(for request: URLRequest) async throws -> (Data, URLResponse) {
        #if canImport(Darwin)
        if #available(macOS 12, iOS 15.0, tvOS 15, watchOS 8, *) {
            return try await self.data(for: request, delegate: nil)
        } else {
            return try await _data(for: request)
        }
        #else
        return try await _data(for: request)
        #endif
    }
}

internal extension URLSession {
    
    func _data(for request: URLRequest) async throws -> (Data, URLResponse) {
        try await withCheckedThrowingContinuation { continuation in
            self.dataTask(with: request) { data, response, error in
                if let error = error {
                    continuation.resume(throwing: error)
                } else {
                    continuation.resume(returning: (data ?? .init(), response!))
                }
            }
        }
    }
}
