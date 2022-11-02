## Apple Platforms

Xcode includes a release of Swift that is supported by Apple.
You can try out a version that is still in development
by downloading one of the packages above.

<div class="danger" markdown="1">
To submit to the App Store you must build your app using the version of Swift that comes included within Xcode.
</div>

#### Requirements for Tools

### Swift 5.1
* macOS 10.14.6
* [Xcode 11](https://developer.apple.com/xcode/) or later

### Swift 5.0
* macOS 10.14.4
* [Xcode 10.2](https://developer.apple.com/xcode/)

### Swift 4.2
* macOS 10.13.6
* [Xcode 10.0](https://developer.apple.com/xcode/)

### Swift 4.1
* macOS 10.13.2
* [Xcode 9.3 - 9.4](https://developer.apple.com/xcode/)

### Swift 4.0
* macOS 10.13.2
* [Xcode 9.0 - 9.2](https://developer.apple.com/xcode/)

### Swift 3.1
* macOS 10.11.5
* [Xcode 8.3 - 8.3.3](https://developer.apple.com/xcode/)

### Swift 3.0
* macOS 10.11.5
* [Xcode 8.0 - 8.2](https://developer.apple.com/xcode/)

### Swift 2.2.x
* macOS 10.11
* [Xcode 7.2 - 7.3](https://developer.apple.com/xcode/)

#### Supported Target Platforms

* macOS 10.9.0 or later
* iOS 7.0 or later
* watchOS 2.0 or later
* tvOS 9.0 or later

* * *

### Installation

1. Download the [latest package release](/download).

1. Run the package installer,
   which will install an Xcode toolchain into
   `/Library/Developer/Toolchains/`.

   An Xcode toolchain (`.xctoolchain`) includes a copy of the compiler, lldb, and other related tools needed to provide a cohesive development experience for working in a specific version of Swift.

1. Open Xcode's `Preferences`, navigate to `Components > Toolchains`, and select the installed Swift toolchain.

1. Xcode uses the selected toolchain for building Swift code, debugging, and even code completion and syntax coloring.  You'll see a new toolchain indicator in Xcode's toolbar when Xcode is using a Swift toolchain. Select the Xcode toolchain to go back to Xcode's built-in tools.

1. Selecting a Swift toolchain affects the Xcode IDE only.  To use the Swift toolchain with command-line tools, use `xcrun --toolchain swift` and `xcodebuild -toolchain swift`, or add the Swift toolchain to your path as follows:

~~~ shell
$ export PATH=/Library/Developer/Toolchains/swift-latest.xctoolchain/usr/bin:"${PATH}"
~~~

### Code Signing on macOS

The macOS `.pkg` files are digitally signed
by the developer ID of the Swift open source project
to allow verification that they have not been tampered with.
All binaries in the package are signed as well.

The Swift toolchain installer on macOS
should display a lock icon on the right side of the title bar.
Clicking the lock brings up detailed information about the signature.
The signature should be produced by
`Developer ID Installer: Swift Open Source (V9AUD2URP3)`.

<div class="warning" markdown="1">
If the lock is not displayed
or the signature is not produced by the Swift open source developer ID,
do not proceed with the installation.
Instead, quit the installer
and please email <swift-infrastructure@forums.swift.org>
with as much detail as possible,
so that we can investigate the problem.
</div>
