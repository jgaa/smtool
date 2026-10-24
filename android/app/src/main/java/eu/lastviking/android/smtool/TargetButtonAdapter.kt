package eu.lastviking.android.smtool

import android.content.res.ColorStateList
import android.graphics.Color
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import eu.lastviking.android.smtool.databinding.ItemTargetButtonBinding
import eu.lastviking.android.smtool.db.Target

class TargetButtonAdapter(
    private var targets: List<Target>,
    private var selectedTargetId: Long,
    private val onTargetSelected: (Target) -> Unit
) : RecyclerView.Adapter<TargetButtonAdapter.TargetButtonViewHolder>() {

    class TargetButtonViewHolder(val binding: ItemTargetButtonBinding) : RecyclerView.ViewHolder(binding.root)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TargetButtonViewHolder {
        val binding = ItemTargetButtonBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return TargetButtonViewHolder(binding)
    }

    override fun onBindViewHolder(holder: TargetButtonViewHolder, position: Int) {
        val target = targets[position]
        holder.binding.buttonTarget.text = target.name
        val isSelected = target.id == selectedTargetId
        holder.binding.buttonTarget.isChecked = isSelected

        holder.binding.buttonTarget.setOnClickListener {
            if (selectedTargetId != target.id) {
                selectedTargetId = target.id
                notifyDataSetChanged()
                onTargetSelected(target)
            }
        }
    }

    override fun getItemCount() = targets.size

    fun updateData(newTargets: List<Target>, newSelectedId: Long) {
        targets = newTargets
        selectedTargetId = newSelectedId
        notifyDataSetChanged()
    }
}
