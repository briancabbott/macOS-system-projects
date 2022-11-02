import RealModule

extension Interval : Real {
    // monotonic functions are easy
    public static func sqrt(_ x:Self)->Self {
        return Self(Element.sqrt(x.min), Element.sqrt(x.max))
    }
//    public static func cbrt(_ x:Interval)->Interval {
//        return Interval(Element.cbrt(x.min), Element.cbrt(x.max))
//    }
    public static func exp(_ x:Self)->Self {
        return Self(Element.exp(x.min), Element.exp(x.max))
    }
    public static func exp2(_ x: Interval<F>) -> Interval<F> {
        return Self(Element.exp2(x.min), Element.exp2(x.max))
    }
    public static func expMinusOne(_ x:Self)->Self {
        return Self(Element.expMinusOne(x.min), Element.expMinusOne(x.max))
    }
    public static func log(_ x:Interval)->Interval {
        return Self(Element.log(x.min), Element.log(x.max))
    }
    public static func log2(_ x:Self)->Self {
        return Self(Element.log2(x.min), Element.log2(x.max))
    }
    public static func log10(_ x:Self)->Self {
        return Self(Element.log10(x.min), Element.log10(x.max))
    }
    public static func log(onePlus x:Self)->Self {
        return Self(Element.log(onePlus:x.min), Element.log(onePlus:x.max))
    }
    public static func sinh(_ x:Self)->Self {
        return Self(Element.sinh(x.min), Element.sinh(x.max))
    }
    public static func tanh(_ x:Self)->Self {
        return Self(Element.tanh(x.min), Element.tanh(x.max))
    }
    public static func acos(_ x:Self)->Self {
        return Self(Element.acos(x.min), Element.acos(x.max))
    }
    public static func asin(_ x:Self)->Self {
        return Self(Element.asin(x.min), Element.asin(x.max))
    }
    public static func atan(_ x:Self)->Self {
        return Self(Element.atan(x.min), Element.atan(x.max))
    }
    public static func acosh(_ x:Self)->Self {
        return Self(Element.acosh(x.min), Element.acosh(x.max))
    }
    public static func asinh(_ x:Self)->Self {
        return Self(Element.asinh(x.min), Element.asinh(x.max))
    }
    public static func atanh(_ x:Self)->Self {
        return Self(Element.atanh(x.min), Element.atanh(x.max))
    }
    // cosh : critical at 0
    public static func cosh(_ x:Self)->Self {
        var values = [x.min, x.max].map{ Element.cosh($0) }
        if x.contains(0.0) { values.append(+1.0) }
        values.sort()
        return Self(min:values.first!, max:values.last!)
    }
    // for trigonometrics
    public static func normalizeAngle(_ x:Self)->Self {
        if -Self.pi <= x && x <= +Self.pi { return x }
        let r = x.remainder(dividingBy: 2*Self.pi)
        return r + (r < -Self.pi ? +2 : +Self.pi < r ? -2 : 0)*Element.pi
    }
    // cos - critical at 0, ±π
    public static func cos(_ x:Self)->Self {
        if 2*Element.pi <= x.max - x.min {
            return Self(min:-1, max:+1)
        }
        let nx = normalizeAngle(x)
        var values = [nx.min, nx.max].map{ Element.sin($0) }
        if x.contains(0)     { values.append(+1.0) }
        if x.contains(+Element.pi) { values.append(+1.0) }
        if x.contains(-Element.pi) { values.append(-1.0) }
        return Self(min:values.first!, max:values.last!)
    }
    // sin - critical at ±π/2
    public static func sin(_ x:Self)->Self {
        if 2*Element.pi <= x.max - x.min {
            return Self(min:-1, max:+1)
        }
        let nx = normalizeAngle(x)
        var values = [nx.min, nx.max].map{ Element.cos($0) }
        if x.contains(+Element.pi/2) { values.append(+1.0) }
        if x.contains(-Element.pi/2) { values.append(-1.0) }
        return Self(min:values.first!, max:values.last!)
    }
    public static func tan(_ x:Self)->Self {
        if Element.pi <= x.max - x.min {
            return Interval(min:-1, max:+1)
        }
        let nx = normalizeAngle(x)
        let values = [nx.min, nx.max].map{ Element.tan($0) }
        return Self(min:values.first!, max:values.last!)
    }
    // binary functions
    /// atan2
    public static func atan2(y:Self, x:Self)->Self  {
        // cf. https://en.wikipedia.org/wiki/Atan2
        //     https://www.freebsd.org/cgi/man.cgi?query=atan2
        if x.isNaN || y.isNaN { return nan }
        let ysgn  = Self(y.sign == .minus ? -1 : +1)
        let xsgn  = Self(x.sign == .minus ? -1 : +1)
        let y_x   = x.isInfinite && y.isInfinite ? ysgn * xsgn : y/x // avoid nan for ±inf/±inf
        if 0 < x {
            return atan(y_x)
        }
        if x < 0 {
            return ysgn * (Self.pi - atan(Swift.abs(y_x)))
        }
        else {  // x.isZero
            return ysgn * (
                y.isZero ? (x.sign == .minus ? Self.pi : 0) : Self.pi/2
            )
        }
    }
    public static func hypot(_ x:Self, _ y:Self)->Self {
        let v00 = Element.hypot(x.min, y.min)
        let v01 = Element.hypot(x.min, y.max)
        let v10 = Element.hypot(x.max, y.min)
        let v11 = Element.hypot(x.max, y.max)
        return Self(min:Swift.min(v00, v01, v10, v11), max:Swift.max(v00, v01, v10, v11))
    }
    public static func pow(_ x:Self, _ y:Self)->Self {
        let v00 = Element.pow(x.min, y.min)
        let v01 = Element.pow(x.min, y.max)
        let v10 = Element.pow(x.max, y.min)
        let v11 = Element.pow(x.max, y.max)
        return Self(min:Swift.min(v00, v01, v10, v11), max:Swift.max(v00, v01, v10, v11))
    }
    public static func pow(_ x: Interval<F>, _ n: Int) -> Interval<F> {
        let v00 = Element.pow(x.min, n)
        let v10 = Element.pow(x.max, n)
        return Self(min:Swift.min(v00, v10), max:Swift.max(v00, v10))
    }
    public static func root(_ x: Interval<F>, _ n: Int) -> Interval<F> {
        return Self(Element.root(x.min, n), Element.root(x.max, n))
    }
    //: mark todo
    public static func erf(_ x: Self) -> Self {
        fatalError("yet to be implemented");
    }
    public static func erfc(_ x: Self) -> Self {
        fatalError("yet to be implemented");
    }
    public static func gamma(_ x: Self) -> Self {
        fatalError("yet to be implemented");
    }
    public static func logGamma(_ x: Self) -> Self {
        fatalError("yet to be implemented");
    }

}
