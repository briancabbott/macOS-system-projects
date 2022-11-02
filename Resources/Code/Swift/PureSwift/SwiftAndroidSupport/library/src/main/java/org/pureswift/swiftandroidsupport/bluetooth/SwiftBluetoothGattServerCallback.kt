package org.pureswift.swiftandroidsupport.bluetooth

import android.bluetooth.*

@SuppressWarnings("JniMissingFunction")
class SwiftBluetoothGattServerCallback(private val __swiftObject: Long): BluetoothGattServerCallback() {

    override fun onCharacteristicReadRequest(device: BluetoothDevice?, requestId: Int, offset: Int, characteristic: BluetoothGattCharacteristic?) {
        super.onCharacteristicReadRequest(device, requestId, offset, characteristic)

        __onCharacteristicReadRequest(__swiftObject, device, requestId, offset, characteristic)
    }

    override fun onCharacteristicWriteRequest(device: BluetoothDevice?, requestId: Int, characteristic: BluetoothGattCharacteristic?, preparedWrite: Boolean, responseNeeded: Boolean, offset: Int, value: ByteArray?) {
        super.onCharacteristicWriteRequest(device, requestId, characteristic, preparedWrite, responseNeeded, offset, value)

        __onCharacteristicWriteRequest(__swiftObject, device, requestId, characteristic, preparedWrite, responseNeeded, offset, value)
    }

    override fun onConnectionStateChange(device: BluetoothDevice?, status: Int, newState: Int) {
        super.onConnectionStateChange(device, status, newState)

        __onConnectionStateChange(__swiftObject, device, status, newState)
    }

    override fun onDescriptorReadRequest(device: BluetoothDevice?, requestId: Int, offset: Int, descriptor: BluetoothGattDescriptor?) {
        super.onDescriptorReadRequest(device, requestId, offset, descriptor)

        __onDescriptorReadRequest(__swiftObject, device, requestId, offset, descriptor)
    }

    override fun onDescriptorWriteRequest(device: BluetoothDevice?, requestId: Int, descriptor: BluetoothGattDescriptor?, preparedWrite: Boolean, responseNeeded: Boolean, offset: Int, value: ByteArray?) {
        super.onDescriptorWriteRequest(device, requestId, descriptor, preparedWrite, responseNeeded, offset, value)

        __onDescriptorWriteRequest(__swiftObject, device, requestId, descriptor, preparedWrite, responseNeeded, offset, value)
    }

    override fun onExecuteWrite(device: BluetoothDevice?, requestId: Int, execute: Boolean) {
        super.onExecuteWrite(device, requestId, execute)

        __onExecuteWrite(__swiftObject, device, requestId, execute)
    }

    override fun onMtuChanged(device: BluetoothDevice?, mtu: Int) {
        super.onMtuChanged(device, mtu)

        __onMtuChanged(__swiftObject, device, mtu)
    }

    override fun onNotificationSent(device: BluetoothDevice?, status: Int) {
        super.onNotificationSent(device, status)

        __onNotificationSent(__swiftObject, device, status)
    }

    override fun onPhyRead(device: BluetoothDevice?, txPhy: Int, rxPhy: Int, status: Int) {
        super.onPhyRead(device, txPhy, rxPhy, status)

        __onPhyRead(__swiftObject, device, txPhy, rxPhy, status)
    }

    override fun onPhyUpdate(device: BluetoothDevice?, txPhy: Int, rxPhy: Int, status: Int) {
        super.onPhyUpdate(device, txPhy, rxPhy, status)

        __onPhyUpdate(__swiftObject, device, txPhy, rxPhy, status)
    }

    override fun onServiceAdded(status: Int, service: BluetoothGattService?) {
        super.onServiceAdded(status, service)

        __onServiceAdded(__swiftObject, status, service)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    //Native Methods

    private external fun __onCharacteristicReadRequest(__swiftObject: Long, device: BluetoothDevice?, requestId: Int, offset: Int, characteristic: BluetoothGattCharacteristic?)

    private external fun __onCharacteristicWriteRequest(__swiftObject: Long, device: BluetoothDevice?, requestId: Int, characteristic: BluetoothGattCharacteristic?, preparedWrite: Boolean, responseNeeded: Boolean, offset: Int, value: ByteArray?)

    private external fun __onConnectionStateChange(__swiftObject: Long, device: BluetoothDevice?, status: Int, newState: Int)

    private external fun __onDescriptorReadRequest(__swiftObject: Long, device: BluetoothDevice?, requestId: Int, offset: Int, descriptor: BluetoothGattDescriptor?)

    private external fun __onDescriptorWriteRequest(__swiftObject: Long, device: BluetoothDevice?, requestId: Int, descriptor: BluetoothGattDescriptor?, preparedWrite: Boolean, responseNeeded: Boolean, offset: Int, value: ByteArray?)

    private external fun __onExecuteWrite(__swiftObject: Long, device: BluetoothDevice?, requestId: Int, execute: Boolean)

    private external fun __onMtuChanged(__swiftObject: Long, device: BluetoothDevice?, mtu: Int)

    private external fun __onNotificationSent(__swiftObject: Long, device: BluetoothDevice?, status: Int)

    private external fun __onPhyRead(__swiftObject: Long, device: BluetoothDevice?, txPhy: Int, rxPhy: Int, status: Int)

    private external fun __onPhyUpdate(__swiftObject: Long, device: BluetoothDevice?, txPhy: Int, rxPhy: Int, status: Int)

    private external fun __onServiceAdded(__swiftObject: Long, status: Int, service: BluetoothGattService?)

    private external fun __finalize(__swiftObject: Long)
}