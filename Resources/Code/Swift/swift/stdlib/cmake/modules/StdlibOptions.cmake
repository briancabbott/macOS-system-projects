include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/modules/SwiftUtils.cmake)

precondition(SWIFT_HOST_VARIANT_SDK)
precondition(SWIFT_DARWIN_PLATFORMS)

# +----------------------------------------------------------------------+
# |                                                                      |
# |  NOTE: It makes no sense setting defaults here on the basis of       |
# |        SWIFT_HOST_VARIANT_SDK, because the stdlib is a *TARGET*      |
# |        library, not a host library.                                  |
# |                                                                      |
# |        Rather, if you have a default to set, you need to do that     |
# |        in AddSwiftStdlib.cmake, in an appropriate place,             |
# |        likely on the basis of CFLAGS_SDK, SWIFTLIB_SINGLE_SDK or     |
# |        similar.                                                      |
# |                                                                      |
# +----------------------------------------------------------------------+

if("${SWIFT_HOST_VARIANT_SDK}" MATCHES "CYGWIN")
  set(SWIFT_STDLIB_SUPPORTS_BACKTRACE_REPORTING_default FALSE)
elseif("${SWIFT_HOST_VARIANT_SDK}" MATCHES "HAIKU")
  set(SWIFT_STDLIB_SUPPORTS_BACKTRACE_REPORTING_default FALSE)
elseif("${SWIFT_HOST_VARIANT_SDK}" MATCHES "WASI")
  set(SWIFT_STDLIB_SUPPORTS_BACKTRACE_REPORTING_default FALSE)
else()
  set(SWIFT_STDLIB_SUPPORTS_BACKTRACE_REPORTING_default TRUE)
endif()

option(SWIFT_STDLIB_SUPPORTS_BACKTRACE_REPORTING
       "Build stdlib assuming the runtime environment provides the backtrace(3) API."
       "${SWIFT_STDLIB_SUPPORTS_BACKTRACE_REPORTING_default}")

if("${SWIFT_HOST_VARIANT_SDK}" IN_LIST SWIFT_DARWIN_PLATFORMS)
  set(SWIFT_STDLIB_HAS_ASL_default TRUE)
else()
  set(SWIFT_STDLIB_HAS_ASL_default FALSE)
endif()

option(SWIFT_STDLIB_HAS_ASL
       "Build stdlib assuming we can use the asl_log API."
       "${SWIFT_STDLIB_HAS_ASL_default}")

if("${SWIFT_HOST_VARIANT_SDK}" MATCHES "CYGWIN")
  set(SWIFT_STDLIB_HAS_LOCALE_default FALSE)
elseif("${SWIFT_HOST_VARIANT_SDK}" MATCHES "HAIKU")
  set(SWIFT_STDLIB_HAS_LOCALE_default FALSE)
else()
  set(SWIFT_STDLIB_HAS_LOCALE_default TRUE)
endif()

option(SWIFT_STDLIB_HAS_LOCALE
       "Build stdlib assuming the platform has locale support."
       "${SWIFT_STDLIB_HAS_LOCALE_default}")

if("${SWIFT_HOST_VARIANT_SDK}" IN_LIST SWIFT_DARWIN_PLATFORMS)
  # All Darwin platforms have ABI stability.
  set(SWIFT_STDLIB_STABLE_ABI_default TRUE)
elseif("${SWIFT_HOST_VARIANT_SDK}" STREQUAL "LINUX")
  # TODO(mracek): This should get turned off, as this is not an ABI stable platform.
  set(SWIFT_STDLIB_STABLE_ABI_default TRUE)
elseif("${SWIFT_HOST_VARIANT_SDK}" STREQUAL "FREEBSD")
  # TODO(mracek): This should get turned off, as this is not an ABI stable platform.
  set(SWIFT_STDLIB_STABLE_ABI_default TRUE)
elseif("${SWIFT_HOST_VARIANT_SDK}" STREQUAL "OPENBSD")
  # TODO(mracek): This should get turned off, as this is not an ABI stable platform.
  set(SWIFT_STDLIB_STABLE_ABI_default TRUE)
