/*
 This source file is part of the Swift System open source project

 Copyright (c) 2020 Apple Inc. and the Swift System project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
*/

// MARK: - Public typealiases

/// The C `mode_t` type.
/*System 0.0.1, @available(macOS 11.0, iOS 14.0, watchOS 7.0, tvOS 14.0, *)*/
@available(*, deprecated, renamed: "CInterop.Mode")
public typealias CModeT =  CInterop.Mode

#if os(macOS) || os(iOS) || os(watchOS) || os(tvOS)
import Darwin
#elseif os(Linux) || os(FreeBSD) || os(Android)
import CSystem
import Glibc
#elseif os(Windows)
import CSystem
import ucrt
#else
#error("Unsupported Platform")
#endif

/// A namespace for C and platform types
/*System 0.0.2, @available(macOS 12.0, iOS 15.0, watchOS 8.0, tvOS 15.0, *)*/
public enum CInterop {
#if os(Windows)
  public typealias Mode = CInt
#else
  public typealias Mode = mode_t
#endif

  /// The C `char` type
  public typealias Char = CChar

  #if os(Windows)
  /// The platform's preferred character type. On Unix, this is an 8-bit C
  /// `char` (which may be signed or unsigned, depending on platform). On
  /// Windows, this is `UInt16` (a "wide" character).
  public typealias PlatformChar = UInt16
  #else
  /// The platform's preferred character type. On Unix, this is an 8-bit C
  /// `char` (which may be signed or unsigned, depending on platform). On
  /// Windows, this is `UInt16` (a "wide" character).
  public typealias PlatformChar = CInterop.Char
  #endif
  
  #if os(Windows)
  /// The platform's preferred Unicode encoding. On Unix this is UTF-8 and on
  /// Windows it is UTF-16. Native strings may contain invalid Unicode,
  /// which will be handled by either error-correction or failing, depending
  /// on API.
  public typealias PlatformUnicodeEncoding = UTF16
  #else
  /// The platform's preferred Unicode encoding. On Unix this is UTF-8 and on
  /// Windows it is UTF-16. Native strings may contain invalid Unicode,
  /// which will be handled by either error-correction or failing, depending
  /// on API.
  public typealias PlatformUnicodeEncoding = UTF8
  #endif
    
  /// The C `clock_t` type
  public typealias Clock = clock_t
  
  /// The C `clockid_t` type
  public typealias ClockID = clockid_t
    
  /// The C `time_t` type
  public typealias Time = time_t
    
  /// The C `tm` type
  public typealias TimeComponents = tm
  
  /// The C `timeval` type
  public typealias TimeIntervalMicroseconds = timeval
    
  /// The C `timespec` type
  public typealias TimeIntervalNanoseconds = timespec
  
  #if os(OSX) || os(iOS) || os(watchOS) || os(tvOS)
  public typealias Microseconds = __darwin_suseconds_t
  #elseif os(Linux)
  public typealias Microseconds = __suseconds_t
  #endif
  
  public typealias Nanoseconds = CLong
  
  public typealias UserID = uid_t

  /// The platform file descriptor set.
  public typealias FileDescriptorSet = fd_set
  
  public typealias PollFileDescriptor = pollfd
  
  public typealias FileDescriptorCount = nfds_t
  
  public typealias FileEvent = Int16
    
  public typealias Signal = CInt
    
  public typealias SignalSet = sigset_t
  
  #if os(macOS) || os(iOS) || os(watchOS) || os(tvOS)
  public typealias SignalHandler = sig_t
  #elseif os(Linux)
  public typealias SignalHandler = __sighandler_t
  #elseif os(Windows)
  #endif
    
  public typealias SignalAction = sigaction
    
  public typealias SignalInformation = siginfo_t
    
  public typealias SignalValue = sigval
    
  public typealias SignalActionHandler = (@convention(c) (Int32, UnsafeMutablePointer<CInterop.SignalInformation>?, UnsafeMutableRawPointer?) -> Void)
  
  /// The platform process identifier.
  public typealias ProcessID = pid_t
  
  #if os(macOS) || os(iOS) || os(watchOS) || os(tvOS)
  public typealias ProcessTaskInfo = proc_taskinfo
  #endif
  
  #if os(Linux)
  public typealias ResourceUsageInfo = rusage
  #elseif os(macOS) || os(iOS) || os(watchOS) || os(tvOS)
  public typealias ResourceUsageInfo = rusage_info_current
  #elseif os(Windows)
  #endif

  #if os(Windows)
  /// The platform socket descriptor.
  public typealias SocketDescriptor = SOCKET
  #else
  /// The platform socket descriptor, which is the same as a file desciptor on Unix systems.
  public typealias SocketDescriptor = CInt
  #endif

    /// The C `msghdr` type
  public typealias MessageHeader = msghdr
  
  /// The C `sa_family_t` type
  public typealias SocketAddressFamily = sa_family_t

  /// Socket Type
  #if os(Linux)
  public typealias SocketType = __socket_type
  #else
  public typealias SocketType = CInt
  #endif
    
  /// The C `addrinfo` type
  public typealias AddressInfo = addrinfo
    
  /// The C `in_addr` type
  public typealias IPv4Address = in_addr
    
  /// The C `in6_addr` type
  public typealias IPv6Address = in6_addr
    
  /// The C `sockaddr_in` type
  public typealias SocketAddress = sockaddr
    
  /// The C `sockaddr_in` type
  public typealias UnixSocketAddress = sockaddr_un
  
  /// The C `sockaddr_in` type
  public typealias IPv4SocketAddress = sockaddr_in
    
  /// The C `sockaddr_in6` type
  public typealias IPv6SocketAddress = sockaddr_in6
}
