// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// RUN: %target-run-simple-swift
// REQUIRES: executable_test
// REQUIRES: objc_interop

import Foundation
import XCTest

#if os(macOS) || os(iOS) || os(watchOS) || os(tvOS)
    import CoreGraphics
#endif

#if swift(>=3.2)
// import nothing
#elseif swift(>=3.0)
import Codable
#endif

class TestCodableSuper : XCTestCase { }

extension NSRange : Equatable {

    public static func == (lhs: NSRange, rhs: NSRange) -> Bool {
        return NSEqualRanges(lhs, rhs)
    }
}

// MARK: - Helper Functions
@available(OSX 10.11, iOS 9.0, *)
func makePersonNameComponents(namePrefix: String? = nil,
                              givenName: String? = nil,
                              middleName: String? = nil,
                              familyName: String? = nil,
                              nameSuffix: String? = nil,
                              nickname: String? = nil) -> PersonNameComponents {
    var result = PersonNameComponents()
    result.namePrefix = namePrefix
    result.givenName = givenName
    result.middleName = middleName
    result.familyName = familyName
    result.nameSuffix = nameSuffix
    result.nickname = nickname
    return result
}

func expectRoundTripEquality<T : Codable>(of value: T, encode: (T) throws -> Data, decode: (Data) throws -> T) where T : Equatable {
    let data: Data
    do {
        data = try encode(value)
    } catch {
        XCTFail("Unable to encode \(T.self) <\(value)>: \(error)")
        return
    }

    let decoded: T
    do {
        decoded = try decode(data)
    } catch {
        XCTFail("Unable to decode \(T.self) <\(value)>: \(error)")
        return
    }

    XCTAssertEqual(value, decoded, "Decoded \(T.self) <\(decoded)> not equal to original <\(value)>")
}

func expectRoundTripEqualityThroughJSON<T : Codable>(for value: T) where T : Equatable {
    let inf = "INF", negInf = "-INF", nan = "NaN"
    let encode = { (_ value: T) throws -> Data in
        let encoder = JSONEncoder()
        encoder.nonConformingFloatEncodingStrategy = .convertToString(positiveInfinity: inf,
                                                                      negativeInfinity: negInf,
                                                                      nan: nan)
        return try encoder.encode(value)
    }

    let decode = { (_ data: Data) throws -> T in
        let decoder = JSONDecoder()
        decoder.nonConformingFloatDecodingStrategy = .convertFromString(positiveInfinity: inf,
                                                                        negativeInfinity: negInf,
                                                                        nan: nan)
        return try decoder.decode(T.self, from: data)
    }

    expectRoundTripEquality(of: value, encode: encode, decode: decode)
}

// MARK: - Helper Types
// A wrapper around a UUID that will allow it to be encoded at the top level of an encoder.
struct UUIDCodingWrapper : Codable, Equatable {
    let value: UUID

    private enum CodingKeys: String, CodingKey {
        case value
    }

    init(_ value: UUID) {
        self.value = value
    }

    static func ==(_ lhs: UUIDCodingWrapper, _ rhs: UUIDCodingWrapper) -> Bool {
        return lhs.value == rhs.value
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        value = try container.decode(UUID.self, forKey: .value)
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(value, forKey: .value)
    }
}

// MARK: - Tests

class TestCodable : TestCodableSuper {
    
    static var allTests = [
        //("test_Calendar_JSON", test_Calendar_JSON),
        //("test_CharacterSet_JSON", test_CharacterSet_JSON),
        //("test_DateComponents_JSON", test_DateComponents_JSON),
        //("test_Decimal_JSON", test_Decimal_JSON),
        //("test_TimeZone_JSON", test_TimeZone_JSON),
        ("test_IndexPath_JSON", test_IndexPath_JSON),
        ("test_IndexSet_JSON", test_IndexSet_JSON),
        ("test_Locale_JSON", test_Locale_JSON),
        ("test_NSRange_JSON", test_NSRange_JSON),
        ("test_URL_JSON", test_URL_JSON),
        ("test_UUID_JSON", test_UUID_JSON)
        ]
    
    // MARK: - AffineTransform
    #if os(OSX)
    // FIXME: Comment the tests back in once rdar://problem/33363218 is in the SDK.
    lazy var affineTransformValues: [AffineTransform] = [
        AffineTransform.identity,
        AffineTransform(),
        AffineTransform(translationByX: 2.0, byY: 2.0),
        AffineTransform(scale: 2.0),
        AffineTransform(rotationByDegrees: .pi / 2),

        AffineTransform(m11: 1.0, m12: 2.5, m21: 66.2, m22: 40.2, tX: -5.5, tY: 3.7),
        // AffineTransform(m11: -55.66, m12: 22.7, m21: 1.5, m22: 0.0, tX: -22, tY: -33),
        AffineTransform(m11: 4.5, m12: 1.1, m21: 0.025, m22: 0.077, tX: -0.55, tY: 33.2),
        // AffineTransform(m11: 7.0, m12: -2.3, m21: 6.7, m22: 0.25, tX: 0.556, tY: 0.99),
        // AffineTransform(m11: 0.498, m12: -0.284, m21: -0.742, m22: 0.3248, tX: 12, tY: 44)
    ]

