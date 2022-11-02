//===--------------- ModuleDependencyGraphTests.swift --------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2019 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import XCTest
@_spi(Testing) import SwiftDriver
import TSCBasic

class ModuleDependencyGraphTests: XCTestCase, ModuleDependencyGraphMocker {
  static let mockGraphCreator = MockModuleDependencyGraphCreator(maxIndex: 12)

  func testBasicLoad() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.topLevel: ["a->", "b->"]])

    graph.simulateLoad(1, [.nominal: ["c->", "d->"]])
    graph.simulateLoad(2, [.topLevel: ["e", "f"]])
    graph.simulateLoad(3, [.nominal: ["g", "h"]])
    graph.simulateLoad(4, [.dynamicLookup: ["i", "j"]])
    graph.simulateLoad(5, [.dynamicLookup: ["k->", "l->"]])
    graph.simulateLoad(6, [.member: ["m,mm", "n,nn"]])
    graph.simulateLoad(7, [.member: ["o,oo->", "p,pp->"]])
    graph.simulateLoad(8, [.externalDepend: ["/foo->", "/bar->"]])

    graph.simulateLoad(9, [.nominal: ["a", "b", "c->", "d->"],
                           .topLevel: ["b", "c", "d->", "a->"] ])
  }

  func testIndependentNodes() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.topLevel: ["a0", "a->"]])
    graph.simulateLoad(1, [.topLevel: ["b0", "b->"]])
    graph.simulateLoad(2, [.topLevel: ["c0", "c->"]])

    XCTAssertEqual(1, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 2))

    // Mark 0 again -- should be no change.
    XCTAssertEqual(0, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 2))

    XCTAssertEqual(1, graph.collectMockInputsUsing(2).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))

    XCTAssertEqual(1, graph.collectMockInputsUsing(1).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))
  }

  func testIndependentDepKinds() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.nominal: ["a", "a->"]])
    graph.simulateLoad(1, [.topLevel: ["a", "b->"]])

    XCTAssertEqual(1, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testIndependentDepKinds2() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.nominal: ["a->", "b"]])
    graph.simulateLoad(1, [.topLevel: ["b->", "a"]])

    XCTAssertEqual(1, graph.collectMockInputsUsing(1).count)
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testIndependentMembers() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.member: ["a,aa"]])
    graph.simulateLoad(1, [.member: ["a,bb->"]])
    graph.simulateLoad(2, [.potentialMember: ["a"]])
    graph.simulateLoad(3, [.member: ["b,aa->"]])
    graph.simulateLoad(4, [.member: ["b,bb->"]])

    XCTAssertEqual(1, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 2))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 3))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 4))
  }

  func testSimpleDependent() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.topLevel: ["a", "b", "c"]])
    graph.simulateLoad(1, [.topLevel: ["x->", "b->", "z->"]])
    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))

    XCTAssertEqual(0, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testSimpleDependentReverse() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.topLevel: ["a->", "b->", "c->"]])
    graph.simulateLoad(1, [.topLevel: ["x", "b", "z"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(1)
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(0))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))

    XCTAssertEqual(0, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testSimpleDependent2() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.nominal: ["a", "b", "c"]])
    graph.simulateLoad(1, [.nominal: ["x->", "b->", "z->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))

    XCTAssertEqual(0, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testSimpleDependent3() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.nominal: ["a"], .topLevel: ["a"]])
    graph.simulateLoad(1, [.nominal: ["a->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))

    XCTAssertEqual(0, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testSimpleDependent4() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.nominal: ["a"]])
    graph.simulateLoad(1,
                       [.nominal: ["a->"], .topLevel: ["a->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))

    XCTAssertEqual(0, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testSimpleDependent5() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0,
                       [.nominal: ["a"], .topLevel: ["a"]])
    graph.simulateLoad(1,
                       [.nominal: ["a->"], .topLevel: ["a->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))

    let _ = graph.collectMockInputsUsing(0)
    XCTAssertEqual(0, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testSimpleDependent6() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.dynamicLookup: ["a", "b", "c"]])
    graph.simulateLoad(1,
                       [.dynamicLookup: ["x->", "b->", "z->"]])
    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))

    XCTAssertEqual(0, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testSimpleDependentMember() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.member: ["a,aa", "b,bb", "c,cc"]])
    graph.simulateLoad(1,
                       [.member: ["x,xx->", "b,bb->", "z,zz->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))

    XCTAssertEqual(0, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testMultipleDependentsSame() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.nominal: ["a", "b", "c"]])
    graph.simulateLoad(1, [.nominal: ["x->", "b->", "z->"]])
    graph.simulateLoad(2, [.nominal: ["q->", "b->", "s->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(3, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
      XCTAssertTrue(swiftDeps.contains(2))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))

    XCTAssertEqual(0, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))
  }

  func testMultipleDependentsDifferent() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.nominal: ["a", "b", "c"]])
    graph.simulateLoad(1, [.nominal: ["x->", "b->", "z->"]])
    graph.simulateLoad(2, [.nominal: ["q->", "r->", "c->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(3, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
      XCTAssertTrue(swiftDeps.contains(2))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))

    XCTAssertEqual(0, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))
  }

  func testChainedDependents() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.nominal: ["a", "b", "c"]])
    graph.simulateLoad(1, [.nominal: ["x->", "b->", "z"]])
    graph.simulateLoad(2, [.nominal: ["z->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(3, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
      XCTAssertTrue(swiftDeps.contains(2))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))

    XCTAssertEqual(0, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))
  }

  func testChainedNoncascadingDependents() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.nominal: ["a", "b", "c"]])
    graph.simulateLoad(1, [.nominal: ["x->", "b->", "#z"]])
    graph.simulateLoad(2, [.nominal: ["#z->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(3, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
      XCTAssertTrue(swiftDeps.contains(2))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))

    XCTAssertEqual(0, graph.collectMockInputsUsing(0).count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))
  }

  func testChainedNoncascadingDependents2() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.topLevel: ["a", "b", "c"]])
    graph.simulateLoad( 1, [.topLevel: ["x->", "#b->"], .nominal: ["z"]])
    graph.simulateLoad(2, [.nominal: ["z->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 2))
  }

  func testMarkTwoNodes() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.topLevel: ["a", "b"]])
    graph.simulateLoad(1, [.topLevel: ["a->", "z"]])
    graph.simulateLoad(2, [.topLevel: ["z->"]])
    graph.simulateLoad(10, [.topLevel: ["y", "z", "q->"]])
    graph.simulateLoad(11, [.topLevel: ["y->"]])
    graph.simulateLoad(12, [.topLevel: ["q->", "q"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(3, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
      XCTAssertTrue(swiftDeps.contains(2)) //?????
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 10))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 11))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 12))

    do {
      let swiftDeps = graph.collectMockInputsUsing(10)
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(10))
      XCTAssertTrue(swiftDeps.contains(11))
      XCTAssertFalse(swiftDeps.contains(2))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 10))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 11))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 12))
  }

  func testMarkOneNodeTwice() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.nominal: ["a"]])
    graph.simulateLoad(1, [.nominal: ["a->"]])
    graph.simulateLoad(2, [.nominal: ["b->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 2))

    do {
      let swiftDeps = graph.simulateReload(0, [.nominal: ["b"]])
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(2))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))
  }

  func testMarkOneNodeTwice2() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.nominal: ["a"]])
    graph.simulateLoad(1, [.nominal: ["a->"]])
    graph.simulateLoad(2, [.nominal: ["b->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 2))

    do {
      let swiftDeps = graph.simulateReload(0, [.nominal: ["a", "b"]])
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(2))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))
  }

  func testReloadDetectsChange() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.nominal: ["a"]])
    graph.simulateLoad(1, [.nominal: ["a->"]])
    graph.simulateLoad(2, [.nominal: ["b->"]])
    do {
      let swiftDeps = graph.collectMockInputsUsing(1)
      XCTAssertEqual(1, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 2))

    do {
      let swiftDeps =
        graph.simulateReload(1, [.nominal: ["b", "a->"]])
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
      XCTAssertTrue(swiftDeps.contains(2))
    }
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))
  }

  func testNotTransitiveOnceMarked() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.nominal: ["a"]])
    graph.simulateLoad(1, [.nominal: ["a->"]])
    graph.simulateLoad(2, [.nominal: ["b->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(1)
      XCTAssertEqual(1, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 2))

    do {
      let swiftDeps =
        graph.simulateReload(1, [.nominal: ["b", "a->"]])
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
      XCTAssertTrue(swiftDeps.contains(2))
    }
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))
  }

  func testDependencyLoops() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.topLevel: ["a", "b", "c", "a->"]])
    graph.simulateLoad(1,
                       [.topLevel: ["x", "x->", "b->", "z->"]])
    graph.simulateLoad(2, [.topLevel: ["x->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(3, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
      XCTAssertTrue(swiftDeps.contains(2))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(0, swiftDeps.count)
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 2))
  }

  func testMarkIntransitive() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.topLevel: ["a", "b", "c"]])
    graph.simulateLoad(1, [.topLevel: ["x->", "b->", "z->"]])

    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 1))

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testMarkIntransitiveTwice() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.topLevel: ["a", "b", "c"]])
    graph.simulateLoad(1, [.topLevel: ["x->", "b->", "z->"]])

    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testMarkIntransitiveThenIndirect() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.topLevel: ["a", "b", "c"]])
    graph.simulateLoad(1, [.topLevel: ["x->", "b->", "z->"]])

    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 1))

    do {
      let swiftDeps = graph.collectMockInputsUsing(0)
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(0))
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testSimpleExternal() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0,
                       [.externalDepend: ["/foo->", "/bar->"]])

    XCTAssertTrue(graph.containsExternalDependency( "/foo"))
    XCTAssertTrue(graph.containsExternalDependency( "/bar"))

    do {
      let swiftDeps = graph.findUntracedInputsDependent(onExternal: "/foo")
      XCTAssertEqual(swiftDeps.count, 1)
      XCTAssertTrue(swiftDeps.contains(0))
    }

    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))

    XCTAssertEqual(0, graph.findUntracedInputsDependent(onExternal: "/foo").count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
  }

  func testSimpleExternal2() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0,
                       [.externalDepend: ["/foo->", "/bar->"]])

    XCTAssertEqual(1, graph.findUntracedInputsDependent(onExternal: "/bar").count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))

    XCTAssertEqual(0, graph.findUntracedInputsDependent(onExternal: "/bar").count)
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
  }

  func testChainedExternal() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(
      0,
      [.externalDepend: ["/foo->"], .topLevel: ["a"]])
    graph.simulateLoad(
      1,
      [.externalDepend: ["/bar->"], .topLevel: ["a->"]])

    XCTAssertTrue(graph.containsExternalDependency( "/foo"))
    XCTAssertTrue(graph.containsExternalDependency( "/bar"))

    do {
      let swiftDeps = graph.findUntracedInputsDependent(onExternal: "/foo")
      XCTAssertEqual(swiftDeps.count, 2)
      XCTAssertTrue(swiftDeps.contains(0))
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))

    do {
      let swiftDeps = graph.findUntracedInputsDependent(onExternal: "/foo")
      XCTAssertEqual(swiftDeps.count, 0)
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testChainedExternalReverse() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(
      0,
      [.externalDepend: ["/foo->"], .topLevel: ["a"]])
    graph.simulateLoad(
      1,
      [.externalDepend: ["/bar->"], .topLevel: ["a->"]])

    do {
      let swiftDeps = graph.findUntracedInputsDependent(onExternal: "/bar")
      XCTAssertEqual(1, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))

    XCTAssertEqual(0, graph.findUntracedInputsDependent(onExternal: "/bar").count)
    XCTAssertFalse(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))

    do {
      let swiftDeps = graph.findUntracedInputsDependent(onExternal: "/foo")
      XCTAssertEqual(1, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(0))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testChainedExternalPreMarked() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(
      0,
      [.externalDepend: ["/foo->"], .topLevel: ["a"]])
    graph.simulateLoad(
      1,
      [.externalDepend: ["/bar->"], .topLevel: ["a->"]])

    do {
      let swiftDeps = graph.findUntracedInputsDependent(onExternal: "/foo")
      XCTAssertEqual(2, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(0))
      XCTAssertTrue(swiftDeps.contains(1))
    }
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 0))
    XCTAssertTrue(graph.haveAnyNodesBeenTraversed(inMock: 1))
  }

  func testMutualInterfaceHash() {
    let graph = Self.mockGraphCreator.mockUpAGraph()
    graph.simulateLoad(0, [.topLevel: ["a", "b->"]])
    graph.simulateLoad(1, [.topLevel: ["a->", "b"]])

    let swiftDeps = graph.collectMockInputsUsing(0)
    XCTAssertTrue(swiftDeps.contains(1))
  }

  func testEnabledTypeBodyFingerprints() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    graph.simulateLoad(0, [.nominal: ["B2->"]])
    graph.simulateLoad(1, [.nominal: ["B1", "B2"]])
    graph.simulateLoad(2, [.nominal: ["B1->"]])

    do {
      let swiftDeps = graph.collectMockInputsUsing(1)
      XCTAssertEqual(3, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(0))
      XCTAssertTrue(swiftDeps.contains(1))
      XCTAssertTrue(swiftDeps.contains(2))
    }
  }

  func testBaselineForPrintsAndCrossType() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    // Because when A1 changes, B1 and not B2 is affected, only jobs1 and 2
    // should be recompiled, except type fingerprints is off!

    graph.simulateLoad(0, [.nominal: ["A1", "A2"]])
    graph.simulateLoad(1, [.nominal: ["B1", "A1->"]])
    graph.simulateLoad(2, [.nominal: ["C1", "A2->"]])
    graph.simulateLoad(3, [.nominal: ["D1"]])

    do {
      let swiftDeps = graph.simulateReload( 0, [.nominal: ["A1", "A2"]], "changed")
      XCTAssertEqual(3, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(0))
      XCTAssertTrue(swiftDeps.contains(1))
      XCTAssertTrue(swiftDeps.contains(2))
      XCTAssertFalse(swiftDeps.contains(3))
    }
  }

  func testLoadPassesWithFingerprint() {
    let graph = Self.mockGraphCreator.mockUpAGraph()
    _ = graph.getInvalidatedNodesForSimulatedLoad(
      0,
      [MockDependencyKind.nominal: ["A@1"]])
  }

  func testUseFingerprints() {
    let graph = Self.mockGraphCreator.mockUpAGraph()

    // Because when A1 changes, B1 and not B2 is affected, only jobs1 and 2
    // should be recompiled, except type fingerprints is off!
    // Include a dependency on A1, to ensure it does not muck things up.

    graph.simulateLoad(0, [.nominal: ["A1@1", "A2@2", "A1->"]])
    graph.simulateLoad(1, [.nominal: ["B1", "A1->"]])
    graph.simulateLoad(2, [.nominal: ["C1", "A2->"]])
    graph.simulateLoad(3, [.nominal: ["D1"]])

    do {
      let swiftDeps =
        graph.simulateReload(0, [.nominal: ["A1@11", "A2@2"]])
      XCTAssertEqual(3, swiftDeps.count)
      XCTAssertTrue(swiftDeps.contains(0))
      XCTAssertTrue(swiftDeps.contains(1))
      XCTAssertTrue(swiftDeps.contains(2))
      XCTAssertFalse(swiftDeps.contains(3))
    }
  }

  func testUseFingerprintsPingPong() {
    let graph = Self.mockGraphCreator.mockUpAGraph()
    // Because of the cross-type dependency, A->B,
    // when A changes, only B is dirtied in 1.

    graph.simulateLoad(0, [.nominal: ["A@1"]])
    graph.simulateLoad(1, [.nominal: ["B", "A->B"]])
    graph.ensureIsSerializable()

    do {
      let swiftDeps = graph.simulateReload(0, [.nominal: ["A@2"]]).sorted()
      XCTAssertEqual(swiftDeps, [0, 1])
      graph.ensureIsSerializable()
    }

    do {
      // In the real world, we would have a graph with untraced nodes from the
      // priors, remembering the fingerprints from before.
      graph.setUntraced()
      // When the driver integrates a node that preexists but has a new fingerprint
      // it must change that fingerprint in the node in the graph.
      // If it does not, and subsequently integrates the old fingerprint,
      // the change will be missed.
      let swiftDeps = graph.simulateReload(0, [.nominal: ["A@1"]]).sorted()
      XCTAssertEqual(swiftDeps, [0, 1])
      graph.ensureIsSerializable()
    }
  }

  func testUseFingerprintsPingPong2() {
    let graph = Self.mockGraphCreator.mockUpAGraph()
    // Because of the cross-type dependency, A->B,
    // when A changes, only B is dirtied in 1.

    graph.simulateLoad(0, [.nominal: ["A@1", "C@3"]])
    graph.simulateLoad(1, [.nominal: ["B", "A->B"]])
    graph.simulateLoad(2, [.nominal: ["D", "C->D"]])
    graph.ensureIsSerializable()

    do {
      let swiftDeps = graph.simulateReload(0, [.nominal: ["A@2"]]).sorted()
      XCTAssertEqual(swiftDeps, [0, 1, 2])
      graph.ensureIsSerializable()
    }

    do {
      // In the real world, we would have a graph with untraced nodes from the
      // priors, remembering the fingerprints from before.
      graph.setUntraced()
      // When the driver integrates a node that preexists but has a new fingerprint
      // it must change that fingerprint in the node in the graph.
      // If it does not, and subsequently integrates the old fingerprint,
      // the change will be missed.
      let swiftDeps = graph.simulateReload(0, [.nominal: ["A@1"]]).sorted()
      XCTAssertEqual(swiftDeps, [0, 1])
      graph.ensureIsSerializable()
    }
  }

  func testCrossTypeDependencyBaseline() {
    let graph = Self.mockGraphCreator.mockUpAGraph()
    graph.simulateLoad(0, [.nominal: ["A"]])
    graph.simulateLoad(1, [.nominal: ["B", "C", "A->"]])
    graph.simulateLoad(2, [.nominal: ["B->"]])
    graph.simulateLoad(3, [.nominal: ["C->"]])

    let swiftDeps = graph.collectMockInputsUsing(0)
    XCTAssertTrue(swiftDeps.contains(0))
    XCTAssertTrue(swiftDeps.contains(1))
    XCTAssertTrue(swiftDeps.contains(2))
    XCTAssertTrue(swiftDeps.contains(3))
  }

  func testCrossTypeDependency() {
    let graph = Self.mockGraphCreator.mockUpAGraph()
    // Because of the cross-type dependency, A->B,
    // when A changes, only B is dirtied in 1.

    graph.simulateLoad(0, [.nominal: ["A"]])
    graph.simulateLoad(1, [.nominal: ["B", "C", "A->B"]])
    graph.simulateLoad(2, [.nominal: ["B->"]])
    graph.simulateLoad(3, [.nominal: ["C->"]])

    let swiftDeps = graph.collectMockInputsUsing(0)
    XCTAssertTrue(swiftDeps.contains(0))
    XCTAssertTrue(swiftDeps.contains(1))
    XCTAssertTrue(swiftDeps.contains(2))
    XCTAssertFalse(swiftDeps.contains(3))
  }

  func testCrossTypeDependencyBaselineWithFingerprints() {
    let graph = Self.mockGraphCreator.mockUpAGraph()
    graph.simulateLoad(0, [.nominal: ["A1@1", "A2@2"]])
    graph.simulateLoad(1, [.nominal: ["B1", "C1", "A1->"]])
    graph.simulateLoad(2, [.nominal: ["B1->"]])
    graph.simulateLoad(3, [.nominal: ["C1->"]])
    graph.simulateLoad(4, [.nominal: ["B2", "C2", "A2->"]])
    graph.simulateLoad(5, [.nominal: ["B2->"]])
    graph.simulateLoad(6, [.nominal: ["C2->"]])

    let swiftDeps =
      graph.simulateReload(0, [.nominal: ["A1@11", "A2@2"]])
    XCTAssertTrue(swiftDeps.contains(0))
    XCTAssertTrue(swiftDeps.contains(1))
    XCTAssertTrue(swiftDeps.contains(2))
    XCTAssertTrue(swiftDeps.contains(3))
    XCTAssertFalse(swiftDeps.contains(4))
    XCTAssertFalse(swiftDeps.contains(5))
    XCTAssertFalse(swiftDeps.contains(6))
  }

  func testCrossTypeDependencyWithFingerprints() {
    let graph = Self.mockGraphCreator.mockUpAGraph()
    // Because of the cross-type dependency, A->B,
    // when A changes, only B is dirtied in 1.

    graph.simulateLoad(0, [.nominal: ["A1@1", "A2@2"]])
    graph.simulateLoad(1, [.nominal: ["B1", "C1", "A1->B1"]])
    graph.simulateLoad(2, [.nominal: ["B1->"]])
    graph.simulateLoad(3, [.nominal: ["C1->"]])
    graph.simulateLoad(4, [.nominal: ["B2", "C2", "A2->B2"]])
    graph.simulateLoad(5, [.nominal: ["B2->"]])
    graph.simulateLoad(6, [.nominal: ["C2->"]])

    let swiftDeps =
      graph.simulateReload(0, [.nominal: ["A1@11", "A2@2"]])
    XCTAssertTrue(swiftDeps.contains(0))
    XCTAssertTrue(swiftDeps.contains(1))
    XCTAssertTrue(swiftDeps.contains(2))
    XCTAssertFalse(swiftDeps.contains(3))
    XCTAssertFalse(swiftDeps.contains(4))
    XCTAssertFalse(swiftDeps.contains(5))
    XCTAssertFalse(swiftDeps.contains(6))
  }
}

