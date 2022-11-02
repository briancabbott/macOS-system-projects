package org.pureswift.swiftandroidsupport.view.animation

import android.util.Log
import android.view.animation.Animation

/** An animation listener receives notifications from an animation.
 * Notifications indicate animation related events, such as the end or the repetition of the animation.
 * */
open class SwiftAnimationListener(private val __swiftObject: Long): Animation.AnimationListener {

    override fun onAnimationRepeat(animation: Animation?) {
        __onAnimationRepeat(__swiftObject, animation)
    }

    override fun onAnimationEnd(animation: Animation?) {
        onAnimationEnd(__swiftObject, animation)
    }

    override fun onAnimationStart(animation: Animation?) {
        onAnimationStart(__swiftObject, animation)
    }

    fun finalize() {
        Log.e("Swift","finalize()")
        __finalize(__swiftObject)
    }

    private external fun __onAnimationRepeat(__swiftObject: Long, animation: Animation?)
    private external fun onAnimationEnd(__swiftObject: Long, animation: Animation?)
    private external fun onAnimationStart(__swiftObject: Long, animation: Animation?)
    private external fun __finalize(__swiftObject: Long)
}