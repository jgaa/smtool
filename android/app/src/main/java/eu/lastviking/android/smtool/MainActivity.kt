package eu.lastviking.android.smtool

import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import android.widget.ArrayAdapter
import android.widget.CheckBox
import android.widget.EditText
import android.widget.Spinner
import android.widget.Toast
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
import eu.lastviking.android.smtool.db.Target
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
        menuInflater.inflate(R.menu.menu_main, menu)
        
        val navHostFragment =
            supportFragmentManager.findFragmentById(R.id.nav_host_fragment_content_main) as NavHostFragment
        val firstFragment = navHostFragment.childFragmentManager.fragments.firstOrNull { it is FirstFragment } as? FirstFragment
        val selectedIds = firstFragment?.getSelectedIds() ?: emptyList()
        val hasSelection = selectedIds.isNotEmpty()
        menu.findItem(R.id.action_share_selected)?.isVisible = hasSelection
        
        val prefs = getSharedPreferences("settings", MODE_PRIVATE)
        val dbHelper = DatabaseHelper(this)
        val targets = dbHelper.getAllTargets()
        val transferEnabled = targets.any { it.transferEnabled }
        menu.findItem(R.id.action_transfer)?.isVisible = transferEnabled

        val moveItem = menu.findItem(R.id.action_move_to_target)
        if (moveItem != null) {
            if (targets.size > 1 && hasSelection) {
                moveItem.isVisible = true
                val subMenu = moveItem.subMenu
                subMenu?.clear()
                val currentTargetId = firstFragment?.getCurrentSelectedTargetId() ?: 1
                val otherTargets = targets.filter { it.id != currentTargetId }
                for ((index, target) in otherTargets.withIndex()) {
                    subMenu?.add(Menu.NONE, 2000 + index, index, target.name)
                }
            } else {
                moveItem.isVisible = false
            }
        }
        
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.action_targets -> {
                showTargetManagerDialog()
                true
            }
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
            else -> {
                if (item.itemId >= 2000 && item.itemId < 3000) {
                    val index = item.itemId - 2000
                    val dbHelper = DatabaseHelper(this)
                    val targets = dbHelper.getAllTargets()
                    val navHostFragment =
                        supportFragmentManager.findFragmentById(R.id.nav_host_fragment_content_main) as NavHostFragment
                    val firstFragment = navHostFragment.childFragmentManager.fragments.firstOrNull { it is FirstFragment } as? FirstFragment
                    val currentTargetId = firstFragment?.getCurrentSelectedTargetId() ?: 1
                    val otherTargets = targets.filter { it.id != currentTargetId }
                    val targetToMoveTo = otherTargets.getOrNull(index)
                    val selectedIds = firstFragment?.getSelectedIds() ?: emptyList()

                    if (targetToMoveTo != null && selectedIds.isNotEmpty()) {
                        lifecycleScope.launch(Dispatchers.IO) {
                            val db = dbHelper.writableDatabase
                            for (id in selectedIds) {
                                val values = android.content.ContentValues().apply {
                                    put(DatabaseHelper.COLUMN_TARGET_ID, targetToMoveTo.id)
                                }
                                db.update(DatabaseHelper.TABLE_IDEAS, values, "${DatabaseHelper.COLUMN_ID} = ?", arrayOf(id.toString()))
                            }
                            withContext(Dispatchers.Main) {
                                firstFragment?.clearSelection()
                                firstFragment?.refreshIdeas()
                                Toast.makeText(this@MainActivity, "Moved ideas to target '${targetToMoveTo.name}'", Toast.LENGTH_SHORT).show()
                            }
                        }
                    }
                    true
                } else {
                    super.onOptionsItemSelected(item)
                }
            }
        }
    }

    private fun showTargetManagerDialog() {
        val dialogView = layoutInflater.inflate(R.layout.dialog_target_manager, null)
        val recycler = dialogView.findViewById<androidx.recyclerview.widget.RecyclerView>(R.id.recycler_targets)
        val addButton = dialogView.findViewById<android.widget.Button>(R.id.button_add_target)

        val dbHelper = DatabaseHelper(this)
        val targets = dbHelper.getAllTargets()

        val adapter = TargetAdapter(targets, { target ->
            showEditTargetDialog(target) {
                refreshTargetManagerList(recycler)
                refreshFirstFragment()
            }
        }, { target ->
            val prefs = getSharedPreferences("settings", MODE_PRIVATE)
            val defaultTargetId = prefs.getLong("default_target_id", 1)
            if (target.id == defaultTargetId) {
                AlertDialog.Builder(this)
                    .setTitle("Cannot Delete")
                    .setMessage(R.string.cannot_delete_default_target)
                    .setPositiveButton(android.R.string.ok, null)
                    .show()
                return@TargetAdapter
            }
            AlertDialog.Builder(this)
                .setTitle("Delete Target")
                .setMessage(R.string.delete_target_confirmation)
                .setPositiveButton(android.R.string.ok) { _, _ ->
                    lifecycleScope.launch(Dispatchers.IO) {
                        dbHelper.deleteTarget(target.id, defaultTargetId)
                        withContext(Dispatchers.Main) {
                            refreshTargetManagerList(recycler)
                            refreshFirstFragment()
                        }
                    }
                }
                .setNegativeButton(android.R.string.cancel, null)
                .show()
        })

        recycler.layoutManager = androidx.recyclerview.widget.LinearLayoutManager(this)
        recycler.adapter = adapter

        addButton.setOnClickListener {
            showEditTargetDialog(null) {
                refreshTargetManagerList(recycler)
                refreshFirstFragment()
            }
        }

        AlertDialog.Builder(this)
            .setTitle(R.string.target_manager_title)
            .setView(dialogView)
            .setPositiveButton(android.R.string.ok, null)
            .show()
    }

    private fun refreshTargetManagerList(recycler: androidx.recyclerview.widget.RecyclerView) {
        val dbHelper = DatabaseHelper(this)
        val targets = dbHelper.getAllTargets()
        (recycler.adapter as? TargetAdapter)?.updateData(targets)
    }

    private fun showEditTargetDialog(target: Target?, onSaved: () -> Unit) {
        val dialogView = layoutInflater.inflate(R.layout.dialog_edit_target, null)
        val editName = dialogView.findViewById<EditText>(R.id.edit_target_name)
        val editAddress = dialogView.findViewById<EditText>(R.id.edit_server_address)
        val editPort = dialogView.findViewById<EditText>(R.id.edit_server_port)
        val checkTransfer = dialogView.findViewById<CheckBox>(R.id.check_transfer_enabled)
        val checkDelete = dialogView.findViewById<CheckBox>(R.id.check_delete_after_transfer)

        target?.let {
            editName.setText(it.name)
            editAddress.setText(it.serverAddress)
            editPort.setText(it.serverPort.toString())
            checkTransfer.isChecked = it.transferEnabled
            checkDelete.isChecked = it.deleteAfterTransfer
        } ?: run {
            checkTransfer.isChecked = true
            checkDelete.isChecked = false
            editPort.setText("45437")
        }

        AlertDialog.Builder(this)
            .setTitle(if (target == null) R.string.add_target else R.string.edit_target)
            .setView(dialogView)
            .setPositiveButton(android.R.string.ok) { _, _ ->
                val name = editName.text.toString().trim()
                if (name.isBlank() || name.length > 32) {
                    Toast.makeText(this, "Target name must be between 1 and 32 characters", Toast.LENGTH_SHORT).show()
                    return@setPositiveButton
                }
                val address = editAddress.text.toString().trim()
                val port = editPort.text.toString().toIntOrNull() ?: 45437
                val transfer = checkTransfer.isChecked
                val delete = checkDelete.isChecked

                lifecycleScope.launch(Dispatchers.IO) {
                    val dbHelper = DatabaseHelper(this@MainActivity)
                    if (target == null) {
                        dbHelper.insertTarget(Target(name = name, serverAddress = address, serverPort = port, transferEnabled = transfer, deleteAfterTransfer = delete))
                    } else {
                        dbHelper.updateTarget(target.copy(name = name, serverAddress = address, serverPort = port, transferEnabled = transfer, deleteAfterTransfer = delete))
                    }
                    withContext(Dispatchers.Main) {
                        onSaved()
                    }
                }
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    private fun performTransfer(filterIds: List<Long>? = null) {
        val dbHelper = DatabaseHelper(this)
        lifecycleScope.launch(Dispatchers.IO) {
            val targets = dbHelper.getAllTargets()
            if (targets.isEmpty()) return@launch

            withContext(Dispatchers.Main) {
                val targetNames = targets.map { it.name }.toTypedArray()
                var selectedIndex = 0
                AlertDialog.Builder(this@MainActivity)
                    .setTitle(R.string.transfer_target_prompt)
                    .setSingleChoiceItems(targetNames, selectedIndex) { _, which ->
                        selectedIndex = which
                    }
                    .setPositiveButton(android.R.string.ok) { _, _ ->
                        val chosenTarget = targets[selectedIndex]
                        executeTransferForTarget(chosenTarget, filterIds)
                    }
                    .setNegativeButton(android.R.string.cancel, null)
                    .show()
            }
        }
    }

    private fun executeTransferForTarget(target: Target, filterIds: List<Long>?) {
        val serverAddress = target.serverAddress
        val serverPort = target.serverPort

        if (serverAddress.isBlank() || !target.transferEnabled) {
            AlertDialog.Builder(this)
                .setTitle("Cannot Transfer")
                .setMessage("Target '${target.name}' does not have a valid server address or transfer is disabled.")
                .setPositiveButton(android.R.string.ok, null)
                .show()
            return
        }

        lifecycleScope.launch(Dispatchers.IO) {
            val dbHelper = DatabaseHelper(this@MainActivity)
            val allIdeas = dbHelper.getIdeasForTarget(target.id)
            
            val ideasToTransfer = if (!filterIds.isNullOrEmpty()) {
                val selected = allIdeas.filter { filterIds.contains(it.id) }
                val nonTranscribed = selected.filter { it.status != "transcript" || it.transcript.isNullOrBlank() }
                
                if (nonTranscribed.isNotEmpty()) {
                    withContext(Dispatchers.Main) {
                        AlertDialog.Builder(this@MainActivity)
                            .setTitle("Cannot Transfer")
                            .setMessage("Some selected ideas for this target have not been transcribed yet. Only transcribed text can be transferred.")
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
                        .setMessage("There are no transcribed ideas for target '${target.name}' to transfer.")
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

                        val hello = JSONObject().apply {
                            put("type", "hello")
                            put("app", "smtool-transfer")
                            put("version", 1)
                            put("code", code)
                        }
                        writer.println(hello.toString())

                        val continueLine = reader.readLine() ?: throw Exception("Server closed connection")
                        val continueJson = JSONObject(continueLine)
                        if (continueJson.getString("type") != "continue" || !continueJson.getBoolean("ok")) {
                            throw Exception(continueJson.optString("message", "Rejected by server"))
                        }

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
                                    put("list", target.name)
                                }
                                itemsArray.put(item)
                            }
                            put("items", itemsArray)
                        }
                        writer.println(payload.toString())

                        val resultLine = reader.readLine() ?: throw Exception("Server closed connection during result")
                        val resultJson = JSONObject(resultLine)
                        if (resultJson.getString("type") == "result" && resultJson.getBoolean("ok")) {
                            for (idea in ideasToTransfer) {
                                if (target.deleteAfterTransfer) {
                                    dbHelper.deleteIdea(idea.id)
                                } else {
                                    dbHelper.updateExportedStatus(idea.id, true)
                                }
                            }
                            withContext(Dispatchers.Main) {
                                dialog.dismiss()
                                AlertDialog.Builder(this@MainActivity)
                                    .setTitle(R.string.transfer_success)
                                    .setMessage("Imported ${resultJson.optInt("imported", ideasToTransfer.size)} ideas to target '${target.name}'.")
                                    .setPositiveButton(android.R.string.ok, null)
                                    .show()
                                
                                refreshFirstFragment()
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
        var currentPrompt = prefs.getString("whisper_prompt", "") ?: ""
        val defaultTargetId = prefs.getLong("default_target_id", 1)
        
        val dialogView = layoutInflater.inflate(R.layout.dialog_settings, null)
        val checkTranscribe = dialogView.findViewById<CheckBox>(R.id.check_transcribe_immediately)
        val checkKeepAudio = dialogView.findViewById<CheckBox>(R.id.check_keep_audio)
        val spinnerLanguage = dialogView.findViewById<Spinner>(R.id.spinner_language)
        val editWords = dialogView.findViewById<EditText>(R.id.edit_title_word_limit)
        val buttonWhisperPrompt = dialogView.findViewById<android.widget.Button>(R.id.button_whisper_prompt)
        val spinnerDefaultTarget = dialogView.findViewById<Spinner>(R.id.spinner_default_target)
        
        checkTranscribe.isChecked = currentTranscribe
        checkKeepAudio.isChecked = currentKeepAudio
        editWords.setText(currentWords.toString())

        buttonWhisperPrompt.setOnClickListener {
            showWhisperPromptDialog(currentPrompt) { newPrompt ->
                currentPrompt = newPrompt
            }
        }

        val adapter = ArrayAdapter.createFromResource(
            this, R.array.languages, android.R.layout.simple_spinner_item
        )
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        spinnerLanguage.adapter = adapter
        
        val languageCodes = resources.getStringArray(R.array.language_codes)
        val initialPosition = languageCodes.indexOf(currentLanguage).coerceAtLeast(0)
        spinnerLanguage.setSelection(initialPosition)

        val dbHelper = DatabaseHelper(this)
        val targets = dbHelper.getAllTargets()
        val targetNames = targets.map { it.name }
        val targetAdapter = ArrayAdapter(this, android.R.layout.simple_spinner_item, targetNames)
        targetAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        spinnerDefaultTarget.adapter = targetAdapter

        val defaultTargetIndex = targets.indexOfFirst { it.id == defaultTargetId }.coerceAtLeast(0)
        spinnerDefaultTarget.setSelection(defaultTargetIndex)

        AlertDialog.Builder(this)
            .setTitle(R.string.action_settings)
            .setView(dialogView)
            .setPositiveButton(android.R.string.ok) { _, _ ->
                val words = editWords.text.toString().toIntOrNull() ?: 6
                val selectedLang = languageCodes[spinnerLanguage.selectedItemPosition]
                val selectedTarget = targets.getOrNull(spinnerDefaultTarget.selectedItemPosition)

                prefs.edit {
                    putBoolean("transcribe_immediately", checkTranscribe.isChecked)
                    putBoolean("keep_audio", checkKeepAudio.isChecked)
                    putString("language", selectedLang)
                    putInt("title_word_limit", words)
                    putString("whisper_prompt", currentPrompt)
                    selectedTarget?.let { putLong("default_target_id", it.id) }
                }
                invalidateOptionsMenu()
                refreshFirstFragment()
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    private fun showWhisperPromptDialog(initialPrompt: String, onSaved: (String) -> Unit) {
        val dialogView = layoutInflater.inflate(R.layout.dialog_whisper_prompt, null)
        val editPrompt = dialogView.findViewById<EditText>(R.id.edit_whisper_prompt)
        editPrompt.setText(initialPrompt)

        AlertDialog.Builder(this)
            .setTitle(R.string.pref_whisper_prompt)
            .setView(dialogView)
            .setPositiveButton(android.R.string.ok) { _, _ ->
                onSaved(editPrompt.text.toString())
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

    private fun refreshFirstFragment() {
        val navHostFragment =
            supportFragmentManager.findFragmentById(R.id.nav_host_fragment_content_main) as NavHostFragment
        val firstFragment = navHostFragment.childFragmentManager.fragments.firstOrNull { it is FirstFragment } as? FirstFragment
        firstFragment?.refreshIdeas()
    }

    override fun onSupportNavigateUp(): Boolean {
        val navController = findNavController(R.id.nav_host_fragment_content_main)
        return navController.navigateUp(appBarConfiguration)
                || super.onSupportNavigateUp()
    }
}