enum MockDependencyKind {
  case topLevel
  case dynamicLookup
  case externalDepend
  case sourceFileProvide
  case nominal
  case potentialMember
  case member

  var singleNameIsContext: Bool? {
    switch self {
    case .nominal, .potentialMember: return true
    case .topLevel, .dynamicLookup, .externalDepend, .sourceFileProvide: return false
    case .member: return nil
    }
  }
}

extension ModuleDependencyGraph {
  func simulateLoad(
    _ swiftDepsIndex: Int,
    _ dependencyDescriptions: [MockDependencyKind: [String]],
    _ interfaceHash: String? = nil,
    includePrivateDeps: Bool = true,
    hadCompilationError: Bool = false)
  {
    _ = getInvalidatedNodesForSimulatedLoad(
      swiftDepsIndex, dependencyDescriptions,
      interfaceHash,
      includePrivateDeps: includePrivateDeps,
      hadCompilationError: hadCompilationError)
  }

  func simulateReload(_ swiftDepsIndex: Int,
                      _ dependencyDescriptions: [MockDependencyKind: [String]],
                      _ interfaceHash: String? = nil,
                      includePrivateDeps: Bool = true,
                      hadCompilationError: Bool = false)
  -> [Int]
  {
    blockingConcurrentAccessOrMutation {
        setPhase(to: .updatingAfterCompilation)
    }
    let directlyInvalidatedNodes = getInvalidatedNodesForSimulatedLoad(
      swiftDepsIndex,
      dependencyDescriptions,
      interfaceHash,
      includePrivateDeps: includePrivateDeps,
      hadCompilationError: hadCompilationError)
    
    return collectInputsUsingInvalidated(nodes: directlyInvalidatedNodes)
      .map { $0.mockID }
  }

  
  func getInvalidatedNodesForSimulatedLoad(
    _ swiftDepsIndex: Int,
    _ dependencyDescriptions: [MockDependencyKind: [String]],
    _ interfaceHashIfPresent: String? = nil,
    includePrivateDeps: Bool = true,
    hadCompilationError: Bool = false
  ) -> DirectlyInvalidatedNodeSet {
    blockingConcurrentAccessOrMutation {
      let input = SwiftSourceFile(mock: swiftDepsIndex)
      let dependencySource = DependencySource(input, internedStringTable)
      let interfaceHash =
      interfaceHashIfPresent ?? dependencySource.interfaceHashForMockDependencySource
      
      let sfdg = SourceFileDependencyGraphMocker.mock(
        includePrivateDeps: includePrivateDeps,
        hadCompilationError: hadCompilationError,
        dependencySource: dependencySource,
        interfaceHash: interfaceHash,
        dependencyDescriptions,
        in: internedStringTable)
      
      return Integrator.integrate(from: sfdg,
                                  dependencySource: DependencySource(input, internedStringTable),
                                  into: self)
    }
  }

