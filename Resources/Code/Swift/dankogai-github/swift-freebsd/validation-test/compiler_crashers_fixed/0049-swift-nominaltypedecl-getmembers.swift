// RUN: not %target-swift-frontend %s -emit-sil 

// Distributed under the terms of the MIT license
// Test case submitted to project by https://github.com/rnapier (Rob Napier)

var f = 1
var e: Int -> Int = {
    return $0
}
let d: Int =  { c, b in 
}(f, e)
