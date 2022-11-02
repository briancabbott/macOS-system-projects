### Modular structure

Tokamak is built with modularity in mind, providing a multi-platform `TokamakCore` module and
separate modules for platform-specific renderers. Currently, the only available renderer modules are
`TokamakDOM` and `TokamakStaticHTML`, the latter can be used for static websites and server-side
rendering. If you'd like to implement your own custom renderer, please refer to our [renderers
guide](docs/RenderersGuide.md) for more details.

Tokamak users only need to import a renderer module they would like to use, while
`TokamakCore` is hidden as an "internal" `Tokamak` package target. Unfortunately, Swift does not
allow us to specify that certain symbols in `TokamakCore` are private to a package, but they need to
stay `public` for renderer modules to get access to them. Thus, the current workaround is to mark
those symbols with underscores in their names to indicate this. It can be formulated as these
"rules":

1. If a symbol is restricted to a module and has no `public` access control, no need for an
   underscore.
2. If a symbol is part of a public renderer module API (e.g. `TokamakDOM`), no need for an
   underscore, users may use those symbols directly, and it is re-exported from `TokamakCore` by the
   renderer module via `public typealias`.
3. If a function or a type have `public` on them only by necessity to make them available in
   `TokamakDOM`, but unavailable to users (or not intended for public use), underscore is needed to
   indicate that.

The benefit of separate modules is that they allow us to provide separate renderers for different
platforms. Users can pick and choose what they want to use, e.g. purely static websites would use
only `TokamakStaticHTML`, single-page apps would use `TokamakDOM`, maybe in conjuction with
`TokamakStaticHTML` for pre-rendering. As we'd like to try to implement a native renderer for
Android at some point, probably in a separate `TokamakAndroid` module, Android apps would use
`TokamakAndroid` with no need to be aware of any of the web modules.

### Coding Style

This project uses [SwiftFormat](https://github.com/nicklockwood/SwiftFormat) and
[SwiftLint](https://github.com/realm/SwiftLint) to enforce formatting and coding style. SwiftFormat
0.45.3 and SwiftLint 0.39.2 or later versions are recommended. We encourage you to run SwiftFormat
and SwiftLint within a local clone of the repository in whatever way works best for you. You can do
that either manually, or automatically with VSCode extensions for
[SwiftFormat](https://github.com/vknabel/vscode-swiftformat) and
[SwiftLint](https://github.com/vknabel/vscode-swiftlint) respectively, or with the [Xcode
extension](https://github.com/nicklockwood/SwiftFormat#xcode-source-editor-extension), or [build
phase](https://github.com/nicklockwood/SwiftFormat#xcode-build-phase).

To guarantee that these tools run before you commit your changes on macOS, you're encouraged to run
this once to set up the [pre-commit](https://pre-commit.com/) hook:

```
brew bundle # installs SwiftLint, SwiftFormat and pre-commit
pre-commit install # installs pre-commit hook to run checks before you commit
```

Refer to [the pre-commit documentation page](https://pre-commit.com/) for more details
and installation instructions for other platforms.

SwiftFormat and SwiftLint also run on CI for every PR and thus a CI build can
fail with inconsistent formatting or style. We require CI builds to pass for all
PRs before merging.
