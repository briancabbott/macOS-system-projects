func foo() {
  let x = 1

  x.
}

// RUN: %sourcekitd-test -req=complete.open -pos=4:5 %s -- %s > %t.open

// RUN: %sourcekitd-test -req=complete.open -pos=4:5 %s -- %s \
// RUN:   == -req=complete.update -pos=4:5 %s -- %s \
// RUN:   == -req=complete.update -pos=4:5 %s -- %s > %t.update
// RUN: FileCheck %s < %t.update
// CHECK:   key.name: "advancedBy
// CHECK: key.kind: source.lang.swift.codecomplete.group
// CHECK:   key.name: "advancedBy
// CHECK: key.kind: source.lang.swift.codecomplete.group
// CHECK:   key.name: "advancedBy
// CHECK: key.kind: source.lang.swift.codecomplete.group

// RUN: cat %t.open %t.open %t.open > %t.check
// RUN: diff -u %t.update %t.check


struct X {
  func aaaBbb() {}
  func aaaCcc() {}
  func aaaa() {}
}
func test(x: X) {
  x.
}

// Update can have different grouping settings
// RUN: %sourcekitd-test -req=complete.open -pos=30:5 -req-opts=group.stems=0,group.overloads=0 %s -- %s \
// RUN:   == -req=complete.update -pos=30:5 -req-opts=group.stems=1 %s -- %s \
// RUN:   == -req=complete.update -pos=30:5 -req-opts=group.stems=0,group.overloads=0 %s -- %s > %t.update.groupings
// RUN: FileCheck %s -check-prefix=ONE_GROUP < %t.update.groupings
// ONE_GROUP-NOT: key.name: "aaa"
// ONE_GROUP: key.kind: source.lang.swift.codecomplete.group,
// ONE_GROUP-NEXT: key.name: ""
// ONE_GROUP-NOT: key.name: "aaa"
// ONE_GROUP: key.kind: source.lang.swift.codecomplete.group,
// ONE_GROUP-NEXT: key.name: "aaa"
// ONE_GROUP-NOT: key.name: "aaa"
