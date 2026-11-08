package com.novaengine.app.ui.editor

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.unit.dp

/** Timeline panel: animation tracks (placeholder tracks for Phase 3). */
@Composable
fun TimelinePanel() {
    Column(Modifier.fillMaxWidth().padding(8.dp)) {
        Text("Timeline", style = MaterialTheme.typography.titleMedium)
        Spacer(Modifier.height(8.dp))
        Canvas(
            modifier = Modifier
                .fillMaxWidth()
                .height(200.dp)
                .clip(RoundedCornerShape(12.dp))
                .background(MaterialTheme.colorScheme.background),
            onDraw = {
                drawTracks()
            },
        )
    }
}

private fun DrawScope.drawTracks() {
    // Track header column
    drawRect(Color(0xFF1B2029), topLeft = Offset(0, 0), size = Size(80.dp.toFloat() * density, size.height))
    // Timeline ruler
    for (i in 0..12) {
        val x = 80.dp.toFloat() * density + i * 40.dp.toFloat() * density
        drawLine(Color(0xFF333A47), Offset(x, 0), Offset(x, size.height))
    }
    // Sample keyframe track
    drawRect(
        Color(0xFF00E5FF),
        topLeft = Offset(90.dp.toFloat() * density, 40.dp.toFloat() * density),
        size = Size(160.dp.toFloat() * density, 12.dp.toFloat() * density),
    )
    drawCircle(Color(0xFFFF6BCB), Offset(90.dp.toFloat() * density, 46.dp.toFloat() * density), 4.dp.toFloat() * density)
}