elseif("${SWIFT_HOST_VARIANT_SDK}" STREQUAL "CYGWIN")
  # TODO(mracek): This should get turned off, as this is not an ABI stable platform.
  set(SWIFT_STDLIB_STABLE_ABI_default TRUE)
elseif("${SWIFT_HOST_VARIANT_SDK}" STREQUAL "WINDOWS")
  # TODO(mracek): This should get turned off, as this is not an ABI stable platform.
  set(SWIFT_STDLIB_STABLE_ABI_default TRUE)
elseif("${SWIFT_HOST_VARIANT_SDK}" STREQUAL "HAIKU")
  # TODO(mracek): This should get turned off, as this is not an ABI stable platform.
  set(SWIFT_STDLIB_STABLE_ABI_default TRUE)
elseif("${SWIFT_HOST_VARIANT_SDK}" STREQUAL "ANDROID")
  # TODO(mracek): This should get turned off, as this is not an ABI stable platform.
  set(SWIFT_STDLIB_STABLE_ABI_default TRUE)
else()
  # Any new platform should have non-stable ABI to start with.
  set(SWIFT_STDLIB_STABLE_ABI_default FALSE)
endif()

option(SWIFT_STDLIB_STABLE_ABI
       "Should stdlib be built with stable ABI (library evolution, resilience)."
       "${SWIFT_STDLIB_STABLE_ABI_default}")

option(SWIFT_STDLIB_COMPACT_ABSOLUTE_FUNCTION_POINTER
       "Force compact function pointer to always be absolute mainly for WebAssembly"
       FALSE)

option(SWIFT_ENABLE_MODULE_INTERFACES
       "Generate .swiftinterface files alongside .swiftmodule files"
       "${SWIFT_STDLIB_STABLE_ABI}")

if("${SWIFT_HOST_VARIANT_SDK}" IN_LIST SWIFT_DARWIN_PLATFORMS)
  set(SWIFT_STDLIB_ENABLE_PRESPECIALIZATION_default TRUE)
elseif("${SWIFT_HOST_VARIANT_SDK}" STREQUAL "LINUX")
  set(SWIFT_STDLIB_ENABLE_PRESPECIALIZATION_default TRUE)
else()
  set(SWIFT_STDLIB_ENABLE_PRESPECIALIZATION_default FALSE)
endif()

option(SWIFT_STDLIB_ENABLE_PRESPECIALIZATION
       "Should stdlib be built with generic metadata prespecialization enabled. Defaults to On on Darwin and on Linux."
       "${SWIFT_STDLIB_ENABLE_PRESPECIALIZATION_default}")
 
option(SWIFT_STDLIB_ENABLE_UNICODE_DATA
       "Should stdlib be built with full unicode support"
       TRUE)

option(SWIFT_STDLIB_SUPPORT_BACK_DEPLOYMENT
       "Support back-deployment of built binaries to older OS versions."
       TRUE)

option(SWIFT_STDLIB_SHORT_MANGLING_LOOKUPS
       "Build stdlib with fast-path context descriptor lookups based on well-known short manglings."
       TRUE)

option(SWIFT_STDLIB_ENABLE_VECTOR_TYPES
       "Build stdlib with support for SIMD and vector types"
       TRUE)

option(SWIFT_STDLIB_HAS_TYPE_PRINTING
       "Build stdlib with support for printing user-friendly type name as strings at runtime"
       TRUE)

set(SWIFT_STDLIB_TRAP_FUNCTION "" CACHE STRING
  "Name of function to call instead of emitting a trap instruction in the stdlib")

option(SWIFT_STDLIB_BUILD_PRIVATE
       "Build private part of the Standard Library."
       TRUE)

option(SWIFT_STDLIB_HAS_DLADDR
       "Build stdlib assuming the runtime environment runtime environment provides dladdr API."
       TRUE)

option(SWIFT_RUNTIME_STATIC_IMAGE_INSPECTION
       "Build stdlib assuming the runtime environment runtime environment only supports a single runtime image with Swift code."
       FALSE)

option(SWIFT_STDLIB_HAS_DARWIN_LIBMALLOC
       "Build stdlib assuming the Darwin build of stdlib can use extended libmalloc APIs"
       TRUE)

