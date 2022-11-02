package org.pureswift.swiftandroidsupport.view

import android.view.View

open class SwiftOnClickListener(private val __swiftObject: Long): View.OnClickListener {

    override fun onClick(v: View?) {
        __onclick(__swiftObject)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    private external fun __onclick(__swiftObject: Long)

    private external fun __finalize(__swiftObject: Long)
}