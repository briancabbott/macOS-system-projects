package org.pureswift.swiftandroidsupport.content

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

class SwiftBroadcastReceiver(private val __swiftObject: Long): BroadcastReceiver() {

    override fun onReceive(context: Context?, intent: Intent?) {

        __onReceive(__swiftObject, context, intent)
    }

    fun finalize() {

        __finalize(__swiftObject)
    }

    private external fun __onReceive(__swiftObject: Long, context: Context?, intent: Intent?)

    private external fun __finalize(__swiftObject: Long)
}