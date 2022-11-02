//
//  CoreGraphics+Codable.swift
//  Codable

#if os(macOS) || os(iOS) || os(watchOS) || os(tvOS)

import CoreGraphics

extension CGPoint : Codable {
    public init(from decoder: Decoder) throws {
        var container = try decoder.unkeyedContainer()
        let x = try container.decode(CGFloat.self)
        let y = try container.decode(CGFloat.self)
        self.init(x: x, y: y)
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.unkeyedContainer()
        try container.encode(x)
        try container.encode(y)
    }
}

extension CGSize : Codable {
    public init(from decoder: Decoder) throws {
        var container = try decoder.unkeyedContainer()
        let width = try container.decode(CGFloat.self)
        let height = try container.decode(CGFloat.self)
        self.init(width: width, height: height)
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.unkeyedContainer()
        try container.encode(width)
        try container.encode(height)
    }
}

extension CGVector : Codable {
    public init(from decoder: Decoder) throws {
        var container = try decoder.unkeyedContainer()
        let dx = try container.decode(CGFloat.self)
        let dy = try container.decode(CGFloat.self)
        self.init(dx: dx, dy: dy)
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.unkeyedContainer()
        try container.encode(dx)
        try container.encode(dy)
    }
}

extension CGRect : Codable {
    public init(from decoder: Decoder) throws {
        var container = try decoder.unkeyedContainer()
        let origin = try container.decode(CGPoint.self)
        let size = try container.decode(CGSize.self)
        self.init(origin: origin, size: size)
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.unkeyedContainer()
        try container.encode(origin)
        try container.encode(size)
    }
}

extension CGAffineTransform : Codable {
    public init(from decoder: Decoder) throws {
        var container = try decoder.unkeyedContainer()
        let a = try container.decode(CGFloat.self)
        let b = try container.decode(CGFloat.self)
        let c = try container.decode(CGFloat.self)
        let d = try container.decode(CGFloat.self)
        let tx = try container.decode(CGFloat.self)
        let ty = try container.decode(CGFloat.self)
        self.init(a: a, b: b, c: c, d: d, tx: tx, ty: ty)
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.unkeyedContainer()
        try container.encode(a)
        try container.encode(b)
        try container.encode(c)
        try container.encode(d)
        try container.encode(tx)
        try container.encode(ty)
    }
}
    
#endif

