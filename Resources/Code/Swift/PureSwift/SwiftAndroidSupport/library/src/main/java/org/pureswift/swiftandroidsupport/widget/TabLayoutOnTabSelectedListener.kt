package org.pureswift.swiftandroidsupport.widget

import android.support.design.widget.TabLayout

class TabLayoutOnTabSelectedListener(private val __swiftObject: Long): TabLayout.OnTabSelectedListener {

    override fun onTabReselected(tab: TabLayout.Tab?) {
        __onTabReselected(__swiftObject, tab)
    }

    override fun onTabUnselected(tab: TabLayout.Tab?) {
        __onTabUnselected(__swiftObject, tab)
    }

    override fun onTabSelected(tab: TabLayout.Tab?) {
        __onTabSelected(__swiftObject, tab)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    private external fun __finalize(__swiftObject: Long)

    private external fun __onTabReselected(__swiftObject: Long, tab: TabLayout.Tab?)
    private external fun __onTabUnselected(__swiftObject: Long, tab: TabLayout.Tab?)
    private external fun __onTabSelected(__swiftObject: Long, tab: TabLayout.Tab?)
}