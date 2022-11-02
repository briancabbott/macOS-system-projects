/*
 This source file is part of the Swift System open source project

 Copyright (c) 2021 Apple Inc. and the Swift System project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
*/

public extension Signal {
    
    /// Signal Information
    @frozen
    struct Information {
        
        /// Signal
        public let id: Signal
        
        /// Error
        public let error: Errno?
        
        public let code: Int32

        #if os(macOS)
        public let process: ProcessID

        public let user: UserID

        public let status: Int32
        
        public let address: UnsafeMutableRawPointer?

        public let value: CInterop.SignalValue

        public let band: Int
        #endif
        
        public init(_ bytes: CInterop.SignalInformation) {
            self.id = Signal(rawValue: bytes.si_signo)
            self.error = bytes.si_errno == 0 ? nil : Errno(rawValue: bytes.si_errno)
            self.code = bytes.si_code
            #if os(macOS)
            self.process = .init(rawValue: bytes.si_pid)
            self.user = .init(rawValue: bytes.si_uid)
            self.status = bytes.si_status
            self.address = bytes.si_addr
            self.value = bytes.si_value
            self.band = bytes.si_band
            #elseif os(Linux)
            
            #endif
        }
    }
}
