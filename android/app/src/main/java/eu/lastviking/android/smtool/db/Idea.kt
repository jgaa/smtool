package eu.lastviking.android.smtool.db

data class Idea(
    val id: Long = 0,
    val name: String? = null,
    val createdAt: Long,
    val transcribedAt: Long? = null,
    val status: String, // "recording", "transcript"
    val exported: Boolean = false,
    val transcript: String? = null
)
