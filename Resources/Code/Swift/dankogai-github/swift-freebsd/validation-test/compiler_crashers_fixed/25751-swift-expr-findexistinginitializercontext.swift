// RUN: not %target-swift-frontend %s -parse

// Distributed under the terms of the MIT license
// Test case submitted to project by https://github.com/practicalswift (practicalswift)
// Test case found by fuzzing

struct D{
struct B<a
}
enum B{
func a<T where f:b
var f:D.B
