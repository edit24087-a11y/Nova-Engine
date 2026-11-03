package com.novaengine.app.ui.editor

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp

/** Asset browser: lists project asset directories (textures, models, audio...). */
@Composable
fun AssetsPanel() {
    Column(Modifier.fillMaxWidth().padding(8.dp)) {
        Text("Assets", style = MaterialTheme.typography.titleMedium)
        Spacer(Modifier.height(8.dp))
        val categories = listOf(
            "textures", "models", "audio", "fonts", "materials",
            "animations", "prefabs", "plugins",
        )
        LazyColumn(Modifier.fillMaxWidth().height(400.dp)) {
            items(categories) { cat ->
                Text(
                    "$cat/", style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurface,
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(34.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .background(MaterialTheme.colorScheme.surfaceVariant)
                        .padding(horizontal = 12.dp),
                )
            }
        }
    }
}