  func findUntracedInputsDependent(onExternal s: String) -> [Int] {
    blockingConcurrentAccessOrMutation {
      findUntracedInputsDependent(
        on: FingerprintedExternalDependency(.mocking(s, in: internedStringTable), nil))
        .map { $0.mockID }
    }
  }

  /// Can return duplicates
  func findUntracedInputsDependent(
    on fingerprintedExternalDependency: FingerprintedExternalDependency
  ) -> [SwiftSourceFile] {
    var foundSources = [SwiftSourceFile]()
    for dependent in collectUntracedNodes(thatUse: fingerprintedExternalDependency, .testing) {
      guard case let .known(dependencySource) = dependent.definitionLocation
      else {
        XCTFail("definition location should be known")
        break
      }
      foundSources.append(SwiftSourceFile(dependencySource.typedFile))
      // findSwiftDepsToRecompileWhenWholeSwiftDepChanges is reflexive
      // Don't return job twice.
      let filesToRebuild =
        collectInputsUsing(dependencySource: dependencySource)
        .filter({ marked in marked != SwiftSourceFile(dependencySource.typedFile) })
      foundSources.append(contentsOf: filesToRebuild)
    }
    return foundSources
  }

  fileprivate func collectMockInputsUsing(_ i: Int) -> TransitivelyInvalidatedMockInputArray {
    blockingConcurrentAccessOrMutation {
      collectInputsUsing(dependencySource: DependencySource(SwiftSourceFile(mock: i), internedStringTable))
        .map { $0.mockID }
    }
  }

