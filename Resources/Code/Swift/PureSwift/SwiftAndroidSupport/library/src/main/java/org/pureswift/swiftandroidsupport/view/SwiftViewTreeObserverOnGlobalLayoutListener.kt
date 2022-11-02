package org.pureswift.swiftandroidsupport.view

import android.view.ViewTreeObserver

class SwiftViewTreeObserverOnGlobalLayoutListener(private val __swiftObject: Long): ViewTreeObserver.OnGlobalLayoutListener {

    override fun onGlobalLayout() {
        __onGlobalLayout(__swiftObject)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    private external fun __onGlobalLayout(__swiftObject: Long)

    private external fun __finalize(__swiftObject: Long)
}