package org.pureswift.swiftandroidsupport.widget

import android.support.design.widget.BottomNavigationView
import android.view.MenuItem

class BottomNavigationViewOnNavigationItemSelectedListener(private val __swiftObject: Long) : BottomNavigationView.OnNavigationItemSelectedListener {

    override fun onNavigationItemSelected(item: MenuItem): Boolean {
        return __onNavigationItemSelected(__swiftObject, item)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    private external fun __finalize(__swiftObject: Long)

    private external fun __onNavigationItemSelected(__swiftObject: Long, item: MenuItem): Boolean

}