  fileprivate typealias TransitivelyInvalidatedMockInputArray = InvalidatedArray<Transitively, Int>

  func containsExternalDependency(_ path: String, fingerprint: String? = nil)
  -> Bool {
    blockingConcurrentAccessOrMutation {
      let internedPath = path.intern(in: self)
      return fingerprintedExternalDependencies.contains(
        FingerprintedExternalDependency(ExternalDependency(fileName: internedPath, internedStringTable),
                                        fingerprint.map {$0.intern(in: internedStringTable)}))
    }
  }
}

/// *Dependency info format:*
/// A list of entries, each of which is keyed by a \c DependencyKey.Kind and contains a
/// list of dependency nodes.
///
/// *Dependency node format:*
/// Each node here is either a "provides" (i.e. a declaration provided by the
/// file) or a "depends" (i.e. a declaration that is depended upon).
///
/// For "provides" (i.e. declarations provided by the source file):
/// <provides> = [#]<contextAndName>[@<fingerprint>],
/// where the '#' prefix indicates that the declaration is file-private.
///
/// <contextAndName> = <name> |  <context>,<name>
/// where <context> is a mangled type name, and <name> is a base-name.
///
/// For "depends" (i.e. uses of declarations in the source file):
/// [#][~]<contextAndName>->[<provides>]
/// where the '#' prefix indicates that the use does not cascade,
/// the '~' prefix indicates that the holder is private,
/// <contextAndName> is the depended-upon declaration and the optional
/// <provides> is the dependent declaration if known. If not known, the
/// use will be the entire file.

