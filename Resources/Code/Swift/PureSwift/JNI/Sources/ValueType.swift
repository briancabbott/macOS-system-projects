//
//  JNIValueType.swift
//  JNI
//
//  Created by Alsey Coleman Miller on 3/19/18.
//  Copyright © 2018 PureSwift. All rights reserved.
//

public enum JNIValueType: String {
    
    case boolean = "Z"
    case byte = "B"
    case char = "C"
    case double = "D"
    case float = "F"
    case long = "J"
    case object = "L"
    case int = "I"
    case short = "S"
    case void = "V"
    case array = "["
}

public indirect enum JNIValueTypeSignature {
    
    case boolean
    case byte
    case char
    case double
    case float
    case long
    case int
    case short
    case void
    case array(JNIValueTypeSignature)
    case object(JNIClassName)
}

public extension JNIValueTypeSignature {
    
    var type: JNIValueType {
        
        switch self {
        case .boolean: return .boolean
        case .byte: return .byte
        case .char: return .char
        case .double: return .double
        case .float: return .float
        case .long: return .long
        case .int: return .int
        case .short: return .short
        case .void: return .void
        case .array: return .array
        case .object: return .object
        }
    }
}

extension JNIValueTypeSignature: Equatable {
    
    public static func == (lhs: JNIValueTypeSignature, rhs: JNIValueTypeSignature) -> Bool {
        
        return lhs.rawValue == rhs.rawValue
    }
}

extension JNIValueTypeSignature: Hashable {
    
    public var hashValue: Int {
        
        return rawValue.hashValue
    }
}

extension JNIValueTypeSignature: RawRepresentable {
    
    public init?(rawValue: String) {
        
        var errorContext = Parser.Error.Context()
        
        guard let (value, substring) = try? Parser.firstTypeSignature(from: rawValue, context: &errorContext),
            substring == rawValue
            else { return nil } // string too big
        
        self = value
    }
    
    public var rawValue: String {
        
        var value = type.rawValue
        
        switch self {
            
        case let .object(classSignature):
            
            value += classSignature.rawValue + ";"
            
        case let .array(elementType):
            
            value += elementType.rawValue
            
        default:
            
            break
        }
        
        return value
    }
}

// MARK: - Parser

internal extension JNIValueTypeSignature {
    
    struct Parser {
        
        enum Error: Swift.Error {
            
            case isEmpty(Context)
            case invalidType(String, Context)
            
            struct Context {
                
                var offset = 0
            }
        }
        
        static func firstType(from string: String, context: inout Error.Context) throws -> (JNIValueType, String) {
            
            guard let typeCharacter = string.first
                else { throw Error.isEmpty(context) }
            
            let typeString = String(typeCharacter)
            
            guard let type = JNIValueType(rawValue: typeString)
                else { throw Error.invalidType(typeString, context) }
            
            return (type, typeString)
        }
        
        static func firstTypeSignature(from string: String, context: inout Error.Context) throws -> (JNIValueTypeSignature, String) {
            
            let (type, _) = try firstType(from: string, context: &context)
            
            let signatureString = try firstSubstring(from: string, context: &context)
            
            let typeSignature: JNIValueTypeSignature
            
            switch type {
                
            case .boolean:
                typeSignature = .boolean
            case .byte:
                typeSignature = .byte
            case .char:
                typeSignature = .char
            case .double:
                typeSignature = .double
            case .float:
                typeSignature = .float
            case .long:
                typeSignature = .long
            case .int:
                typeSignature = .int
            case .short:
                typeSignature = .short
            case .void:
                typeSignature = .void
                
            case .object:
                
                guard let classSignatureString = String(signatureString.utf8.dropFirst().dropLast())
                    else { throw Error.isEmpty(context) }
                
                guard let classSignature = JNIClassName(rawValue: classSignatureString)
                    else { throw Error.isEmpty(context) } // FIXME: Proper error
                
                typeSignature = .object(classSignature)
                
            case .array:
                
                guard let subtypeString = String(signatureString.utf8.dropFirst())
                    else { throw Error.isEmpty(context) }
                
                let (arrayElementType, _) = try firstTypeSignature(from: subtypeString, context: &context)
                
                typeSignature = .array(arrayElementType)
            }
            
            return (typeSignature, signatureString)
        }
        
        static func firstSubstring (from string: String, context: inout Error.Context) throws -> String {
            
            let (type, typeString) = try firstType(from: string, context: &context)
            
            switch type {
                
            case .object:
                
                // get substring
                guard let prefix = String(string.utf8.prefix(while: { $0 != ";".utf8.first }))
                    else { throw Error.isEmpty(context) }
                
                assert(prefix.isEmpty == false)
                
                return prefix + ";"
                
            case .array:
                
                // get substring
                guard let suffix = String(string.utf8.dropFirst())
                    else { throw Error.isEmpty(context) }
                
                let substring = try firstSubstring(from: suffix, context: &context)
                
                assert(suffix.isEmpty == false)
                
                return typeString + substring
                
            default:
                
                return typeString
            }
        }
    }
}
