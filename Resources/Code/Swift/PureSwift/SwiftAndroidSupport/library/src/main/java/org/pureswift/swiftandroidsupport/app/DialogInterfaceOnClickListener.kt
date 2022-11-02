package org.pureswift.swiftandroidsupport.app

import android.content.DialogInterface

class DialogInterfaceOnClickListener(private val __swiftObject: Long): DialogInterface.OnClickListener {

    override fun onClick(dialog: DialogInterface?, which: Int) {
        __onClick(__swiftObject, dialog, which)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    private external fun __finalize(__swiftObject: Long)

    private external fun __onClick(__swiftObject: Long, dialog: DialogInterface?, which: Int)
}