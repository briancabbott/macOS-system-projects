import RealModule

public protocol IntervalElement: Real,
    ExpressibleByFloatLiteral, CustomDebugStringConvertible {}

extension Double:   IntervalElement {}
extension Float:    IntervalElement {}

#if os(iOS) || os(watchOS)
#else
// extension Float80:  IntervalElement {}
#endif
/// definition
public struct Interval<F:IntervalElement> : Hashable {
    public typealias Element = F
    public var (min, max):(F, F)
    public init(min:F, max:F) {
        self.min = min
        self.max = max
    }
}
/// inits
extension Interval : ExpressibleByIntegerLiteral, ExpressibleByFloatLiteral {
    public typealias IntegerLiteralType = Element.IntegerLiteralType
    public typealias FloatLiteralType = Double
    public init(_ x:Element, _ y:Element) {
        self.min = Swift.min(x, y)
        self.max = Swift.max(x, y)
    }
    public init(_ x:Element) {
        self.init(x - x.ulp, x + x.ulp)
    }
    public init(_ x:Interval) {
        self = x
    }
    public init(_ r:Range<Element>) {
        self.init(r.lowerBound, r.upperBound)
    }
    public init(signOf s:Interval, magnitudeOf m:Interval) {
        let signum = Element(s.sign == .minus ? -1 : +1)
        self.init(signum * m.min, signum * m.max)
    }
   public init(integerLiteral value: F.IntegerLiteralType) {
        self.init(Element(integerLiteral:value))
    }
    public init(floatLiteral value: Double) {
        self.init(Element(floatLiteral:value as! F.FloatLiteralType))
    }
    public var avg:Element {
        return (min + max)/2
    }
    public var mid:Element { return avg }
    public var err:Element {
        return Swift.abs(avg - min)
    }
}
/// comparison
extension Interval: Comparable {
    public func isIdentical(to other:Interval)->Bool {
        return self.min == other.min && self.max == other.max
    }
    public static func ===(lhs: Interval<F>, rhs: Interval<F>) -> Bool {
        return lhs.isIdentical(to:rhs)
    }
    public func contains(_ other:Element)->Bool {
        return (self.min...self.max).contains(other)
    }
    public static func ==(lhs: Interval, rhs: Element) -> Bool {
        return lhs.contains(rhs)
    }
    public static func ==(lhs: Element, rhs: Interval) -> Bool {
        return rhs.contains(lhs)
    }
    public func contains(_ other:Interval)->Bool {
        return self.contains(other.min) && self.contains(other.max)
    }
    public static func ==(lhs: Interval, rhs: Interval) -> Bool {
        return lhs.contains(rhs) || rhs.contains(lhs)
    }
    public func isEqual(to other: Interval) -> Bool {
        return self == other
    }
    public static func <(lhs: Interval, rhs: Interval) -> Bool {
        return lhs.max < rhs.min
    }
    public func isLess(than other: Interval<F>) -> Bool {
        return self < other
    }
    public func isLessThanOrEqualTo(_ other: Interval<F>) -> Bool {
        return self.isLess(than: other) || self.isEqual(to: other)
    }
    public func isTotallyOrdered(belowOrEqualTo other: Interval<F>) -> Bool {
        return self.isNaN || other.isNaN || self.isLessThanOrEqualTo(other)
    }
    public static func <(lhs: Interval, rhs: Element) -> Bool {
        return lhs.max < rhs
    }
    public static func <(lhs: Element, rhs: Interval) -> Bool {
        return lhs < rhs.min
    }
}
/// now fun part
extension Interval : SignedNumeric { // already Numeric
    public typealias Magnitude = Interval
    public init?<T>(exactly source: T) where T : BinaryInteger {
        guard let f = Element(exactly: source) else { return nil }
        self.init(f)
    }
    public static prefix func +(_ i:Interval)->Interval {
        return i
    }
    public static prefix func -(_ i:Interval)->Interval {
        return Interval(min:-i.max, max:-i.min)
    }
    public mutating func negate() {
        self = -self
    }
    public var magnitude: Interval {
        return min.sign != max.sign ? self : min.sign == .minus ? -self : self
    }
    public static func * (lhs: Element, rhs: Interval) -> Interval {
        return Interval(min:lhs * rhs.min, max:lhs * rhs.max)
    }
    public static func * (lhs: Interval, rhs: Element) -> Interval {
        return Interval(min:lhs.min * rhs, max:lhs.max * rhs)
    }
    public static func * (lhs: Interval, rhs: Interval) -> Interval {
        let v00 = lhs.min * rhs.min
        let v01 = lhs.min * rhs.max
        let v10 = lhs.max * rhs.min
        let v11 = lhs.max * rhs.max
        return Interval(min:Swift.min(v00, v01, v10, v11), max:Swift.max(v00, v01, v10, v11))
    }
    public static func *= (lhs: inout Interval, rhs: Interval) {
        lhs = lhs * rhs
    }
    public static func + (lhs: Element, rhs: Interval) -> Interval {
        return Interval(min:lhs + rhs.min, max:lhs + rhs.max)
    }
    public static func + (lhs: Interval, rhs: Element) -> Interval {
        return Interval(min:lhs.min + rhs, max:lhs.max + rhs)
    }
    public static func + (lhs: Interval, rhs: Interval) -> Interval {
        return Interval(min:lhs.min + rhs.min, max:lhs.max + rhs.max)
    }
    public static func += (lhs: inout Interval, rhs: Interval) {
        lhs = lhs + rhs
    }
    public static func - (lhs: Element, rhs: Interval) -> Interval {
        return Interval(min:lhs - rhs.min, max:lhs - rhs.max)
    }
    public static func - (lhs: Interval, rhs: Element) -> Interval {
        return Interval(min:lhs.min - rhs, max:lhs.max - rhs)
    }
    public static func - (lhs: Interval, rhs: Interval) -> Interval {
        return lhs + (-rhs)
    }
    public static func -= (lhs: inout Interval, rhs: Interval) {
        lhs = lhs - rhs
    }
}
/// FloatingPoint is Strideable
extension Interval : Strideable {
    public typealias Stride = Interval
    public func distance(to other: Interval) -> Interval {
        return self - other
    }
    public func advanced(by n: Interval) -> Interval {
        return self + n
    }
}
/// and Finally, make it conform to FloatingPoint
extension Interval : FloatingPoint {
    public typealias Exponent = Element.Exponent
    
