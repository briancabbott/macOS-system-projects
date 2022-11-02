//
//  NetworkingPolyfill.swift
//  
//
//  Created by Carson Katri on 5/28/20.
//

import JavaScriptKit

// Basically we're going to map URLSession to the browser's `fetch` function
public struct URL {
    let string: String
    
    // There is literally no reason for this to be failable besides compatibility with Foundation
    // Although this whole file is overly engineered to match Foundation, so...
    public init?(string: String) {
        self.string = string
    }
}

public struct Data {
    let stringValue: String
}

public extension String {
    init?(data: Data, encoding: String.Encoding) {
        self = data.stringValue
    }
    enum Encoding {
        case utf8
    }
}

public struct URLResponse {
    
}

public struct URLSessionDataTask {
    let request: URLRequest
    let completionHandler: (Data?, URLResponse?, Error?) -> Void
    
    // Perform the request
    public func resume() {
        if let fetch = URLSession.fetch {
            fetch(request.url.string, JSValue.function { props in
                print(String(describing: props))
                if props.count > 0, let text = props[0].string {
                    completionHandler(Data(stringValue: text), nil, nil)
                } else {
                    completionHandler(nil, nil, FetchError.decodeFailed)
                }
                return .undefined
            }, JSValue.function { props in
                if props.count > 0 {
                    let error = props[0]
                } else {
                    completionHandler(nil, nil, FetchError.decodeErrorFailed)
                }
                return .undefined
            }).object
        } else {
            completionHandler(nil, nil, FetchError.functionNotFound)
        }
    }
    
    enum FetchError: Error {
        case functionNotFound
        case fetchFailed
        case decodeFailed
        case decodeErrorFailed
    }
}

public struct URLRequest {
    public let url: URL
    
    public init(url: URL) {
        self.url = url
    }
}

public class URLSession {
    public static var shared = URLSession()
    fileprivate static let fetch = JSObjectRef.global.SwiftUIFetch.function
    
    public func dataTask(with url: URL, completionHandler: @escaping (Data?, URLResponse?, Error?) -> Void) -> URLSessionDataTask {
        dataTask(with: URLRequest(url: url), completionHandler: completionHandler)
    }
    
    public func dataTask(with request: URLRequest, completionHandler: @escaping (Data?, URLResponse?, Error?) -> Void) -> URLSessionDataTask {
        URLSessionDataTask(request: request, completionHandler: completionHandler)
    }
}
