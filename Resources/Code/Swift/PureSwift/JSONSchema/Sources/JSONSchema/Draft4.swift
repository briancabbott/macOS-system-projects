//
//  Draft4.swift
//  JSONSchema
//
//  Created by Alsey Coleman Miller on 12/16/17.
//

import Foundation

public extension JSONSchema {
    
    /// Core schema meta-schema
    ///
    /// [JSON Schema](http://json-schema.org/draft-04/schema#)
    ///
    /// [Validation](http://json-schema.org/draft-04/json-schema-validation.html)
    public enum Draft4: Codable {
        
        public static var type: SchemaType { return .draft4 }
        
        case object(Object)
        case reference(Reference)
        
        struct DecodingErrors: Error {
            
            var errors = [Error]()
        }
        
        public init(from decoder: Decoder) throws {
            
            var decodingErrors = DecodingErrors()
            
            do {
                
                let reference = try Reference(from: decoder)
                
                self = .reference(reference)
                
                return
            }
            
            catch {
                
                decodingErrors.errors.append(error)
            }
            
            do {
                
                let object = try Object(from: decoder)
                
                self = .object(object)
                
                return
            }
            
            catch {
                
                decodingErrors.errors.append(error)
                
                throw DecodingError.typeMismatch(Draft4.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Invalid raw value", underlyingError: decodingErrors))
            }
        }
        
        public func encode(to encoder: Encoder) throws {
            
            switch self {
            case let .object(value): try value.encode(to: encoder)
            case let .reference(value): try value.encode(to: encoder)
            }
        }
    }
}

// MARK: BuiltIn

public extension JSONSchema.Draft4 {
    
    public typealias Schema = JSONSchema.Draft4
    
    public typealias SchemaType = JSONSchema
}

public extension Format {
    
    /// JSON Scheme Draft-04 definitions
    ///
    /// [Draft](http://json-schema.org/draft-04/json-schema-validation.html#rfc.section.7.3)
    public struct Draft4 {
        
        /**
         A string instance is valid against this attribute if it is a valid date representation as defined by [RFC 3339, section 5.6](http://json-schema.org/draft-04/json-schema-validation.html#RFC3339) [RFC3339].
         */
        public static var dateTime: Format { return "date-time" }
        
        /**
         A string instance is valid against this attribute if it is a valid Internet email address as defined by [RFC 5322, section 3.4.1](http://json-schema.org/draft-04/json-schema-validation.html#RFC5322) [RFC5322].
         */
        public static var email: Format { return "email" }
        
        /**
         A string instance is valid against this attribute if it is a valid representation for an Internet host name, as defined by RFC 1034, section 3.1 [RFC1034].
         */
        public static var hostname: Format { return "hostname" }
        
        /**
         A string instance is valid against this attribute if it is a valid representation of an IPv4 address according to the "dotted-quad" ABNF syntax as defined in RFC 2673, section 3.2 [RFC2673].
         */
        public static var ipv4: Format { return "ipv4" }
        
        /**
         A string instance is valid against this attribute if it is a valid representation of an IPv6 address as defined in RFC 2373, section 2.2 [RFC2373].
         */
        public static var ipv6: Format { return "ipv6" }
        
        /**
         A string instance is valid against this attribute if it is a valid URI, according to [[RFC3986]](http://json-schema.org/draft-04/json-schema-validation.html#RFC3986).
         */
        public static var uri: Format { return "uri" }
        
        public static let formats: [Format] = [dateTime, email, hostname, ipv4, ipv6, uri]
    }
}

// MARK: JSON

public extension JSONSchema.Draft4 {
    
    public struct Object: Codable /*, Equatable */ {
        
        public var identifier: URL?
        
        public var schema: SchemaType? // should only be draft-04
        
        public var type: ObjectType?
        
        public var title: String?
        
        public var description: String?
        
        //public var `default`: JSON.Value? // typically {} or false
        
        public var format: Format? // not in official schema
        
        public var multipleOf: Double? // minimum: 0, exclusiveMinimum: true
        
        public var maximum: Double?
        
        public var exclusiveMaximum: Bool? // false
        
        public var minimum: Double?
        
        public var exclusiveMinimum: Bool? // false
        
        public var maxLength: PositiveInteger?
        
        public var minLength: PositiveIntegerDefault0?
        
        public var pattern: String? // format: regex
        
        public var additionalItems: AdditionalItems?
        
        public var items: Items?
        
        public var maxItems: PositiveInteger?
        
        public var minItems: PositiveIntegerDefault0?
        
        public var uniqueItems: Bool? // false
        