set(SWIFT_STDLIB_EXTRA_SWIFT_COMPILE_FLAGS "" CACHE STRING
    "Extra flags to pass when compiling swift stdlib files")

set(SWIFT_STDLIB_EXTRA_C_COMPILE_FLAGS "" CACHE STRING
    "Extra flags to pass when compiling C/C++ stdlib files")

option(SWIFT_STDLIB_EXPERIMENTAL_HERMETIC_SEAL_AT_LINK
       "Should stdlib be built with -experimental-hermetic-seal-at-link"
       FALSE)

option(SWIFT_STDLIB_PASSTHROUGH_METADATA_ALLOCATOR
       "Build stdlib without a custom implementation of MetadataAllocator, relying on malloc+free instead."
       FALSE)

option(SWIFT_STDLIB_DISABLE_INSTANTIATION_CACHES
       "Build stdlib with -disable-preallocated-instantiation-caches"
       FALSE)

option(SWIFT_STDLIB_HAS_COMMANDLINE
       "Build stdlib with the CommandLine enum and support for argv/argc."
       TRUE)

option(SWIFT_ENABLE_REFLECTION
  "Build stdlib with support for runtime reflection and mirrors."
  TRUE)

if(SWIFT_FREESTANDING_FLAVOR STREQUAL "apple" AND NOT SWIFT_FREESTANDING_IS_DARWIN)
  set(SWIFT_STDLIB_TASK_TO_THREAD_MODEL_CONCURRENCY_default TRUE)
else()
  set(SWIFT_STDLIB_TASK_TO_THREAD_MODEL_CONCURRENCY_default FALSE)
endif()

option(SWIFT_STDLIB_TASK_TO_THREAD_MODEL_CONCURRENCY
       "Should concurrency use the task-to-thread model."
       "${SWIFT_STDLIB_TASK_TO_THREAD_MODEL_CONCURRENCY_default}")

option(SWIFT_STDLIB_HAS_STDIN
       "Build stdlib assuming the platform supports stdin and getline API."
       TRUE)

option(SWIFT_STDLIB_HAS_ENVIRON
       "Build stdlib assuming the platform supports environment variables."
       TRUE)

option(SWIFT_STDLIB_SINGLE_THREADED_CONCURRENCY
       "Build the standard libraries assuming that they will be used in an environment with only a single thread."
       FALSE)

if(SWIFT_STDLIB_SINGLE_THREADED_CONCURRENCY)
  set(SWIFT_CONCURRENCY_GLOBAL_EXECUTOR_default "singlethreaded")
else()
  set(SWIFT_CONCURRENCY_GLOBAL_EXECUTOR_default "dispatch")
endif()

set(SWIFT_CONCURRENCY_GLOBAL_EXECUTOR
    "${SWIFT_CONCURRENCY_GLOBAL_EXECUTOR_default}" CACHE STRING
    "Build the concurrency library to use the given global executor (options: dispatch, singlethreaded, hooked)")

option(SWIFT_STDLIB_OS_VERSIONING
       "Build stdlib with availability based on OS versions (Darwin only)."
       TRUE)

option(SWIFT_FREESTANDING_FLAVOR
       "When building the FREESTANDING stdlib, which build style to use (options: apple, linux)")

set(SWIFT_STDLIB_ENABLE_LTO OFF CACHE STRING "Build Swift stdlib with LTO. One
    must specify the form of LTO by setting this to one of: 'full', 'thin'. This
    option only affects the standard library and runtime, not tools.")

if("${SWIFT_HOST_VARIANT_SDK}" IN_LIST SWIFT_DARWIN_PLATFORMS)
  set(SWIFT_STDLIB_CONCURRENCY_TRACING_default TRUE)
else()
  set(SWIFT_STDLIB_CONCURRENCY_TRACING_default FALSE)
endif()

option(SWIFT_STDLIB_CONCURRENCY_TRACING
  "Enable concurrency tracing in the runtime; assumes the presence of os_log(3)
   and the os_signpost(3) API."
  "${SWIFT_STDLIB_CONCURRENCY_TRACING_default}")