    func test_AffineTransform_JSON() {
        for transform in affineTransformValues {
            expectRoundTripEqualityThroughJSON(for: transform)
        }
    }
    #endif

    // MARK: - Calendar
    lazy var calendarValues: [Calendar] = [
        Calendar(identifier: .gregorian),
        Calendar(identifier: .buddhist),
        Calendar(identifier: .chinese),
        Calendar(identifier: .coptic),
        Calendar(identifier: .ethiopicAmeteMihret),
        Calendar(identifier: .ethiopicAmeteAlem),
        Calendar(identifier: .hebrew),
        Calendar(identifier: .iso8601),
        Calendar(identifier: .indian),
        Calendar(identifier: .islamic),
        Calendar(identifier: .islamicCivil),
        Calendar(identifier: .japanese),
        Calendar(identifier: .persian),
        Calendar(identifier: .republicOfChina),
        ]

    func test_Calendar_JSON() {
        for calendar in calendarValues {
            expectRoundTripEqualityThroughJSON(for: calendar)
        }
    }

    // MARK: - CharacterSet
    lazy var characterSetValues: [CharacterSet] = [
        CharacterSet.controlCharacters,
        CharacterSet.whitespaces,
        CharacterSet.whitespacesAndNewlines,
        CharacterSet.decimalDigits,
        CharacterSet.letters,
        CharacterSet.lowercaseLetters,
        CharacterSet.uppercaseLetters,
        CharacterSet.nonBaseCharacters,
        CharacterSet.alphanumerics,
        CharacterSet.decomposables,
        CharacterSet.illegalCharacters,
        CharacterSet.punctuationCharacters,
        CharacterSet.capitalizedLetters,
        CharacterSet.symbols,
        CharacterSet.newlines
    ]

    func test_CharacterSet_JSON() {
        for characterSet in characterSetValues {
            expectRoundTripEqualityThroughJSON(for: characterSet)
        }
    }
    
    #if os(macOS) || os(iOS) || os(watchOS) || os(tvOS)

    // MARK: - CGAffineTransform
    lazy var cg_affineTransformValues: [CGAffineTransform] = {
        // FIXME: Comment the tests back in once rdar://problem/33363218 is in the SDK.
        var values = [
            CGAffineTransform.identity,
            CGAffineTransform(),
            CGAffineTransform(translationX: 2.0, y: 2.0),
            CGAffineTransform(scaleX: 2.0, y: 2.0),
            CGAffineTransform(a: 1.0, b: 2.5, c: 66.2, d: 40.2, tx: -5.5, ty: 3.7),
            // CGAffineTransform(a: -55.66, b: 22.7, c: 1.5, d: 0.0, tx: -22, ty: -33),
            CGAffineTransform(a: 4.5, b: 1.1, c: 0.025, d: 0.077, tx: -0.55, ty: 33.2),
            // CGAffineTransform(a: 7.0, b: -2.3, c: 6.7, d: 0.25, tx: 0.556, ty: 0.99),
            // CGAffineTransform(a: 0.498, b: -0.284, c: -0.742, d: 0.3248, tx: 12, ty: 44)
        ]

        if #available(OSX 10.13, iOS 11.0, watchOS 4.0, tvOS 11.0, *) {
            values.append(CGAffineTransform(rotationAngle: .pi / 2))
        }

