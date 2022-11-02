package org.pureswift.swiftandroidsupport.bluetooth

import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor

@SuppressWarnings("JniMissingFunction")
class SwiftBluetoothGattCallback(private val __swiftObject: Long): BluetoothGattCallback() {

    override fun onCharacteristicChanged(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?) {
        super.onCharacteristicChanged(gatt, characteristic)

        __onCharacteristicChanged(__swiftObject, gatt, characteristic)
    }

    override fun onCharacteristicRead(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?, status: Int) {
        super.onCharacteristicRead(gatt, characteristic, status)

        __onCharacteristicRead(__swiftObject, gatt, characteristic, status)
    }

    override fun onCharacteristicWrite(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?, status: Int) {
        super.onCharacteristicWrite(gatt, characteristic, status)

        __onCharacteristicWrite(__swiftObject, gatt, characteristic, status)
    }

    override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
        super.onConnectionStateChange(gatt, status, newState)

        __onConnectionStateChange(__swiftObject, gatt, status, newState)
    }

    override fun onDescriptorRead(gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int) {
        super.onDescriptorRead(gatt, descriptor, status)

        __onDescriptorRead(__swiftObject, gatt, descriptor, status)
    }

    override fun onDescriptorWrite(gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int) {
        super.onDescriptorWrite(gatt, descriptor, status)

        __onDescriptorWrite(__swiftObject, gatt, descriptor, status)
    }

    override fun onMtuChanged(gatt: BluetoothGatt?, mtu: Int, status: Int) {
        super.onMtuChanged(gatt, mtu, status)

        __onMtuChanged(__swiftObject, gatt, mtu, status)
    }

    override fun onPhyRead(gatt: BluetoothGatt?, txPhy: Int, rxPhy: Int, status: Int) {
        super.onPhyRead(gatt, txPhy, rxPhy, status)

        __onPhyRead(__swiftObject, gatt, txPhy, rxPhy, status)
    }

    override fun onPhyUpdate(gatt: BluetoothGatt?, txPhy: Int, rxPhy: Int, status: Int) {
        super.onPhyUpdate(gatt, txPhy, rxPhy, status)

        __onPhyUpdate(__swiftObject, gatt, txPhy, rxPhy, status)
    }

    override fun onReadRemoteRssi(gatt: BluetoothGatt?, rssi: Int, status: Int) {
        super.onReadRemoteRssi(gatt, rssi, status)

        __onReadRemoteRssi(__swiftObject, gatt, rssi, status)
    }

    override fun onReliableWriteCompleted(gatt: BluetoothGatt?, status: Int) {
        super.onReliableWriteCompleted(gatt, status)

        __onReliableWriteCompleted(__swiftObject, gatt, status)
    }

    override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
        super.onServicesDiscovered(gatt, status)

        __onServicesDiscovered(__swiftObject, gatt, status)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    //Native methods

    private external fun __onCharacteristicChanged(__swiftObject: Long, gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?)

    private external fun __onCharacteristicRead(__swiftObject: Long, gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?, status: Int)

    private external fun __onCharacteristicWrite(__swiftObject: Long, gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?, status: Int)

    private external fun __onConnectionStateChange(__swiftObject: Long, gatt: BluetoothGatt?, status: Int, newState: Int)

    private external fun __onDescriptorRead(__swiftObject: Long, gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int)

    private external fun __onDescriptorWrite(__swiftObject: Long, gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int)

    private external fun __onMtuChanged(__swiftObject: Long, gatt: BluetoothGatt?, mtu: Int, status: Int)

    private external fun __onPhyRead(__swiftObject: Long, gatt: BluetoothGatt?, txPhy: Int, rxPhy: Int, status: Int)

    private external fun __onPhyUpdate(__swiftObject: Long, gatt: BluetoothGatt?, txPhy: Int, rxPhy: Int, status: Int)

    private external fun __onReadRemoteRssi(__swiftObject: Long, gatt: BluetoothGatt?, rssi: Int, status: Int)

    private external  fun __onReliableWriteCompleted(__swiftObject: Long, gatt: BluetoothGatt?, status: Int)

    private external  fun __onServicesDiscovered(__swiftObject: Long, gatt: BluetoothGatt?, status: Int)

    private external fun __finalize(__swiftObject: Long)
}