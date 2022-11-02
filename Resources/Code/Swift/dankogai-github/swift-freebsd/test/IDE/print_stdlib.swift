// Check interface produced for the standard library.
//
// RUN: %target-swift-ide-test -print-module -module-to-print=Swift -source-filename %s -print-interface > %t.txt
// RUN: FileCheck -check-prefix=CHECK-ARGC %s < %t.txt
// RUN: FileCheck %s < %t.txt
// RUN: FileCheck -check-prefix=CHECK-SUGAR %s < %t.txt
// RUN: FileCheck -check-prefix=CHECK-MUTATING-ATTR %s < %t.txt
// RUN: FileCheck -check-prefix=NO-FIXMES %s < %t.txt
// RUN: FileCheck -check-prefix=CHECK-ARGC %s < %t.txt

// RUN: %target-swift-ide-test -print-module -module-to-print=Swift -source-filename %s -print-interface-doc > %t-doc.txt
// RUN: FileCheck %s < %t-doc.txt

// RUN: %target-swift-ide-test -print-module -module-to-print=Swift -source-filename %s -print-interface -skip-underscored-stdlib-protocols > %t-prot.txt
// RUN: FileCheck -check-prefix=CHECK-UNDERSCORED-PROT %s < %t-prot.txt
// CHECK-UNDERSCORED-PROT-NOT: protocol _

// CHECK-ARGC: static var argc: CInt { get }

// CHECK-NOT: @rethrows
// CHECK-NOT: {{^}}import
// CHECK-NOT: _Double
// CHECK-NOT: _StringBuffer
// CHECK-NOT: _StringCore
// CHECK-NOT: _ArrayBody
// DONT_CHECK-NOT: {{([^I]|$)([^n]|$)([^d]|$)([^e]|$)([^x]|$)([^a]|$)([^b]|$)([^l]|$)([^e]|$)}}
// CHECK-NOT: buffer: _ArrayBuffer
// CHECK-NOT: func ~>
// FIXME: Builtin.
// FIXME: RawPointer
// CHECK-NOT: extension [
// CHECK-NOT: extension {{.*}}?
// CHECK-NOT: extension {{.*}}!
// CHECK-NOT: addressWithOwner
// CHECK-NOT: mutableAddressWithOwner
// CHECK-NOT: _ColorLiteralConvertible
// CHECK-NOT: _FileReferenceLiteralConvertible
// CHECK-NOT: _ImageLiteralConvertible

// CHECK-SUGAR: extension Array :
// CHECK-SUGAR: extension ImplicitlyUnwrappedOptional :
// CHECK-SUGAR: extension Optional :

// CHECK-MUTATING-ATTR: mutating func

// NO-FIXMES-NOT: FIXME
