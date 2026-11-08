package com.novaengine.app.ui.editor

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle

/** Right panel: node inspector — transform fields, properties, script editor. */
@Composable
fun InspectorPanel(vm: EditorViewModel) {
    val node by vm.selectedNode.collectAsStateWithLifecycle()

    Column(Modifier.fillMaxWidth().padding(8.dp)) {
        Text("Inspector", style = MaterialTheme.typography.titleMedium)
        Spacer(Modifier.height(8.dp))

        if (node == null) {
            Text(
                "No node selected",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            return
        }

        // --- Transform ------------------------------------------------------
        Text("Transform", style = MaterialTheme.typography.labelLarge)
        Row(Modifier.padding(top = 4.dp)) {
            FloatField("X", node.posX) { vm.setTransform(node.id, it, node.posY, node.rot, node.scaleX, node.scaleY) }
            FloatField("Y", node.posY) { vm.setTransform(node.id, node.posX, it, node.rot, node.scaleX, node.scaleY) }
        }
        Row {
            FloatField("R", node.rot) { vm.setTransform(node.id, node.posX, node.posY, it, node.scaleX, node.scaleY) }
            FloatField("SX", node.scaleX) { vm.setTransform(node.id, node.posX, node.posY, node.rot, it, node.scaleY) }
            FloatField("SY", node.scaleY) { vm.setTransform(node.id, node.posX, node.posY, node.rot, node.scaleX, it) }
        }

        Spacer(Modifier.height(12.dp))

        // --- Script editor ---------------------------------------------------
        Text("Script", style = MaterialTheme.typography.labelLarge)
        ScriptEditor(node.id, onAttach = vm::attachScript)
    }
}

@Composable
private fun FloatField(label: String, value: Float, onChange: (Float) -> Unit) {
    Column(Modifier.width(64.dp)) {
        OutlinedTextField(
            value = String.format("%.1f", value),
            onValueChange = { onChange(it.toFloatOrNull() ?: 0f) },
            label = { Text(label) },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
        )
    }
}
