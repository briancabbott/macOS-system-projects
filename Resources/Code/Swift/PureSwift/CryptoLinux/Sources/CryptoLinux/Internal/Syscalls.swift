#if canImport(Glibc)
import Glibc
#elseif canImport(Darwin)
import Darwin
#endif
import SystemPackage

@usableFromInline
internal func system_getpagesize() -> CInt {
  return getpagesize()
}
