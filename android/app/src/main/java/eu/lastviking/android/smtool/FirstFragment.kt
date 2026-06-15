package eu.lastviking.android.smtool

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.content.res.ColorStateList
import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.core.content.ContextCompat
import androidx.core.content.edit
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import com.google.android.material.progressindicator.LinearProgressIndicator
import eu.lastviking.android.smtool.databinding.DialogNewIdeaBinding
import eu.lastviking.android.smtool.databinding.FragmentFirstBinding
import eu.lastviking.android.smtool.db.DatabaseHelper
import eu.lastviking.android.smtool.db.Idea
import eu.lastviking.android.smtool.util.AudioRecorder
import eu.lastviking.android.smtool.util.ModelDownloader
import eu.lastviking.android.smtool.util.WhisperManager
import kotlinx.coroutines.*
import java.io.File

class FirstFragment : Fragment() {

    private var _binding: FragmentFirstBinding? = null
    private val binding get() = _binding!!

    private lateinit var audioRecorder: AudioRecorder
    private lateinit var whisperManager: WhisperManager
    private lateinit var dbHelper: DatabaseHelper
    private lateinit var adapter: IdeaAdapter

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { isGranted: Boolean ->
        if (isGranted) {
            showNewIdeaDialog()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        audioRecorder = AudioRecorder()
        whisperManager = WhisperManager(requireContext())
        dbHelper = DatabaseHelper(requireContext())
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentFirstBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        adapter = IdeaAdapter(emptyList(), { idea ->
            val bundle = Bundle().apply { putLong("ideaId", idea.id) }
            findNavController().navigate(R.id.action_FirstFragment_to_EditIdeaFragment, bundle)
        }, { hasSelection ->
            (requireActivity() as? MainActivity)?.updateSelectionState(hasSelection)
        })
        binding.recyclerIdeas.adapter = adapter

        binding.fabAddIdea.setOnClickListener {
            checkPermissionAndShowDialog()
        }

        refreshIdeas()
        checkModel()
    }

    fun getSelectedIds(): List<Long> {
        return if (::adapter.isInitialized) adapter.getSelectedIds() else emptyList()
    }

    private fun refreshIdeas() {
        lifecycleScope.launch(Dispatchers.IO) {
            val ideas = dbHelper.getAllIdeas()
            withContext(Dispatchers.Main) {
                adapter.updateData(ideas)
            }
        }
    }

    private fun checkModel() {
        lifecycleScope.launch {
            if (!whisperManager.isModelAvailable()) {
                binding.textModelStatus.visibility = View.VISIBLE
                binding.fabAddIdea.isEnabled = false
                AlertDialog.Builder(requireContext())
                    .setTitle("Download Model")
                    .setMessage("Whisper model is required for transcription. Download now?")
                    .setPositiveButton("Download") { _, _ -> downloadModel() }
                    .setNegativeButton("Later", null)
                    .show()
            } else {
                binding.textModelStatus.visibility = View.GONE
                binding.fabAddIdea.isEnabled = true
            }
        }
    }

    private fun downloadModel() {
        binding.progressDownload.visibility = View.VISIBLE
        binding.progressDownload.progress = 0
        whisperManager.downloadModel(object : ModelDownloader.DownloadListener {
            override fun onProgress(progress: Int) {
                lifecycleScope.launch { binding.progressDownload.progress = progress }
            }
            override fun onComplete(file: File) {
                lifecycleScope.launch {
                    binding.progressDownload.visibility = View.GONE
                    binding.textModelStatus.visibility = View.GONE
                    binding.fabAddIdea.isEnabled = true
                }
            }
            override fun onError(e: Exception) {
                lifecycleScope.launch {
                    binding.progressDownload.visibility = View.GONE
                    AlertDialog.Builder(requireContext()).setMessage("Error downloading model: ${e.message}").show()
                }
            }
        })
    }

    private fun checkPermissionAndShowDialog() {
        if (ContextCompat.checkSelfPermission(requireContext(), Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            showNewIdeaDialog()
        } else {
            requestPermissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
        }
    }

    private fun showNewIdeaDialog() {
        val dialogBinding = DialogNewIdeaBinding.inflate(layoutInflater)
        val dialog = AlertDialog.Builder(requireContext())
            .setView(dialogBinding.root)
            .setCancelable(false)
            .create()

        var isPaused = false
        val spoolDir = File(requireContext().cacheDir, "spool")
        spoolDir.mkdirs()
        val audioFile = File(spoolDir, "recording_${System.currentTimeMillis()}.wav")

        // Start recording immediately
        audioRecorder.startRecording(audioFile)

        val vuUpdateJob = lifecycleScope.launch {
            val vuViews = listOf(
                dialogBinding.vu1, dialogBinding.vu2, dialogBinding.vu3,
                dialogBinding.vu4, dialogBinding.vu5
            )
            while (isActive) {
                val amp = audioRecorder.getMaxAmplitude()
                // Max amplitude for 16-bit PCM is 32767.
                // We map this to 0-5 levels for our VU meter.
                val level = (amp / 6554.0).toInt().coerceIn(0, 5)
                
                withContext(Dispatchers.Main) {
                    vuViews.forEachIndexed { index, view ->
                        view.alpha = if (index < level) 1.0f else 0.3f
                    }
                }
                delay(100)
            }
        }

        dialogBinding.buttonPauseResume.setOnClickListener {
            if (!isPaused) {
                audioRecorder.pauseRecording()
                isPaused = true
                dialogBinding.buttonPauseResume.text = getString(R.string.resume)
                dialogBinding.buttonPauseResume.backgroundTintList = ColorStateList.valueOf(ContextCompat.getColor(requireContext(), R.color.green))
                dialogBinding.textRecordingStatus.text = getString(R.string.paused)
            } else {
                audioRecorder.resumeRecording()
                isPaused = false
                dialogBinding.buttonPauseResume.text = getString(R.string.pause)
                dialogBinding.buttonPauseResume.backgroundTintList = ColorStateList.valueOf(ContextCompat.getColor(requireContext(), R.color.yellow))
                dialogBinding.textRecordingStatus.text = getString(R.string.recording)
            }
        }

        dialogBinding.buttonDone.setOnClickListener {
            vuUpdateJob.cancel()
            audioRecorder.stopRecording()
            val name = dialogBinding.editIdeaName.text.toString().ifBlank { null }
            
            val prefs = requireContext().getSharedPreferences("settings", Context.MODE_PRIVATE)
            val transcribeImmediately = prefs.getBoolean("transcribe_immediately", false)

            lifecycleScope.launch(Dispatchers.IO) {
                val status = if (transcribeImmediately) "recording" else "pending"
                val newIdea = Idea(name = name, createdAt = System.currentTimeMillis(), status = status)
                val id = dbHelper.insertIdea(newIdea)
                
                withContext(Dispatchers.Main) {
                    dialog.dismiss()
                    refreshIdeas()
                    if (transcribeImmediately) {
                        transcribeIdea(id, audioFile)
                    } else {
                        // Move file to permanent storage
                        val recordingsDir = File(requireContext().filesDir, "recordings")
                        recordingsDir.mkdirs()
                        val permanentFile = File(recordingsDir, "idea_${id}.wav")
                        audioFile.renameTo(permanentFile)
                    }
                }
            }
        }

        dialogBinding.buttonCancel.setOnClickListener {
            vuUpdateJob.cancel()
            audioRecorder.stopRecording()
            if (audioFile.exists()) audioFile.delete()
            dialog.dismiss()
        }

        dialog.show()
    }

    fun transcribeAllPending() {
        lifecycleScope.launch {
            val pendingIdeas = withContext(Dispatchers.IO) { dbHelper.getPendingIdeas() }
            if (pendingIdeas.isEmpty()) return@launch

            val progressIndicator = LinearProgressIndicator(requireContext()).apply {
                this.max = pendingIdeas.size
                this.progress = 0
            }

            var transcriptionJob: Job? = null
            val dialog = AlertDialog.Builder(requireContext())
                .setTitle(R.string.transcribing_ideas)
                .setView(progressIndicator)
                .setNegativeButton(R.string.cancel) { _, _ ->
                    transcriptionJob?.cancel()
                }
                .setCancelable(false)
                .create()
            
            dialog.show()

            transcriptionJob = lifecycleScope.launch {
                pendingIdeas.forEachIndexed { index, idea ->
                    val recordingsDir = File(requireContext().filesDir, "recordings")
                    val audioFile = File(recordingsDir, "idea_${idea.id}.wav")
                    
                    if (audioFile.exists()) {
                        val prefs = requireContext().getSharedPreferences("settings", Context.MODE_PRIVATE)
                        val lang = prefs.getString("language", "en") ?: "en"
                        Log.d("FirstFragment", "Bulk transcription: idea ${idea.id}, lang: $lang")
                        val transcript = whisperManager.transcribe(audioFile, lang)
                        
                        if (transcript != null) {
                            withContext(Dispatchers.IO) {
                                var finalName = idea.name
                                if (finalName.isNullOrBlank()) {
                                    val prefs = requireContext().getSharedPreferences("settings", android.content.Context.MODE_PRIVATE)
                                    val wordLimit = prefs.getInt("title_word_limit", 6)
                                    finalName = deduceTitle(transcript, wordLimit)
                                }
                                dbHelper.updateIdeaWithTranscriptAndName(idea.id, transcript, finalName, System.currentTimeMillis())
                                
                                val prefs = requireContext().getSharedPreferences("settings", android.content.Context.MODE_PRIVATE)
                                val keepAudio = prefs.getBoolean("keep_audio", false)
                                if (!keepAudio) {
                                    audioFile.delete()
                                }
                            }
                        }
                        withContext(Dispatchers.Main) {
                            progressIndicator.progress = index + 1
                            refreshIdeas()
                        }
                    }
                }
                dialog.dismiss()
            }
        }
    }

    fun deleteAllIdeas() {
        lifecycleScope.launch(Dispatchers.IO) {
            dbHelper.deleteAllIdeas()
            // Delete all audio files
            val recordingsDir = File(requireContext().filesDir, "recordings")
            recordingsDir.listFiles()?.forEach { it.delete() }
            
            // Delete any temporary recordings in spool
            val spoolDir = File(requireContext().cacheDir, "spool")
            spoolDir.listFiles()?.forEach { it.delete() }
            
            withContext(Dispatchers.Main) {
                refreshIdeas()
            }
        }
    }

    private fun transcribeIdea(ideaId: Long, audioFile: File) {
        lifecycleScope.launch {
            val prefs = requireContext().getSharedPreferences("settings", Context.MODE_PRIVATE)
            val lang = prefs.getString("language", "en") ?: "en"
            Log.d("FirstFragment", "Immediate transcription: idea $ideaId, lang: $lang")
            val transcript = whisperManager.transcribe(audioFile, lang)
            
            withContext(Dispatchers.IO) {
                if (transcript != null) {
                    val idea = dbHelper.getAllIdeas().find { it.id == ideaId }
                    var finalName = idea?.name
                    
                    if (finalName.isNullOrBlank()) {
                        val prefs = requireContext().getSharedPreferences("settings", android.content.Context.MODE_PRIVATE)
                        val wordLimit = prefs.getInt("title_word_limit", 6)
                        finalName = deduceTitle(transcript, wordLimit)
                    }
                    
                    dbHelper.updateIdeaWithTranscriptAndName(ideaId, transcript, finalName, System.currentTimeMillis())
                    
                    val prefs = requireContext().getSharedPreferences("settings", android.content.Context.MODE_PRIVATE)
                    val keepAudio = prefs.getBoolean("keep_audio", false)
                    if (!keepAudio) {
                        if (audioFile.exists()) audioFile.delete()
                    }
                } else {
                    // Transcription failed. Move to recordings and set to pending.
                    val recordingsDir = File(requireContext().filesDir, "recordings")
                    recordingsDir.mkdirs()
                    val permanentFile = File(recordingsDir, "idea_${ideaId}.wav")
                    audioFile.renameTo(permanentFile)
                    dbHelper.updateIdeaStatus(ideaId, "pending")
                }
            }
            refreshIdeas()
        }
    }

    private fun deduceTitle(transcript: String, limit: Int): String {
        val firstSentence = transcript.trim().split(Regex("[.!?]")).firstOrNull() ?: transcript
        val words = firstSentence.trim().split(Regex("\\s+"))
        return if (words.size <= limit) {
            firstSentence.trim()
        } else {
            words.take(limit).joinToString(" ") + "..."
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
