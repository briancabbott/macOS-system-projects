# libjvm Swift package

Allows using `libjvm` in other Swift packages.

Note: this has only been tested on OS X 10.11 right now and will require
different paths if used on any other operating system.

## Usage

```swift
import PackageDescription

let package = Package(
    name: "example",
    dependencies: [
        .Package(url: "https://github.com/neonichu/CJavaVM", majorVersion: 1)
    ]
)
```

Build your dependent packages somewhat like this:

```bash
JVM_LIBRARY_PATH=/Library/Java/JavaVirtualMachines/jdk1.8.0_45.jdk/Contents/Home/jre/lib/server
swift build -Xlinker -L${JVM_LIBRARY_PATH} -Xlinker -rpath -Xlinker ${JVM_LIBRARY_PATH} -ljvm
```
