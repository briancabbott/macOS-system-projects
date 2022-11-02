//
//  InternetProtocolTests.swift
//  
//
//  Created by Alsey Coleman Miller on 5/15/20.
//

import XCTest
#if SYSTEM_PACKAGE
@testable import SystemPackage
#else
@testable import System
#endif

final class InternetProtocolTests: XCTestCase {
    
    func testAddress() {
        
        let values: [(IPAddress, String)] = [
            (.v4(.any), "0.0.0.0"),
            (.init(127, 0, 0, 1), "127.0.0.1"),
            (.v4(IPv4Address(rawValue: "192.168.0.110")!), "192.168.0.110"),
            (.v6(.any), "::"),
            (.v6(.loopback), "::1"),
            (.init(0xfe80, 0x0000, 0x0000, 0x0000, 0x9eae, 0xd3ff, 0xfe97, 0x92c5), "fe80::9eae:d3ff:fe97:92c5"),
            (IPAddress(rawValue: "2001:db8::8a2e:370:7334")!, "2001:db8::8a2e:370:7334")
        ]
        
        for (address, string) in values {
            XCTAssertEqual(address.description, string)
            XCTAssertEqual(address.rawValue, string)
            XCTAssertEqual(IPAddress(rawValue: string), address)
        }
    }
    
    func testIPv4Address() {
        
        let strings = [
            "127.0.0.1",
            "192.168.0.110"
        ]
        
        for string in strings {
            
            guard let address = IPv4Address(rawValue: string)
                else { XCTFail("Invalid string \(string)"); return }
            
            XCTAssertEqual(address.description, string)
            XCTAssertEqual(address, address)
            XCTAssertEqual(address.hashValue, address.hashValue)
        }
    }
    
    func testInvalidIPv4Address() {
        
        let strings = [
            "",
            "fe80::9eae:d3ff:fe97:92c5",
            "192.168.0.110."
        ]
        
        strings.forEach { XCTAssertNil(IPv4Address(rawValue: $0)) }
    }
    
    func testIPv6Address() {
        
        let strings = [
            "fe80::9eae:d3ff:fe97:92c5"
        ]
        
        for string in strings {
            
            guard let address = IPv6Address(rawValue: string)
                else { XCTFail("Invalid string \(string)"); return }
            
            XCTAssertEqual(address.description, string)
            XCTAssertEqual(address, address)
            XCTAssertEqual(address.hashValue, address.hashValue)
        }
    }
    
    func testInvalidIPv6Address() {
        
        let strings = [
            "",
            ":9eae:d3ff:fe97:92c5",
            "192.168.0.110"
        ]
        
        strings.forEach { XCTAssertNil(IPv6Address(rawValue: $0)) }
    }
}
