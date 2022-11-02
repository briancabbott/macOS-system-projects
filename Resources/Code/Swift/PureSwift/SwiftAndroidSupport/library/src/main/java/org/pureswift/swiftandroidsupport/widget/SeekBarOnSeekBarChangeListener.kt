package org.pureswift.swiftandroidsupport.widget

import android.widget.SeekBar

class SeekBarOnSeekBarChangeListener(private val __swiftObject: Long): SeekBar.OnSeekBarChangeListener {

    // Notification that the progress level has changed.
    override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
        __onProgressChanged(__swiftObject, seekBar, progress, fromUser)
    }

    override fun onStartTrackingTouch(seekBar: SeekBar?) {
        __onStartTrackingTouch(__swiftObject, seekBar)
    }

    override fun onStopTrackingTouch(seekBar: SeekBar?) {
        __onStopTrackingTouch(__swiftObject, seekBar)
    }

    fun finalize() {
        __finalize(__swiftObject)
    }

    private external fun __onProgressChanged(__swiftObject: Long, seekBar: SeekBar?, progress: Int, fromUser: Boolean)
    private external fun __onStartTrackingTouch(__swiftObject: Long, seekBar: SeekBar?)
    private external fun __onStopTrackingTouch(__swiftObject: Long, seekBar: SeekBar?)
    private external fun __finalize(__swiftObject: Long)

}