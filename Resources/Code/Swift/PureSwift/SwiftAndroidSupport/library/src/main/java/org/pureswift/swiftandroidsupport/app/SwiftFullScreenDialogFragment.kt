package org.pureswift.swiftandroidsupport.app

import android.app.Dialog
import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.support.v4.app.DialogFragment
import android.support.v4.app.FragmentManager
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.RelativeLayout

class SwiftFullScreenDialogFragment: DialogFragment() {

    internal var __swiftObject: Long = 0L

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        onCreateNative(__swiftObject, savedInstanceState)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        Log.v("dialog", "onCreateView 1")
        val view =  onCreateViewNative(__swiftObject, inflater, container, savedInstanceState)
        Log.v("dialog", "onCreateView 2")
        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        onViewCreatedNative(__swiftObject, view, savedInstanceState)
    }

    override fun onStart() {
        super.onStart()
        onStartNative(__swiftObject)
    }

    override fun onResume() {
        super.onResume()

        onResumeNative(__swiftObject)
    }

    override fun onPause() {
        super.onPause()
        onPauseNative(__swiftObject)
    }

    override fun onStop() {
        super.onStop()
        onStopNative(__swiftObject)
    }

    override fun onDestroy() {
        super.onDestroy()
        onDestroyNative(__swiftObject)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        onDestroyViewNative(__swiftObject)
    }

    fun finalize() {
        finalizeNative(__swiftObject)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        // the content
        val root = RelativeLayout(activity)
        root.layoutParams = ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)

        // creating the fullscreen dialog
        val dialog = Dialog(activity!!)
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE)
        dialog.setContentView(root)
        dialog.window!!.setBackgroundDrawable(ColorDrawable(Color.WHITE))

        dialog.window!!.setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)

        return dialog
    }

    override fun show(manager: FragmentManager?, tag: String?) {
        super.show(manager, tag)
    }

    override fun getContext(): Context? {
        return super.getContext()
    }

    private external fun onCreateNative(__swiftObject: Long, savedInstanceState: Bundle?)

    private external fun onCreateViewNative(__swiftObject: Long, inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?

    private external fun onViewCreatedNative(__swiftObject: Long, view: View, savedInstanceState: Bundle?)

    private external fun onStartNative(__swiftObject: Long)

    private external fun onResumeNative(__swiftObject: Long)

    private external fun onPauseNative(__swiftObject: Long)

    private external fun onStopNative(__swiftObject: Long)

    private external fun onDestroyNative(__swiftObject: Long)

    private external fun onDestroyViewNative(__swiftObject: Long)

    private external fun finalizeNative(__swiftObject: Long)
}