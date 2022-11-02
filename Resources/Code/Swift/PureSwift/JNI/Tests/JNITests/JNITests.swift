//
//  JNITests.swift
//  PureSwift
//
//  Created by Alsey Coleman Miller on 3/19/18.
//  Copyright © 2018 PureSwift. All rights reserved.
//

import Foundation
import XCTest
@testable import JNI

class JNITests: XCTestCase {
    
    static var allTests = [
        ("testMethodSignature", testMethodSignature),
        ]
    
    func testMethodSignature() {
        
        let testData: [(JNIMethodSignature, String)] = [
            
            (JNIMethodSignature(
                argumentTypes: [],
                returnType: .void),
             "()V"), // void f1()
            
            (JNIMethodSignature(
                argumentTypes: [
                    .int,
                    .long
                ],
                returnType: .int),
             "(IJ)I"), // int f2(int n, long l)
            
            (JNIMethodSignature(
                argumentTypes: [
                    .array(.int)
                ],
                returnType: .boolean),
             "([I)Z"), // boolean f3(int[] arr)
            
            (JNIMethodSignature(
                argumentTypes: [
                    .object(.java("lang") ☕️ ["String"]),
                    .int
                ],
                returnType: .double),
             "(Ljava/lang/String;I)D"), // double f4(String s, int n)
            
            (JNIMethodSignature(
                argumentTypes: [
                    .int,
                    .array(.object(.java("lang") ☕️ ["String"])),
                    .char
                ], returnType: .void),
             "(I[Ljava/lang/String;C)V"), // void f5(int n, String[] arr, char c)
            
            (JNIMethodSignature(
                argumentTypes: [
                    .int,
                    .object(.java("lang") ☕️ ["String"]),
                    .array(.int)
                ],
                returnType: .long),
             "(ILjava/lang/String;[I)J"), // long f6(int n, String s, int[] arr)
            
            (JNIMethodSignature(
                argumentTypes: [
                    .object(.java("lang") ☕️ ["Boolean"]),
                    .boolean
                ],
                returnType: .object(.java("lang") ☕️ ["String"])),
             "(Ljava/lang/Boolean;Z)Ljava/lang/String;"), // String f7(Boolean b, boolean b2)
            
            (JNIMethodSignature(
                argumentTypes: [
                    .object(["android", "content"] ☕️ ["Context"]),
                    .boolean,
                    .object(["android", "bluetooth"] ☕️ ["BluetoothGattCallback"]),
                    .int
                ],
                returnType:
                    .object(["android", "bluetooth"] ☕️ ["BluetoothGatt"])
                ),
             "(Landroid/content/Context;ZLandroid/bluetooth/BluetoothGattCallback;I)Landroid/bluetooth/BluetoothGatt;"
            ) // public android.bluetooth.BluetoothGatt connectGatt(android.content.Context, boolean, android.bluetooth.BluetoothGattCallback, int);
            // com/jmarkstar/swiftandroid/R$layout
        ]
        
        for (signature, string) in testData {
            
            XCTAssert(signature.rawValue == string, "Unable to encode\n\(signature.rawValue)\n\(string)")
            
            guard let decoded = JNIMethodSignature(rawValue: string)
                else { XCTFail("Unable to decode\n\(string)"); continue }
            
            XCTAssert(signature == decoded)
        }
    }
    
    func testPackage() {
        
        do {
            
            let stringValue = "java/lang"
            
            let package: JNIPackage = ["java", "lang"]
            
            XCTAssert(package.rawValue == stringValue)
            XCTAssert(package == JNIPackage(rawValue: stringValue))
        }
        
        do {
            
            let stringValue = "java/util"
            
            let package: JNIPackage = ["java", "util"]
            
            XCTAssert(package.rawValue == stringValue)
            XCTAssert(package == JNIPackage(rawValue: stringValue))
        }
        
        do {
            
            let package: JNIPackage = ["android"]
            
            let subpackage: JNIPackage = package + ["content"]
            
            XCTAssert(subpackage.elements == ["android", "content"])
        }
    }
    
    func testClassName() {
        
        do {
            
            let stringValue = "com/jmarkstar/swiftandroid/R$layout"
            
            let className = ["com", "jmarkstar", "swiftandroid"] ☕️ ["R", "layout"]
            
            XCTAssert(className.rawValue == stringValue, "\(className.rawValue) == \(stringValue)")
            XCTAssert(className == JNIClassName(rawValue: stringValue))
        }
    }
}