    public init(sign:FloatingPointSign, exponent:Exponent, significand:Interval) {
        self.init(
            min: Element(sign:sign, exponent:Exponent(exponent), significand:significand.min),
            max: Element(sign:sign, exponent:Exponent(exponent), significand:significand.max)
        )
    }
    public var sign:FloatingPointSign {
        return min.sign != max.sign ? avg.sign : min.sign
    }
    public var exponent:Exponent {
        return min.exponent != max.exponent ? avg.exponent : min.exponent
    }
    public var significand: Interval {
        let (smin, smax) = (self.min.significand, self.max.significand)
        return Interval(min:Swift.min(smin, smax), max:Swift.max(smin, smax))
    }
    public static var radix:Int {
        return Element.radix
    }
    // Thanks to Swift 5 this is now generic!
    //    public init (_ value:UInt)  { self.init(Element(value)) }
    //    public init (_ value:UInt8) { self.init(Element(value)) }
    //    public init (_ value:UInt16){ self.init(Element(value)) }
    //    public init (_ value:UInt32){ self.init(Element(value)) }
    //    public init (_ value:UInt64){ self.init(Element(value)) }
    //    public init (_ value:Int)   { self.init(Element(value)) }
    //    public init (_ value:Int8)  { self.init(Element(value)) }
    //    public init (_ value:Int16) { self.init(Element(value)) }
    //    public init (_ value:Int32) { self.init(Element(value)) }
    //    public init (_ value:Int64) { self.init(Element(value)) }
    public init<Source>(_ value: Source) where Source : BinaryInteger {
        self.init(Element(value))
    }
    //
    public static var nan:Interval          { return Interval(min:Element.nan, max:Element.nan) }
    public var isNaN:Bool                   { return self.min.isNaN || self.max.isNaN }
    public static var signalingNaN:Interval { return Interval(min:Element.signalingNaN, max:Element.signalingNaN) }
    public var isSignalingNaN:Bool          { return self.min.isNaN || self.max.isNaN }
    public static var infinity:Interval     { return Interval(min:Element.infinity, max:Element.infinity) }
    public var isInfinite:Bool              { return self.min.isInfinite || self.max.isInfinite }
    public var isFinite:Bool                { return self.min.isFinite   && self.max.isFinite   }
    public static var zero:Interval         { return Interval(min:0.0, max:0.0) }
    public var isZero:Bool                  { return self.contains(0.0) }
    public var isNormal:Bool        { return self.min.isNormal && self.max.isNormal }
    public var isSubnormal: Bool    { return self.min.isSubnormal && self.max.isSubnormal }
    public var isCanonical: Bool    { return self.min.isCanonical && self.max.isCanonical }
    public static var pi:Interval   { return Interval(Element.pi) }
    public static var greatestFiniteMagnitude:Interval {
        return Interval(min:Element.greatestFiniteMagnitude, max:Element.greatestFiniteMagnitude)
    }
    public static var leastNormalMagnitude:Interval {
        return Interval(min:Element.leastNormalMagnitude, max:Element.leastNormalMagnitude)
    }
    public static var leastNonzeroMagnitude:Interval {
        return Interval(min:Element.leastNonzeroMagnitude, max:Element.leastNonzeroMagnitude)
    }
    public var ulp: Interval {
        let u = Swift.max(self.min.ulp, self.max.ulp)
        return Interval(min:u, max:u)
    }
    public var nextUp: Interval<F> {
        return self + ulp
    }
    public var reciprocal:Interval {
        return self.isZero
            ? self.min.isZero ? Interval(min:1.0/self.max,      max:+Element.infinity)
            : self.max.isZero ? Interval(min:-Element.infinity, max:1.0/self.min     )
            :                   Interval(min:-Element.infinity, max:+Element.infinity)
            :                   Interval(min:1.0/self.max,      max:1.0/self.min     )
    }
    public static func / (lhs: Interval, rhs: Interval) -> Interval {
        return lhs * rhs.reciprocal
    }
    public static func / (lhs: Element, rhs: Interval) -> Interval {
        return Interval(min:lhs, max:lhs) * rhs.reciprocal
    }
    public static func / (lhs: Interval, rhs: Element) -> Interval {
        return Interval(min:lhs.min/rhs, max:lhs.max/rhs)
    }
    public static func /= (lhs: inout Interval, rhs: Interval) {
        lhs = lhs / rhs
    }
    public mutating func formTruncatingRemainder(dividingBy other: Interval) {
        self = self / other.rounded()
    }
    public mutating func formRemainder(dividingBy other: Interval) {
        self -= self.truncatingRemainder(dividingBy: other)
    }
    public mutating func formSquareRoot() {
        self.min = self.min.squareRoot()
        self.max = self.max.squareRoot()
    }
    public mutating func addProduct(_ lhs: Interval, _ rhs: Interval) {
        self += lhs * rhs
    }
    public mutating func round(_ rule: FloatingPointRoundingRule) {
        let v0 = min.rounded(rule)
        let v1 = max.rounded(rule)
        min = Swift.min(v0, v1)
        max = Swift.max(v0, v1)
    }
}
/// CustomStringConvertible
extension Interval : CustomStringConvertible, CustomDebugStringConvertible {
    public var description:String {
        if self.isNaN || self.isInfinite { return self.debugDescription }
        return "\(self.avg)±\(self.err)"
    }
    public var debugDescription:String {
        return "(\(self.min.debugDescription)...\(self.max.debugDescription))"
    }
}
extension Interval : Codable where F:Codable {
    public enum CodingKeys : String, CodingKey {
        public typealias RawValue = String
        case min, max
    }
    public init(from decoder: Decoder) throws {
        let values = try decoder.container(keyedBy: CodingKeys.self)
        self.min = try values.decode(Element.self, forKey: .min)
        self.max = try values.decode(Element.self, forKey: .max)
    }
    public func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(self.min, forKey: .min)
        try container.encode(self.max, forKey: .max)
    }
}
/// operators
infix operator +-: AdditionPrecedence
infix operator ±: AdditionPrecedence
extension IntervalElement {
    public static func +- (_ lhs:Self, _ rhs:Self)->Interval<Self> {
        return Interval(lhs - rhs, lhs + rhs)
    }
    public static func ± (_ lhs:Self, _ rhs:Self)->Interval<Self> {
        return Interval(lhs - rhs, lhs + rhs)
    }
}
