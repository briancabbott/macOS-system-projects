// RUN: not %target-swift-frontend %s -parse 

// Distributed under the terms of the MIT license
// Test case submitted to project by https://github.com/fluidsonic (Marc Knaup)

class A {
    class func a() -> Self {
        return b(self.dynamicType) 
    }
}
func b<T>(t: AnyObject.Type) -> T! {
    return nil
}