fileprivate struct SourceFileDependencyGraphMocker: InternedStringTableHolder {
  private typealias Node = SourceFileDependencyGraph.Node
  private struct NodePair {
    let interface, implementation: Node
  }

  private let includePrivateDeps: Bool
  private let hadCompilationError: Bool
  private let dependencySource: DependencySource
  private let interfaceHash: String
  private let dependencyDescriptions: [(MockDependencyKind, String)]
  fileprivate let internedStringTable: InternedStringTable

  private var allNodes: [Node] = []
  private var dependencyAccumulator = [DependencyHolder?]()
  private var memoizedNodes: [DependencyKey: Node] = [:]
  private var sourceFileNodePair: NodePair? = nil

  static func mock(
    includePrivateDeps: Bool,
    hadCompilationError: Bool,
    dependencySource: DependencySource,
    interfaceHash: String,
    _ dependencyDescriptions: [MockDependencyKind: [String]],
    in internedStringTable: InternedStringTable
  ) -> SourceFileDependencyGraph
  {
    var m = Self.init(
      includePrivateDeps: includePrivateDeps,
      hadCompilationError: hadCompilationError,
      dependencySource: dependencySource,
      interfaceHash: interfaceHash,
      dependencyDescriptions:
        dependencyDescriptions.flatMap { (kind, descs) in descs.map {(kind, $0)}},
      internedStringTable: internedStringTable
    )
    return m.mock()
  }

  private mutating func mock() -> SourceFileDependencyGraph {
    buildNodes()
    return SourceFileDependencyGraph(nodesForTesting: allNodes,
                                     internedStringTable: internedStringTable)
  }

  private mutating func buildNodes() {
    addSourceFileNodesToGraph();
    if (!hadCompilationError) {
      addAllDefinedDecls()
      addAllUsedDecls()
      fixupDependencies()
    }
  }

  private mutating func addSourceFileNodesToGraph() {
    let key = DependencyKey.createKeyForWholeSourceFile(
      .interface, dependencySource, in: internedStringTable)
    sourceFileNodePair = findExistingNodePairOrCreateAndAddIfNew(
      key,
      interfaceHash.intern(in: internedStringTable));
  }

  private mutating func findExistingNodePairOrCreateAndAddIfNew(
    _ interfaceKey: DependencyKey,
    _ fingerprint: InternedString?)
  -> NodePair {
    // Optimization for whole-file users:
    if case .sourceFileProvide = interfaceKey.designator, !allNodes.isEmpty {
      return getSourceFileNodePair()
    }
    let implementationKey = try! XCTUnwrap(interfaceKey.correspondingImplementation)
    let nodePair = NodePair(
      interface: findExistingNodeOrCreateIfNew(interfaceKey, fingerprint,
                                               definitionVsUse: .definition),
      implementation: findExistingNodeOrCreateIfNew(implementationKey, fingerprint,
                                                    definitionVsUse: .definition))

    // if interface changes, have to rebuild implementation.
    // This dependency used to be represented by
    // addArc(nodePair.getInterface(), nodePair.getImplementation());
    // However, recall that the dependency scheme as of 1/2020 chunks
    // declarations together by base name.
    // So if the arc were added, a dirtying of a same-based-named interface
    // in a different file would dirty the implementation in this file,
    // causing the needless recompilation of this file.
    // But, if an arc is added for this, then *any* change that causes
    // a same-named interface to be dirty will dirty this implementation,
    // even if that interface is in another file.
    // Therefore no such arc is added here, and any dirtying of either
    // the interface or implementation of this declaration will cause
    // the driver to recompile this source file.
    return nodePair
  }

  private mutating func getSourceFileNodePair() -> NodePair {
    NodePair(
      interface: getNode(sourceFileProvidesInterfaceSequenceNumber),
      implementation: getNode(sourceFileProvidesImplementationSequenceNumber));
  }
  let sourceFileProvidesInterfaceSequenceNumber = 0
  let sourceFileProvidesImplementationSequenceNumber = 1

  private func getNode(_ i: Int) -> Node {
    assert(allNodes[i].sequenceNumber == i)
    return allNodes[i]
  }

  private mutating func findExistingNodeOrCreateIfNew(_ key: DependencyKey,
                                                      _ fingerprint: InternedString?,
                                                      definitionVsUse: DefinitionVsUse) -> Node {
    func createNew() -> Node {
      let n = try! Node(key: key,
                        fingerprint: fingerprint,
                        sequenceNumber: allNodes.count,
                        defsIDependUpon: [],
                        definitionVsUse: definitionVsUse)
      allNodes.append(n)
      memoizedNodes[key] = n
      return n
    }
    let result = memoizedNodes[key] ?? createNew()

    assert(key == result.key)
    if definitionVsUse == .use {
      return result
    }
    // If have provides and depends with same key, result is one node that
    // is a definition
    if let fingerprint = fingerprint, result.definitionVsUse == .use {
      assert(result.fingerprint == nil, "A use should not have a fingerprint");
      let newNode =
        try! Node(key: result.key, fingerprint: fingerprint,
                 sequenceNumber: result.sequenceNumber,
                 defsIDependUpon: result.defsIDependUpon,
                 definitionVsUse: .definition)
      memoizedNodes[key] = newNode
      return newNode
    }
    // If there are two Decls with same base name but differ only in fingerprint,
    // since we won't be able to tell which Decl is depended-upon (is this right?)
    // just use the one node, but erase its print:
    if fingerprint != result.fingerprint {
      let newNode =
        try! Node(key: result.key, fingerprint: nil,
                 sequenceNumber: result.sequenceNumber,
                 defsIDependUpon: result.defsIDependUpon,
                 definitionVsUse: .definition)
      memoizedNodes[key] = newNode
      return newNode
    }
    return result
  }

  private mutating func addAllDefinedDecls() {
    dependencyDescriptions.forEach { kind, s in
      if s.isADefinedDecl { addADefinedDecl(kind, s) }
    }
  }
  private mutating func addAllUsedDecls() {
    dependencyDescriptions.forEach { kind, s in
      if !s.isADefinedDecl { addAUsedDecl(kind, s) }
    }
  }

  private mutating func addADefinedDecl(_ kind: MockDependencyKind, _ s: String) {
    guard let interfaceKey = DependencyKey.parseADefinedDecl(s, kind, .interface, includePrivateDeps: includePrivateDeps, in: internedStringTable)
    else {
      return
    }
    let fingerprint = s.range(of: String.fingerprintSeparator)
      .map { String(s.suffix(from: $0.upperBound)).intern(in: internedStringTable) }

    let nodePair =
      findExistingNodePairOrCreateAndAddIfNew(interfaceKey, fingerprint);
    // Since the current type fingerprints only include tokens in the body,
    // when the interface hash changes, it is possible that the type in the
    // file has changed.
    addArc(def: sourceFileNodePair!.interface, use: nodePair.interface)
  }

  private mutating func addAUsedDecl(_ kind: MockDependencyKind, _ s: String) {
    guard let defAndUseKeys = DependencyKey.parseAUsedDecl(
            s,
            kind,
            in: internedStringTable,
            includePrivateDeps: includePrivateDeps,
            dependencySource: dependencySource)
    else { return }
    let defNode = findExistingNodeOrCreateIfNew(defAndUseKeys.def, nil, definitionVsUse: .use)

    // If the depended-upon node is defined in this file, then don't
    // create an arc to the user, when the user is the whole file.
    // Otherwise, if the defNode's type-body fingerprint changes,
    // the whole file will be marked as dirty, losing the benefit of the
    // fingerprint.

    //  if (defNode->getIsProvides() &&
    //      useKey.getKind() == DependencyKey.Kind::sourceFileProvide)
    //    return;

    // Turns out the above three lines cause miscompiles, so comment them out
    // for now. We might want them back if we can change the inputs to this
    // function to be more precise.

    // Example of a miscompile:
    // In main.swift
    // func foo(_: Any) { print("Hello Any") }
    //    foo(123)
    // Then add the following line to another file:
    // func foo(_: Int) { print("Hello Int") }
    // Although main.swift needs to get recompiled, the commented-out code below
    // prevents that.
    guard let useNode = memoizedNodes[defAndUseKeys.use]
    else {
      fatalError("Use must be an already-added provides")
    }
    assert(useNode.definitionVsUse == .definition, "Use (using node) must be a definition");
    addArc(def: defNode, use: useNode)
  }

  private mutating func addArc(def: Node, use: Node) {
    dependencyAccumulator.reserveCapacity(use.sequenceNumber)
    while dependencyAccumulator.count <= use.sequenceNumber {
      dependencyAccumulator.append(nil)
    }
    let dh = dependencyAccumulator[use.sequenceNumber] ?? {
      let newOne = DependencyHolder()
      dependencyAccumulator[use.sequenceNumber] = newOne
      return newOne
    }()
    dh.add(def.sequenceNumber)
  }
  
  private mutating func fixupDependencies() {
    for (useSequenceNumber, depHolder) in dependencyAccumulator.enumerated() {
      if let depHolder = depHolder {
        let oldNode = allNodes[useSequenceNumber]
        assert(oldNode.sequenceNumber == useSequenceNumber)
        allNodes[useSequenceNumber] = try! Node(
          key: oldNode.key,
          fingerprint: oldNode.fingerprint,
          sequenceNumber: useSequenceNumber,
          defsIDependUpon: depHolder.dependedUpon,
          definitionVsUse: oldNode.definitionVsUse)
      }
    }
  }
}


