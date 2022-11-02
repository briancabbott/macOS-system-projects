[![Swift 5](https://img.shields.io/badge/swift-5-blue.svg)](https://swift.org)
[![MIT LiCENSE](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![build status](https://secure.travis-ci.org/dankogai/swift-bf.png)](http://travis-ci.org/dankogai/swift-bf)

swift-bf
========

Brainfuck Interpreter/Compiler in Swift

## SYNOPSIS

### Interpreter

```swift
import BF
var bf = BF("+[,<>.]-")!  // echo
print(bf.run(input:"Hello, Swift!")); // Hello, Swift!
```

### Compiler

```swift
import BF
print(BF.compile("+[,<>.]-"));
```

prints

```swift
import Darwin
var data = [CChar](repeating: CChar(0), count:65536)
var (sp, pc) = (0, 0)
data[sp]+=1
while data[sp] != CChar(0) {
data[sp] = {c in CChar(c < 0 ? 0 : c)}(getchar())
sp-=1
sp+=1
putchar(Int32(data[sp]))
}
data[sp]-=1

```

## Usage

Add this project to your `Package.swift`.

```swift

import PackageDescription
let package = Package(
  // ...
  dependencies: [
    .Package(
      url: "https://github.com/dankogai/swift-bf.git", .branch("main")
    )
  ],
  // ...
)
```
