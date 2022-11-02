/*
 This source file is part of the Swift System open source project

 Copyright (c) 2021 Apple Inc. and the Swift System project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
*/

/// POSIX Signal
@frozen
public struct Signal: RawRepresentable, Hashable {
    
    public let rawValue: CInt
    
    @_alwaysEmitIntoClient
    public init(rawValue: CInt) { self.rawValue = rawValue }
    
    @_alwaysEmitIntoClient
    private init(_ rawValue: CInt) { self.init(rawValue: rawValue) }
}

public extension Signal {

  /// SIGHUP (1): terminal line hangup (default behavior: terminate process)
  @_alwaysEmitIntoClient
  static var hangup: Signal { Signal(_SIGHUP) }

  /// SIGINT (2): interrupt program (default behavior: terminate process)
  @_alwaysEmitIntoClient
  static var interrupt: Signal { Signal(_SIGINT) }

  /// SIGQUIT (3): quit program (default behavior: create core image)
  @_alwaysEmitIntoClient
  static var quit: Signal { Signal(_SIGQUIT) }

  /// SIGILL (4): illegal instruction (default behavior: create core image)
  @_alwaysEmitIntoClient
  static var illegalInstruction: Signal { Signal(_SIGILL) }

  /// SIGTRAP (5): trace trap (default behavior: create core image)
  @_alwaysEmitIntoClient
  static var traceTrap: Signal { Signal(_SIGTRAP) }

  /// SIGABRT (6): abort program (formerly SIGIOT) (default behavior: create core image)
  @_alwaysEmitIntoClient
  static var abort: Signal { Signal(_SIGABRT) }

  /// SIGFPE (8): floating-point exception (default behavior: create core image)
  @_alwaysEmitIntoClient
  static var floatingPointException: Signal { Signal(_SIGFPE) }

  /// SIGKILL (9): kill program (default behavior: terminate process)
  @_alwaysEmitIntoClient
  static var kill: Signal { Signal(_SIGKILL) }

  /// SIGBUS (10): bus error (default behavior: create core image)
  @_alwaysEmitIntoClient
  static var busError: Signal { Signal(_SIGBUS) }

  /// SIGSEGV (11): segmentation violation (default behavior: create core image)
  @_alwaysEmitIntoClient
  static var segmentationViolation: Signal { Signal(_SIGSEGV) }

  /// SIGSYS (12): non-existent system call invoked (default behavior: create core image)
  @_alwaysEmitIntoClient
  static var unknownSystemCall: Signal { Signal(_SIGSYS) }

  /// SIGPIPE (13): write on a pipe with no reader (default behavior: terminate process)
  @_alwaysEmitIntoClient
  static var brokenPipe: Signal { Signal(_SIGPIPE) }

  /// SIGALRM (14): real-time timer expired (default behavior: terminate process)
  @_alwaysEmitIntoClient
  static var alarm: Signal { Signal(_SIGALRM) }

  /// SIGTERM (15): software termination signal (default behavior: terminate process)
  @_alwaysEmitIntoClient
  static var terminate: Signal { Signal(_SIGTERM) }

  /// SIGURG (16): urgent condition present on socket (default behavior: discard signal)
  @_alwaysEmitIntoClient
  static var urgentCondition: Signal { Signal(_SIGURG) }

  /// SIGSTOP (17): stop (cannot be caught or ignored) (default behavior: stop process)
  @_alwaysEmitIntoClient
  static var stop: Signal { Signal(_SIGSTOP) }

  /// SIGTSTP (18): stop signal generated from keyboard (default behavior: stop process)
  @_alwaysEmitIntoClient
  static var temporaryStop: Signal { Signal(_SIGTSTP) }

  /// SIGCONT (19): continue after stop (default behavior: discard signal)
  @_alwaysEmitIntoClient
  static var `continue`: Signal { Signal(_SIGCONT) }

  /// SIGCHLD (20): child status has changed (default behavior: discard signal)
  @_alwaysEmitIntoClient
  static var childProcessStatusChange: Signal { Signal(_SIGCHLD) }

  /// SIGTTIN (21): background read attempted from control terminal (default behavior: stop process)
  @_alwaysEmitIntoClient
  static var backgroundReadFromControllingTerminal: Signal { Signal(_SIGTTIN) }

  /// SIGTTOU (22): background write attempted to control terminal (default behavior: stop process)
  @_alwaysEmitIntoClient
  static var backgroundWriteToControllingTerminal: Signal { Signal(_SIGTTOU) }

  /// SIGIO (23): I/O is possible on a descriptor (see fcntl(2)) (default behavior: discard signal)
  @_alwaysEmitIntoClient
  static var ioAvailable: Signal { Signal(_SIGIO) }

  /// SIGXCPU (24): cpu time limit exceeded (see setrlimit(2)) (default behavior: terminate process)
  @_alwaysEmitIntoClient
  static var cpuLimitExceeded: Signal { Signal(_SIGXCPU) }

  /// SIGXFSZ (25): file size limit exceeded (see setrlimit(2)) (default behavior: terminate process)
  @_alwaysEmitIntoClient
  static var fileSizeLimitExceeded: Signal { Signal(_SIGXFSZ) }

  /// SIGVTALRM (26): virtual time alarm (see setitimer(2)) (default behavior: terminate process)
  @_alwaysEmitIntoClient
  static var virtualAlarm: Signal { Signal(_SIGVTALRM) }

  /// SIGPROF (27): profiling timer alarm (see setitimer(2)) (default behavior: terminate process)
  @_alwaysEmitIntoClient
  static var profilingAlarm: Signal { Signal(_SIGPROF) }

  /// SIGWINCH (28): Window size change (default behavior: discard signal)
  @_alwaysEmitIntoClient
  static var windowSizeChange: Signal { Signal(_SIGWINCH) }

  /// SIGUSR1 (30): User defined signal 1 (default behavior: terminate process)
  @_alwaysEmitIntoClient
  static var user1: Signal { Signal(_SIGUSR1) }

  /// SIGUSR2 (31): User defined signal 2 (default behavior: terminate process)
  @_alwaysEmitIntoClient
  static var user2: Signal { Signal(_SIGUSR2) }

  #if os(macOS) || os(iOS) || os(watchOS) || os(tvOS)
  /// SIGINFO (29): status request from keyboard (default behavior: discard signal)
  @_alwaysEmitIntoClient
  static var info: Signal { Signal(_SIGINFO) }

  /// SIGEMT (7): emulate instruction executed (default behavior: create core image)
  @_alwaysEmitIntoClient
  static var emulatorTrap: Signal { Signal(_SIGEMT) }
  #endif
}