fileprivate extension DependencyKey {
  static func parseADefinedDecl(_ s: String, _ kind: MockDependencyKind, _ aspect: DeclAspect, includePrivateDeps: Bool, in t: InternedStringTable) -> Self? {
    let privatePrefix = "#"
    let isPrivate = s.hasPrefix(privatePrefix)
    guard !isPrivate || includePrivateDeps else {return nil}
    let ss = s.drop {String($0) == privatePrefix}
    let sss = ss.range(of: String.fingerprintSeparator).map { ss.prefix(upTo: $0.lowerBound) } ?? ss
    return try! Self(aspect: aspect,
                     designator: Designator(kind: kind,
                                            String(sss).parseContextAndName(kind),
                                            in: t))
  }

  static func parseAUsedDecl(
    _ s: String,
    _ kind: MockDependencyKind,
    in t: InternedStringTable,
    includePrivateDeps: Bool,
    dependencySource: DependencySource
  ) -> (def: Self, use: Self)? {
    let noncascadingPrefix = "#"
    let privateHolderPrefix = "~"

    let isCascadingUse = !s.hasPrefix(noncascadingPrefix)
    let withoutNCPrefix = s.drop {String($0) == noncascadingPrefix}
    // Someday, we might differentiate.
    let aspectOfDefUsed = DeclAspect.interface

    let isHolderPrivate = withoutNCPrefix.hasPrefix(privateHolderPrefix)
    if !includePrivateDeps && isHolderPrivate {
      return nil
    }
    let withoutPrivatePrefix = withoutNCPrefix.drop {String($0) == privateHolderPrefix}
    let defUseStrings = withoutPrivatePrefix.splitDefUse
    let defKey = try! Self(
      aspect: aspectOfDefUsed,
      designator: Designator(kind: kind,
                             defUseStrings.def.parseContextAndName(kind),
                             in: t))
    return (def: defKey,
            use: computeUseKey(defUseStrings.use,
                               in: t,
                               isCascadingUse: isCascadingUse,
                               includePrivateDeps: includePrivateDeps,
                               dependencySource: dependencySource))
  }

  static func computeUseKey(
    _ s: String,
    in t: InternedStringTable,
    isCascadingUse: Bool,
    includePrivateDeps: Bool,
    dependencySource: DependencySource
  ) -> Self {
    // For now, in unit tests, mock uses are always nominal
    let aspectOfUse: DeclAspect = isCascadingUse ? .interface : .implementation
    if !s.isEmpty {
      let kindOfUse = MockDependencyKind.nominal
      return parseADefinedDecl(s,
                               kindOfUse,
                               aspectOfUse,
                               includePrivateDeps: includePrivateDeps,
                               in: t)!
    }
    return Self(
      aspect: aspectOfUse,
      designator: try! Designator(
        kind: .sourceFileProvide,
        (context: "", name: dependencySource.sourceFileProvideNameForMockDependencySource),
        in: t))
  }
}

