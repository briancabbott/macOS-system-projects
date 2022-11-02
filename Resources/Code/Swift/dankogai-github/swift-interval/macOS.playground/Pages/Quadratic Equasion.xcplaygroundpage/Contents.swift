//: [Previous](@previous)
import Interval

//: cf http://verifiedby.me/adiary/070

func det<T:IntervalElement>(_ a:Interval<T>, _ b:Interval<T>, _ c:Interval<T>)->Interval<T> {
    return Interval<T>.sqrt(b*b - T(4)*a*c)
}

let a = Interval(1.0)
let b = Interval(1e15)
let c = Interval(1e14)
let x = (-b + det(a,b,c)) / (2.0 * a)
let y = (2.0 * c) / (-b - det(a,b,c))

//: [Next](@next)
