//
//  Model.swift
//  JSONSchemaSwiftGenerator
//
//  Created by Alsey Coleman Miller on 12/17/17.
//

import Foundation
import JSONSchema

public protocol SwiftCodeGenerator {
    
    func generateCode() -> String
}

public struct SwiftNamedType {
    
    public var type: SwiftType
    
    public var name: String
    
    public var comment: String
    
    public var members: [SwiftMember]
}

public enum SwiftType: String {
    
    case `class`
    case `struct`
    case `enum`
}

public protocol SwiftMember {
    
    var memberType: SwiftMemberType { get }
    
}

public struct SwiftEnumCase {
    
    public var memberType: SwiftMemberType { return .case }
    
    public var name: String
    
    public var value: SwiftLiteralValue?
}

public struct SwiftProperty {
    
    var propertyType: SwiftPropertyType
    
    var memberType: SwiftMemberType { return propertyType.memberType }
    
    
}

public enum SwiftPropertyType: String {
    
    case `let`
    case `var`
    
    public var memberType: SwiftMemberType {
        
        switch self {
        case .let: return .let
        case .var: return .var
        }
    }
}

public enum SwiftMemberType: String {
    
    // enum
    case `case`
    
    // any type
    case `let`
    case `var`
}

public enum SwiftLiteralValue {
    
    case string(String)
    case integer(Int)
    case float(Float)
}

public struct SwiftMethod {
    
    public var declaration: String
    
    public var body: String
}
