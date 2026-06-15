package eu.lastviking.android.smtool.util

import android.annotation.SuppressLint
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import kotlin.concurrent.thread

class AudioRecorder {

    private var audioRecord: AudioRecord? = null
    private var isRecording = false
    private var isPaused = false
    private var lastMaxAmplitude = 0

    private val sampleRate = 16000
    private val channelConfig = AudioFormat.CHANNEL_IN_MONO
    private val audioFormat = AudioFormat.ENCODING_PCM_16BIT
    
    fun getMaxAmplitude(): Int {
        synchronized(this) {
            val amp = lastMaxAmplitude
            lastMaxAmplitude = 0
            return amp
        }
    }

    private val bufferSize: Int by lazy {
        val size = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat)
        if (size == AudioRecord.ERROR_BAD_VALUE) {
            Log.e("AudioRecorder", "16 kHz sample rate not supported on this device!")
        }
        size
    }

    @SuppressLint("MissingPermission")
    fun startRecording(outputFile: File) {
        if (isRecording) return
        
        if (bufferSize <= 0) {
            Log.e("AudioRecorder", "Invalid buffer size: $bufferSize. Cannot start recording.")
            return
        }

        try {
            audioRecord = AudioRecord(
                MediaRecorder.AudioSource.MIC,
                sampleRate,
                channelConfig,
                audioFormat,
                bufferSize
            )
            
            if (audioRecord?.state != AudioRecord.STATE_INITIALIZED) {
                Log.e("AudioRecorder", "AudioRecord failed to initialize at 16 kHz")
                return
            }

            isRecording = true
            isPaused = false
            synchronized(this) { lastMaxAmplitude = 0 }
            audioRecord?.startRecording()

            thread {
                val tempFile = File(outputFile.absolutePath + ".tmp")
                tempFile.parentFile?.mkdirs()
                FileOutputStream(tempFile).use { fos ->
                    val data = ShortArray(bufferSize / 2)
                    while (isRecording) {
                        if (!isPaused) {
                            val read = audioRecord?.read(data, 0, data.size) ?: 0
                            if (read > 0) {
                                var max = 0
                                val byteData = ByteArray(read * 2)
                                for (i in 0 until read) {
                                    val s = data[i].toInt()
                                    if (Math.abs(s) > max) max = Math.abs(s)
                                    
                                    byteData[i * 2] = (data[i].toInt() and 0xff).toByte()
                                    byteData[i * 2 + 1] = (data[i].toInt() shr 8 and 0xff).toByte()
                                }
                                synchronized(this@AudioRecorder) {
                                    if (max > lastMaxAmplitude) {
                                        lastMaxAmplitude = max
                                    }
                                }
                                fos.write(byteData, 0, read * 2)
                            }
                        } else {
                            synchronized(this@AudioRecorder) { lastMaxAmplitude = 0 }
                            Thread.sleep(100)
                        }
                    }
                }
                // Add WAV header
                saveAsWav(tempFile, outputFile)
                tempFile.delete()
            }
        } catch (e: Exception) {
            Log.e("AudioRecorder", "Failed to start recording", e)
        }
    }

    fun pauseRecording() {
        if (isRecording) {
            isPaused = true
        }
    }

    fun resumeRecording() {
        if (isRecording) {
            isPaused = false
        }
    }

    fun stopRecording() {
        isRecording = false
        isPaused = false
        audioRecord?.stop()
        audioRecord?.release()
        audioRecord = null
    }

    private fun saveAsWav(pcmFile: File, wavFile: File) {
        val totalAudioLen = pcmFile.length()

        FileOutputStream(wavFile).use { out ->
            WavUtil.writeWavHeader(out, totalAudioLen, sampleRate)
            
            // Stream the PCM data
            pcmFile.inputStream().use { input ->
                val buffer = ByteArray(8192)
                var bytesRead: Int
                while (input.read(buffer).also { bytesRead = it } != -1) {
                    out.write(buffer, 0, bytesRead)
                }
            }
        }
    }
}
