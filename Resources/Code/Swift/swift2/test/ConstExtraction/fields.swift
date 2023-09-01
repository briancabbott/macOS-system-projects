// RUN: %empty-directory(%t)
// RUN: %empty-directory(%t/inputs)
// RUN: echo "[MyProto]" > %t/inputs/protocols.json

// RUN: %target-swift-frontend -typecheck -emit-const-values-path %t/fields.swiftconstvalues -const-gather-protocols-file %t/inputs/protocols.json -primary-file %s
// RUN: cat %t/fields.swiftconstvalues 2>&1 | %FileCheck %s

// CHECK: [
// CHECK-NEXT:  {
// CHECK-NEXT:    "typeName": "fields.Foo",
// CHECK-NEXT:    "kind": "struct",
// CHECK-NEXT:    "properties": [
// CHECK-NEXT:      {
// CHECK-NEXT:        "label": "p1",
// CHECK-NEXT:        "type": "Swift.String",
// CHECK-NEXT:        "isStatic": "false",
// CHECK-NEXT:        "isComputed": "false",
// CHECK-NEXT:        "valueKind": "RawLiteral",
// CHECK-NEXT:        "value": "\"Hello, World\""
// CHECK-NEXT:      },
// CHECK-NEXT:      {
// CHECK-NEXT:        "label": "p5",
// CHECK-NEXT:        "type": "[Swift.Int]",
// CHECK-NEXT:        "isStatic": "false",
// CHECK-NEXT:        "isComputed": "false",
// CHECK-NEXT:        "valueKind": "RawLiteral",
// CHECK-NEXT:        "value": "[1, 2, 3, 4, 5, 6, 7, 8, 9]"
// CHECK-NEXT:      },
// CHECK-NEXT:      {
// CHECK-NEXT:        "label": "p6",
// CHECK-NEXT:        "type": "Swift.Bool",
// CHECK-NEXT:        "isStatic": "false",
// CHECK-NEXT:        "isComputed": "false",
// CHECK-NEXT:        "valueKind": "RawLiteral",
// CHECK-NEXT:        "value": "false"
// CHECK-NEXT:      },
// CHECK-NEXT:      {
// CHECK-NEXT:        "label": "p7",
// CHECK-NEXT:        "type": "Swift.Bool?",
// CHECK-NEXT:        "isStatic": "false",
// CHECK-NEXT:        "isComputed": "false",
// CHECK-NEXT:        "valueKind": "RawLiteral",
// CHECK-NEXT:        "value": "nil"
// CHECK-NEXT:      },
// CHECK-NEXT:      {
// CHECK-NEXT:        "label": "p8",
// CHECK-NEXT:        "type": "(Swift.Int, Swift.Float)",
// CHECK-NEXT:        "isStatic": "false",
// CHECK-NEXT:        "isComputed": "false",
// CHECK-NEXT:        "valueKind": "RawLiteral",
// CHECK-NEXT:        "value": "(42, 6.6)"
// CHECK-NEXT:      },
// CHECK-NEXT:      {
// CHECK-NEXT:        "label": "p9",
// CHECK-NEXT:        "type": "[Swift.String : Swift.Int]",
// CHECK-NEXT:        "isStatic": "false",
// CHECK-NEXT:        "isComputed": "false",
// CHECK-NEXT:        "valueKind": "RawLiteral",
// CHECK-NEXT:        "value": "[(\"One\", 1), (\"Two\", 2), (\"Three\", 3)]"
// CHECK-NEXT:      },
// CHECK-NEXT:      {
// CHECK-NEXT:        "label": "p10",
// CHECK-NEXT:        "type": "fields.Bar",
// CHECK-NEXT:        "isStatic": "false",
// CHECK-NEXT:        "isComputed": "false",
// CHECK-NEXT:        "valueKind": "InitCall",
// CHECK-NEXT:        "value": {
// CHECK-NEXT:          "type": "fields.Bar",
// CHECK-NEXT:          "arguments": []
// CHECK-NEXT:        }
// CHECK-NEXT:      },
// CHECK-NEXT:      {
// CHECK-NEXT:        "label": "p11",
// CHECK-NEXT:        "type": "fields.Bat",
// CHECK-NEXT:        "isStatic": "false",
// CHECK-NEXT:        "isComputed": "false",
// CHECK-NEXT:        "valueKind": "InitCall",
// CHECK-NEXT:        "value": {
// CHECK-NEXT:          "type": "fields.Bat",
// CHECK-NEXT:          "arguments": [
// CHECK-NEXT:            {
// CHECK-NEXT:              "label": "buz",
// CHECK-NEXT:              "type": "Swift.String",
// CHECK-NEXT:              "valueKind": "RawLiteral",
// CHECK-NEXT:              "value": "\"\""
// CHECK-NEXT:            },
// CHECK-NEXT:            {
// CHECK-NEXT:              "label": "fuz",
// CHECK-NEXT:              "type": "Swift.Int",
// CHECK-NEXT:              "valueKind": "RawLiteral",
// CHECK-NEXT:              "value": "0"
// CHECK-NEXT:            }
// CHECK-NEXT:          ]
// CHECK-NEXT:        }
// CHECK-NEXT:      },
// CHECK-NEXT:      {
// CHECK-NEXT:        "label": "p12",
// CHECK-NEXT:        "type": "fields.Bat",
// CHECK-NEXT:        "isStatic": "false",
// CHECK-NEXT:        "isComputed": "false",
// CHECK-NEXT:        "valueKind": "InitCall",
// CHECK-NEXT:        "value": {
// CHECK-NEXT:          "type": "fields.Bat",
// CHECK-NEXT:          "arguments": [
// CHECK-NEXT:            {
// CHECK-NEXT:              "label": "buz",
// CHECK-NEXT:              "type": "Swift.String",
// CHECK-NEXT:              "valueKind": "RawLiteral",
// CHECK-NEXT:              "value": "\"hello\""
// CHECK-NEXT:            },
// CHECK-NEXT:            {
// CHECK-NEXT:              "label": "fuz",
// CHECK-NEXT:              "type": "Swift.Int",
// CHECK-NEXT:              "valueKind": "Runtime"
// CHECK-NEXT:            }
// CHECK-NEXT:          ]
// CHECK-NEXT:        }
// CHECK-NEXT:      },
// CHECK-NEXT:      {
// CHECK-NEXT:        "label": "p13",
// CHECK-NEXT:        "type": "Swift.Int",
// CHECK-NEXT:        "isStatic": "false",
// CHECK-NEXT:        "isComputed": "false",
// CHECK-NEXT:        "valueKind": "Runtime"
// CHECK-NEXT:      },
// CHECK-NEXT:      {
// CHECK-NEXT:        "label": "p0",
// CHECK-NEXT:        "type": "Swift.Int",
// CHECK-NEXT:        "isStatic": "true",
// CHECK-NEXT:        "isComputed": "false",
// CHECK-NEXT:        "valueKind": "RawLiteral",
// CHECK-NEXT:        "value": "11"
// CHECK-NEXT:      },
// CHECK-NEXT:      {
// CHECK-NEXT:        "label": "p2",
// CHECK-NEXT:        "type": "Swift.Float",
// CHECK-NEXT:        "isStatic": "true",
// CHECK-NEXT:        "isComputed": "false",
// CHECK-NEXT:        "valueKind": "RawLiteral",
// CHECK-NEXT:        "value": "42.2"
// CHECK-NEXT:      },
// CHECK-NEXT:      {
// CHECK-NEXT:        "label": "p3",
// CHECK-NEXT:        "type": "Swift.Int",
// CHECK-NEXT:        "isStatic": "false",
// CHECK-NEXT:        "isComputed": "true",
// CHECK-NEXT:        "valueKind": "RawLiteral",
// CHECK-NEXT:        "value": "3"
// CHECK-NEXT:      },
// CHECK-NEXT:      {
// CHECK-NEXT:        "label": "p4",
// CHECK-NEXT:        "type": "Swift.Int",
// CHECK-NEXT:        "isStatic": "true",
// CHECK-NEXT:        "isComputed": "true",
// CHECK-NEXT:        "valueKind": "RawLiteral",
// CHECK-NEXT:        "value": "3"
// CHECK-NEXT:      }
// CHECK-NEXT:    ]
// CHECK-NEXT:  }
// CHECK-NEXT:]

protocol MyProto {}

public struct Foo {
    static _const let p0: Int = 11
    let p1: String = "Hello, World"
    static let p2: Float = 42.2
    var p3: Int {3}
    static var p4: Int { return 3 }
    let p5: [Int] = [1,2,3,4,5,6,7,8,9]
    let p6: Bool = false
    let p7: Bool? = nil
    let p8: (Int, Float) = (42, 6.6)
    let p9: [String: Int] = ["One": 1, "Two": 2, "Three": 3]
    let p10 = Bar()
    let p11: Bat = .init()
    let p12 = Bat(buz: "hello", fuz: adder(2, 3))
    let p13: Int = adder(2, 3)
}

func adder(_ x: Int, _ y: Int) -> Int {
    x + y
}

public struct Bar {}
public struct Bat {
    let buz: String
    let fuz: Int

    init(buz: String = "", fuz: Int = 0) {
        self.buz = buz
        self.fuz = fuz
    }
}

extension Foo : MyProto {}
