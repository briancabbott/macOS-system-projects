//
//  MethodSignature.swift
//  JNI
//
//  Created by Alsey Coleman Miller on 3/19/18.
//  Copyright © 2018 PureSwift. All rights reserved.
//

public struct JNIMethodSignature {
    
    public var argumentTypes: [JNIValueTypeSignature]
    
    public var returnType: JNIValueTypeSignature
    
    public init(argumentTypes: [JNIValueTypeSignature],
                returnType: JNIValueTypeSignature) {
        
        self.argumentTypes = argumentTypes
        self.returnType = returnType
    }
}

extension JNIMethodSignature: Equatable {
    
    public static func == (lhs: JNIMethodSignature, rhs: JNIMethodSignature) -> Bool {
        
        return lhs.argumentTypes == rhs.argumentTypes
            && lhs.returnType == rhs.returnType
    }
}

extension JNIMethodSignature: Hashable {
    
    public var hashValue: Int {
        
        return rawValue.hashValue
    }
}

extension JNIMethodSignature: RawRepresentable {
    
    public init?(rawValue: String) {
        
        var isParsingArguments = true
        
        let utf8 = rawValue.toUTF8Data()
        
        var offset = 0
        
        var arguments = [JNIValueTypeSignature]()
        
        while offset < utf8.count {
            
            let character = String(Character(UnicodeScalar(utf8[offset])))
            
            switch character {
                
            case "(":
                
                isParsingArguments = true
                offset += 1
                
            case ")":
                
                isParsingArguments = false
                offset += 1
                
            default:
             
                var errorContext = JNIValueTypeSignature.Parser.Error.Context()
                
                guard let suffix = rawValue.utf8Substring(range: offset ..< utf8.count),
                    let (typeSignature, substring) = try? JNIValueTypeSignature.Parser.firstTypeSignature(from: suffix, context: &errorContext)
                    else { return nil }
                
                offset += substring.utf8.count
                
                if isParsingArguments {
                    
                    arguments.append(typeSignature)
                    
                } else {
                    
                    self.init(argumentTypes: arguments, returnType: typeSignature)
                    return
                }
            }
        }
        
        return nil
    }
    
    public var rawValue: String {
        
        let arguments = self.argumentTypes.reduce("") { $0 + $1.rawValue }
        
        return "(" + arguments + ")" + returnType.rawValue
    }
}

extension JNIMethodSignature: CustomStringConvertible {
    
    public var description: String {
        
        return rawValue
    }
}
