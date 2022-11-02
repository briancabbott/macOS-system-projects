package org.pureswift.swiftandroidsupport.recyclerview

import android.support.v7.widget.RecyclerView
import android.util.Log
import android.view.ViewGroup

class SwiftRecyclerViewAdapter(private val __swiftObject: Long): RecyclerView.Adapter<SwiftRecyclerViewViewHolder>() {

    init {
        Log.e("Swift","RecyclerViewAdapter init")
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SwiftRecyclerViewViewHolder {
        Log.e("Swift","onCreateViewHolder()")
        return __onCreateViewHolder(__swiftObject, parent, viewType)
    }

    override fun getItemViewType(position: Int): Int {
        return __getItemViewType(__swiftObject, position)
    }

    override fun getItemCount(): Int {
        return __getItemCount(__swiftObject)
    }

    override fun onBindViewHolder(holder: SwiftRecyclerViewViewHolder, position: Int) {
        Log.e("Swift","onBindViewHolder()")

        __onBindViewHolder(__swiftObject, holder.__swiftObject, position)
    }
/*
    override fun onBindViewHolder(holder: SwiftRecyclerViewViewHolder, position: Int, payloads: MutableList<Any>) {
        super.onBindViewHolder(holder, position, payloads)
        Log.e("Swift","onBindViewHolderWithPayload()")
        __onBindViewHolderPayload(__swiftObject, holder.__swiftObject, position, payloads)
    }*/

    fun finalize() {
        Log.e("Swift","finalize()")
        __finalize(__swiftObject)
    }

    private external fun __onCreateViewHolder(__swiftObject: Long, parent: ViewGroup, viewType: Int): SwiftRecyclerViewViewHolder

    private external fun __getItemCount(__swiftObject: Long): Int

    private external fun __onBindViewHolder(__swiftObject: Long, holder: Long, position: Int)

    private external fun __onBindViewHolderPayload(__swiftObject: Long, holder: Long, position: Int, payloads: MutableList<Any>)

    private external fun __getItemViewType(__swiftObject: Long, position: Int): Int

    private external fun __finalize(__swiftObject: Long)
}