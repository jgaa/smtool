package eu.lastviking.android.smtool

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import eu.lastviking.android.smtool.databinding.ItemIdeaBinding
import eu.lastviking.android.smtool.db.Idea
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class IdeaAdapter(
    private var ideas: List<Idea>,
    private val onLongClick: (Idea) -> Unit,
    private val onSelectionChanged: (Boolean) -> Unit
) : RecyclerView.Adapter<IdeaAdapter.IdeaViewHolder>() {

    private val selectedIds = mutableSetOf<Long>()

    class IdeaViewHolder(val binding: ItemIdeaBinding) : RecyclerView.ViewHolder(binding.root)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): IdeaViewHolder {
        val binding = ItemIdeaBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return IdeaViewHolder(binding)
    }

    override fun onBindViewHolder(holder: IdeaViewHolder, position: Int) {
        val idea = ideas[position]
        val sdf = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault())
        
        holder.binding.textIdeaName.text = idea.name ?: "Untitled Idea"
        holder.binding.textIdeaDate.text = sdf.format(Date(idea.createdAt))
        holder.binding.textIdeaStatus.text = "Status: ${idea.status.replaceFirstChar { it.uppercase() }}"
        holder.binding.textIdeaExported.text = "Exported: ${if (idea.exported) "Yes" else "No"}"

        val isSelected = selectedIds.contains(idea.id)
        holder.binding.imageSelected.visibility = if (isSelected) View.VISIBLE else View.GONE

        holder.itemView.setOnClickListener {
            if (isSelected) {
                selectedIds.remove(idea.id)
            } else {
                selectedIds.add(idea.id)
            }
            notifyItemChanged(position)
            onSelectionChanged(selectedIds.isNotEmpty())
        }

        holder.itemView.setOnLongClickListener {
            onLongClick(idea)
            true
        }
    }

    override fun getItemCount() = ideas.size

    fun updateData(newIdeas: List<Idea>) {
        ideas = newIdeas
        notifyDataSetChanged()
    }

    fun getSelectedIds(): List<Long> = selectedIds.toList()

    fun clearSelection() {
        selectedIds.clear()
        notifyDataSetChanged()
        onSelectionChanged(false)
    }
}
