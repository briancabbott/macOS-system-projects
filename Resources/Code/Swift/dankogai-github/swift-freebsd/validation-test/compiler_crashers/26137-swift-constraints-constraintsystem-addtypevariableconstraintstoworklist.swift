// RUN: not --crash %target-swift-frontend %s -parse
// Distributed under the terms of the MIT license
// Test case submitted to project by https://github.com/practicalswift (practicalswift)
// Test case found by fuzzing

struct Q<T where I:a{{{}}struct Q{{
}struct B{class b{var f=B<T
var f=B{
