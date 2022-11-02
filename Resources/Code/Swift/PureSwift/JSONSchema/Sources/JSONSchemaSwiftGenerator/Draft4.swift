//
//  JSONSchema.swift
//  JSONSchema
//
//  Created by Alsey Coleman Miller on 12/17/17.
//

import Foundation
import JSONSchema

public extension JSONSchema.Draft4 {
    
    public final class Generator {
        
        public let schema: Schema.Object
        
        public private(set) var code: String = ""
        
        public private(set) var indentationLevel: UInt = 0
        
        public private(set) var resolvedReferences = [Reference: Object]()
        
        public var supportedFormats = [Format: FormatImplementation]()
        
        public init(schema: Schema.Object) {
            
            self.schema = schema
        }
        
        public func append(line: String) {
            
            let indentation = String(repeating: "\t", count: Int(indentationLevel))
            
            code += indentation + line + "\n"
        }
        
        public func indent(_ indent: Bool = true) {
            
            indentationLevel = indent ? indentationLevel + 1 : indentationLevel - 1
        }
        
        
    }

}

public extension JSONSchema.Draft4.Generator {
    
    public enum FormatImplementation {
        
        //case nativeValue(type: String, validation)
    }
}

public extension JSONSchema.Draft4 {
    
    public func generateCode(_ generator: Generator) {
        
        
    }
}
