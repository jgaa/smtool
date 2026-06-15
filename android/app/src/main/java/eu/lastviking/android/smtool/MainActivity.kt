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
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

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
                showExportFormatDialog()
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
            else -> super.onOptionsItemSelected(item)
        }
    }

    private fun showSettingsDialog() {
        val prefs = getSharedPreferences("settings", MODE_PRIVATE)
        val currentTranscribe = prefs.getBoolean("transcribe_immediately", false)
        val currentKeepAudio = prefs.getBoolean("keep_audio", false)
        val currentLanguage = prefs.getString("language", "en") ?: "en"
        val currentWords = prefs.getInt("title_word_limit", 6)
        
        val dialogView = layoutInflater.inflate(R.layout.dialog_settings, null)
        val checkTranscribe = dialogView.findViewById<CheckBox>(R.id.check_transcribe_immediately)
        val checkKeepAudio = dialogView.findViewById<CheckBox>(R.id.check_keep_audio)
        val spinnerLanguage = dialogView.findViewById<Spinner>(R.id.spinner_language)
        val editWords = dialogView.findViewById<EditText>(R.id.edit_title_word_limit)
        
        checkTranscribe.isChecked = currentTranscribe
        checkKeepAudio.isChecked = currentKeepAudio
        editWords.setText(currentWords.toString())

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
                val selectedLang = languageCodes[spinnerLanguage.selectedItemPosition]
                prefs.edit {
                    putBoolean("transcribe_immediately", checkTranscribe.isChecked)
                    putBoolean("keep_audio", checkKeepAudio.isChecked)
                    putString("language", selectedLang)
                    putInt("title_word_limit", words)
                }
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    private enum class ExportFormat {
        JSON, MARKDOWN
    }

    private fun showExportFormatDialog() {
        val options = arrayOf(getString(R.string.format_json), getString(R.string.format_markdown))
        AlertDialog.Builder(this)
            .setTitle(R.string.export_format)
            .setItems(options) { _, which ->
                val format = if (which == 0) ExportFormat.JSON else ExportFormat.MARKDOWN
                shareAllTranscripts(format)
            }
            .show()
    }

    private fun shareAllTranscripts(format: ExportFormat = ExportFormat.JSON) {
        val dbHelper = DatabaseHelper(this)
        lifecycleScope.launch(Dispatchers.IO) {
            val ideas = dbHelper.getAllIdeas()
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
                    Triple("all_ideas.json", "application/json", jsonArray.toString(2))
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
                    Triple("all_ideas.md", "text/markdown", sb.toString())
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
                    putExtra(android.content.Intent.EXTRA_TITLE, getString(R.string.share_all))
                    clipData = android.content.ClipData.newRawUri(null, contentUri)
                    addFlags(android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION)
                }
                startActivity(android.content.Intent.createChooser(shareIntent, getString(R.string.share_all)))
            }
        }
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
