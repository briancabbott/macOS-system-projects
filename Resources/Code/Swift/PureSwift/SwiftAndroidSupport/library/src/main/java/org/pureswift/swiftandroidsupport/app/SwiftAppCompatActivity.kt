package org.pureswift.swiftandroidsupport.app

import android.content.*
import android.content.res.Resources
import android.os.Bundle
import android.support.v4.app.FragmentManager
import android.support.v7.app.AppCompatActivity
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager

open class SwiftAppCompatActivity: AppCompatActivity() {

    private var __swiftObject: Long = 0L

    init{
        __swiftObject = bind()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        onCreateNative(__swiftObject, savedInstanceState)
    }

    override fun setContentView(layoutResID: Int) {
        super.setContentView(layoutResID)

    }

    override fun setContentView(view: View) {
        super.setContentView(view)

    }

    override fun <T : View> findViewById(id: Int): T {
        return super.findViewById(id)
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

    override fun onRestart() {
        super.onRestart()
        onRestartNative(__swiftObject)
    }

    override fun onStop() {
        super.onStop()
        onStopNative(__swiftObject)
    }

    override fun onDestroy() {
        super.onDestroy()
        onDestroyNative(__swiftObject)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        onActivityResultNative(__swiftObject, requestCode, resultCode, data)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        onRequestPermissionsResultNative(__swiftObject, requestCode, permissions, grantResults)
    }

    override fun onBackPressed() {
        onBackPressed(__swiftObject)
    }

    fun finalize() {
        finalizeNative(__swiftObject)
    }

    public fun getDensity(): Float {
        val metrics = resources.displayMetrics
        return metrics.density
    }

    override fun addContentView(view: View?, params: ViewGroup.LayoutParams?) {
        super.addContentView(view, params)
    }

    override fun startActivity(intent: Intent) {
        super.startActivity(intent)
    }

    override fun startActivityForResult(intent: Intent, requestCode: Int) {
        super.startActivityForResult(intent, requestCode)
    }

    override fun startService(service: Intent): ComponentName? {
        return super.startService(service)
    }

    override fun stopService(name: Intent): Boolean {
        return super.stopService(name)
    }

    override fun finish() {
        super.finish()
    }

    override fun isFinishing(): Boolean {
        return super.isFinishing()
    }

    //return 0 if the id is not found
    fun getIdentifier(name: String, type: String): Int {
        return resources.getIdentifier(name, type, packageName)
    }

    override fun getResources(): Resources {
        return super.getResources()
    }

    override fun getSupportFragmentManager(): FragmentManager {
        return super.getSupportFragmentManager()
    }

    override fun getWindowManager(): WindowManager {
        return super.getWindowManager()
    }

    fun hasNavBar(): Boolean {
        val id = resources.getIdentifier("config_showNavigationBar", "bool", "android")
        return id > 0 && resources.getBoolean(id)
    }

    fun readJsonResource(jsonName: String): String? {

        val resourcesId = getIdentifier(jsonName,"raw")

        if(resourcesId <= 0) return null

        return resources.openRawResource(resourcesId).bufferedReader().use { it.readText() }
    }

    //navigationBarBackground
    fun getStatusBarHeightPixels(): Int {
        var statusBarHeight = 0
        val resourceId = resources.getIdentifier("status_bar_height", "dimen", "android")
        if (resourceId > 0) {
            statusBarHeight = resources.getDimensionPixelSize(resourceId)
        }
        return statusBarHeight
    }

    fun getActionBarHeighPixels(): Int {
        var actionBarHeight = 0
        val styledAttributes = theme.obtainStyledAttributes(
                intArrayOf(android.R.attr.actionBarSize)
        )
        actionBarHeight = styledAttributes.getDimension(0, 0.0F).toInt()
        styledAttributes.recycle()

        return actionBarHeight
    }

    fun runOnMainThread(runnable: Runnable){
        runOnUiThread(runnable)
    }

    private external fun bind(): Long

    private external fun onCreateNative(__swiftObject: Long, savedInstanceState: Bundle?)

    private external fun onStartNative(__swiftObject: Long)

    private external fun onResumeNative(__swiftObject: Long)

    private external fun onPauseNative(__swiftObject: Long)

    private external fun onRestartNative(__swiftObject: Long)

    private external fun onStopNative(__swiftObject: Long)

    private external fun onDestroyNative(__swiftObject: Long)

    private external fun onActivityResultNative(__swiftObject: Long, requestCode: Int, resultCode: Int, data: Intent?)

    private external fun onRequestPermissionsResultNative(__swiftObject: Long, requestCode: Int, permissions: Array<out String>, grantResults: IntArray)

    private external fun onBackPressed(__swiftObject: Long)

    private external fun finalizeNative(__swiftObject: Long)

}