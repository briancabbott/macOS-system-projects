package org.pureswift.swiftandroidsupport.bluetooth.le

import android.bluetooth.le.AdvertisingSet
import android.bluetooth.le.AdvertisingSetCallback
import android.os.Build
import android.support.annotation.RequiresApi

@SuppressWarnings("JniMissingFunction")
@RequiresApi(Build.VERSION_CODES.O)
class SwiftAdvertisingSetCallback(val __swiftObject: Long): AdvertisingSetCallback(){

    override fun onAdvertisingDataSet(advertisingSet: AdvertisingSet?, status: Int) {
        super.onAdvertisingDataSet(advertisingSet, status)
        __on_advertising_dataSet(__swiftObject, advertisingSet, status)
    }

    override fun onAdvertisingEnabled(advertisingSet: AdvertisingSet?, enable: Boolean, status: Int) {
        super.onAdvertisingEnabled(advertisingSet, enable, status)
        __on_advertising_enabled(__swiftObject, advertisingSet, enable, status)
    }

    override fun onAdvertisingParametersUpdated(advertisingSet: AdvertisingSet?, txPower: Int, status: Int) {
        super.onAdvertisingParametersUpdated(advertisingSet, txPower, status)
        __on_avertising_parameters_updated(__swiftObject, advertisingSet, txPower, status)
    }

    override fun onAdvertisingSetStarted(advertisingSet: AdvertisingSet?, txPower: Int, status: Int) {
        super.onAdvertisingSetStarted(advertisingSet, txPower, status)
        __on_advertising_set_started(__swiftObject, advertisingSet, txPower, status)
    }

    override fun onAdvertisingSetStopped(advertisingSet: AdvertisingSet?) {
        super.onAdvertisingSetStopped(advertisingSet)
        __on_advertising_set_stopped(__swiftObject, advertisingSet)
    }

    override fun onPeriodicAdvertisingDataSet(advertisingSet: AdvertisingSet?, status: Int) {
        super.onPeriodicAdvertisingDataSet(advertisingSet, status)
        __on_periodic_advertising_dataSet(__swiftObject, advertisingSet, status)
    }

    override fun onPeriodicAdvertisingEnabled(advertisingSet: AdvertisingSet?, enable: Boolean, status: Int) {
        super.onPeriodicAdvertisingEnabled(advertisingSet, enable, status)
        __on_periodic_advertising_enabled(__swiftObject, advertisingSet, enable, status)
    }

    override fun onPeriodicAdvertisingParametersUpdated(advertisingSet: AdvertisingSet?, status: Int) {
        super.onPeriodicAdvertisingParametersUpdated(advertisingSet, status)
        __on_periodic_advertising_parameters_updated(__swiftObject, advertisingSet, status)
    }

    override fun onScanResponseDataSet(advertisingSet: AdvertisingSet?, status: Int) {
        super.onScanResponseDataSet(advertisingSet, status)
        __on_scan_response_dataSet(__swiftObject, advertisingSet, status)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    //Native Methods

    external fun __on_advertising_dataSet(swiftObject: Long, advertisingSet: AdvertisingSet?, status: Int)

    external fun __on_advertising_enabled(swiftObject: Long, advertisingSet: AdvertisingSet?, enable: Boolean, status: Int)

    external fun __on_avertising_parameters_updated(swiftObject: Long, advertisingSet: AdvertisingSet?, txPower: Int, status: Int)

    external fun __on_advertising_set_started(swiftObject: Long, advertisingSet: AdvertisingSet?, txPower: Int, status: Int)

    external fun __on_advertising_set_stopped(swiftObject: Long, advertisingSet: AdvertisingSet?)

    external fun __on_periodic_advertising_dataSet(swiftObject: Long, advertisingSet: AdvertisingSet?, status: Int)

    external fun __on_periodic_advertising_enabled(swiftObject: Long, advertisingSet: AdvertisingSet?, enable: Boolean, status: Int)

    external fun __on_periodic_advertising_parameters_updated(swiftObject: Long, advertisingSet: AdvertisingSet?, status: Int)

    external fun __on_scan_response_dataSet(swiftObject: Long, advertisingSet: AdvertisingSet?, status: Int)

    external fun __finalize(__swiftObject: Long)
}