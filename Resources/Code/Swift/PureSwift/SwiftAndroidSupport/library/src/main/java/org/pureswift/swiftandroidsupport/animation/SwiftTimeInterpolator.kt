package org.pureswift.swiftandroidsupport.animation

import android.animation.TimeInterpolator
import android.util.Log

/** A time interpolator defines the rate of change of an animation.
 * This allows animations to have non-linear motion, such as acceleration and deceleration.
 * */
open class SwiftTimeInterpolator(private val __swiftObject: Long): TimeInterpolator {

    override fun getInterpolation(input: Float): Float {
        return __getInterpolation(__swiftObject, input)
    }

    fun finalize() {
        Log.e("Swift","finalize()")
        __finalize(__swiftObject)
    }

    private external fun __getInterpolation(__swiftObject: Long, input: Float): Float
    private external fun __finalize(__swiftObject: Long)
}