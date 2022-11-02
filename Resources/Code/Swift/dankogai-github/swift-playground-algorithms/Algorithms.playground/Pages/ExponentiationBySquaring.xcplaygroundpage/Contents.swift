//: [Previous](@previous)
/*:

# Exponentiation by squaring

* [Wikipedia]

[Wikipedia]: https://en.wikipedia.org/wiki/Exponentiation_by_squaring

*/

//: ## naive, O(n)
func expNaively(base:Double, _ exponent:UInt)->Double {
    var result = 1.0
    for _ in 0..<exponent {
        result *= base
    }
    return result
}

expNaively(1.001, 1000)

//: ## By Squaring, O(log(n))
func expBySquaring(base:Double, _ exponent:UInt)->Double {
    var bits = exponent
    var square = base
    var result = 1.0
    while 0 < bits {
        if bits & 1 == 1 { result *= square }
        bits >>= 1
        square *= square
    }
    return result
}

expBySquaring(1.001, 1000)

//: ## Generalization

protocol HyperOperable: Equatable, Comparable {}

func hyperOf<B:HyperOperable>(zero:B, _ binop:(B,B)->B)->(B,Int)->B {
    return { (b:B, n:Int) in
        var u = UInt(n), t = b, r = zero;
        while u > 0 {
            if u & 1 == 1 { r = binop(t, r) }
            u >>= 1; t = binop(t, t);
        }
        return r
    }
}

extension Int: HyperOperable {}

let mul = hyperOf(0, +)
mul(3, 3)
let pow = hyperOf(1, *)
pow(3, 3)

extension String: HyperOperable {}

let xstr = hyperOf(""){ $0 + $1 }
xstr("x", 15)


//: [Next](@next)
