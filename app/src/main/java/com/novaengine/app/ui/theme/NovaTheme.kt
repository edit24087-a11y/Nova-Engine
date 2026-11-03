package com.novaengine.app.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

/**
 * NOVA ENGINE dark theme — professional game-dev aesthetic.
 * Deep charcoal surfaces, neon-cyan accent, high-contrast text.
 */
val NovaDarkColors = darkColorScheme(
    primary = Color(0xFF00E5FF),
    onPrimary = Color(0xFF00222B),
    primaryContainer = Color(0xFF003A44),
    onPrimaryContainer = Color(0xFF8CF3FF),
    secondary = Color(0xFF7C5CFF),
    onSecondary = Color(0xFF17004D),
    secondaryContainer = Color(0xFF2E0066),
    onSecondaryContainer = Color(0xFFE5DBFF),
    tertiary = Color(0xFFFF6BCB),
    onTertiary = Color(0xFF3D0030),
    tertiaryContainer = Color(0xFF571844),
    onTertiaryContainer = Color(0xFFFFD0E6),
    background = Color(0xFF0B0D12),
    onBackground = Color(0xFFE6E9F0),
    surface = Color(0xFF13161D),
    onSurface = Color(0xFFE6E9F0),
    surfaceVariant = Color(0xFF1B2029),
    onSurfaceVariant = Color(0xFFA0AAB8),
    outline = Color(0xFF333A47),
    outlineVariant = Color(0xFF1E2430),
    error = Color(0xFFFF5454),
    onError = Color(0xFF450000),
    errorContainer = Color(0xFF6B0000),
    onErrorContainer = Color(0xFFFFB4AB),
)

val NovaLightColors = lightColorScheme(
    primary = Color(0xFF006E8C),
    onPrimary = Color(0xFFFFFFFF),
    primaryContainer = Color(0xFF6FF3FF),
    onPrimaryContainer = Color(0xFF00202B),
    secondary = Color(0xFF5840C8),
    onSecondary = Color(0xFFFFFFFF),
    secondaryContainer = Color(0xFFE5DBFF),
    onSecondaryContainer = Color(0xFF1A0050),
    tertiary = Color(0xFFB43971),
    onTertiary = Color(0xFFFFFFFF),
    tertiaryContainer = Color(0xFFFFD0E6),
    onTertiaryContainer = Color(0xFF3D0030),
    background = Color(0xFFF4F6FA),
    onBackground = Color(0xFF1A1D24),
    surface = Color(0xFFFBFCFE),
    onSurface = Color(0xFF1A1D24),
    surfaceVariant = Color(0xFFE2E6EE),
    onSurfaceVariant = Color(0xFF464D59),
    outline = Color(0xFF747B89),
    outlineVariant = Color(0xFFC4C9D4),
)

@Composable
fun NovaTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    accent: Accent = Accent.CYAN,
    content: @Composable () -> Unit,
) {
    val colors = NovaDarkColors.copy(
        primary = accent.color,
        onPrimary = accent.onColor,
        secondary = accent.secondary,
    )
    MaterialTheme(
        colorScheme = if (darkTheme) colors else NovaLightColors,
        typography = NovaTypography,
        content = content,
    )
}

enum class Accent(val color: Color, val onColor: Color, val secondary: Color) {
    CYAN(Color(0xFF00E5FF), Color(0xFF00222B), Color(0xFF7C5CFF)),
    MAGENTA(Color(0xFFFF4081), Color(0xFF3D0026), Color(0xFF7C5CFF)),
    GREEN(Color(0xFF00E676), Color(0xFF00351F), Color(0xFF00B0FF)),
    AMBER(Color(0xFFFFB300), Color(0xFF3A2A00), Color(0xFFFF6BCB)),
}
