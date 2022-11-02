package org.pureswift.swiftandroidsupport.view

import android.animation.LayoutTransition
import android.content.Context
import android.content.res.Configuration
import android.graphics.Point
import android.graphics.Rect
import android.graphics.Region
import android.os.Bundle
import android.util.AttributeSet
import android.view.*
import android.view.accessibility.AccessibilityEvent
import android.view.animation.Animation
import android.view.animation.LayoutAnimationController
import java.util.ArrayList

class SwiftViewGroup(context: Context): android.view.ViewGroup(context) {

    override fun addChildrenForAccessibility(outChildren: ArrayList<View>?) {
        super.addChildrenForAccessibility(outChildren)
    }

    override fun getClipChildren(): Boolean {
        return super.getClipChildren()
    }

    override fun addView(child: View?) {
        super.addView(child)
    }

    override fun addView(child: View?, index: Int) {
        super.addView(child, index)
    }

    override fun addView(child: View?, width: Int, height: Int) {
        super.addView(child, width, height)
    }

    override fun addView(child: View?, params: LayoutParams?) {
        super.addView(child, params)
    }

    override fun addView(child: View?, index: Int, params: LayoutParams?) {
        super.addView(child, index, params)
    }

    override fun onRequestSendAccessibilityEvent(child: View?, event: AccessibilityEvent?): Boolean {
        return super.onRequestSendAccessibilityEvent(child, event)
    }

    override fun setClipToPadding(clipToPadding: Boolean) {
        super.setClipToPadding(clipToPadding)
    }

    override fun hasFocus(): Boolean {
        return super.hasFocus()
    }

    override fun dispatchDrawableHotspotChanged(x: Float, y: Float) {
        super.dispatchDrawableHotspotChanged(x, y)
    }

    override fun dispatchDisplayHint(hint: Int) {
        super.dispatchDisplayHint(hint)
    }

    override fun getNestedScrollAxes(): Int {
        return super.getNestedScrollAxes()
    }

    override fun dispatchProvideStructure(structure: ViewStructure?) {
        super.dispatchProvideStructure(structure)
    }

    override fun getOverlay(): ViewGroupOverlay {
        return super.getOverlay()
    }

    override fun onNestedFling(target: View?, velocityX: Float, velocityY: Float, consumed: Boolean): Boolean {
        return super.onNestedFling(target, velocityX, velocityY, consumed)
    }

    override fun getClipToPadding(): Boolean {
        return super.getClipToPadding()
    }

    override fun dispatchPointerCaptureChanged(hasCapture: Boolean) {
        super.dispatchPointerCaptureChanged(hasCapture)
    }

    override fun dispatchUnhandledMove(focused: View?, direction: Int): Boolean {
        return super.dispatchUnhandledMove(focused, direction)
    }

    override fun setOnHierarchyChangeListener(listener: OnHierarchyChangeListener?) {
        super.setOnHierarchyChangeListener(listener)
    }

    override fun dispatchCapturedPointerEvent(event: MotionEvent?): Boolean {
        return super.dispatchCapturedPointerEvent(event)
    }

    override fun clearChildFocus(child: View?) {
        super.clearChildFocus(child)
    }

    override fun isAnimationCacheEnabled(): Boolean {
        return super.isAnimationCacheEnabled()
    }

    override fun removeAllViews() {
        super.removeAllViews()
    }

    override fun hasTransientState(): Boolean {
        return super.hasTransientState()
    }

    override fun getLayoutAnimation(): LayoutAnimationController {
        return super.getLayoutAnimation()
    }

    override fun requestChildFocus(child: View?, focused: View?) {
        super.requestChildFocus(child, focused)
    }

    override fun setClipChildren(clipChildren: Boolean) {
        super.setClipChildren(clipChildren)
    }

    override fun requestFocus(direction: Int, previouslyFocusedRect: Rect?): Boolean {
        return super.requestFocus(direction, previouslyFocusedRect)
    }

    override fun onResolvePointerIcon(event: MotionEvent?, pointerIndex: Int): PointerIcon {
        return super.onResolvePointerIcon(event, pointerIndex)
    }

