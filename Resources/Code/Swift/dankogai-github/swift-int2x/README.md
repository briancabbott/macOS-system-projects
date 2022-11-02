[![Swift 5](https://img.shields.io/badge/swift-5-brightgreen.svg)](https://swift.org)
[![MIT LiCENSE](https://img.shields.io/badge/license-MIT-brightgreen.svg)](LICENSE)
[![CI via GitHub Actions](https://github.com/dankogai/swift-int2x/actions/workflows/swift.yml/badge.svg)](https://github.com/dankogai/swift-int2x/actions/workflows/swift.yml)

# swift-int2x

Create Double-Width Integers with Ease

## Synopsis

```swift
import Int2X

typealias U128 = UInt2X<UInt64> // Yes.  That's it!
typealias I128 = Int2X<UInt64>  // ditto for signed integers
```

## Description

Thanks to [SE-0104], making your own integer types is easier than ever.  This module makes use of it -- creating double-width integer from any given [FixedWidthInteger].

[SE-0104]: https://github.com/apple/swift-evolution/blob/master/proposals/0104-improved-integers.md
[FixedWidthInteger]: https://developer.apple.com/documentation/swift/fixedwidthinteger

U?Int{128,256,512,1024} are predefined as follows:

```swift
public typealias UInt128    = UInt2X<UInt64>
public typealias UInt256    = UInt2X<UInt128>
public typealias UInt512    = UInt2X<UInt256>
public typealias UInt1024   = UInt2X<UInt512>
```

```swift
public typealias Int128    = Int2X<UInt64>
public typealias Int256    = Int2X<UInt128>
public typealias Int512    = Int2X<UInt256>
public typealias Int1024   = Int2X<UInt512>
```

As you see, `UInt2X` and `Int2X` themselves are [FixedWidthInteger] so you can stack them up.

## Usage

### build

```sh
$ git clone https://github.com/dankogai/swift-int2x.git
$ cd swift-int2x # the following assumes your $PWD is here
$ swift build
```

### REPL

```sh
$ scripts/run-repl.sh
```

or

```sh
$ swift run --repl
```

and in your repl,

```sh
Welcome to Apple Swift version 4.2 (swiftlang-1000.11.37.1 clang-1000.11.45.1). Type :help for assistance.
  1> import Int2X 
  2> Int1024.max.description
$R0: String = "89884656743115795386465259539451236680898848947115328636715040578866337902750481566354238661203768010560056939935696678829394884407208311246423715319737062188883946712432742638151109800623047059726541476042502884419075341171231440736956555270413618581675255342293149119973622969239858152417678164812112068607"
```

### From Your SwiftPM-Managed Projects

Add the following to the `dependencies` section:

```swift
.package(
  url: "https://github.com/dankogai/swift-int2x.git", .branch("main")
)
```

and the following to the `.target` argument:

```swift
.target(
  name: "YourSwiftyPackage",
  dependencies: ["Int2X"])
```

Now all you have to do is:

```swift
import Int2X
```

in your code.  Enjoy!

# Prerequisite

Swift 5 or better, OS X or Linux to build.
