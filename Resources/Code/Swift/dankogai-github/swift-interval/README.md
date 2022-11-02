[![Swift 5](https://img.shields.io/badge/swift-5-blue.svg)](https://swift.org)
[![MIT LiCENSE](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![build status](https://secure.travis-ci.org/dankogai/swift-interval.png)](http://travis-ci.org/dankogai/swift-interval)

# swift-interval

[Interval Arithmetic] in Swift with Interval Type

[Interval Arithmetic]: https://en.wikipedia.org/wiki/Interval_arithmetic

## Synopsis

````swift
import Interval         // needed only if you "make repl"
let about1 = 1.0 ± 0.1  // 0.9...1.1
about1+about1           // 1.8...2.2
about1-about1           // -0.2...0.2
about1*about1           // 0.81...1.21
about1/about1           // 0.818181818181818...1.22222222222222
````
## Prerequisite

Swift 5.0 or better, OS X or Linux.

## Usage

### in your project:

Just add [interval.swift] to it.

[interval.swift]: ./interval/interval.swift

### with playground

Have fun with [Interval.playground] that is a part of this git repo.

[Interval.playground]: ./Interval.playground

When you use it, make sure you turn on the left pane (it's off right after you pulled since UI settings are `.gitignore`d).  As you see above, this playground consists of multiple pages and sources.

#### with your playground

Just drop [interval.swift] to `Sources`.  In git `Interval.playground/Sources/interval.swift` is a symlink thereto.

### REPL via command line:

#### OS X with Xcode
````shell
git clone https://github.com/dankogai/swift-interval.git
cd swift-interval
make repl
````

#### Linux
````shell
git clone https://github.com/dankogai/swift-interval.git
cd swift-interval
make SWIFTPATH=${YOUR_SWIFT_PATH} repl # ${YOUR_SWIFT_PATH}=~/swift/usr/bin in my case
````

### Prerequisite

Swift 5 or better, OS X or Linux to build.
