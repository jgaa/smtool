package eu.lastviking.android.smtool

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import eu.lastviking.android.smtool.databinding.ItemTargetBinding
import eu.lastviking.android.smtool.db.Target

class TargetAdapter(
    private var targets: List<Target>,
    private val onEdit: (Target) -> Unit,
    private val onDelete: (Target) -> Unit
) : RecyclerView.Adapter<TargetAdapter.TargetViewHolder>() {

    class TargetViewHolder(val binding: ItemTargetBinding) : RecyclerView.ViewHolder(binding.root)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TargetViewHolder {
        val binding = ItemTargetBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return TargetViewHolder(binding)
    }

    override fun onBindViewHolder(holder: TargetViewHolder, position: Int) {
        val target = targets[position]
        holder.binding.textTargetName.text = target.name
        holder.binding.textTargetServer.text = "${target.serverAddress.ifBlank { "No server" }}:${target.serverPort}"

        holder.binding.buttonEditTarget.setOnClickListener {
            onEdit(target)
        }

        holder.binding.buttonDeleteTarget.setOnClickListener {
            onDelete(target)
        }
    }

    override fun getItemCount() = targets.size

    fun updateData(newTargets: List<Target>) {
        targets = newTargets
        notifyDataSetChanged()
    }
}