        public var maxProperties: PositiveInteger?
        
        public var minProperties: PositiveIntegerDefault0?
        
        public var required: StringArray?
        
        public var propertyOrder: [String]?
        
        public var additionalProperties: AdditionalProperties?
        
        public var definitions: [String: Schema]?
        
        public var properties: [String: Schema]?
        
        public var patternProperties: [String: Schema]?  // "additionalProperties": { "$ref": "#" }
        
        public var dependencies: [String: Dependencies]?
        
        public var `enum`: StringArray? // "enum": { "type": "array", "minItems": 1, "uniqueItems": true }
        
        public var allOf: SchemaArray? // { "$ref": "#/definitions/schemaArray" }
        
        public var anyOf: SchemaArray? // { "$ref": "#/definitions/schemaArray" }
        
        public var oneOf: SchemaArray? // { "$ref": "#/definitions/schemaArray" }
        
        public var not: Indirect<Schema>? // { "$ref": "#" }
        
        public init() { }
        
        internal enum CodingKeys: String, CodingKey {
            
            case type = "type"
            case identifier = "id"
            case schema = "$schema"
            case title = "title"
            case description = "description"
            //case `default` = "default"
            case multipleOf = "multipleOf"
            case maximum = "maximum"
            case exclusiveMaximum = "exclusiveMaximum"
            case minimum = "minimum"
            case exclusiveMinimum = "exclusiveMinimum"
            case maxLength = "maxLength"
            case minLength = "minLength"
            case pattern = "pattern"
            case additionalItems = "additionalItems"
            case items = "items"
            case maxItems = "maxItems"
            case minItems = "minItems"
            case uniqueItems = "uniqueItems"
            case maxProperties = "maxProperties"
            case minProperties = "minProperties"
            case required = "required"
            case propertyOrder = "propertyOrder"
            case additionalProperties = "additionalProperties"
            case definitions = "definitions"
            case properties = "properties"
            case patternProperties = "patternProperties"
            case dependencies = "dependencies"
            case `enum` = "enum"
            case allOf = "allOf"
            case anyOf = "anyOf"
            case oneOf = "oneOf"
            case not = "not"
            
            // not in official JSON meta schema
            case format = "format"
        }
    }
}

// MARK: Definitions

public extension JSONSchema.Draft4.Object {
    
    public typealias Schema = JSONSchema.Draft4
    
    /// ```JSON
    /// "schemaArray": {
    /// "type": "array",
    /// "minItems": 1,
    /// "items": { "$ref": "#" }
    /// }
    /// ```
    public struct SchemaArray: RawRepresentable, Codable /*, Equatable */ {
        
        public let rawValue: [Schema]
        
        public init?(rawValue: [Schema]) {
            
            guard rawValue.count >= 1
                else { return nil }
            
            self.rawValue = rawValue
        }
        
