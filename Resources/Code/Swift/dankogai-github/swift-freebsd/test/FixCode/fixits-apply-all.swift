// This tests whether we accept fixits from warnings without filtering.
// The particular diagnostics used are not important.

// RUN: %swift -parse -target %target-triple %s -fixit-all -emit-fixits-path %t.remap
// RUN: c-arcmt-test %t.remap | arcmt-test -verify-transformed-files %s.result

func ftest1() {
  let myvar = 0
}
