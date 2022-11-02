package org.pureswift.swiftandroidsupport.bluetooth

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice

class SwiftBluetoothAdapter {

    class LeScanCallback(private val __swiftObject: Long): BluetoothAdapter.LeScanCallback {

        override fun onLeScan(device: BluetoothDevice?, rssi: Int, scanRecord: ByteArray?) {
            __on_le_scan(__swiftObject, device, rssi, scanRecord)
        }

        fun finalize() {
            __finalize(__swiftObject)
        }

        private external fun __on_le_scan(__swiftObject: Long, device: BluetoothDevice?, rssi: Int, scanRecord: ByteArray?)

        private external fun __finalize(__swiftObject: Long)
    }
}