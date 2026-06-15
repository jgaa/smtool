package eu.lastviking.android.smtool.util

import okhttp3.OkHttpClient
import okhttp3.Request
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

class ModelDownloader {
    private val client = OkHttpClient()

    interface DownloadListener {
        fun onProgress(progress: Int)
        fun onComplete(file: File)
        fun onError(e: Exception)
    }

    fun downloadModel(url: String, outputFile: File, listener: DownloadListener) {
        val request = Request.Builder().url(url).build()

        client.newCall(request).enqueue(object : okhttp3.Callback {
            override fun onFailure(call: okhttp3.Call, e: IOException) {
                listener.onError(e)
            }

            override fun onResponse(call: okhttp3.Call, response: okhttp3.Response) {
                if (!response.isSuccessful) {
                    listener.onError(IOException("Unexpected code $response"))
                    return
                }

                try {
                    val body = response.body ?: throw IOException("Empty body")
                    val contentLength = body.contentLength()
                    
                    body.byteStream().use { input ->
                        FileOutputStream(outputFile).use { output ->
                            val buffer = ByteArray(8192)
                            var totalBytesRead = 0L
                            var bytesRead: Int
                            
                            while (input.read(buffer).also { bytesRead = it } != -1) {
                                output.write(buffer, 0, bytesRead)
                                totalBytesRead += bytesRead
                                if (contentLength > 0) {
                                    val progress = (totalBytesRead * 100 / contentLength).toInt()
                                    listener.onProgress(progress)
                                }
                            }
                        }
                    }
                    listener.onComplete(outputFile)
                } catch (e: Exception) {
                    listener.onError(e)
                }
            }
        })
    }
}
