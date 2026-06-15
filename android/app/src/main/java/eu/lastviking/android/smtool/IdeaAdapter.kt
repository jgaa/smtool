package eu.lastviking.android.smtool

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import eu.lastviking.android.smtool.databinding.ItemIdeaBinding
import eu.lastviking.android.smtool.db.Idea
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class IdeaAdapter(
    private var ideas: List<Idea>,
    private val onLongClick: (Idea) -> Unit
) : RecyclerView.Adapter<IdeaAdapter.IdeaViewHolder>() {

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
}
