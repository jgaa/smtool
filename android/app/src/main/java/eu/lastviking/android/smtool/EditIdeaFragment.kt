package eu.lastviking.android.smtool

import android.content.ClipboardManager
import android.content.ClipData
import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import eu.lastviking.android.smtool.databinding.FragmentEditIdeaBinding
import eu.lastviking.android.smtool.db.DatabaseHelper
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class EditIdeaFragment : Fragment() {

    private var _binding: FragmentEditIdeaBinding? = null
    private val binding get() = _binding!!
    private lateinit var dbHelper: DatabaseHelper
    private var ideaId: Long = -1

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        ideaId = arguments?.getLong("ideaId") ?: -1
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentEditIdeaBinding.inflate(inflater, container, false)
        dbHelper = DatabaseHelper(requireContext())
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        lifecycleScope.launch(Dispatchers.IO) {
            val idea = dbHelper.getAllIdeas().find { it.id == ideaId }
            withContext(Dispatchers.Main) {
                idea?.let {
                    binding.editIdeaName.setText(it.name)
                    binding.editIdeaTranscript.setText(it.transcript)
                    binding.textIdeaUuid.text = getString(R.string.uuid_label, it.uuid)
                }
            }
        }

        binding.buttonCopy.setOnClickListener {
            val clipboard = requireContext().getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            val clip = ClipData.newPlainText("Idea Transcript", binding.editIdeaTranscript.text)
            clipboard.setPrimaryClip(clip)
            Toast.makeText(requireContext(), "Copied to clipboard", Toast.LENGTH_SHORT).show()
        }

        binding.buttonDelete.setOnClickListener {
            lifecycleScope.launch(Dispatchers.IO) {
                dbHelper.deleteIdea(ideaId)
                withContext(Dispatchers.Main) {
                    findNavController().navigateUp()
                }
            }
        }
    }

    override fun onPause() {
        super.onPause()
        if (ideaId != -1L) {
            val name = binding.editIdeaName.text.toString()
            val transcript = binding.editIdeaTranscript.text.toString()
            lifecycleScope.launch(Dispatchers.IO) {
                dbHelper.updateIdea(ideaId, name, transcript)
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
