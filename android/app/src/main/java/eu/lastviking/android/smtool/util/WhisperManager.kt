package eu.lastviking.android.smtool.util

import android.content.Context
import android.util.Log
import com.whispercpp.whisper.WhisperContext
import com.whispercpp.whisper.WhisperCallback
import eu.lastviking.android.smtool.utils.StreamingAudioChunker
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class WhisperManager(private val context: Context) {

    private var whisperContext: WhisperContext? = null
    
    companion object {
        private const val TAG = "WhisperManager"
        private const val CURRENT_MODEL_NAME = "ggml-base.bin"
        private const val MODEL_URL = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/$CURRENT_MODEL_NAME"
    }

    private val modelDir = File(context.filesDir, "models/whisper")
    private val modelFile = File(modelDir, CURRENT_MODEL_NAME)

    init {
        cleanupObsoleteModels()
    }

    private fun cleanupObsoleteModels() {
        val legacyFiles = listOf("ggml-tiny.en.bin", "ggml-tiny.bin")
        legacyFiles.forEach { name ->
            val file = File(context.filesDir, name)
            if (file.exists()) file.delete()
        }

        if (!modelDir.exists()) {
            modelDir.mkdirs()
            return
        }

        modelDir.listFiles()?.forEach { file ->
            if (file.name != CURRENT_MODEL_NAME) {
                file.delete()
            }
        }
    }

    suspend fun isModelAvailable(): Boolean {
        return modelFile.exists()
    }

    fun downloadModel(listener: ModelDownloader.DownloadListener) {
        if (!modelDir.exists()) modelDir.mkdirs()
        ModelDownloader().downloadModel(MODEL_URL, modelFile, listener)
    }

    suspend fun transcribe(audioFile: File, lang: String = "en"): String? = withContext(Dispatchers.Default) {
        if (!isModelAvailable()) return@withContext "Model not downloaded"

        val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)
        val prompt = prefs.getString("whisper_prompt", "")?.replace("\n", " ")?.trim()
        val finalPrompt = if (prompt.isNullOrEmpty()) null else prompt

        val finalLang = if (lang == "auto") "" else lang
        Log.d(TAG, "Starting transcription for file: ${audioFile.name}, requested language: '$finalLang', prompt: '$finalPrompt'")

        try {
            if (whisperContext == null) {
                whisperContext = WhisperContext.createContextFromFile(modelFile.absolutePath)
            }

            val chunker = StreamingAudioChunker()
            val chunks = chunker.splitWavFileIntoChunks(audioFile.absolutePath)
            val fullTranscript = StringBuilder()

            for (chunk in chunks) {
                val data = chunker.readChunkData(chunk)
                val result = whisperContext?.transcribeData(data, finalLang, finalPrompt, false, object : WhisperCallback {
                    override fun onNewSegment(startMs: Long, endMs: Long, text: String) {
                        Log.d(TAG, "Segment: $text")
                    }
                    override fun onProgress(progress: Int) {
                        Log.d(TAG, "Progress: $progress%")
                    }
                    override fun onComplete() {
                        Log.d(TAG, "Chunk complete")
                    }
                })
                
                if (result != null) {
                    if (fullTranscript.isNotEmpty()) fullTranscript.append(" ")
                    fullTranscript.append(result.trim())
                }
            }

            return@withContext fullTranscript.toString()
        } catch (e: Exception) {
            Log.e(TAG, "Transcription failed", e)
            return@withContext null
        }
    }

    fun release() {
        // WhisperContext handling
    }
}
