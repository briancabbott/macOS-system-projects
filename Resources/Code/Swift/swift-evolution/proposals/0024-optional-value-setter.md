# Optional Value Setter `??=`

* Proposal: [SE-0024](0024-optional-value-setter.md)
* Author: [James Campbell](https://github.com/jcampbell05)
* Review Manager: [Doug Gregor](https://github.com/DougGregor)
* Status: **Rejected**
* Decision Notes: [Rationale](https://forums.swift.org/t/rejected-se-0024-optional-value-setter/1528)

## Introduction

Introduce a new operator an "Optional Value Setter". If the optional is set via this operator then the new value is 
only set if there isn't an already existing value.

Swift-evolution thread: [link to the discussion thread for that proposal](https://forums.swift.org/t/optional-setting/553)

## Motivation

In certain cases the `??` operation doesn't help with lengthy variable names, i.e., `really.long.lvalue[expression] = really.long.lvalue[expression] ?? ""`. In addtition to this other languages such as Ruby contain a pipe operator `really.long.lvalue[expression] ||= ""` which works the same way and which is very popular. This lowers the barrier of entry for programmers from that language.

In the interest in conciseness and clarity I feel this would be a great addition to swift and would bring the length of that previous statement from

```swift
really.long.lvalue[expression] = really.long.lvalue[expression] ?? ""
```

to

```swift
really.long.lvalue[expression] ??= ""
```

## Proposed solution

In this solution an optonals value wouldn't be set if it already contains a value (i.e .Some), ideally willSet and didSet are only called if this operation occurs.

```swift
var itemsA:[Item]? = nil
var itemsB:[Item]? = [Item()]

itemsA ??= [] // itemsA is set since its value is .None
itemsB ??= [] // itemsB's value isn't changed since its value is .Some
```

## Impact on existing code

Since this is a strictly additive and optional feature, it won't affect existing code and should make code more concise going forward.

## Alternatives considered

Other syntaxes included `?=` but we felt it didn't match the convention of the `??` in `a = a ?? ""`.
