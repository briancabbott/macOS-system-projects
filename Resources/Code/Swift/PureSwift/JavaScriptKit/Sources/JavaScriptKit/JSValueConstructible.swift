public protocol JSValueConstructible {
    static func construct(from value: JSValue) -> Self?
}

extension Bool: JSValueConstructible {
    public static func construct(from value: JSValue) -> Bool? {
        value.boolean
    }
}

extension String: JSValueConstructible {
    public static func construct(from value: JSValue) -> String? {
        value.string
    }
}

extension Double: JSValueConstructible {
    public static func construct(from value: JSValue) -> Double? {
        return value.number
    }
}

extension Float: JSValueConstructible {
    public static func construct(from value: JSValue) -> Float? {
        return value.number.map(Float.init)
    }
}

extension Int: JSValueConstructible {
    public static func construct(from value: JSValue) -> Self? {
        value.number.map(Self.init)
    }
}

extension Int8: JSValueConstructible {
    public static func construct(from value: JSValue) -> Self? {
        value.number.map(Self.init)
    }
}

extension Int16: JSValueConstructible {
    public static func construct(from value: JSValue) -> Self? {
        value.number.map(Self.init)
    }
}

extension Int32: JSValueConstructible {
    public static func construct(from value: JSValue) -> Self? {
        value.number.map(Self.init)
    }
}

extension Int64: JSValueConstructible {
    public static func construct(from value: JSValue) -> Self? {
        value.number.map(Self.init)
    }
}

extension UInt: JSValueConstructible {
    public static func construct(from value: JSValue) -> Self? {
        value.number.map(Self.init)
    }
}

extension UInt8: JSValueConstructible {
    public static func construct(from value: JSValue) -> Self? {
        value.number.map(Self.init)
    }
}

extension UInt16: JSValueConstructible {
    public static func construct(from value: JSValue) -> Self? {
        value.number.map(Self.init)
    }
}

extension UInt32: JSValueConstructible {
    public static func construct(from value: JSValue) -> Self? {
        value.number.map(Self.init)
    }
}

extension UInt64: JSValueConstructible {
    public static func construct(from value: JSValue) -> Self? {
        value.number.map(Self.init)
    }
}

extension Array: JSValueConstructible where Element: JSValueConstructible {
    
    public static func construct(from value: JSValue) -> Self? {
        return value.array?.compactMap { Element.construct(from: $0) }
    }
}
