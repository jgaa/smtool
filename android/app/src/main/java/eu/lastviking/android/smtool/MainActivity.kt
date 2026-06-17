package eu.lastviking.android.smtool

import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import android.widget.ArrayAdapter
import android.widget.CheckBox
import android.widget.EditText
import android.widget.Spinner
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.FileProvider
import androidx.core.content.edit
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.lifecycleScope
import androidx.navigation.findNavController
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.AppBarConfiguration
import androidx.navigation.ui.navigateUp
import androidx.navigation.ui.setupActionBarWithNavController
import eu.lastviking.android.smtool.databinding.ActivityMainBinding
import eu.lastviking.android.smtool.db.DatabaseHelper
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.BufferedReader
import java.io.File
import java.io.InputStreamReader
import java.io.PrintWriter
import java.net.InetSocketAddress
import java.net.Socket
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.Random

class MainActivity : AppCompatActivity() {

    private lateinit var appBarConfiguration: AppBarConfiguration
    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        ViewCompat.setOnApplyWindowInsetsListener(binding.main) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }
        setSupportActionBar(binding.toolbar)

        val navHostFragment =
            supportFragmentManager.findFragmentById(R.id.nav_host_fragment_content_main) as NavHostFragment
        val navController = navHostFragment.navController

        appBarConfiguration = AppBarConfiguration(navController.graph)
        setupActionBarWithNavController(navController, appBarConfiguration)
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        // Inflate the menu; this adds items to the action bar if it is present.
        menuInflater.inflate(R.menu.menu_main, menu)
        
        val navHostFragment =
            supportFragmentManager.findFragmentById(R.id.nav_host_fragment_content_main) as NavHostFragment
        val firstFragment = navHostFragment.childFragmentManager.fragments.firstOrNull { it is FirstFragment } as? FirstFragment
        val hasSelection = firstFragment?.getSelectedIds()?.isNotEmpty() ?: false
        menu.findItem(R.id.action_share_selected)?.isVisible = hasSelection
        
        val prefs = getSharedPreferences("settings", MODE_PRIVATE)
        val transferEnabled = prefs.getBoolean("transfer_enabled", true)
        menu.findItem(R.id.action_transfer)?.isVisible = transferEnabled
        
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        return when (item.itemId) {
            R.id.action_transcribe_all -> {
                val navHostFragment =
                    supportFragmentManager.findFragmentById(R.id.nav_host_fragment_content_main) as NavHostFragment
                val firstFragment = navHostFragment.childFragmentManager.fragments.firstOrNull { it is FirstFragment } as? FirstFragment
                firstFragment?.transcribeAllPending()
                true
            }
            R.id.action_share_all -> {
                showExportFormatDialog(null)
                true
            }
            R.id.action_share_selected -> {
                val navHostFragment =
                    supportFragmentManager.findFragmentById(R.id.nav_host_fragment_content_main) as NavHostFragment
                val firstFragment = navHostFragment.childFragmentManager.fragments.firstOrNull { it is FirstFragment } as? FirstFragment
                val selectedIds = firstFragment?.getSelectedIds()
                if (!selectedIds.isNullOrEmpty()) {
                    showExportFormatDialog(selectedIds)
                }
                true
            }
            R.id.action_delete_all -> {
                showDeleteAllConfirmation()
                true
            }
            R.id.action_settings -> {
                showSettingsDialog()
                true
            }
            R.id.action_transfer -> {
                val navHostFragment =
                    supportFragmentManager.findFragmentById(R.id.nav_host_fragment_content_main) as NavHostFragment
                val firstFragment = navHostFragment.childFragmentManager.fragments.firstOrNull { it is FirstFragment } as? FirstFragment
                val selectedIds = firstFragment?.getSelectedIds()
                performTransfer(selectedIds)
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    private fun performTransfer(filterIds: List<Long>? = null) {
        val prefs = getSharedPreferences("settings", MODE_PRIVATE)
        val serverAddress = prefs.getString("server_address", "") ?: ""
        val serverPort = prefs.getInt("server_port", 45437)

        if (serverAddress.isBlank()) {
            showSettingsDialog()
            return
        }

        lifecycleScope.launch(Dispatchers.IO) {
            val dbHelper = DatabaseHelper(this@MainActivity)
            val allIdeas = dbHelper.getAllIdeas()
            
            val ideasToTransfer = if (!filterIds.isNullOrEmpty()) {
                val selected = allIdeas.filter { filterIds.contains(it.id) }
                val nonTranscribed = selected.filter { it.status != "transcript" || it.transcript.isNullOrBlank() }
                
                if (nonTranscribed.isNotEmpty()) {
                    withContext(Dispatchers.Main) {
                        AlertDialog.Builder(this@MainActivity)
                            .setTitle("Cannot Transfer")
                            .setMessage("Some selected ideas have not been transcribed yet. Only transcribed text can be transferred.")
                            .setPositiveButton(android.R.string.ok, null)
                            .show()
                    }
                    return@launch
                }
                selected
            } else {
                allIdeas.filter { it.status == "transcript" && !it.transcript.isNullOrBlank() }
            }

            if (ideasToTransfer.isEmpty()) {
                withContext(Dispatchers.Main) {
                    AlertDialog.Builder(this@MainActivity)
                        .setTitle("No Ideas")
                        .setMessage("There are no transcribed ideas to transfer.")
                        .setPositiveButton(android.R.string.ok, null)
                        .show()
                }
                return@launch
            }

            val code = String.format(Locale.US, "%08d", Random().nextInt(100000000))
            val displayCode = "${code.substring(0, 4)}-${code.substring(4)}"

            withContext(Dispatchers.Main) {
                val dialog = AlertDialog.Builder(this@MainActivity)
                    .setTitle(R.string.transferring)
                    .setMessage(getString(R.string.connecting_to, serverAddress, serverPort) + "\n\n" + getString(R.string.transfer_code, displayCode))
                    .setNegativeButton(android.R.string.cancel, null)
                    .create()

                val transferJob = lifecycleScope.launch(Dispatchers.IO) {
                    var socket: Socket? = null
                    try {
                        socket = Socket()
                        socket.connect(InetSocketAddress(serverAddress, serverPort), 5000)
                        val writer = PrintWriter(socket.getOutputStream(), true)
                        val reader = BufferedReader(InputStreamReader(socket.getInputStream()))

                        // 1. Hello
                        val hello = JSONObject().apply {
                            put("type", "hello")
                            put("app", "smtool-transfer")
                            put("version", 1)
                            put("code", code)
                        }
                        writer.println(hello.toString())

                        // 2. Wait for Continue
                        val continueLine = reader.readLine() ?: throw Exception("Server closed connection")
                        val continueJson = JSONObject(continueLine)
                        if (continueJson.getString("type") != "continue" || !continueJson.getBoolean("ok")) {
                            throw Exception(continueJson.optString("message", "Rejected by server"))
                        }

                        // 3. Send Payload
                        val payload = JSONObject().apply {
                            put("type", "ideas")
                            put("version", 1)
                            val itemsArray = JSONArray()
                            for (idea in ideasToTransfer) {
                                val item = JSONObject().apply {
                                    put("id", idea.uuid)
                                    put("title", idea.name ?: "Untitled Idea")
                                    put("text", idea.transcript ?: "")
                                    put("created_at", SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'", Locale.US).format(Date(idea.createdAt)))
                                    put("source", "android")
                                    put("list", "inbox")
                                }
                                itemsArray.put(item)
                            }
                            put("items", itemsArray)
                        }
                        writer.println(payload.toString())

                        // 4. Wait for Result
                        val resultLine = reader.readLine() ?: throw Exception("Server closed connection during result")
                        val resultJson = JSONObject(resultLine)
                        if (resultJson.getString("type") == "result" && resultJson.getBoolean("ok")) {
                            // Success! 
                            val deleteAfterTransfer = prefs.getBoolean("delete_after_transfer", false)
                            
                            for (idea in ideasToTransfer) {
                                if (deleteAfterTransfer) {
                                    dbHelper.deleteIdea(idea.id)
                                } else {
                                    dbHelper.updateExportedStatus(idea.id, true)
                                }
                            }
                            withContext(Dispatchers.Main) {
                                dialog.dismiss()
                                AlertDialog.Builder(this@MainActivity)
                                    .setTitle(R.string.transfer_success)
                                    .setMessage("Imported ${resultJson.optInt("imported", ideasToTransfer.size)} ideas.")
                                    .setPositiveButton(android.R.string.ok, null)
                                    .show()
                                
                                val navHostFragment =
                                    supportFragmentManager.findFragmentById(R.id.nav_host_fragment_content_main) as NavHostFragment
                                val firstFragment = navHostFragment.childFragmentManager.fragments.firstOrNull { it is FirstFragment } as? FirstFragment
                                firstFragment?.refreshIdeas()
                            }
                        } else {
                            throw Exception(resultJson.optString("message", "Transfer failed"))
                        }

                    } catch (e: Exception) {
                        if (isActive) {
                            withContext(Dispatchers.Main) {
                                dialog.dismiss()
                                AlertDialog.Builder(this@MainActivity)
                                    .setTitle("Transfer Error")
                                    .setMessage(e.message)
                                    .setPositiveButton(android.R.string.ok, null)
                                    .show()
                            }
                        }
                    } finally {
                        socket?.close()
                    }
                }

                dialog.setOnDismissListener {
                    transferJob.cancel()
                }
                dialog.show()
            }
        }
    }

    private fun showSettingsDialog() {
        val prefs = getSharedPreferences("settings", MODE_PRIVATE)
        val currentTranscribe = prefs.getBoolean("transcribe_immediately", false)
        val currentKeepAudio = prefs.getBoolean("keep_audio", false)
        val currentLanguage = prefs.getString("language", "en") ?: "en"
        val currentWords = prefs.getInt("title_word_limit", 6)
        val currentPrompt = prefs.getString("whisper_prompt", "") ?: ""
        val currentTransferEnabled = prefs.getBoolean("transfer_enabled", true)
        val currentServerAddress = prefs.getString("server_address", "") ?: ""
        val currentServerPort = prefs.getInt("server_port", 45437)
        val currentDeleteAfterTransfer = prefs.getBoolean("delete_after_transfer", false)
        
        val dialogView = layoutInflater.inflate(R.layout.dialog_settings, null)
        val checkTranscribe = dialogView.findViewById<CheckBox>(R.id.check_transcribe_immediately)
        val checkKeepAudio = dialogView.findViewById<CheckBox>(R.id.check_keep_audio)
        val spinnerLanguage = dialogView.findViewById<Spinner>(R.id.spinner_language)
        val editWords = dialogView.findViewById<EditText>(R.id.edit_title_word_limit)
        val editPrompt = dialogView.findViewById<EditText>(R.id.edit_whisper_prompt)
        val checkTransferEnabled = dialogView.findViewById<CheckBox>(R.id.check_transfer_enabled)
        val checkDeleteAfterTransfer = dialogView.findViewById<CheckBox>(R.id.check_delete_after_transfer)
        val editServerAddress = dialogView.findViewById<EditText>(R.id.edit_server_address)
        val editServerPort = dialogView.findViewById<EditText>(R.id.edit_server_port)
        
        checkTranscribe.isChecked = currentTranscribe
        checkKeepAudio.isChecked = currentKeepAudio
        editWords.setText(currentWords.toString())
        editPrompt.setText(currentPrompt)
        checkTransferEnabled.isChecked = currentTransferEnabled
        checkDeleteAfterTransfer.isChecked = currentDeleteAfterTransfer
        editServerAddress.setText(currentServerAddress)
        editServerPort.setText(currentServerPort.toString())

        val adapter = ArrayAdapter.createFromResource(
            this, R.array.languages, android.R.layout.simple_spinner_item
        )
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        spinnerLanguage.adapter = adapter
        
        val languageCodes = resources.getStringArray(R.array.language_codes)
        val initialPosition = languageCodes.indexOf(currentLanguage).coerceAtLeast(0)
        spinnerLanguage.setSelection(initialPosition)

        AlertDialog.Builder(this)
            .setTitle(R.string.action_settings)
            .setView(dialogView)
            .setPositiveButton(android.R.string.ok) { _, _ ->
                val words = editWords.text.toString().toIntOrNull() ?: 6
                val prompt = editPrompt.text.toString()
                val selectedLang = languageCodes[spinnerLanguage.selectedItemPosition]
                val serverAddress = editServerAddress.text.toString()
                val serverPort = editServerPort.text.toString().toIntOrNull() ?: 45437

                prefs.edit {
                    putBoolean("transcribe_immediately", checkTranscribe.isChecked)
                    putBoolean("keep_audio", checkKeepAudio.isChecked)
                    putString("language", selectedLang)
                    putInt("title_word_limit", words)
                    putString("whisper_prompt", prompt)
                    putBoolean("transfer_enabled", checkTransferEnabled.isChecked)
                    putBoolean("delete_after_transfer", checkDeleteAfterTransfer.isChecked)
                    putString("server_address", serverAddress)
                    putInt("server_port", serverPort)
                }
                invalidateOptionsMenu()
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    private enum class ExportFormat {
        JSON, MARKDOWN
    }

    private fun showExportFormatDialog(filterIds: List<Long>? = null) {
        val options = arrayOf(getString(R.string.format_json), getString(R.string.format_markdown))
        AlertDialog.Builder(this)
            .setTitle(R.string.export_format)
            .setItems(options) { _, which ->
                val format = if (which == 0) ExportFormat.JSON else ExportFormat.MARKDOWN
                shareTranscripts(format, filterIds)
            }
            .show()
    }

    private fun shareTranscripts(format: ExportFormat = ExportFormat.JSON, filterIds: List<Long>? = null) {
        val dbHelper = DatabaseHelper(this)
        lifecycleScope.launch(Dispatchers.IO) {
            var ideas = dbHelper.getAllIdeas()
            if (filterIds != null) {
                ideas = ideas.filter { filterIds.contains(it.id) }
            }
            
            if (ideas.isEmpty()) return@launch

            val sdf = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())

            val (fileName, mimeType, content) = when (format) {
                ExportFormat.JSON -> {
                    val jsonArray = JSONArray()
                    for (idea in ideas) {
                        val jsonObject = JSONObject()
                        jsonObject.put("title", idea.name ?: "Untitled Idea")
                        jsonObject.put("content", idea.transcript ?: "")
                        jsonObject.put("date", sdf.format(Date(idea.createdAt)))
                        jsonArray.put(jsonObject)
                    }
                    val name = if (filterIds == null) "all_ideas.json" else "selected_ideas.json"
                    Triple(name, "application/json", jsonArray.toString(2))
                }
                ExportFormat.MARKDOWN -> {
                    val sb = StringBuilder()
                    for ((index, idea) in ideas.withIndex()) {
                        sb.append("# ${idea.name ?: "Untitled Idea"}\n\n")
                        sb.append("${idea.transcript ?: ""}\n\n")
                        sb.append("*captured: ${sdf.format(Date(idea.createdAt))}*\n")
                        if (index < ideas.size - 1) {
                            sb.append("\n---\n\n")
                        }
                    }
                    val name = if (filterIds == null) "all_ideas.md" else "selected_ideas.md"
                    Triple(name, "text/markdown", sb.toString())
                }
            }

            val shareFile = File(cacheDir, fileName)
            shareFile.writeText(content)

            withContext(Dispatchers.Main) {
                val contentUri = FileProvider.getUriForFile(
                    this@MainActivity,
                    "eu.lastviking.android.smtool.talk.fileprovider",
                    shareFile
                )

                val shareIntent = android.content.Intent(android.content.Intent.ACTION_SEND).apply {
                    type = mimeType
                    putExtra(android.content.Intent.EXTRA_STREAM, contentUri)
                    putExtra(android.content.Intent.EXTRA_SUBJECT, "SMtool Ideas Export")
                    putExtra(android.content.Intent.EXTRA_TITLE, if (filterIds == null) getString(R.string.share_all) else getString(R.string.share_selected))
                    clipData = android.content.ClipData.newRawUri(null, contentUri)
                    addFlags(android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION)
                }
                val title = if (filterIds == null) getString(R.string.share_all) else getString(R.string.share_selected)
                startActivity(android.content.Intent.createChooser(shareIntent, title))
            }
        }
    }

    fun updateSelectionState(hasSelection: Boolean) {
        invalidateOptionsMenu()
    }

    private fun showDeleteAllConfirmation() {
        AlertDialog.Builder(this)
            .setTitle(R.string.delete_all)
            .setMessage(R.string.delete_all_confirmation)
            .setPositiveButton(android.R.string.ok) { _, _ ->
                val navHostFragment =
                    supportFragmentManager.findFragmentById(R.id.nav_host_fragment_content_main) as NavHostFragment
                val firstFragment = navHostFragment.childFragmentManager.fragments.firstOrNull { it is FirstFragment } as? FirstFragment
                firstFragment?.deleteAllIdeas()
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    override fun onSupportNavigateUp(): Boolean {
        val navController = findNavController(R.id.nav_host_fragment_content_main)
        return navController.navigateUp(appBarConfiguration)
                || super.onSupportNavigateUp()
    }
}
