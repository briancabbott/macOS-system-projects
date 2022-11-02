//
//  Reference.swift
//  JSONSchema
//
//  Created by Alsey Coleman Miller on 12/17/17.
//

import Foundation

/// JSON Reference
///
/// JSON Reference allows a JSON value to reference another value in a JSON document.
///
/// Defines a structure which allows a JSON value to reference another value in a JSON document.
/// This provides the basis for transclusion in JSON: the use of a target resource as an effective
/// substitute for the reference.
///
/// [Internet Draft](https://tools.ietf.org/html/draft-pbryan-zyp-json-ref-03)
public struct Reference {
    
    // MARK: - Static values
    
    public static let selfReference: Reference = "#"
    
    // MARK: - Properties
    
    public var path: [String]
    
    public var remote: URL?
    
    // MARK: - Initialization
    
    public init(path: [String] = [],
                remote: URL? = nil) {
        
        self.path = path
        self.remote = remote
    }
    
    private init(_ unsafe: String) {
        
        self.init(rawValue: unsafe)!
    }
}

// MARK: - RawRepresentable

extension Reference: RawRepresentable {
    
    public init?(rawValue: String) {
        
        guard let referenceURL = URLComponents(string: rawValue),
            let fragment = referenceURL.fragment
            else { return nil }
        
        // parse path
        if let fragmentURL = URL(string: fragment) {
            
            self.path = fragmentURL.pathComponents.filter { $0 != "/" && $0.isEmpty == false }
            
        } else {
            
            self.path = []
        }
        
        if referenceURL.host != nil {
            
            var remoteURL = referenceURL
            remoteURL.fragment = nil
            
            self.remote = remoteURL.url
            
        } else {
            
            self.remote = nil
        }
        
    }
    
    public var rawValue: String {
        
        var url = URLComponents()
        
        if let remoteURL = self.remote,
            let remoteURLComponents = URLComponents(url: remoteURL, resolvingAgainstBaseURL: false) {
            
            url = remoteURLComponents
        }
        
        url.fragment = self.path.reduce("") { $0 + "/" + $1 }
        
        return url.string ?? ""
    }
}

// MARK: - Codable

extension Reference: Codable {
    
    private enum CodingKeys: String, CodingKey {
        
        case rawValue = "$ref"
    }
    
    public init(from decoder: Decoder) throws {
        
        let singleValue = try decoder.singleValueContainer()
        
        let rawValue = try singleValue.decode(RawValue.self)
        
        guard let value = Reference(rawValue: rawValue)
            else { throw DecodingError.typeMismatch(RawValue.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Invalid raw value")) }
        
        self = value
    }
    
    public func encode(to encoder: Encoder) throws {
        
        var singleValue = encoder.singleValueContainer()
        
        try singleValue.encode(rawValue)
    }
}

// MARK: - CustomStringConvertible

extension Reference: CustomStringConvertible {
    
    public var description: String {
        
        return rawValue
    }
}

// MARK: - Equatable

extension Reference: Equatable {
    
    public static func == (lhs: Reference, rhs: Reference) -> Bool {
        
        return lhs.rawValue == rhs.rawValue
    }
}

// MARK: - Hashable

extension Reference: Hashable {
    
    public var hashValue: Int {
        
        return rawValue.hashValue
    }
}
    
// MARK: - ExpressibleByStringLiteral

extension Reference: ExpressibleByStringLiteral {
    
    public init(stringLiteral value: String) {
        
        // init with unsafe value
        self.init(value)
    }
}
