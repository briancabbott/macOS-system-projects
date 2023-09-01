//===--- EnvironmentVariables.h - Debug variables. --------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// Debug behavior conditionally enabled using environment variables.
//
//===----------------------------------------------------------------------===//

#include "swift/Threading/Once.h"

namespace swift {
namespace runtime {
namespace environment {

void initialize(void *);

extern swift::once_t initializeToken;

// Declare backing variables.
#define VARIABLE(name, type, defaultValue, help) extern type name ## _variable;
#include "../../../stdlib/public/runtime/EnvironmentVariables.def"

// Define getter functions.
#define VARIABLE(name, type, defaultValue, help)                               \
  inline type name() {                                                         \
    swift::once(initializeToken, initialize, nullptr);                         \
    return name##_variable;                                                    \
  }
#include "../../../stdlib/public/runtime/EnvironmentVariables.def"

// Wrapper around SWIFT_DEBUG_CONCURRENCY_ENABLE_COOPERATIVE_QUEUES that the
// Concurrency library can call.
SWIFT_RUNTIME_STDLIB_SPI bool concurrencyEnableCooperativeQueues();

// Wrapper around SWIFT_ENABLE_ASYNC_JOB_DISPATCH_INTEGRATION that the
// Concurrency library can call.
SWIFT_RUNTIME_STDLIB_SPI bool concurrencyEnableJobDispatchIntegration();

// Wrapper around SWIFT_DEBUG_VALIDATE_UNCHECKED_CONTINUATIONS that the
// Concurrency library can call.
SWIFT_RUNTIME_STDLIB_SPI bool concurrencyValidateUncheckedContinuations();

} // end namespace environment
} // end namespace runtime
} // end namespace Swift
