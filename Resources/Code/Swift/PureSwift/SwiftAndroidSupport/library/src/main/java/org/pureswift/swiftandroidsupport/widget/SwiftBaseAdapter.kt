package org.pureswift.swiftandroidsupport.widget

import android.view.View
import android.view.ViewGroup
import android.widget.BaseAdapter

/**
 * Created by coleman on 3/18/18.
 */
open class SwiftBaseAdapter(private val __swiftObject: Long) : BaseAdapter() {
    
    override fun getCount(): Int {

        return __get_count(__swiftObject)
    }

    override fun getItem(position: Int): Any? {

        return __get_item(__swiftObject, position)
    }

    override fun getItemId(position: Int): Long {

        return position.toLong()
    }

    override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {

        return __get_view(__swiftObject, position, convertView, parent)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    //Natives methods

    private external fun __get_count(__swiftObject: Long): Int

    private external fun __get_item(__swiftObject: Long, position: Int): Any?

    private external fun __get_view(__swiftObject: Long, position: Int, convertView: View?, parent: ViewGroup): View

    private external fun __finalize(__swiftObject: Long)
}