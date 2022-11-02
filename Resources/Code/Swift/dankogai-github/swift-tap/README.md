[![build status](https://secure.travis-ci.org/dankogai/swift-tap.png)](http://travis-ci.org/dankogai/swift-tap)

# swift-tap

Test Anything Protocol ([TAP]) for Swift

[TAP]: https://testanything.org


## Usage

0. Add this project to your `Package.swift`.

```swift
import PackageDescription
let package = Package(
  // ...
  dependencies: [
    .Package(
      url: "https://github.com/dankogai/swift-tap.git", from: "0.6.0"
    )
  ],
  // ...
)

```

1. write [main.swift] like below:

```swift
import TAP
let test = TAP(tests:3)
test.ok(42+0.195 == 42.195, "42 + 0.195 == 42.195") // ok 1 - 42 + 0.195 == 42.195
test.eq(42+0.195,   42.195, "42 + 0.195 is 42.195") // ok 2 - 42 + 0.195 is 42.195
test.ne(42+0.195,   42,     "42 + 0.195 is not 42") // ok 3 - 42 + 0.195 is not 42
test.done() 
```

2. build and test like below:

```shell
$ swift run && prove .build/debug/main
./main .. ok   
All tests successful.
Files=1, Tests=2,  0 wallclock secs ( 0.02 usr +  0.00 sys =  0.02 CPU)
Result: PASS
```

`./main` actually prints the following:

```
1..3
ok 1 - 42 + 0.195 == 42.195
ok 2 - 42 + 0.195 is 42.195
ok 3 - 42 + 0.195 is not 42
```

### test without plan

You should specify the number of tests to run via `TAP(tests:Int)` or `.plan(_:Int)` but this is optional.  But when you do so you cannot omit `.done()` which prints `1..number_of_tests` that the TAP protocol demands.

```
ok 1 - 42 + 0.195 == 42.195
ok 2 - 42 + 0.195 is 42.195
ok 3 - 42 + 0.195 is not 42
1..2
```

### `.ok`, `.eq` and `.ne`

#### .ok

Checks if `predicate` is true.

```swift
func ok(@autoclosure predicate:()->Bool, _ message:String = "")->Bool
```

#### .eq

Checks if `actual` == `expected`.  Analogous to `.ok(actual == expected)` but it is more informative when it is not ok.

On the other hand both `actual` and `expected` must be `Equatable`.  So:

* use `.eq` whenever you can.
* use `.ok` as a last resort.

```swift
public func eq<T:Equatable>(actual:T, _ expected:T, _ message:String = "")->Bool
```

As you can tell from the type signatures, `.eq` and `.ne` accepts arrays and dictionaries if they are equatable.

#### .ne

The opposite of `.eq`.

```swift
public func ne<T:Equatable>(actual:T, _ expected:T, _ message:String = "")->Bool
```

## Why NOT [XCTest]?

[XCTest]: https://developer.apple.com/library/ios/documentation/DeveloperTools/Conceptual/testing_with_xcode/chapters/01-introduction.html
[travis]: https://travis-ci.org

* Still way too fancy and resource-demanding for some occasions

## See Also

* https://testanything.org
* https://en.wikipedia.org/wiki/Test_Anything_Protocol
