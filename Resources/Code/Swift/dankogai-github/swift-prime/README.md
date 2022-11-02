# OBSOLETED BY

[PONS], Protocol-Oriented Number System in Pure Swift. 

* https://github.com/dankogai/swift-pons

It has all functionality of swift-prime plus `BigInt` support.

[PONS]: https://github.com/dankogai/swift-pons

[![build status](https://secure.travis-ci.org/dankogai/swift-prime.png)](http://travis-ci.org/dankogai/swift-prime)

# swift-prime

Prime number extension in Pure Swift

## SYNOPSIS

````swift
import Prime                            // not needed if you are using prime.swift directly
2.isPrime                               // true
42.isPrime                              // false
0x7FFFffff.isPrime                      // true (M31)
0.nextPrime                             // 2
Int.max.prevPrime                       // 9223372036854775783 on OS X
Int.Prime.within(0..<100)               // [2, 3, 5, 7, ... 97]
Int.Prime.within(Int.max-100..<Int.max) // [9223372036854775783]
Int.max.primeFactors                    // [7, 7, 73, 127, 337, 92737, 649657]
````

## Requirement

Swift 2.x, both Xcode 7 and Linux.

* It emits warings on Swift 2.2 but works okay.

## USAGE

### In your project

Just add [prime.swift] to it.

[prime.swift]: prime/prime.swift

### Workspace

Browse all the playgrounds and the project via `Prime.xcworkspace`.

### Playground

Just open the playground file of your OS of choice and enjoy.  OSX most detaild.  iOS and tvOS just for testing purpurse.

### REPL

just `make repl`.

#### CAVEAT: you still have to `import Prime` in REPL.

````shell
  1> 42.isPrime
repl.swift:1:1: error: value of type 'Int' has no member 'isPrime'
42.isPrime
^~ ~~~~~~~

  1> import Prime
  2> 42.isPrime 
$R0: Bool = false
  3>
````


#### OS X

````shell
git clone https://github.com/dankogai/swift-prime.git
cd swift-prime
make repl
````

#### Linux

````shell
git clone https://github.com/dankogai/swift-prime.git
cd swift-prime
make SWIFTPATH=${YOUR_SWIFT_PATH} repl # ${YOUR_SWIFT_PATH}=~/swift/usr/bin in my case
````

## CAVEAT

It used to depend on 128-bit arithmetics bridged to C but it is written to be C-Free.  In exchange it is a little slower.
