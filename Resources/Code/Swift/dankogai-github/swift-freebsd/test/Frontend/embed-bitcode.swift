// REQUIRES: CPU=x86_64
// REQUIRES: rdar23493035
// RUN: %target-swift-frontend -c -module-name someModule -embed-bitcode-marker  -o %t.o %s
// RUN: llvm-objdump -macho -section="__LLVM,__bitcode" %t.o | FileCheck -check-prefix=MARKER %s
// RUN: llvm-objdump -macho -section="__LLVM,__swift_cmdline" %t.o | FileCheck -check-prefix=MARKER-CMD %s

// UNSUPPORTED: OS=linux-gnu

// MARKER: Contents of (__LLVM,__bitcode) section
// MARKER-NEXT: 00
// MARKER-CMD: Contents of (__LLVM,__swift_cmdline) section
// MARKER-CMD-NEXT: 00

func test() {
}