    override fun getLayoutMode(): Int {
        return super.getLayoutMode()
    }

    override fun setLayoutMode(layoutMode: Int) {
        super.setLayoutMode(layoutMode)
    }

    override fun scheduleLayoutAnimation() {
        super.scheduleLayoutAnimation()
    }

    override fun indexOfChild(child: View?): Int {
        return super.indexOfChild(child)
    }

    override fun dispatchWindowVisibilityChanged(visibility: Int) {
        super.dispatchWindowVisibilityChanged(visibility)
    }

    override fun getChildVisibleRect(child: View?, r: Rect?, offset: Point?): Boolean {
        return super.getChildVisibleRect(child, r, offset)
    }

    override fun jumpDrawablesToCurrentState() {
        super.jumpDrawablesToCurrentState()
    }

    override fun onNestedPreScroll(target: View?, dx: Int, dy: Int, consumed: IntArray?) {
        super.onNestedPreScroll(target, dx, dy, consumed)
    }

    override fun removeViewInLayout(view: View?) {
        super.removeViewInLayout(view)
    }

    override fun endViewTransition(view: View?) {
        super.endViewTransition(view)
    }

    override fun invalidateChildInParent(location: IntArray?, dirty: Rect?): ViewParent {
        return super.invalidateChildInParent(location, dirty)
    }

    override fun onStopNestedScroll(child: View?) {
        super.onStopNestedScroll(child)
    }

    override fun focusSearch(focused: View?, direction: Int): View {
        return super.focusSearch(focused, direction)
    }

    override fun dispatchKeyEventPreIme(event: KeyEvent?): Boolean {
        return super.dispatchKeyEventPreIme(event)
    }

    override fun clearDisappearingChildren() {
        super.clearDisappearingChildren()
    }

    override fun setAlwaysDrawnWithCacheEnabled(always: Boolean) {
        super.setAlwaysDrawnWithCacheEnabled(always)
    }

    override fun dispatchProvideAutofillStructure(structure: ViewStructure?, flags: Int) {
        super.dispatchProvideAutofillStructure(structure, flags)
    }

    override fun dispatchSetActivated(activated: Boolean) {
        super.dispatchSetActivated(activated)
    }

    override fun removeViews(start: Int, count: Int) {
        super.removeViews(start, count)
    }

    override fun getAccessibilityClassName(): CharSequence {
        return super.getAccessibilityClassName()
    }

    override fun onInterceptHoverEvent(event: MotionEvent?): Boolean {
        return super.onInterceptHoverEvent(event)
    }

    override fun isMotionEventSplittingEnabled(): Boolean {
        return super.isMotionEventSplittingEnabled()
    }

    override fun requestChildRectangleOnScreen(child: View?, rectangle: Rect?, immediate: Boolean): Boolean {
        return super.requestChildRectangleOnScreen(child, rectangle, immediate)
    }

    override fun getTouchscreenBlocksFocus(): Boolean {
        return super.getTouchscreenBlocksFocus()
    }

    override fun restoreDefaultFocus(): Boolean {
        return super.restoreDefaultFocus()
    }

    override fun addStatesFromChildren(): Boolean {
        return super.addStatesFromChildren()
    }

    override fun onViewAdded(child: View?) {
        super.onViewAdded(child)
    }

    override fun clearFocus() {
        super.clearFocus()
    }

    override fun dispatchKeyEvent(event: KeyEvent?): Boolean {
        return super.dispatchKeyEvent(event)
    }

    override fun onViewRemoved(child: View?) {
        super.onViewRemoved(child)
    }

    override fun addTouchables(views: ArrayList<View>?) {
        super.addTouchables(views)
    }

    override fun setLayoutTransition(transition: LayoutTransition?) {
        super.setLayoutTransition(transition)
    }

    override fun onStartNestedScroll(child: View?, target: View?, nestedScrollAxes: Int): Boolean {
        return super.onStartNestedScroll(child, target, nestedScrollAxes)
    }

