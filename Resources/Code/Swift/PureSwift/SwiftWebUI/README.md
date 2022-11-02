# SwiftWebUI and WASM
This is a fork of the incredible [SwiftWebUI](https://github.com/SwiftWebUI/SwiftWebUI) to support WebAssembly via [swiftwasm](https://swiftwasm.org/).

## What does it do?
This repo allows you to run your SwiftUI projects completely on the web.
```swift
import SwiftWebUI

SwiftWebUI.serve(Text("Hello, world!"))
```
The code is compiled to WebAssembly, and run entirely in the browser.

**This is not a static site generator**. By compiling Swift code to WASM, we can create stateful webapps entirely in Swift:

![Demo GIF](docs/wasmgif.gif)

## How is it different from the main SwiftWebUI?
SwiftWebUI runs a Swift NIO server which handles the changes. With that approach, every time an action is made on the client, the server has to run the operations and send down the changes to the DOM.

This repo removes the need for a server by compiling the code to run in the browser itself. This allows for more flexible and efficient apps.

## Get Started
To create a new SwiftWebUI and WASM project, you can run:
```sh
npx carson-katri/swiftwebui-scripts create MyApp
```
For more information on `swiftwebui-scripts`, view the [repo](https://github.com/carson-katri/swiftwebui-scripts).

## Helpful Packages
* [swiftwebui-router](https://github.com/carson-katri/swiftwebui-router)
  
  *A simple Router for SwiftWebUI*
* [JavaScriptKit](https://github.com/kateinoigakukun/JavaScriptKit)
  
  *Swift framework to interact with JavaScript through WebAssembly*

## Contributing
You can build the project with the following command:
```sh
swift build --triple wasm32-unknown-wasi
```
