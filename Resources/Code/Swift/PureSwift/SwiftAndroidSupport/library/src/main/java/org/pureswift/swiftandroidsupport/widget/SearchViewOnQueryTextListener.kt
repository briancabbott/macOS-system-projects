package org.pureswift.swiftandroidsupport.widget

import android.support.v7.widget.SearchView

public class SearchViewOnQueryTextListener(private val __swiftObject: Long): SearchView.OnQueryTextListener {


    override fun onQueryTextSubmit(query: String?): Boolean {

        return __onQueryTextSubmit(__swiftObject, query)
    }

    override fun onQueryTextChange(newText: String?): Boolean {

        return onQueryTextChange(__swiftObject, newText)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    private external fun __onQueryTextSubmit(__swiftObject: Long, query: String?): Boolean
    private external fun onQueryTextChange(__swiftObject: Long, newText: String?): Boolean
    private external fun __finalize(__swiftObject: Long)
}