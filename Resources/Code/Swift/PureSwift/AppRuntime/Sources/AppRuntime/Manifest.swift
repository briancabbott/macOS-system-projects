//
//  Manifest.swift
//  
//
//  Created by Alsey Coleman Miller on 3/8/22.
//

/// App manifest
public struct Manifest: Equatable, Hashable, Codable, Identifiable {
    
    enum CodingKeys: String, CodingKey {
        case id
        case name
        case appDescription = "description"
        case sdk
        case executable
        case version
        case build
        case copyright
        case capabilities
    }
    
    /// Reverse DNS bundle ID
    public let id: String
    
    /// Human-readable name
    public let name: String
    
    /// Human-readable description
    public let appDescription: String
    
    /// SDK version this app was compiled against.
    public let sdk: SDKVersion
    
    /// Name of the binary executable.
    public let executable: String
    
    /// App version
    public let version: String
    
    /// App build number
    public let build: String
    
    /// App copyright
    public let copyright: String?
    
    /// List of capabilities
    public let capabilities: [String]?
}