fileprivate extension String {
  static var fingerprintSeparator: Self {"@"}

  var isADefinedDecl: Bool {
    range(of: Self.defUseSeparator) == nil
  }
  static var defUseSeparator: String { "->" }

  static var nameContextSeparator: String { "," }

  func parseContextAndName( _ kind: MockDependencyKind) -> (context: String?, name: String?) {
    switch kind.singleNameIsContext {
    case true?:  return (context: self, name: nil)
    case false?: return (context: nil,  name: self)
    case nil:
      let r = range(of: Self.nameContextSeparator) ?? (endIndex ..< endIndex)
      return (
        context: String(prefix(upTo: r.lowerBound)),
        name:    String(suffix(from: r.upperBound))
      )
    }
  }
}

fileprivate extension ExternalDependency {
  static func mocking(_ name: String, in t: InternedStringTable) -> Self {
    return Self(fileName: name.intern(in: t), t)
  }
}


fileprivate extension Substring {
  var splitDefUse: (def: String, use: String) {
    let r = range(of: String.defUseSeparator)!
    return (String(prefix(upTo: r.lowerBound)), String(suffix(from: r.upperBound)))
  }
}

fileprivate extension DependencyKey {
  static func createKeyForWholeSourceFile(
    _ aspect: DeclAspect,
    _ dependencySource: DependencySource,
    in internedStringTable: InternedStringTable
  ) -> Self {
    let designator = try! Designator(
      kind: .sourceFileProvide,
      dependencySource.sourceFileProvideNameForMockDependencySource
                                            .parseContextAndName(.sourceFileProvide),
      in: internedStringTable)
    return Self(aspect: aspect, designator: designator)
  }
}

