package org.pureswift.swiftandroidsupport.widget

import android.widget.CompoundButton

class CompoundButtonOnCheckedChangeListener(private val __swiftObject: Long): CompoundButton.OnCheckedChangeListener {

    override fun onCheckedChanged(buttonView: CompoundButton?, isChecked: Boolean) {

        __onCheckedChanged(__swiftObject, buttonView, isChecked)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    private external fun __finalize(__swiftObject: Long)

    private external fun __onCheckedChanged(__swiftObject: Long, buttonView: CompoundButton?, isChecked: Boolean)
}