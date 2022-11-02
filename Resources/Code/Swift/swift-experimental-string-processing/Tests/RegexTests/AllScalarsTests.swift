//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2021-2022 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
//
//===----------------------------------------------------------------------===//

import XCTest
import _StringProcessing

let allScalars = Unicode.Scalar.allScalars

extension Unicode.Scalar {
  var isUnicode12OrEarlier: Bool {
    properties.age.map { age in age <= (12, 0) } ?? false
  }
}

class AllScalarsTests: XCTestCase {
  func testCollectionConformance() {
    let calculatedCount = (0...0xD7FF).count + (0xE000...0x10FFFF).count
    XCTAssertEqual(calculatedCount, allScalars.count)
    XCTAssertEqual(calculatedCount, allScalars.reduce(0, { sum, _ in sum + 1 }))
  }
  
  func testIndexOf() throws {
    for scalar in "béड🥰 \u{0} \u{D7FF} \u{E000} \u{10FFFF}".unicodeScalars {
      let i = try XCTUnwrap(allScalars.firstIndex(of: scalar))
      XCTAssertEqual(scalar, allScalars[i])
    }
  }
  
  func testProperties() throws {
    let whitespaces = allScalars.filter { $0.properties.isWhitespace }
    XCTAssertEqual(25, whitespaces.count)
    
    let numericIndices = allScalars
      .indices
      .filter {
        allScalars[$0].isUnicode12OrEarlier
          && allScalars[$0].properties.numericType == .decimal
      }
    XCTAssertEqual(630, numericIndices.count)
    
    let digitSum = try numericIndices
      .map { try XCTUnwrap(allScalars[$0].properties.numericValue) }
      .reduce(0, +)
    XCTAssertEqual(2835, digitSum)
    XCTAssertEqual(4.5, digitSum / Double(numericIndices.count))
  }
}
