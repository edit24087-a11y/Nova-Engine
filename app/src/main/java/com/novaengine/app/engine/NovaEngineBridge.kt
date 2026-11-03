package com.novaengine.app.engine

/**
 * Kotlin wrapper around libnova.so (the C++ engine core compiled via CMake/NDK).
 * All native methods are declared here; the JNI implementations live in
 * app/src/main/cpp/jni_bridge.cpp.
 *
 * Node identity crosses the JNI boundary as an Int (NodeId). Scene data crosses
 * as tab-separated strings for compact transfer.
 */
object NovaEngineBridge {

    init {
        System.loadLibrary("nova")
    }

    // --- lifecycle ------------------------------------------------------------
    external fun nativeCreate(): Boolean
    external fun nativeInitialize(): Boolean
    external fun nativeShutdown()

    // --- project ----------------------------------------------------------------
    external fun nativeOpenProject(path: String): Boolean
    external fun nativeSaveProject(): Boolean
    external fun nativeProjectPath(): String
    external fun nativeCreateProject(path: String, name: String): Boolean

    // --- scenes ------------------------------------------------------------------
    external fun nativeNewScene(name: String): Boolean
    external fun nativeOpenScene(name: String): Boolean
    external fun nativeSaveScene(): Boolean
    external fun nativeSceneName(): String

    // --- nodes ---------------------------------------------------------------------
    external fun nativeCreateNode(type: String, name: String, parentId: Int): Int
    external fun nativeDeleteNode(nodeId: Int): Boolean
    external fun nativeRenameNode(nodeId: Int, name: String): Boolean
    external fun nativeMoveNode(nodeId: Int, parentId: Int): Boolean
    external fun nativeDumpTree(): String

    // --- inspector -------------------------------------------------------------
    external fun nativeGetNodeInfo(nodeId: Int): String
    external fun nativeSetNodeTransform(
        nodeId: Int, x: Float, y: Float, rot: Float, sx: Float, sy: Float
    )
    external fun nativeSetNodeProp(nodeId: Int, key: String, value: String)
    external fun nativeGetNodeProp(nodeId: Int, key: String): String
    external fun nativeAttachScript(nodeId: Int, source: String): Boolean

    // --- console / input / loop --------------------------------------------------
    external fun nativeConsoleDump(): String
    external fun nativeConsoleClear()
    external fun nativeTouch(id: Int, x: Float, y: Float, down: Boolean)
    external fun nativeResize(w: Int, h: Int)
    external fun nativeUpdate(dt: Float)
    external fun nativeRender()
    external fun nativeStats(): Long
    external fun nativeVersion(): String

    // --- helpers -------------------------------------------------------------------

    /** Parse "id\tname\ttype\tcolor\tx,y\trot\tsx,sy" into a data row. */
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

    /** Parse packed 64-bit stats from nativeStats(). */
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