    override fun getLayoutTransition(): LayoutTransition {
        return super.getLayoutTransition()
    }

    override fun getChildCount(): Int {
        return super.getChildCount()
    }

    override fun requestTransparentRegion(child: View?) {
        super.requestTransparentRegion(child)
    }

    override fun getChildAt(index: Int): View {
        return super.getChildAt(index)
    }

    override fun removeView(view: View?) {
        super.removeView(view)
    }

    override fun onNestedScrollAccepted(child: View?, target: View?, axes: Int) {
        super.onNestedScrollAccepted(child, target, axes)
    }

    override fun isTransitionGroup(): Boolean {
        return super.isTransitionGroup()
    }

    override fun dispatchKeyShortcutEvent(event: KeyEvent?): Boolean {
        return super.dispatchKeyShortcutEvent(event)
    }

    override fun generateLayoutParams(attrs: AttributeSet?): LayoutParams {
        return super.generateLayoutParams(attrs)
    }

    override fun removeAllViewsInLayout() {
        super.removeAllViewsInLayout()
    }

    override fun gatherTransparentRegion(region: Region?): Boolean {
        return super.gatherTransparentRegion(region)
    }

    override fun bringChildToFront(child: View?) {
        super.bringChildToFront(child)
    }

    override fun removeViewAt(index: Int) {
        super.removeViewAt(index)
    }

    override fun findFocus(): View {
        return super.findFocus()
    }

    override fun removeViewsInLayout(start: Int, count: Int) {
        super.removeViewsInLayout(start, count)
    }

    override fun findViewsWithText(outViews: ArrayList<View>?, text: CharSequence?, flags: Int) {
        super.findViewsWithText(outViews, text, flags)
    }

    override fun dispatchDragEvent(event: DragEvent?): Boolean {
        return super.dispatchDragEvent(event)
    }

    override fun focusableViewAvailable(v: View?) {
        super.focusableViewAvailable(v)
    }

    override fun getLayoutAnimationListener(): Animation.AnimationListener {
        return super.getLayoutAnimationListener()
    }

    override fun recomputeViewAttributes(child: View?) {
        super.recomputeViewAttributes(child)
    }

    override fun addFocusables(views: ArrayList<View>?, direction: Int, focusableMode: Int) {
        super.addFocusables(views, direction, focusableMode)
    }

    override fun isAlwaysDrawnWithCacheEnabled(): Boolean {
        return super.isAlwaysDrawnWithCacheEnabled()
    }

    override fun addKeyboardNavigationClusters(views: MutableCollection<View>?, direction: Int) {
        super.addKeyboardNavigationClusters(views, direction)
    }

    override fun dispatchConfigurationChanged(newConfig: Configuration?) {
        super.dispatchConfigurationChanged(newConfig)
    }

    override fun dispatchWindowFocusChanged(hasFocus: Boolean) {
        super.dispatchWindowFocusChanged(hasFocus)
    }

    override fun onDescendantInvalidated(child: View?, target: View?) {
        super.onDescendantInvalidated(child, target)
    }

    override fun requestDisallowInterceptTouchEvent(disallowIntercept: Boolean) {
        super.requestDisallowInterceptTouchEvent(disallowIntercept)
    }

    override fun setAddStatesFromChildren(addsStates: Boolean) {
        super.setAddStatesFromChildren(addsStates)
    }

    override fun dispatchSetSelected(selected: Boolean) {
        super.dispatchSetSelected(selected)
    }

    override fun requestSendAccessibilityEvent(child: View?, event: AccessibilityEvent?): Boolean {
        return super.requestSendAccessibilityEvent(child, event)
    }

    override fun dispatchApplyWindowInsets(insets: WindowInsets?): WindowInsets {
        return super.dispatchApplyWindowInsets(insets)
    }

    override fun notifySubtreeAccessibilityStateChanged(child: View?, source: View?, changeType: Int) {
        super.notifySubtreeAccessibilityStateChanged(child, source, changeType)
    }