extension Job {
  init(_ dummyBaseName: String) {
    let input = try! TypedVirtualPath(file: VirtualPath.intern(path: dummyBaseName + ".swift"),
                                      type: .swift)
    try! self.init(moduleName: "nothing",
                   kind: .compile,
                   tool: ResolvedTool(path: AbsolutePath("/dummy"), supportsResponseFiles: false),
                   commandLine: [],
                   inputs:  [input],
                   primaryInputs: [input],
                   outputs: [TypedVirtualPath(file: VirtualPath.intern(path: dummyBaseName + ".swiftdeps"), type: .swiftDeps)])
  }

}

fileprivate extension DependencyKey.Designator {
  init(kind: MockDependencyKind,
       _ contextAndName: (context: String?, name: String?),
       in t: InternedStringTable)
  throws
  {
    func mustBeAbsent(_ s: InternedString?) {
      if let s = s, !s.isEmpty {
        XCTFail()
      }
    }
    let context = contextAndName.context?.intern(in: t)
    let    name = contextAndName.name?   .intern(in: t)
    
    switch kind {
    case .topLevel:
      mustBeAbsent(context)
      self = .topLevel(name: name!)
    case .nominal:
      mustBeAbsent(name)
      self = .nominal(context: context!)
    case .potentialMember:
      mustBeAbsent(name)
      self = .potentialMember(context: context!)
    case .member:
      self = .member(context: context!, name: name!)
    case .dynamicLookup:
      mustBeAbsent(context)
      self = .dynamicLookup(name: name!)
    case .externalDepend:
      mustBeAbsent(context)
      self = .externalDepend(ExternalDependency(fileName: name!, t))
    case .sourceFileProvide:
      mustBeAbsent(context)
      self = .sourceFileProvide(name: name!)
    }
  }
}

fileprivate extension Set where Element == ExternalDependency {
  func contains(_ s: String) -> Bool {
    contains {$0.path?.name == s}
  }
}

fileprivate class DependencyHolder {
  private(set) var dependedUpon = [Int]()
  func add(_ dep: Int) {
    dependedUpon.append(dep)
  }
}
