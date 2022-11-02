
class DREraseRef {
}
@available(OSX 10.2, *)
func DREraseGetTypeID() -> CFTypeID
@available(OSX 10.2, *)
func DREraseCreate(_ device: DRDeviceRef!) -> Unmanaged<DREraseRef>!
@available(OSX 10.2, *)
func DREraseStart(_ erase: DREraseRef!) -> OSStatus
@available(OSX 10.2, *)
func DREraseCopyStatus(_ erase: DREraseRef!) -> Unmanaged<CFDictionary>!
@available(OSX 10.2, *)
let kDREraseStatusChangedNotification: CFString!
@available(OSX 10.2, *)
func DREraseGetDevice(_ erase: DREraseRef!) -> Unmanaged<DRDeviceRef>!
@available(OSX 10.2, *)
func DREraseSetProperties(_ erase: DREraseRef!, _ properties: CFDictionary!)
@available(OSX 10.2, *)
func DREraseGetProperties(_ erase: DREraseRef!) -> Unmanaged<CFDictionary>!
@available(OSX 10.2, *)
let kDREraseTypeKey: CFString!
@available(OSX 10.2, *)
let kDREraseTypeQuick: CFString!
@available(OSX 10.2, *)
let kDREraseTypeComplete: CFString!