        public init(from decoder: Decoder) throws {
            
            let singleValue = try decoder.singleValueContainer()
            
            let rawValue = try singleValue.decode(RawValue.self)
            
            guard let value = SchemaArray.init(rawValue: rawValue)
                else { throw DecodingError.typeMismatch(RawValue.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Invalid raw value")) }
            
            self = value
        }
        
        public func encode(to encoder: Encoder) throws {
            
            var singleValue = encoder.singleValueContainer()
            
            try singleValue.encode(rawValue)
        }
    }
    
    /**
     ```JSON
     "positiveInteger": {
     "type": "integer",
     "minimum": 0
     }
     ```
    */
    public struct PositiveInteger: RawRepresentable, Codable {
        
        public let rawValue: Int
        
        public init?(rawValue: Int) {
            
            guard rawValue >= 0
                else { return nil }
            
            self.rawValue = rawValue
        }
        
        public init(from decoder: Decoder) throws {
            
            let singleValue = try decoder.singleValueContainer()
            
            let rawValue = try singleValue.decode(RawValue.self)
            
            guard let value = PositiveInteger.init(rawValue: rawValue)
                else { throw DecodingError.typeMismatch(RawValue.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Invalid raw value")) }
            
            self = value
        }
        
        public func encode(to encoder: Encoder) throws {
            
            var singleValue = encoder.singleValueContainer()
            
            try singleValue.encode(rawValue)
        }
    }
    
    /**
     ```JSON
     "positiveIntegerDefault0": {
     "allOf": [ { "$ref": "#/definitions/positiveInteger" }, { "default": 0 } ]
     }
     ```
    */
    public struct PositiveIntegerDefault0: RawRepresentable, Codable {
        
        public let rawValue: Int
        
        public init?(rawValue: Int) {
            
            guard rawValue >= 0
                else { return nil }
            
            self.rawValue = rawValue
        }
        
        public init() {
            
            self.rawValue = 0
        }
        
        public init(from decoder: Decoder) throws {
            
            let singleValue = try decoder.singleValueContainer()
            
            let rawValue = try singleValue.decode(RawValue.self)
            
            guard let value = PositiveIntegerDefault0.init(rawValue: rawValue)
                else { throw DecodingError.typeMismatch(RawValue.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Invalid raw value")) }
            
            self = value
        }
        
        public func encode(to encoder: Encoder) throws {
            
            var singleValue = encoder.singleValueContainer()
            
            try singleValue.encode(rawValue)
        }
    }
    
    /**
     ```JSON
     "simpleTypes": {
     "enum": [ "array", "boolean", "integer", "null", "number", "object", "string" ]
     }
     ```
     */
    public enum SimpleTypes: String, Codable {
        
        case object
        case array
        case string
        case integer
        case number
        case boolean
        case null
    }
    
    /**
     ```JSON
     "stringArray": {
     "type": "array",
     "items": { "type": "string" },
     "minItems": 1,
     "uniqueItems": true
     }
     ```
     */
    public struct StringArray: RawRepresentable, Codable {
        
        public let rawValue: [String]
        
        public init?(rawValue: [String]) {
            
            guard rawValue.count >= 1,
                rawValue.isUnique
                else { return nil }
            
            self.rawValue = rawValue
        }
        
        public init(from decoder: Decoder) throws {
            
            let singleValue = try decoder.singleValueContainer()
            
            let rawValue = try singleValue.decode(RawValue.self)
            
            guard let value = StringArray.init(rawValue: rawValue)
                else { throw DecodingError.typeMismatch(RawValue.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Invalid raw value")) }
            
            self = value
        }
        
        public func encode(to encoder: Encoder) throws {
            
            var singleValue = encoder.singleValueContainer()
            
            try singleValue.encode(rawValue)
        }
    }
    
}

// MARK: Property definitions

public extension JSONSchema.Draft4.Object {
    
    public enum AdditionalProperties : Codable {
        
        case a(Bool) // { "type": "boolean" }
        case b(Indirect<Schema>) // { "$ref": "#" }
        
        struct EnumDecodingErrors: Error {
            
            var errors = [Error]()
        }
        
        public init(from decoder: Decoder) throws {
            
            var decodingErrors = EnumDecodingErrors()
            
            do {
                
                let value = try Bool(from: decoder)
                
                self = .a(value)
                
                return
            }
                
            catch {
                
                decodingErrors.errors.append(error)
            }
            
            do {
                
                let value = try Schema(from: decoder)
                
                self = .b(Indirect(value))
                
                return
            }
                
            catch {
                
                decodingErrors.errors.append(error)
                
                throw DecodingError.typeMismatch(AdditionalProperties.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Invalid raw value", underlyingError: decodingErrors))
            }
        }
        
        public func encode(to encoder: Encoder) throws {
            
            switch self {
            case let .a(value): try value.encode(to: encoder)
            case let .b(value): try value.encode(to: encoder)
            }
        }
        /*
        public static let `default`: AdditionalProperties = {
            
            
            
        }()*/
    }
    
    public enum Dependencies: Codable {
        
        public typealias A = Indirect<Schema>
        public typealias B = StringArray
        
        case a(A) // { "$ref": "#" }
        case b(B) // { "$ref": "#/definitions/stringArray" }
        
        struct DecodingErrors: Error {
            
            var errors = [Error]()
        }
        
        public init(from decoder: Decoder) throws {
            
            var decodingErrors = DecodingErrors()
            
            do {
                
                let value = try A(from: decoder)
                
                self = .a(value)
                
                return
            }
                
            catch {
                
                decodingErrors.errors.append(error)
            }
            
            do {
                
                let value = try B(from: decoder)
                
                self = .b(value)
                
                return
            }
                
            catch {
                
                decodingErrors.errors.append(error)
                
                throw DecodingError.typeMismatch(AdditionalProperties.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Invalid raw value", underlyingError: decodingErrors))
            }
        }
        
        public func encode(to encoder: Encoder) throws {
            
            switch self {
            case let .a(value): try value.encode(to: encoder)
            case let .b(value): try value.encode(to: encoder)
            }
        }
    }
    
    public enum AdditionalItems: Codable {
        
        public typealias A = Bool
        public typealias B = Indirect<Schema>
        
        case a(A) // { "type": "boolean" }
        case b(B) // { "$ref": "#" }
        
        struct DecodingErrors: Error {
            
            var errors = [Error]()
        }
        
        public init(from decoder: Decoder) throws {
            
            var decodingErrors = DecodingErrors()
            
            do {
                
                let value = try A(from: decoder)
                
                self = .a(value)
                
                return
            }
                
            catch {
                
                decodingErrors.errors.append(error)
            }
            
            do {
                
                let value = try B(from: decoder)
                
                self = .b(value)
                
                return
            }
                
            catch {
                
                decodingErrors.errors.append(error)
                
                throw DecodingError.typeMismatch(AdditionalProperties.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Invalid raw value", underlyingError: decodingErrors))
            }
        }
        
        public func encode(to encoder: Encoder) throws {
            
            switch self {
                case let .a(value): try value.encode(to: encoder)
                case let .b(value): try value.encode(to: encoder)
            }
        }
    }
    
    /**
     ```JSON
     "items": {
     "anyOf": [
     { "$ref": "#" },
     { "$ref": "#/definitions/schemaArray" }
     ],
     "default": {}
     }
     ```
    */
    public enum Items: Codable {
        
        public typealias A = Indirect<Schema>
        public typealias B = SchemaArray
        
        case a(A) // { "$ref": "#" }
        case b(B) // { "$ref": "#/definitions/schemaArray" }
        
        struct DecodingErrors: Error {
            
            var errors = [Error]()
        }
        
        public init(from decoder: Decoder) throws {
            
            var decodingErrors = DecodingErrors()
            
            do {
                
                let value = try A(from: decoder)
                
                self = .a(value)
                
                return
            }
                
            catch {
                
                decodingErrors.errors.append(error)
            }
            
            do {
                
                let value = try B(from: decoder)
                
                self = .b(value)
                
                return
            }
                
            catch {
                
                decodingErrors.errors.append(error)
                
                throw DecodingError.typeMismatch(Items.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Invalid raw value", underlyingError: decodingErrors))
            }
        }
        
        public func encode(to encoder: Encoder) throws {
            
            switch self {
                case let .a(value): try value.encode(to: encoder)
                case let .b(value): try value.encode(to: encoder)
            }
        }
    }
    
    public enum ObjectType: Codable {
        
        case a(A)
        case b(B)
        
        struct DecodingErrors: Error {
            
            var errors = [Error]()
        }
        
        public typealias A = SimpleTypes
        
        public struct B: RawRepresentable, Codable {
            
            public let rawValue: [String]
            
            public init?(rawValue: [String]) {
                
                guard rawValue.count >= 1,
                    rawValue.isUnique
                    else { return nil }
                
                self.rawValue = rawValue
            }
            
            public init(from decoder: Decoder) throws {
                
                let singleValue = try decoder.singleValueContainer()
                
                let rawValue = try singleValue.decode(RawValue.self)
                
                guard let value = B.init(rawValue: rawValue)
                    else { throw DecodingError.typeMismatch(RawValue.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Invalid raw value")) }
                
                self = value
            }
            
            public func encode(to encoder: Encoder) throws {
                
                var singleValue = encoder.singleValueContainer()
                
                try singleValue.encode(rawValue)
            }
        }
        
        public init(from decoder: Decoder) throws {
            
            var decodingErrors = DecodingErrors()
            
            do {
                
                let value = try A(from: decoder)
                
                self = .a(value)
                
                return
            }
                
            catch {
                
                decodingErrors.errors.append(error)
            }
            
            do {
                
                let value = try B(from: decoder)
                
                self = .b(value)
                
                return
            }
                
            catch {
                
                decodingErrors.errors.append(error)
                
                throw DecodingError.typeMismatch(ObjectType.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Invalid raw value", underlyingError: decodingErrors))
            }
        }
        
        public func encode(to encoder: Encoder) throws {
            
            switch self {
            case let .a(value): try value.encode(to: encoder)
            case let .b(value): try value.encode(to: encoder)
            }
        }
    }
    
    public struct MultipleOf: RawRepresentable, Codable {
        
        public let rawValue: [String]
        
        public init?(rawValue: [String]) {
            
            guard rawValue.count > 0,
                rawValue.isUnique
                else { return nil }
            
            self.rawValue = rawValue
        }
        
        public init(from decoder: Decoder) throws {
            
            let singleValue = try decoder.singleValueContainer()
            
            let rawValue = try singleValue.decode(RawValue.self)
            
            guard let value = MultipleOf.init(rawValue: rawValue)
                else { throw DecodingError.typeMismatch(RawValue.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Invalid raw value")) }
            
            self = value
        }
        
        public func encode(to encoder: Encoder) throws {
            
            var singleValue = encoder.singleValueContainer()
            
            try singleValue.encode(rawValue)
        }
    }
    
    public struct Enum: RawRepresentable, Codable /*, Equatable */ {
        
        public let rawValue: [String]
        
        public init?(rawValue: [String]) {
            
            guard rawValue.count >= 1
                else { return nil }
            
            self.rawValue = rawValue
        }
        
        public init(from decoder: Decoder) throws {
            
            let singleValue = try decoder.singleValueContainer()
            
            let rawValue = try singleValue.decode(RawValue.self)
            
            guard let value = Enum.init(rawValue: rawValue)
                else { throw DecodingError.typeMismatch(RawValue.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Invalid raw value")) }
            
            self = value
        }
        
        public func encode(to encoder: Encoder) throws {
            
            var singleValue = encoder.singleValueContainer()
            
            try singleValue.encode(rawValue)
        }
    }
}

// MARK: - Extensions

// MARK: Subscriptins

public extension JSONSchema.Draft4.Object {
    
    public enum ReferenceKey: String {
        
        case definitions
        case properties
    }
    
    public subscript (path: ReferenceKey) -> [String: Schema] {
        
        switch path {
        case .definitions: return definitions ?? [:]
        case .properties: return properties ?? [:]
        }
    }
    
    public subscript (path: [String]) -> Schema? {
        
        guard path.count == 2,
            let referenceKey = ReferenceKey(rawValue: path[0])
            else { return nil }
        
        let definitionName = path[1]
        
        return self[referenceKey][definitionName]
    }
}

// MARK: Reference Resolving

public extension JSONSchema.Draft4.Object {
    
    public enum ReferenceResolverError: Error {
        
        // Could not resolve reference
        case invalid(Reference)
    }
    
    public func resolveReferences() throws -> [Reference: Schema.Object] {
        
        typealias Object = Schema.Object
        
        // initialize with self
        var resolvedReferences: [Reference: Object] = [.selfReference: self]
        
        // scan schema for references
        let references = self.allReferences
        
        // get all enclosing "parent" schemas for all references
        let referencesByParentSchema = try references.map { (reference) -> (Reference, Object) in
            
            let schemaObject: Object
            
            // fetch remote reference and get schema
            if let remoteURL = reference.remote {
                
                let jsonData = try Data(contentsOf: remoteURL)
                
                let jsonDecoder = JSONDecoder()
                
                schemaObject = try jsonDecoder.decode(Object.self, from: jsonData)
                
            } else {
                
                // parent schema is self
                schemaObject = self
            }
            
            return (reference, schemaObject)
        }
        
        // reserve buffer for new entries
        resolvedReferences.reserveCapacity(resolvedReferences.count + referencesByParentSchema.count)
        
        // resolve all references using their parent schema
        for (reference, parentSchema) in referencesByParentSchema {
            
            guard let foundSchema = parentSchema[reference.path],
                case let .object(resolvedSchema) = foundSchema
                else { throw ReferenceResolverError.invalid(reference) }
            
            resolvedReferences[reference] = resolvedSchema
        }
        
        // return value
        return resolvedReferences
    }
}

public extension JSONSchema.Draft4 {
    
    public var allReferences: Set<Reference> {
        
        switch self {
        case let .object(object): return object.allReferences
        case let .reference(reference): return [reference]
        }
    }
}

public extension JSONSchema.Draft4.Object {
    
    public var allReferences: Set<Reference> {
        
        var references = [Reference]()
        
        // get references in schema
        references += definitions?.values.reduce([Reference]()) { $0 + $1.allReferences } ?? []
        references += properties?.values.reduce([Reference]()) { $0 + $1.allReferences } ?? []
        references += patternProperties?.values.reduce([Reference]()) { $0 + $1.allReferences } ?? []
        references += dependencies?.values.reduce([Reference]()) { $0 + $1.allReferences } ?? []
        references += properties?.values.reduce([Reference]()) { $0 + $1.allReferences } ?? []
        references += allOf?.rawValue.reduce([Reference]()) { $0 + $1.allReferences } ?? []
        references += anyOf?.rawValue.reduce([Reference]()) { $0 + $1.allReferences } ?? []
        references += oneOf?.rawValue.reduce([Reference]()) { $0 + $1.allReferences } ?? []
        references += not?.value.allReferences ?? []
        
        return Set(references)
    }
}

public extension JSONSchema.Draft4.Object.Dependencies {
    
    public var allReferences: Set<Reference> {
        
        switch self {
        case let .a(a): return a.value.allReferences
        case .b: return []
        }
    }
}
