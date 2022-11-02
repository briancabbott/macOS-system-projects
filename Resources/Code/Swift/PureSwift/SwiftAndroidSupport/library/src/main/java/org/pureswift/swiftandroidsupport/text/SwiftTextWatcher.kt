package org.pureswift.swiftandroidsupport.text

import android.text.Editable
import android.text.TextWatcher

class SwiftTextWatcher(private val __swiftObject: Long): TextWatcher {

    override fun afterTextChanged(s: Editable?) {

        __afterTextChanged(__swiftObject, s)
    }

    override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {
        __beforeTextChanged(__swiftObject, s, start, count, after)
    }

    override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {

        __onTextChanged(__swiftObject, s, start, before, count)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    private external fun __afterTextChanged(__swiftObject: Long, s: Editable?)
    private external fun __beforeTextChanged(__swiftObject: Long, s: CharSequence?, start: Int, count: Int, after: Int)
    private external fun __onTextChanged(__swiftObject: Long, s: CharSequence?, start: Int, before: Int, count: Int)
    private external fun __finalize(__swiftObject: Long)
}