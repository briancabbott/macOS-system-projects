//
//  HTTPStatusCode.swift
//  SwiftFoundation
//
//  Created by Alsey Coleman Miller on 6/29/15.
//  Copyright © 2015 PureSwift. All rights reserved.
//

/// HTTP Status Code
public struct HTTPStatusCode: RawRepresentable, Codable, Equatable, Hashable, Sendable {
    
    public let rawValue: UInt
    
    public init(rawValue: UInt) {
        self.rawValue = rawValue
    }
}

// MARK: - ExpressibleByIntegerLiteral

extension HTTPStatusCode: ExpressibleByIntegerLiteral {
    
    public init(integerLiteral value: RawValue) {
        self.init(rawValue: value)
    }
}

// MARK: - CustomStringConvertible

extension HTTPStatusCode: CustomStringConvertible, CustomDebugStringConvertible {
    
    public var description: String {
        rawValue.description
    }
    
    public var debugDescription: String {
        rawValue.description
    }
}

// MARK: - Definitions

public extension HTTPStatusCode {
    
    // MARK: - 1xx Informational
    
    /// Continue
    ///
    /// This means that the server has received the request headers,
    /// and that the client should proceed to send the request body
    /// (in the case of a request for which a body needs to be sent; for example, a POST request).
    /// If the request body is large, sending it to a server when a request has already been rejected
    /// based upon inappropriate headers is inefficient.
    /// To have a server check if the request could be accepted based on the request's headers alone,
    /// a client must send Expect: 100-continue as a header in its initial request and check if a 100
    /// Continue status code is received in response before continuing
    /// (or receive 417 Expectation Failed and not continue).
    static var `continue`: HTTPStatusCode               { 100 }
    
    /// Switching Protocols
    ///
    /// This means the requester has asked the server to switch protocols and the server
    /// is acknowledging that it will do so.
    static var switchingProtocols: HTTPStatusCode       { 101 }
    
    /// Processing (WebDAV; RFC 2518)
    ///
    /// As a WebDAV request may contain many sub-requests involving file operations,
    /// it may take a long time to complete the request.
    /// This code indicates that the server has received and is processing the request,
    /// but no response is available yet.
    /// This prevents the client from timing out and assuming the request was lost.
    static var processing: HTTPStatusCode               { 102 }
    
    // MARK: - 2xx Success
    
    /// OK
    ///
    /// Standard response for successful HTTP requests.
    /// The actual response will depend on the request method used.
    /// In a GET request, the response will contain an entity corresponding to the requested resource.
    /// In a POST request, the response will contain an entity describing or containing
    /// the result of the action.
    static var ok: HTTPStatusCode                       { 200 }
    
    /// Created
    ///
    /// The request has been fulfilled and resulted in a new resource being created.
    static var created: HTTPStatusCode                  { 201 }
    
    /// Accepted
    ///
    /// The request has been accepted for processing, but the processing has not been completed.
    /// The request might or might not eventually be acted upon,
    /// as it might be disallowed when processing actually takes place.
    static var accepted: HTTPStatusCode                 { 202 }
    
    // MARK: - 4xx Client Error
    
    /// Bad Request
    ///
    /// The server cannot or will not process the request due to something that is perceived to be a client
    /// error (e.g., malformed request syntax, invalid request message framing,
    /// or deceptive request routing)
    static var badRequest: HTTPStatusCode               { 400 }
    
    /// Unauthorized ([RFC 7235](https://tools.ietf.org/html/rfc7235))
    ///
    /// Similar to **403 Forbidden**, but specifically for use when authentication is required and has
    /// failed or has not yet been provided.
    /// The response must include a WWW-Authenticate header field containing
    /// a challenge applicable to the requested resource.
    static var unauthorized: HTTPStatusCode             { 401 }
    
    /// Payload Too Large ([RFC 7231](https://tools.ietf.org/html/rfc7231))
    ///
    /// The request is larger than the server is willing or able to process.
    ///
    /// Called "Request Entity Too Large " previously.
    static var payloadTooLarge: HTTPStatusCode          { 413 }
    
