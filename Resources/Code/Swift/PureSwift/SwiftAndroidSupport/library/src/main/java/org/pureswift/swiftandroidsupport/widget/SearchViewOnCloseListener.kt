package org.pureswift.swiftandroidsupport.widget

import android.support.v7.widget.SearchView

public class SearchViewOnCloseListener(private val __swiftObject: Long): SearchView.OnCloseListener {

    override fun onClose(): Boolean {
        return __onClose(__swiftObject)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    private external fun __onClose(__swiftObject: Long): Boolean
    private external fun __finalize(__swiftObject: Long)
}