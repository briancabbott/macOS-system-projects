package org.pureswift.swiftandroidsupport.widget

import android.support.design.widget.TabLayout

class TabLayoutOnPageChangeListener(private val __swiftObject: Long, tabLayout: TabLayout): TabLayout.TabLayoutOnPageChangeListener(tabLayout) {

    override fun onPageScrollStateChanged(state: Int) {
        super.onPageScrollStateChanged(state)

        __onPageScrollStateChanged(__swiftObject, state)
    }

    override fun onPageScrolled(position: Int, positionOffset: Float, positionOffsetPixels: Int) {
        super.onPageScrolled(position, positionOffset, positionOffsetPixels)

        __onPageScrolled(__swiftObject, position, positionOffset, positionOffsetPixels)
    }

    override fun onPageSelected(position: Int) {
        super.onPageSelected(position)

        __onPageSelected(__swiftObject, position)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    private external fun __finalize(__swiftObject: Long)

    private external fun __onPageScrollStateChanged(__swiftObject: Long, state: Int)
    private external fun __onPageScrolled(__swiftObject: Long, position: Int, positionOffset: Float, positionOffsetPixels: Int)
    private external fun __onPageSelected(__swiftObject: Long, position: Int)
}