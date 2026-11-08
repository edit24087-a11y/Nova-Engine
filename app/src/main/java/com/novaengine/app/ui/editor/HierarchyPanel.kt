package com.novaengine.app.ui.editor

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.ChevronRight
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.novaengine.app.engine.NovaEngineBridge
import com.novaengine.app.engine.NovaParsers

/** Left panel: scene hierarchy with add/select/delete. */
@Composable
fun HierarchyPanel(vm: EditorViewModel) {
    val hierarchy by vm.hierarchy.collectAsStateWithLifecycle()
    var expandDepth by remember { mutableStateOf(0) }

    Column(Modifier.fillMaxSize()) {
        Text(
            text = "Hierarchy",
            modifier = Modifier.padding(12.dp),
            style = MaterialTheme.typography.titleMedium,
        )
        Row(Modifier.padding(12.dp)) {
            IconButton(onClick = { expandDepth = 0 }) {
                Icon(Icons.Default.Add, contentDescription = "Add Node2D")
            }
            IconButton(onClick = { expandDepth = 1 }) {
                Icon(Icons.Default.Add, contentDescription = "Add Node3D")
            }
        }
        LazyColumn(Modifier.fillMaxSize().weight(1f)) {
            items(hierarchy, key = { it.id.toString() }) { row ->
                if (row.depth > expandDepth) return@items
                HierarchyRow(
                    row = row,
                    selected = vm.selectedNode.value?.id == row.id,
                    onSelect = { vm.selectNode(row.id) },
                    onDelete = { vm.deleteNode(row.id) },
                )
            }
        }
    }
}

@Composable
private fun HierarchyRow(
    row: NovaParsers.HierarchyRow,
    selected: Boolean,
    onSelect: () -> Unit,
    onDelete: () -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(36.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(if (selected) MaterialTheme.colorScheme.primaryContainer
            else MaterialTheme.colorScheme.surfaceVariant)
            .clickable(onClick = onSelect)
            .padding(horizontal = 8.dp, vertical = 2.dp),
        verticalAlignment = androidx.compose.ui.Alignment.CenterVertically,
    ) {
        Spacer(Modifier.width(16.dp * row.depth))
        Icon(
            Icons.Default.ChevronRight, null,
            tint = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.size(16.dp),
        )
        Text(
            row.name,
            style = MaterialTheme.typography.bodySmall,
            color = if (selected) MaterialTheme.colorScheme.onPrimaryContainer
            else MaterialTheme.colorScheme.onSurface,
            fontWeight = if (selected) FontWeight.SemiBold else FontWeight.Normal,
            modifier = Modifier.padding(start = 4.dp).weight(1f),
        )
        Text(
            row.type,
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        IconButton(onClick = onDelete) {
            Icon(
                Icons.Default.Delete, null,
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.size(16.dp),
            )
        }
    }
}
