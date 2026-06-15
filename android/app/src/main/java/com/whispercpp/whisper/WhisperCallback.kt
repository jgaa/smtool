package com.whispercpp.whisper

import androidx.annotation.Keep

interface WhisperCallback {
    fun onNewSegment(startMs: Long, endMs: Long, text: String)
    fun onProgress(progress: Int)
    fun onComplete()
}

@Keep
class DefaultWhisperCallback : WhisperCallback {
    override fun onNewSegment(startMs: Long, endMs: Long, text: String) {
        android.util.Log.d("WhisperCallback", "Segment: $text")
    }

    override fun onProgress(progress: Int) {
        android.util.Log.d("WhisperCallback", "Progress: $progress%")
    }

    override fun onComplete() {
        android.util.Log.d("WhisperCallback", "Completed")
    }
}