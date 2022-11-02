package org.pureswift.swiftandroidsupport.bluetooth.le

import android.bluetooth.le.AdvertiseCallback
import android.bluetooth.le.AdvertiseSettings

@SuppressWarnings("JniMissingFunction")
class SwiftAdvertiseCallback(private val __swiftObject: Long): AdvertiseCallback() {

    override fun onStartSuccess(settingsInEffect: AdvertiseSettings?) {
        super.onStartSuccess(settingsInEffect)

        __on_start_success(__swiftObject, settingsInEffect)
    }

    override fun onStartFailure(errorCode: Int) {
        super.onStartFailure(errorCode)
        __on_start_failure(__swiftObject, errorCode)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    //Native Methods

    external fun __on_start_success(swiftObject: Long, settingsInEffect: AdvertiseSettings?)

    external fun __on_start_failure(swiftObject: Long, errorCode: Int)

    external fun __finalize(__swiftObject: Long)
}