        return values
    }()
    
    func test_CGAffineTransform_JSON() {
        for transform in cg_affineTransformValues {
            expectRoundTripEqualityThroughJSON(for: transform)
        }
    }

    // MARK: - CGPoint
    lazy var cg_pointValues: [CGPoint] = {
        var values = [
            CGPoint.zero,
            CGPoint(x: 10, y: 20)
        ]

        if #available(OSX 10.13, iOS 11.0, watchOS 4.0, tvOS 11.0, *) {
            // Limit on magnitude in JSON. See rdar://problem/12717407
            values.append(CGPoint(x: CGFloat.greatestFiniteMagnitude,
                                  y: CGFloat.greatestFiniteMagnitude))
        }

        return values
    }()

    func test_CGPoint_JSON() {
        for point in cg_pointValues {
            expectRoundTripEqualityThroughJSON(for: point)
        }
    }

    // MARK: - CGSize
    lazy var cg_sizeValues: [CGSize] = {
        var values = [
            CGSize.zero,
            CGSize(width: 30, height: 40)
        ]

        if #available(OSX 10.13, iOS 11.0, watchOS 4.0, tvOS 11.0, *) {
            // Limit on magnitude in JSON. See rdar://problem/12717407
            values.append(CGSize(width: CGFloat.greatestFiniteMagnitude,
                                 height: CGFloat.greatestFiniteMagnitude))
        }

        return values
    }()

    func test_CGSize_JSON() {
        for size in cg_sizeValues {
            expectRoundTripEqualityThroughJSON(for: size)
        }
    }

    // MARK: - CGRect
    lazy var cg_rectValues: [CGRect] = {
        // FIXME: Comment the tests back in once rdar://problem/33363218 is in the SDK.
        var values = [
            CGRect.zero,
            // CGRect.null,
            CGRect(x: 10, y: 20, width: 30, height: 40)
        ]

        if #available(OSX 10.13, iOS 11.0, watchOS 4.0, tvOS 11.0, *) {
            // Limit on magnitude in JSON. See rdar://problem/12717407
            // values.append(CGRect.infinite)
        }

        return values
    }()

    func test_CGRect_JSON() {
        for rect in cg_rectValues {
            expectRoundTripEqualityThroughJSON(for: rect)
        }
    }

    // MARK: - CGVector
    lazy var cg_vectorValues: [CGVector] = {
        // FIXME: Comment the tests back in once rdar://problem/33363218 is in the SDK.
        var values = [
            CGVector.zero,
            // CGVector(dx: 0.0, dy: -9.81)
        ]

        if #available(OSX 10.13, iOS 11.0, watchOS 4.0, tvOS 11.0, *) {
            // Limit on magnitude in JSON. See rdar://problem/12717407
            values.append(CGVector(dx: CGFloat.greatestFiniteMagnitude,
                                   dy: CGFloat.greatestFiniteMagnitude))
        }

        return values
    }()

    func test_CGVector_JSON() {
        for vector in cg_vectorValues {
            expectRoundTripEqualityThroughJSON(for: vector)
        }
    }
    
    #endif

    // MARK: - DateComponents
    lazy var dateComponents: Set<Calendar.Component> = [
        .era, .year, .month, .day, .hour, .minute, .second, .nanosecond,
        .weekday, .weekdayOrdinal, .quarter, .weekOfMonth, .weekOfYear,
        .yearForWeekOfYear, .timeZone, .calendar
    ]

    func test_DateComponents_JSON() {
        let calendar = Calendar(identifier: .gregorian)
        let components = calendar.dateComponents(dateComponents, from: Date())
        expectRoundTripEqualityThroughJSON(for: components)
    }

    // MARK: - Decimal
    lazy var decimalValues: [Decimal] = [
        Decimal.leastFiniteMagnitude,
        Decimal.greatestFiniteMagnitude,
        Decimal.leastNormalMagnitude,
        Decimal.leastNonzeroMagnitude,
        Decimal(),

        // Decimal.pi does not round-trip at the moment.
        // See rdar://problem/33165355
        // Decimal.pi,
    ]

    func test_Decimal_JSON() {
        for decimal in decimalValues {
            // Decimal encodes as a number in JSON and cannot be encoded at the top level.
            expectRoundTripEqualityThroughJSON(for: TopLevelWrapper(decimal))
        }
    }

    // MARK: - IndexPath
    lazy var indexPathValues: [IndexPath] = [
        IndexPath(), // empty
        IndexPath(index: 0), // single
        IndexPath(indexes: [1, 2]), // pair
        IndexPath(indexes: [3, 4, 5, 6, 7, 8]), // array
    ]

    func test_IndexPath_JSON() {
        for indexPath in indexPathValues {
            expectRoundTripEqualityThroughJSON(for: indexPath)
        }
    }

    // MARK: - IndexSet
    lazy var indexSetValues: [IndexSet] = [
        IndexSet(),
        IndexSet(integer: 42),
        IndexSet(integersIn: 0 ..< Int.max)
    ]

    func test_IndexSet_JSON() {
        for indexSet in indexSetValues {
            expectRoundTripEqualityThroughJSON(for: indexSet)
        }
    }

    // MARK: - Locale
    lazy var localeValues: [Locale] = [
        Locale(identifier: ""),
        Locale(identifier: "en"),
        Locale(identifier: "en_US"),
        Locale(identifier: "en_US_POSIX"),
        Locale(identifier: "uk"),
        Locale(identifier: "fr_FR"),
        Locale(identifier: "fr_BE"),
        Locale(identifier: "zh-Hant-HK")
    ]

    func test_Locale_JSON() {
        for locale in localeValues {
            expectRoundTripEqualityThroughJSON(for: locale)
        }
    }

    // MARK: - Measurement
    @available(OSX 10.12, iOS 10.0, watchOS 3.0, tvOS 10.0, *)
    lazy var unitValues: [Dimension] = [
        UnitAcceleration.metersPerSecondSquared,
        UnitMass.kilograms,
        UnitLength.miles
    ]

    // MARK: - NSRange
    lazy var nsrangeValues: [NSRange] = [
        NSRange(),
        NSRange(location: 0, length: Int.max),
        NSRange(location: NSNotFound, length: 0),
        ]

    func test_NSRange_JSON() {
        for range in nsrangeValues {
            expectRoundTripEqualityThroughJSON(for: range)
        }
    }

    // MARK: - PersonNameComponents
    @available(OSX 10.11, iOS 9.0, *)
    lazy var personNameComponentsValues: [PersonNameComponents] = [
        makePersonNameComponents(givenName: "John", familyName: "Appleseed"),
        makePersonNameComponents(givenName: "John", familyName: "Appleseed", nickname: "Johnny"),
        makePersonNameComponents(namePrefix: "Dr.", givenName: "Jane", middleName: "A.", familyName: "Appleseed", nameSuffix: "Esq.", nickname: "Janie")
    ]

    @available(OSX 10.11, iOS 9.0, *)
    func test_PersonNameComponents_JSON() {
        for components in personNameComponentsValues {
            expectRoundTripEqualityThroughJSON(for: components)
        }
    }

    // MARK: - TimeZone
    lazy var timeZoneValues: [TimeZone] = [
        TimeZone(identifier: "America/Los_Angeles")!,
        TimeZone(identifier: "UTC")!,
        TimeZone.current
    ]

    func test_TimeZone_JSON() {
        for timeZone in timeZoneValues {
            expectRoundTripEqualityThroughJSON(for: timeZone)
        }
    }

    // MARK: - URL
    lazy var urlValues: [URL] = {
        var values: [URL] = [
            URL(fileURLWithPath: NSTemporaryDirectory()),
            URL(fileURLWithPath: "/"),
            URL(string: "http://apple.com")!,
            URL(string: "swift", relativeTo: URL(string: "http://apple.com")!)!
        ]

        if #available(OSX 10.11, iOS 9.0, *) {
            values.append(URL(fileURLWithPath: "bin/sh", relativeTo: URL(fileURLWithPath: "/")))
        }

        return values
    }()

    func test_URL_JSON() {
        for url in urlValues {
            // URLs encode as single strings in JSON. They lose their baseURL this way.
            // For relative URLs, we don't expect them to be equal to the original.
            if url.baseURL == nil {
                // This is an absolute URL; we can expect equality.
                expectRoundTripEqualityThroughJSON(for: TopLevelWrapper(url))
            } else {
                // This is a relative URL. Make it absolute first.
                let absoluteURL = URL(string: url.absoluteString)!
                expectRoundTripEqualityThroughJSON(for: TopLevelWrapper(absoluteURL))
            }
        }
    }

    // MARK: - UUID
    lazy var uuidValues: [UUID] = [
        UUID(),
        UUID(uuidString: "E621E1F8-C36C-495A-93FC-0C247A3E6E5F")!,
        UUID(uuidString: "e621e1f8-c36c-495a-93fc-0c247a3e6e5f")!,
        UUID(uuid: uuid_t(0xe6,0x21,0xe1,0xf8,0xc3,0x6c,0x49,0x5a,0x93,0xfc,0x0c,0x24,0x7a,0x3e,0x6e,0x5f))
    ]

    func test_UUID_JSON() {
        for uuid in uuidValues {
            // We have to wrap the UUID since we cannot have a top-level string.
            expectRoundTripEqualityThroughJSON(for: UUIDCodingWrapper(uuid))
        }
    }
}

// MARK: - Helper Types

private enum TopLevelWrapperCodingKeys: String, CodingKey {
    case value
}

struct TopLevelWrapper<T> : Codable, Equatable where T : Codable, T : Equatable {

    

    /// Creates a new instance by decoding from the given decoder.
    ///
    /// This initializer throws an error if reading from the decoder fails, or
    /// if the data read is corrupted or otherwise invalid.
    ///
    /// - Parameter decoder: The decoder to read data from.
    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: TopLevelWrapperCodingKeys.self)
        value = try container.decode(T.self, forKey: .value)
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: TopLevelWrapperCodingKeys.self)
        try container.encode(value, forKey: .value)
    }

    let value: T

    init(_ value: T) {
        self.value = value
    }

    static func ==(_ lhs: TopLevelWrapper<T>, _ rhs: TopLevelWrapper<T>) -> Bool {
        return lhs.value == rhs.value
    }
}