    override fun childHasTransientStateChanged(child: View?, childHasTransientState: Boolean) {
        super.childHasTransientStateChanged(child, childHasTransientState)
    }

    override fun childDrawableStateChanged(child: View?) {
        super.childDrawableStateChanged(child)
    }

    override fun onNestedScroll(target: View?, dxConsumed: Int, dyConsumed: Int, dxUnconsumed: Int, dyUnconsumed: Int) {
        super.onNestedScroll(target, dxConsumed, dyConsumed, dxUnconsumed, dyUnconsumed)
    }

    override fun setDescendantFocusability(focusability: Int) {
        super.setDescendantFocusability(focusability)
    }

    override fun getFocusedChild(): View {
        return super.getFocusedChild()
    }

    override fun dispatchWindowSystemUiVisiblityChanged(visible: Int) {
        super.dispatchWindowSystemUiVisiblityChanged(visible)
    }

    override fun onNestedPrePerformAccessibilityAction(target: View?, action: Int, args: Bundle?): Boolean {
        return super.onNestedPrePerformAccessibilityAction(target, action, args)
    }

    override fun dispatchTrackballEvent(event: MotionEvent?): Boolean {
        return super.dispatchTrackballEvent(event)
    }

    override fun onNestedPreFling(target: View?, velocityX: Float, velocityY: Float): Boolean {
        return super.onNestedPreFling(target, velocityX, velocityY)
    }

    override fun dispatchSystemUiVisibilityChanged(visible: Int) {
        super.dispatchSystemUiVisibilityChanged(visible)
    }

    override fun onInterceptTouchEvent(ev: MotionEvent?): Boolean {
        return super.onInterceptTouchEvent(ev)
    }

    override fun getDescendantFocusability(): Int {
        return super.getDescendantFocusability()
    }

    override fun setMotionEventSplittingEnabled(split: Boolean) {
        super.setMotionEventSplittingEnabled(split)
    }

    override fun dispatchTouchEvent(ev: MotionEvent?): Boolean {
        return super.dispatchTouchEvent(ev)
    }

    override fun setLayoutAnimation(controller: LayoutAnimationController?) {
        super.setLayoutAnimation(controller)
    }

    override fun setAnimationCacheEnabled(enabled: Boolean) {
        super.setAnimationCacheEnabled(enabled)
    }

    override fun setLayoutAnimationListener(animationListener: Animation.AnimationListener?) {
        super.setLayoutAnimationListener(animationListener)
    }

    override fun getPersistentDrawingCache(): Int {
        return super.getPersistentDrawingCache()
    }

    override fun setTouchscreenBlocksFocus(touchscreenBlocksFocus: Boolean) {
        super.setTouchscreenBlocksFocus(touchscreenBlocksFocus)
    }

    override fun updateViewLayout(view: View?, params: LayoutParams?) {
        super.updateViewLayout(view, params)
    }

    override fun shouldDelayChildPressedState(): Boolean {
        return super.shouldDelayChildPressedState()
    }

    override fun startLayoutAnimation() {
        super.startLayoutAnimation()
    }

    override fun startActionModeForChild(originalView: View?, callback: ActionMode.Callback?): ActionMode {
        return super.startActionModeForChild(originalView, callback)
    }

    override fun startActionModeForChild(originalView: View?, callback: ActionMode.Callback?, type: Int): ActionMode {
        return super.startActionModeForChild(originalView, callback, type)
    }

    override fun startViewTransition(view: View?) {
        super.startViewTransition(view)
    }

    override fun showContextMenuForChild(originalView: View?): Boolean {
        return super.showContextMenuForChild(originalView)
    }

    override fun showContextMenuForChild(originalView: View?, x: Float, y: Float): Boolean {
        return super.showContextMenuForChild(originalView, x, y)
    }

    override fun setPersistentDrawingCache(drawingCacheToKeep: Int) {
        super.setPersistentDrawingCache(drawingCacheToKeep)
    }

    override fun setTransitionGroup(isTransitionGroup: Boolean) {
        super.setTransitionGroup(isTransitionGroup)
    }

    override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {

    }
}