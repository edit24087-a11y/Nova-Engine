package com.novaengine.app.ui.editor

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle

/** Debugger panel: engine stats (FPS, physics bodies, draw calls, script errors). */
@Composable
fun DebuggerPanel(vm: EditorViewModel) {
    val stats by vm.stats.collectAsStateWithLifecycle()
    LaunchedEffect(Unit) {
        vm.refreshStats()
    }

    Column(Modifier.fillMaxWidth().padding(8.dp)) {
        Text("Debugger", style = MaterialTheme.typography.titleMedium)
        Spacer(Modifier.height(8.dp))
        StatRow("FPS", stats.fps.toString())
        StatRow("Physics bodies", stats.physicsBodies.toString())
        StatRow("Draw calls", stats.drawCalls.toString())
        StatRow("Script errors", stats.scriptErrors.toString())
    }
}

@Composable
private fun StatRow(label: String, value: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(32.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(MaterialTheme.colorScheme.surfaceVariant)
            .padding(horizontal = 12.dp),
        verticalAlignment = androidx.compose.ui.Alignment.CenterVertically,
    ) {
        Text(
            label, style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.weight(1f),
        )
        Text(
            value, style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurface,
            fontWeight = androidx.compose.ui.text.font.FontWeight.SemiBold,
        )
    }
}
