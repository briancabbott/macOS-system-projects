package org.pureswift.swiftandroidsupport.bluetooth.le

import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult

/**
 * Created by coleman on 3/17/18.
 */

@SuppressWarnings("JniMissingFunction")
class SwiftScanCallback(private val __swiftObject: Long): ScanCallback() {

    override fun onScanResult(callbackType: Int, result: android.bluetooth.le.ScanResult?) {

        __on_scan_result(__swiftObject, callbackType, result)
    }

    override fun onBatchScanResults(results: MutableList<ScanResult>?) {

        __on_batch_scan_results(__swiftObject, results)
    }

    override fun onScanFailed(errorCode: Int) {

        __on_scan_failed(__swiftObject, errorCode)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    //Native Methods

    external fun __on_scan_result(swiftObject: Long, callbackType: Int, result: android.bluetooth.le.ScanResult?)

    external fun __on_batch_scan_results(swiftObject: Long, results: MutableList<ScanResult>?)

    external fun __on_scan_failed(swiftObject: Long, errorCode: Int)

    external fun __finalize(__swiftObject: Long)
}