    // MARK: - 5xx Server Error
    
    /// Internal Server Error
    ///
    /// A generic error message, given when an unexpected condition was encountered and
    /// no more specific message is suitable.
    static var internalServerError: HTTPStatusCode      { 500 }
}

// MARK: - Reason Phrase

public extension HTTPStatusCode {
    
    /// The string reason phrase for a given HTTP response status.
    var reasonPhrase: String? {
        get {
            switch self {
            case .continue:
                return "Continue"
            case .switchingProtocols:
                return "Switching Protocols"
            case .processing:
                return "Processing"
            case .ok:
                return "OK"
            case .created:
                return "Created"
            case .accepted:
                return "Accepted"/*
            case .nonAuthoritativeInformation:
                return "Non-Authoritative Information"
            case .noContent:
                return "No Content"
            case .resetContent:
                return "Reset Content"
            case .partialContent:
                return "Partial Content"
            case .multiStatus:
                return "Multi-Status"
            case .alreadyReported:
                return "Already Reported"
            case .imUsed:
                return "IM Used"
            case .multipleChoices:
                return "Multiple Choices"
            case .movedPermanently:
                return "Moved Permanently"
            case .found:
                return "Found"
            case .seeOther:
                return "See Other"
            case .notModified:
                return "Not Modified"
            case .useProxy:
                return "Use Proxy"
            case .temporaryRedirect:
                return "Temporary Redirect"
            case .permanentRedirect:
                return "Permanent Redirect"*/
            case .badRequest:
                return "Bad Request"
            case .unauthorized:
                return "Unauthorized"/*
            case .paymentRequired:
                return "Payment Required"
            case .forbidden:
                return "Forbidden"
            case .notFound:
                return "Not Found"
            case .methodNotAllowed:
                return "Method Not Allowed"
            case .notAcceptable:
                return "Not Acceptable"
            case .proxyAuthenticationRequired:
                return "Proxy Authentication Required"
            case .requestTimeout:
                return "Request Timeout"
            case .conflict:
                return "Conflict"
            case .gone:
                return "Gone"
            case .lengthRequired:
                return "Length Required"
            case .preconditionFailed:
                return "Precondition Failed"*/
            case .payloadTooLarge:
                return "Payload Too Large"/*
            case .uriTooLong:
                return "URI Too Long"
            case .unsupportedMediaType:
                return "Unsupported Media Type"
            case .rangeNotSatisfiable:
                return "Range Not Satisfiable"
            case .expectationFailed:
                return "Expectation Failed"
            case .imATeapot:
                return "I'm a teapot"
            case .misdirectedRequest:
                return "Misdirected Request"
            case .unprocessableEntity:
                return "Unprocessable Entity"
            case .locked:
                return "Locked"
            case .failedDependency:
                return "Failed Dependency"
            case .upgradeRequired:
                return "Upgrade Required"
            case .preconditionRequired:
                return "Precondition Required"
            case .tooManyRequests:
                return "Too Many Requests"
            case .requestHeaderFieldsTooLarge:
                return "Request Header Fields Too Large"
            case .unavailableForLegalReasons:
                return "Unavailable For Legal Reasons"*/
            case .internalServerError:
                return "Internal Server Error"/*
            case .notImplemented:
                return "Not Implemented"
            case .badGateway:
                return "Bad Gateway"
            case .serviceUnavailable:
                return "Service Unavailable"
            case .gatewayTimeout:
                return "Gateway Timeout"
            case .httpVersionNotSupported:
                return "HTTP Version Not Supported"
            case .variantAlsoNegotiates:
                return "Variant Also Negotiates"
            case .insufficientStorage:
                return "Insufficient Storage"
            case .loopDetected:
                return "Loop Detected"
            case .notExtended:
                return "Not Extended"
            case .networkAuthenticationRequired:
                return "Network Authentication Required"*/
            default:
                return nil
            }
        }
    }
}
