// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2016 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//

#if NS_FOUNDATION_ALLOWS_TESTABLE_IMPORT
    #if canImport(SwiftFoundation) && !DEPLOYMENT_RUNTIME_OBJC
        @testable import SwiftFoundation
    #else
        @testable import Foundation
    #endif
#endif

class TestProcessInfo : XCTestCase {
    
    func test_operatingSystemVersion() {
        let processInfo = ProcessInfo.processInfo
        let versionString = processInfo.operatingSystemVersionString
        XCTAssertFalse(versionString.isEmpty)

#if os(Linux)
        // Since the list of supported distros tends to change, at least check that it used os-release (if it's there).
        if let distroId = try? String(contentsOf: URL(fileURLWithPath: "/etc/os-release", isDirectory: false)),
           distroId.contains("PRETTY_NAME")
        {
            XCTAssertTrue(distroId.contains(versionString))
        } else {
            XCTAssertTrue(versionString.contains("Linux"))
        }
#elseif os(Windows)
        XCTAssertTrue(versionString.hasPrefix("Windows"))
#endif

        let version = processInfo.operatingSystemVersion
        XCTAssert(version.majorVersion != 0)

#if canImport(Darwin) || os(Linux) || os(Windows)
        let minVersion = OperatingSystemVersion(majorVersion: 1, minorVersion: 0, patchVersion: 0)
        XCTAssertTrue(processInfo.isOperatingSystemAtLeast(minVersion))
#endif
    }
    
    func test_processName() {
        // Assert that the original process name is "TestFoundation". This test
        // will fail if the test target ever gets renamed, so maybe it should
        // just test that the initial name is not empty or something?
#if DARWIN_COMPATIBILITY_TESTS
        let targetName = "xctest"
#elseif os(Windows)
        let targetName = "TestFoundation.exe"
#else
        let targetName = "TestFoundation"
#endif
        let processInfo = ProcessInfo.processInfo
        let originalProcessName = processInfo.processName
        XCTAssertEqual(originalProcessName, targetName, "\"\(originalProcessName)\" not equal to \"TestFoundation\"")
        
        // Try assigning a new process name.
        let newProcessName = "TestProcessName"
        processInfo.processName = newProcessName
        XCTAssertEqual(processInfo.processName, newProcessName, "\"\(processInfo.processName)\" not equal to \"\(newProcessName)\"")
        
        // Assign back to the original process name.
        processInfo.processName = originalProcessName
        XCTAssertEqual(processInfo.processName, originalProcessName, "\"\(processInfo.processName)\" not equal to \"\(originalProcessName)\"")
    }
    
    func test_globallyUniqueString() {
        let uuid = ProcessInfo.processInfo.globallyUniqueString
        
        let parts = uuid.components(separatedBy: "-")
        XCTAssertEqual(parts.count, 5)
        XCTAssertEqual(parts[0].utf16.count, 8)
        XCTAssertEqual(parts[1].utf16.count, 4)
        XCTAssertEqual(parts[2].utf16.count, 4)
        XCTAssertEqual(parts[3].utf16.count, 4)
        XCTAssertEqual(parts[4].utf16.count, 12)
    }

    func test_environment() {
#if os(Windows)
        func setenv(_ key: String, _ value: String, _ overwrite: Int) -> Int32 {
          assert(overwrite == 1)
          guard !key.contains("=") else {
              errno = EINVAL
              return -1
          }
          return _putenv("\(key)=\(value)")
        }
#endif

        let preset = ProcessInfo.processInfo.environment["test"]
        setenv("test", "worked", 1)
        let postset = ProcessInfo.processInfo.environment["test"]
        XCTAssertNil(preset)
        XCTAssertEqual(postset, "worked")

        // Bad values that wont be stored
        XCTAssertEqual(setenv("", "", 1), -1)
        XCTAssertEqual(setenv("bad1=", "", 1), -1)
        XCTAssertEqual(setenv("bad2=", "1", 1) ,-1)
        XCTAssertEqual(setenv("bad3=", "=2", 1), -1)

        // Good values that will be, check splitting on '='
        XCTAssertEqual(setenv("var1", "",1 ), 0)
        XCTAssertEqual(setenv("var2", "=", 1), 0)
        XCTAssertEqual(setenv("var3", "=x", 1), 0)
        XCTAssertEqual(setenv("var4", "x=", 1), 0)
        XCTAssertEqual(setenv("var5", "=x=", 1), 0)

        let env = ProcessInfo.processInfo.environment

        XCTAssertNil(env[""])
        XCTAssertNil(env["bad1"])
        XCTAssertNil(env["bad1="])
        XCTAssertNil(env["bad2"])
        XCTAssertNil(env["bad2="])
        XCTAssertNil(env["bad3"])
        XCTAssertNil(env["bad3="])

#if os(Windows)
        // On Windows, adding an empty environment variable removes it from the environment
        XCTAssertNil(env["var1"])
#else
        XCTAssertEqual(env["var1"], "")
#endif
        XCTAssertEqual(env["var2"], "=")
        XCTAssertEqual(env["var3"], "=x")
        XCTAssertEqual(env["var4"], "x=")
        XCTAssertEqual(env["var5"], "=x=")
    }


#if NS_FOUNDATION_ALLOWS_TESTABLE_IMPORT && os(Linux)
    func test_cfquota_parsing() throws {

        let tests = [
            ("50000", "100000", 1),
            ("100000", "100000", 1),
            ("100000\n", "100000", 1),
            ("100000", "100000\n", 1),
            ("150000", "100000", 2),
            ("200000", "100000", 2),
            ("-1", "100000", nil),
            ("100000", "-1", nil),
            ("", "100000", nil),
            ("100000", "", nil),
            ("100000", "0", nil)
        ]

        try withTemporaryDirectory() { (_, tempDirPath) -> Void in
            try tests.forEach { quota, period, count in
                let (fd1, quotaPath) = try _NSCreateTemporaryFile(tempDirPath + "/quota")
                FileHandle(fileDescriptor: fd1, closeOnDealloc: true).write(quota)

                let (fd2, periodPath) = try _NSCreateTemporaryFile(tempDirPath + "/period")
                FileHandle(fileDescriptor: fd2, closeOnDealloc: true).write(period)
                XCTAssertEqual(ProcessInfo.coreCount(quota: quotaPath, period: periodPath), count)
            }
        }
    }
#endif


    static var allTests: [(String, (TestProcessInfo) -> () throws -> Void)] {
        var tests: [(String, (TestProcessInfo) -> () throws -> ())] = [
            ("test_operatingSystemVersion", test_operatingSystemVersion ),
            ("test_processName", test_processName ),
            ("test_globallyUniqueString", test_globallyUniqueString ),
            ("test_environment", test_environment),
        ]

#if NS_FOUNDATION_ALLOWS_TESTABLE_IMPORT && os(Linux)
        tests.append(contentsOf: [ ("test_cfquota_parsing", test_cfquota_parsing) ])
#endif

        return tests
    }
}
