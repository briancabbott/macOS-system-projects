# Add CustomDebugStringConvertible conformance to AnyKeyPath

* Proposal: [SE-0369](0369-add-customdebugdescription-conformance-to-anykeypath.md)
* Author: [Ben Pious](https://github.com/benpious)
* Review Manager: [Xiaodi Wu](https://github.com/xwu)
* Status: **Implemented (Swift 5.8)**
* Implementation: [apple/swift#60133](https://github.com/apple/swift/pull/60133)
* Review: ([pitch](https://forums.swift.org/t/pitch-add-customdebugdescription-conformance-to-anykeypath/58705)) ([review](https://forums.swift.org/t/se-0369-add-customdebugstringconvertible-conformance-to-anykeypath/59704)) ([acceptance](https://forums.swift.org/t/accepted-se-0369-add-customdebugstringconvertible-conformance-to-anykeypath/60001))

## Introduction

This proposal is to add conformance to the protocol `CustomDebugStringConvertible` to `AnyKeyPath`. 

## Motivation

Currently, passing a keypath to `print()`, or to the `po` command in LLDB, yields the standard output for a Swift class. This is not very useful. For example, given
```swift
struct Theme {

    var backgroundColor: Color
    var foregroundColor: Color
    
    var overlay: Color {
        backgroundColor.withAlpha(0.8)
    }
}
```
`print(\Theme.backgroundColor)` would have an output of roughly
```
Swift.KeyPath<Theme, Color>
```
which doesn't allow `foregroundColor` to be distinguished from any other property on `Theme`. 

Ideally, the output would be
```
\Theme.backgroundColor
```
exactly as it was written in the program. 

## Proposed solution

Take advantage of whatever information is available in the binary to implement the `debugDescription` requirement of `CustomDebugStringConvertible`. In the best case, roughly the output above will be produced, in the worst cases, other, potentially useful information will be output instead. 

## Detailed design

### Implementation of `CustomDebugStringConvertible`

Much like the `_project` functions currently implemented in `KeyPath.swift`, this function would loop through the keypath's buffer, handling each segment as follows:

For offset segments, the implementation is simple: use `_getRecursiveChildCount`, `_getChildOffset`, and `_getChildMetadata` to get the string name of the property. I believe these are the same mechanisms used by `Mirror` today.

For optional chain, force-unwrap, etc. the function appends a hard coded "?" or "!", as appropriate.

For computed segments, call `swift::lookupSymbol()` on the result of `getter()` in the `ComputedAccessorsPtr`. Demangle the result to get the property name. 

### Changes to the Swift Runtime

To implement descriptions for computed segments, it is necessary to make two changes to the runtime: 

1. Expose a Swift calling-convention function to call `swift::lookupSymbol()`.
2. Implement and expose a function to demangle keypath functions without the ornamentation the existing demangling functions produce. 

### Dealing with missing data

There are two known cases where data might not be available: 

1. type metadata has not been emitted because the target was built using the `swift-disable-reflection-metadata` flag
2. the linker has stripped the symbol names we're trying to look up

In these cases, we would print the following: 

#### Offset case
`<offset [x] ([typename])>` where `x` is the memory offset we read from the reflection metadata, and `typename` is the type that will be returned. 
So 
```
print(\Theme.backgroundColor) // outputs "\Theme.<offset 0 (Color)>"
```

#### `lookupSymbol` failure case

In this case we'll print the address-in-memory as hex, plus the type name: 
```
print(\Theme.overlay) // outputs \Theme.<computed 0xABCDEFG (Color)>
```

As it might be difficult to correlate a memory address with the name of the function, the type name may be useful here to provide extra context. 

## Source compatibility

Programs that extend `AnyKeyPath` to implement `CustomDebugStringConvertible` themselves will no longer compile and the authors of such code will have to delete the conformance. Based on a search of Github, there are currently no publicly available Swift projects that do this. 

Calling `print` on a KeyPath will, of course, produce different results than before. 

It is unlikely that any existing Swift program is depending on the existing behavior in a production context. While it is likely that someone, somewhere has written code in unit tests that depends on the output of this function, any issues that result will be easy for the authors of such code to identify and fix, and will likely represent an improvement in the readability of those tests. 

## Effect on ABI stability

This proposal will add a new var & protocol conformance to the Standard Library's ABI. It will be availability guarded appropriately.

The new debugging output will not be backdeployed, so Swift programs running on older ABI stable versions of the OS won't be able to rely on the new output. 

## Effect on API resilience

The implementation of `debugDescription` might change after the initial work to implement this proposal is done. In particular, the output format will not be guaranteed to be stable. Here are a few different changes we might anticipate making: 

- As new features are added to the compiler, there may be new metadata available in the binary to draw from. One example would be lookup tables of KeyPath segment to human-readable-name or some other unique, stable identifier
- Whenever a new feature is added to `KeyPath`, it will need to be reflected in the output of this function. For example, the `KeyPath`s produced by [\_forEachFieldWithKeyPath](https://github.com/apple/swift/blob/main/stdlib/public/core/ReflectionMirror.swift#L324) are incomplete, in the sense that they merely set a value at an offset in memory and do not call `didSet` observers. If this function were ever publicly exposed, it would be useful if this was surfaced in the debugging information. 
- The behavior of subscript printing might be changed: for example, we might always print out the value of the argument to the subscript, or we might do so only in cases where the output is short. We might also change from `.subscript()` to `[]`
- The Swift language workgroup may create new policies around debug descriptions and the output of this function might need to be updated to conform to them 

## Alternatives considered

### Print fully qualified names or otherwise add more information to the output

ex. `\ModuleName.MyType.myField`, `<KeyPath<MyType, MyFieldType>> \ModuleName.MyType.myField` `(writable) \Theme.backgroundColor`

As this is just for debugging, it seems likely that the information currently being provided would be enough to resolve any ambiguities. If ambiguities arose during a debugging session, in most cases the user could figure out exactly which keypath they were dealing with simply by running `po myKeyPath == \MyType.myField` till they found the right one. 

### Modify KeyPaths to include a string description

This is an obvious solution to this problem, and would likely be very easy to implement, as the compiler already produces `_kvcString`. 

It has the additional advantage of being 100% reliable, to the point where it arguably could be the basis for implementing `description` rather than `debugDescription`. 

However, it would add to the code size of the compiled code, perhaps unacceptably so. Furthermore, it precludes the possibility of someday printing out the arguments of subscript based keypaths, as
these can be created dynamically. It would also add overhead to appending keypaths, as the string would also have to be appended. 

An alternative implementation of this idea would be the output of additional metadata: a lookup table of function -> name. However, this would require a lot of additional work on the compiler for a relatively small benefit. 

I think that most users who might want this _really_ want to use it to build something else, like encodable keypaths. Those features should be provided opt-in on a per-keypath or per-type basis, which will make it much more useful (and in the context of encodable keypaths specifically, eliminate major potential security issues). Such a feature should also include the option to let the user configure this string, so that it can remain backwards compatible with older versions of the program. 

### Make Keypath functions global to prevent the linker from stripping them

This would also potentially make it feasible to change this proposal from implementing `debugDescription` to implementing `description`. 

This would also potentially bloat the binary and would increase linker times. It could also be a security issue, as `dlysm` would now be able to find these functions.

I am not very knowledgeable about linkers or how typical Swift builds strip symbols, but I think it might be useful to have this as an option in some IDEs that build Swift programs. But that is beyond the scope of this proposal. 

## Future Directions

### Add LLDB formatters/summaries

This would be a good augmentation to this proposal, and might improve the developer experience, as there [might be debug metadata available to the debugger](https://forums.swift.org/t/pitch-add-customdebugdescription-conformance-to-anykeypath/58705/2) that is not available in the binary itself. 

However, I think it might be very difficult to implement this. I see two options: 

1. Implement a public reflection API for KeyPaths in the Swift Standard Library that the formatter can interact with from Python. 
2. The formatter parses the raw memory of the KeyPath, essentially duplicating the code in `debugDescription`. 

I think (1) is overkill, especially considering the limited potential applications of this API beyond its use by the formatter. If it's possible to implement this as an `internal` function in the Swift stdlib then this is a much more attractive option. 
From personal experience trying to parse KeyPath memory from outside the Standard Library, I think (2) would be extremely difficult to implement, and unsustainable to maintain considering that the [memory layout of KeyPaths](https://github.com/apple/swift/blob/main/docs/ABI/KeyPaths.md) is not ABI stable. 

### Make Keypath functions global in DEBUG builds only

This may be necessary to allow `swift::lookupSymbol` to function correctly on Windows, Linux and other platforms that use COFF or ELF-like formats. 

## Acknowledgments

Thanks to Joe Groff for answering several questions on the initial pitch document, and Slava Pestov for answering some questions about the logistics of pitching this. 
