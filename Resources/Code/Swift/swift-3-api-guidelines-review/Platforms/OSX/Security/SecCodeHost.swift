
var kSecCSDedicatedHost: UInt32 { get }
var kSecCSGenerateGuestHash: UInt32 { get }
func SecHostCreateGuest(_ host: SecGuestRef, _ status: UInt32, _ path: CFURL, _ attributes: CFDictionary?, _ flags: SecCSFlags, _ newGuest: UnsafeMutablePointer<SecGuestRef>) -> OSStatus
func SecHostRemoveGuest(_ host: SecGuestRef, _ guest: SecGuestRef, _ flags: SecCSFlags) -> OSStatus
func SecHostSelectGuest(_ guestRef: SecGuestRef, _ flags: SecCSFlags) -> OSStatus
func SecHostSelectedGuest(_ flags: SecCSFlags, _ guestRef: UnsafeMutablePointer<SecGuestRef>) -> OSStatus
func SecHostSetGuestStatus(_ guestRef: SecGuestRef, _ status: UInt32, _ attributes: CFDictionary?, _ flags: SecCSFlags) -> OSStatus
func SecHostSetHostingPort(_ hostingPort: mach_port_t, _ flags: SecCSFlags) -> OSStatus
