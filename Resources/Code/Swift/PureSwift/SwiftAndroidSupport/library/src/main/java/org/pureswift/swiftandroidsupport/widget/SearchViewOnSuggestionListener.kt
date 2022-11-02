package org.pureswift.swiftandroidsupport.widget

import android.support.v7.widget.SearchView

class SearchViewOnSuggestionListener(private val __swiftObject: Long): SearchView.OnSuggestionListener {

    override fun onSuggestionSelect(position: Int): Boolean {

        return __onSuggestionSelect(__swiftObject, position)
    }

    override fun onSuggestionClick(position: Int): Boolean {

        return __onSuggestionClick(__swiftObject, position)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    private external fun __onSuggestionSelect(__swiftObject: Long, position: Int): Boolean
    private external fun __onSuggestionClick(__swiftObject: Long, position: Int): Boolean
    private external fun __finalize(__swiftObject: Long)
}