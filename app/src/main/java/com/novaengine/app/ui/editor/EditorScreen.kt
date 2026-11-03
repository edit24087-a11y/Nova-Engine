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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.ContentPaste
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Save
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel

enum class EditorPanel { HIERARCHY, ASSETS, CONSOLE, DEBUGGER, TIMELINE }

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun EditorScreen(
    projectPath: String,
    viewModel: EditorViewModel = viewModel(),
) {
    LaunchedEffect(projectPath) { viewModel.open(projectPath) }

    var activePanel by remember { mutableStateOf(EditorPanel.HIERARCHY) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Column {
                        Text(
                            projectPath.substringAfterLast('/').ifBlank { "NovaEngine" },
                            style = MaterialTheme.typography.titleMedium,
                        )
                        Text(
                            "Scene: ${viewModel.sceneName.value}",
                            style = MaterialTheme.typography.labelSmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                },
                navigationIcon = {
                    IconButton(onClick = { /* back to dashboard */ }) {
                        Icon(Icons.Default.ArrowBack, contentDescription = "Back")
                    }
                },
                actions = {
                    IconButton(onClick = { viewModel.saveScene() }) {
                        Icon(Icons.Default.Save, contentDescription = "Save scene")
                    }
                    IconButton(onClick = { viewModel.refreshConsole() }) {
                        Icon(Icons.Default.ContentPaste, contentDescription = "Console")
                    }
                    IconButton(onClick = { viewModel.refreshStats() }) {
                        Icon(Icons.Default.PlayArrow, contentDescription = "Refresh stats")
                    }
                },
            )
        },
        bottomBar = {
            BottomNav(active = activePanel, onSelect = { activePanel = it })
        },
    ) { padding ->
        Row(
            Modifier.fillMaxSize().padding(padding),
        ) {
            // Left panel
            Column(
                Modifier.width(240.dp).fillMaxHeight(),
            ) {
                when (activePanel) {
                    EditorPanel.HIERARCHY -> HierarchyPanel(viewModel)
                    EditorPanel.ASSETS -> AssetsPanel()
                    EditorPanel.DEBUGGER -> DebuggerPanel(viewModel)
                    EditorPanel.TIMELINE -> TimelinePanel()
                    EditorPanel.CONSOLE -> {}
                }
            }

            // Center viewport
            Box(Modifier.weight(1f), contentAlignment = Alignment.Center) {
                ViewportSurface(projectPath, viewModel)
            }

            // Right inspector
            Column(Modifier.width(280.dp).fillMaxHeight()) {
                InspectorPanel(viewModel)
            }
        }
    }

    if (activePanel == EditorPanel.CONSOLE) {
        Box(Modifier.fillMaxSize()) {
            ConsolePanel(viewModel, Modifier.fillMaxWidth().padding(16.dp))
        }
    }
}

@Composable
private fun BottomNav(active: EditorPanel, onSelect: (EditorPanel) -> Unit) {
    Row(
        Modifier.fillMaxWidth().height(56.dp).background(MaterialTheme.colorScheme.surfaceVariant),
        horizontalArrangement = Arrangement.SpaceEvenly,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        EditorPanel.entries.forEach { panel ->
            val selected = panel == active
            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(10.dp))
                    .background(
                        if (selected) MaterialTheme.colorScheme.primaryContainer
                        else androidx.compose.ui.graphics.Color.Transparent,
                    )
                    .clickable { onSelect(panel) }
                    .padding(10.dp),
            ) {
                Text(
                    panel.name,
                    style = MaterialTheme.typography.labelMedium,
                    color = if (selected) MaterialTheme.colorScheme.onPrimaryContainer
                    else MaterialTheme.colorScheme.onSurfaceVariant,
                    fontWeight = if (selected) androidx.compose.ui.text.font.FontWeight.Bold
                    else androidx.compose.ui.text.font.FontWeight.Normal,
                )
            }
        }
    }
}
