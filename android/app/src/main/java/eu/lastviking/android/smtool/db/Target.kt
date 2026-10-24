package eu.lastviking.android.smtool.db

data class Target(
    val id: Long = 0,
    val name: String,
    val serverAddress: String = "",
    val serverPort: Int = 45437,
    val transferEnabled: Boolean = true,
    val deleteAfterTransfer: Boolean = false
)
