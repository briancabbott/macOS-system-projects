///
extension ComplexFloat where Element == Double {
    public init(_ cq:Complex<BigRat>) {
        self.init(cq.real.asDouble, cq.imag.asDouble)
    }
}
extension ComplexFloat where Element == BigRat {
    public init(_ cq:Complex<Double>) {
        self.init(cq.real.asBigRat, cq.imag.asBigRat)
    }
}

/// Same functions as `Complex`, but with precision arguments

extension ComplexFloat where Element:BigFloatingPoint {
    /// truncated to `width` bytes
    public func truncated(width: Int=Element.precision)->Self {
        return Self(real:real.truncated(width: width), imag:imag.truncated(width: width))
    }
    /// truncate `real` and `imag` to `width` bytes
    public mutating func truncate(width: Int=Element.precision) {
        self = self.truncated(width: width)
    }
    /// absolute value of `self` to `precision`
    public func abs(precision px:Int=Element.precision)->Element {
        return imag.isZero ? real : Element.hypot(real, imag, precision:px)
    }
    // magnitude = abs
    public var magnitude:Element {
        return self.abs
    }
    /// argument to `precision`
    public func arg(precision px:Int=Element.precision)->Element {
        return imag.isZero ? 0 < real ? 0 : -Element.PI(precision:px) : Element.atan2(imag, real, precision:px)
    }
    /// hypotenuse. defined as √(lhs**2 + rhs**2) though its need for Complex is moot.
    public static func hypot(_ lhs:Self, _ rhs:Self, precision px:Int=Element.precision) -> Self {
        return sqrt(lhs*lhs + rhs*rhs, precision:px)
    }
    public static func hypot(_ lhs:Self, _ rhs:Element, precision px:Int=Element.precision)->Self { return hypot(lhs, Self(rhs)) }
    public static func hypot(_ lhs:Element, _ rhs:Self, precision px:Int=Element.precision)->Self { return hypot(Self(lhs), rhs) }
    public static func hypot(_ lhs:Element, _ rhs:Element, precision px:Int=Element.precision)->Self { return Self(Element.hypot(lhs, rhs)) }
    /// atan2
    public static func atan2(_ lhs:Self, _ rhs:Self, precision px:Int=Element.precision) -> Self {
        return atan2(lhs, rhs, precision:px)
    }
    public static func atan2(_ lhs:Self, _ rhs:Element, precision px:Int=Element.precision)->Self { return atan(lhs/rhs, precision:px) }
    public static func atan2(_ lhs:Element, _ rhs:Self, precision px:Int=Element.precision)->Self { return atan(lhs/rhs, precision:px) }
    public static func atan2(_ lhs:Element, _ rhs:Element, precision px:Int=Element.precision)->Self { return atan(lhs/rhs, precision:px) }
    /// square root of z in Complex
    public static func sqrt(_ z:Self, precision px:Int=Element.precision) -> Self {
        let a = z.abs
        let r = Element.sqrt((a + z.real)/2, precision:px)
        let i = Element.sqrt((a - z.real)/2, precision:px)
        return Self(r, z.imag.sign == .minus ? -i : i)
    }
    public static func sqrt(_ x:Element, precision px:Int=Element.precision)->Self { return sqrt(Self(x),precision: px) }
    /// e ** z in Complex
    public static func exp(_ z:Self, precision px:Int=Element.precision)->Self {
        let r = Element.exp(z.real, precision:px)
        let a = z.imag
        return Self(r * Element.cos(a, precision:px), r * Element.sin(a, precision:px))
    }
    public static func exp(_ x:Element, precision px:Int=Element.precision)->Self { return Self(Element.exp(x, precision:px)) }
    /// e ** z - 1.0 in Complex
    /// cf. https://lists.gnu.org/archive/html/octave-maintainers/2008-03/msg00174.html
    public static func expm1(_ z:Self, precision px:Int=Element.precision)->Self {
        return -exp(z/2, precision:px) * 2 * sin(z.i/2, precision:px).i
    }
    public static func expm1(_ x:Element, precision px:Int=Element.precision)->Self { return Self(Element.expm1(x, precision:px)) }
     /// natural log of z in Complex
    public static func log(_ z:Self, precision px:Int=Element.precision)->Self {
        return Self(Element.log(z.abs(precision:px), precision:px), z.arg(precision:px))
    }
    public static func log(_ x:Element, precision px:Int=Element.precision)->Self { return log(Self(x), precision:px) }
    /// natural log of (z + 1) in Complex
    public static func log1p(_ z:Self, precision px:Int=Element.precision)->Self {
        return 2*atanh(z/(z+2), precision:px)
    }
    public static func log1p(_ x:Element, precision px:Int=Element.precision)->Self { return Self(Element.log1p(x, precision:px)) }
    /// base 2 log of z in Complex
    public static func log2(_ z:Self, precision px:Int=Element.precision)->Self {
        return log(z, precision:px) / Element.log(2, precision:px)
    }
    public static func log2(_ x:Element, precision px:Int=Element.precision)->Self { return log2(Self(x), precision:px) }
    /// base 10 log of z in Complex
    public static func log10(_ z:Self, precision px:Int=Element.precision)->Self {
        return log(z, precision:px) / Element.log(10, precision:px)
    }
    public static func log10(_ x:Element, precision px:Int=Element.precision)->Self { return log10(Self(x), precision:px) }
    /// lhs ** rhs in Complex
    public static func pow(_ lhs:Self, _ rhs:Self, precision px:Int=Element.precision)->Self {
        return exp(log(lhs, precision:px) * rhs, precision:px)
    }
    public static func pow(_ lhs:Self, _ rhs:Element, precision px:Int=Element.precision)->Self { return pow(lhs, Self(rhs), precision:px) }
    public static func pow(_ lhs:Element, _ rhs:Self, precision px:Int=Element.precision)->Self { return pow(Self(lhs), rhs, precision:px) }
    public static func pow(_ lhs:Element, _ rhs:Element, precision px:Int=Element.precision)->Self { return Self(Element.pow(lhs, rhs, precision:px)) }
    /// cosine of z in Complex
    public static func cos(_ z:Self, precision px:Int=Element.precision) -> Self {
        return Self(
            +Element.cos(z.real, precision:px) * Element.cosh(z.imag, precision:px),
            -Element.sin(z.real, precision:px) * Element.sinh(z.imag, precision:px)
        )
    }
    public static func cos(_ x:Element, precision px:Int=Element.precision)->Self { return cos(Self(x)) }
    /// sine of z in Complex
    public static func sin(_ z:Self, precision px:Int=Element.precision) -> Self {
        return Self(
            +Element.sin(z.real, precision:px) * Element.cosh(z.imag, precision:px),
            +Element.cos(z.real, precision:px) * Element.sinh(z.imag, precision:px)
        )
    }
    public static func sin(_ x:Element, precision px:Int=Element.precision)->Self { return sin(Self(x), precision:px) }
    /// tangent of z in Complex
    public static func tan(_ z:Self, precision px:Int=Element.precision) -> Self {
        return sin(z, precision:px) / cos(z, precision:px)
    }
    public static func tan(_ x:Element, precision px:Int=Element.precision) -> Self { return tan(Self(x), precision:px) }
    /// arc cosine of z in Complex
    public static func acos(_ z:Self, precision px:Int=Element.precision) -> Self {
        return log(z - sqrt(1 - z*z, precision:px).i, precision:px).i
    }
    public static func acos(_ x:Element, precision px:Int=Element.precision) -> Self { return acos(Self(x), precision:px) }
    /// arc sine of z in Complex
    public static func asin(_ z:Self, precision px:Int=Element.precision) -> Self {
        return -log(z.i + sqrt(1 - z*z, precision:px), precision:px).i
    }
    public static func asin(_ x:Element, precision px:Int=Element.precision) -> Self { return asin(Self(x), precision:px) }
    /// arc tangent of z in Complex
    public static func atan(_ z:Self, precision px:Int=Element.precision) -> Self {
        let lp = log(1 - z.i, precision:px)
        let lm = log(1 + z.i, precision:px)
        return (lp - lm).i / 2
    }
    public static func atan(_ x:Element, precision px:Int=Element.precision) -> Self { return atan(Self(x), precision:px) }
    /// hyperbolic cosine of z in Complex
    public static func cosh(_ z:Self, precision px:Int=Element.precision) -> Self {
        // return (exp(z) + exp(-z)) / T(2)
        return cos(z.i, precision:px)
    }
    public static func cosh(_ x:Element, precision px:Int=Element.precision) -> Self { return cosh(Self(x), precision:px) }
    /// hyperbolic sine of z in Complex
    public static func sinh(_ z:Self, precision px:Int=Element.precision) -> Self {
        // return (exp(z) - exp(-z)) / T(2)
        return -sin(z.i, precision:px).i;
    }
    public static func sinh(_ x:Element, precision px:Int=Element.precision) -> Self { return sinh(Self(x), precision:px) }
    /// hyperbolic tangent of z in Complex
    public static func tanh(_ z:Self, precision px:Int=Element.precision) -> Self {
        // let ez = exp(z), e_z = exp(-z)
        // return (ez - e_z) / (ez + e_z)
        return sinh(z, precision:px) / cosh(z, precision:px)
    }
    public static func tanh(_ x:Element, precision px:Int=Element.precision) -> Self { return tanh(Self(x), precision:px) }
    /// inverse hyperbolic cosine of z in Complex
    public static func acosh(_ z:Self, precision px:Int=Element.precision) -> Self {
        return log(z + sqrt(z+1)*sqrt(z-1), precision:px)
    }
    public static func acosh(_ x:Element, precision px:Int=Element.precision) -> Self { return acosh(Self(x), precision:px) }
    /// inverse hyperbolic cosine of z in Complex
    public static func asinh(_ z:Self, precision px:Int=Element.precision) -> Self {
        return log(z + sqrt(z*z+1))
    }
    public static func asinh(_ x:Element, precision px:Int=Element.precision) -> Self { return asinh(Self(x), precision:px) }
    /// inverse hyperbolic tangent of z in Complex
    public static func atanh(_ z:Self, precision px:Int=Element.precision) -> Self {
        return (log(1 + z, precision:px) - log(1 - z, precision:px)) / 2
    }
    public static func atanh(_ x:Element, precision px:Int=Element.precision) -> Self { return atanh(Self(x), precision:px) }
 }
