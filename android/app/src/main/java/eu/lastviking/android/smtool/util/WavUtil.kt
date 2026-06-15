package eu.lastviking.android.smtool.util

import java.io.FileOutputStream

object WavUtil {
    fun writeWavHeader(out: FileOutputStream, totalAudioLen: Long, sampleRate: Int) {
        val totalDataLen = totalAudioLen + 36
        val byteRate = (sampleRate * 2).toLong() // 16-bit = 2 bytes

        // RIFF header
        out.write("RIFF".toByteArray())
        out.write(intToByteArray(totalDataLen.toInt()))
        out.write("WAVE".toByteArray())
        // fmt sub-chunk
        out.write("fmt ".toByteArray())
        out.write(intToByteArray(16)) // Sub-chunk size
        out.write(shortToByteArray(1)) // Audio format (PCM = 1)
        out.write(shortToByteArray(1)) // Mono
        out.write(intToByteArray(sampleRate))
        out.write(intToByteArray(byteRate.toInt()))
        out.write(shortToByteArray(2)) // Block align
        out.write(shortToByteArray(16)) // Bits per sample
        // data sub-chunk
        out.write("data".toByteArray())
        out.write(intToByteArray(totalAudioLen.toInt()))
    }

    private fun intToByteArray(value: Int): ByteArray {
        return byteArrayOf(
            (value and 0xff).toByte(),
            (value shr 8 and 0xff).toByte(),
            (value shr 16 and 0xff).toByte(),
            (value shr 24 and 0xff).toByte()
        )
    }

    private fun shortToByteArray(value: Short): ByteArray {
        return byteArrayOf(
            (value.toInt() and 0xff).toByte(),
            (value.toInt() shr 8 and 0xff).toByte()
        )
    }
}
