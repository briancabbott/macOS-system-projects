package org.pureswift.swiftandroidsupport.recyclerview

import android.support.v7.widget.RecyclerView.ViewHolder
import android.util.Log
import android.view.View

class SwiftRecyclerViewViewHolder(internal val __swiftObject: Long, itemView: View): ViewHolder(itemView) {

    init {
        Log.e("Swift","ViewHolder init($__swiftObject, $itemView)")
    }

    fun obtainAdapterPosition(): Int {
        return this.adapterPosition
    }

    fun obtainItemId(): Long {
        return this.itemId
    }

    fun obtainItemViewType(): Int {
        return this.itemViewType
    }

    fun obtainLayoutPosition(): Int {
        return this.layoutPosition
    }

    fun obtainOldPosition(): Int {
        return this.oldPosition
    }

    fun itemIsRecyclable(): Boolean {
        return this.isRecyclable
    }

    fun putIsRecyclable(recyclable: Boolean) {
        this.setIsRecyclable(recyclable)
    }

    fun finalize(){
        Log.e("Swift","finalize()")
        __finalize(__swiftObject)
    }

    private external fun __finalize(__swiftObject: Long)
}