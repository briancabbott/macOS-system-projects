// RUN: %empty-directory(%t)
// RUN: %build-clang-importer-objc-overlays
// RUN: %sourcekitd-test -req=index %s -- %s -Xfrontend -disable-implicit-string-processing-module-import -module-name Swift -target %target-triple %clang-importer-sdk-nosource -I %t | %sed_clean > %t.response1
// RUN: %diff -u %s.response %t.response1
// RUN: %sourcekitd-test -req=index %s -- %s -Xfrontend -disable-implicit-string-processing-module-import -module-name 12345 -target %target-triple %clang-importer-sdk-nosource -I %t | %sed_clean > %t.response2
// RUN: %diff -u %s.response %t.response2

import ObjectiveC
let v: NSObject?

// REQUIRES: objc_interop
// REQUIRES: rdar95219987
