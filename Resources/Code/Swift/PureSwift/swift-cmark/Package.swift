// swift-tools-version:4.0
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
  name: "cmark",
  products: [
    .library(
      name: "cmark",
      targets: ["cmark"]),
  ],
  targets: [
    .target(
      name: "cmark",
      path: "src",
      // Exclude the main file so cmark is built as a library.
      exclude: ["main.c"]
    )
  ]
)
