package org.pureswift.swiftandroidsupport.view

import android.view.View

class SwiftViewOnFocusChangeListener(private val __swiftObject: Long): View.OnFocusChangeListener {

    override fun onFocusChange(v: View?, hasFocus: Boolean) {
        __onFocusChange(__swiftObject, v, hasFocus)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    private external fun __onFocusChange(__swiftObject: Long, v: View?, hasFocus: Boolean)

    private external fun __finalize(__swiftObject: Long)
}