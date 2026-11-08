package com.novaengine.app.engine

/**
 * Pure-Kotlin (JVM-testable) parsing helpers for the engine bridge.
 *
 * These functions translate the tab/line-delimited strings produced by the
 * native engine ([libnova.so]) into typed Kotlin data structures used by the
 * Compose UI. They are intentionally free of any JNI / native-library
 * dependency so they can be exercised by unit tests on the JVM in CI without
 * the Android NDK build or an emulator.
 */
object NovaParsers {

    /** Parse "id\tname\ttype\tcolor\tx,y\trot\tsx,sy" into a [NodeInfo]. */
    fun parseNodeInfo(line: String): NodeInfo? {
        val parts = line.split("\t")
        if (parts.size < 7) return null
        return NodeInfo(
            id = parts[0].toIntOrNull() ?: 0,
            name = parts[1],
            type = parts[2],
            color = parts[3],
            posX = parts[4].substringBefore(",").toFloatOrNull() ?: 0f,
            posY = parts[4].substringAfter(",", "0").toFloatOrNull() ?: 0f,
            rot = parts[5].toFloatOrNull() ?: 0f,
            scaleX = parts[6].substringBefore(",").toFloatOrNull() ?: 1f,
            scaleY = parts[6].substringAfter(",", "1").toFloatOrNull() ?: 1f,
        )
    }

    data class NodeInfo(
        val id: Int,
        val name: String,
        val type: String,
        val color: String,
        val posX: Float,
        val posY: Float,
        val rot: Float,
        val scaleX: Float,
        val scaleY: Float,
    )

    /** Parse the hierarchy dump: one "id\tname\ttype\tdepth" per line. */
    fun parseTree(dump: String): List<HierarchyRow> =
        dump.lineSequence()
            .filter { it.isNotBlank() }
            .mapNotNull { line ->
                val p = line.split("\t")
                if (p.size < 4) return@mapNotNull null
                HierarchyRow(
                    id = p[0].toIntOrNull() ?: 0,
                    name = p[1],
                    type = p[2],
                    depth = p[3].toIntOrNull() ?: 0,
                )
            }
            .toList()

    data class HierarchyRow(val id: Int, val name: String, val type: String, val depth: Int)

    /** Parse the packed 64-bit stats word returned by nativeStats(). */
    fun parseStats(packed: Long): EngineStats = EngineStats(
        fps = (packed and 0xFFFF).toInt(),
        physicsBodies = ((packed ushr 16) and 0xFFFF).toInt(),
        drawCalls = ((packed ushr 32) and 0xFFFF).toInt(),
        scriptErrors = ((packed ushr 48) and 0xFFFF).toInt(),
    )

    data class EngineStats(
        val fps: Int,
        val physicsBodies: Int,
        val drawCalls: Int,
        val scriptErrors: Int,
    )
}
