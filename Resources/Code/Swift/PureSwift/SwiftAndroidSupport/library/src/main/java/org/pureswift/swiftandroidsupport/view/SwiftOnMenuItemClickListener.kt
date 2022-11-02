package org.pureswift.swiftandroidsupport.view

import android.support.v7.widget.Toolbar
import android.view.MenuItem

class SwiftOnMenuItemClickListener(private val __swiftObject: Long): Toolbar.OnMenuItemClickListener {

    override fun onMenuItemClick(item: MenuItem?): Boolean {
        return __onMenuItemClick(__swiftObject, item)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    private external fun __finalize(__swiftObject: Long)

    private external fun __onMenuItemClick(__swiftObject: Long, item: MenuItem?): Boolean
}