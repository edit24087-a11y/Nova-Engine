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
    // The pure parsing logic lives in NovaParsers (no native dependencies) so it can
    // be unit-tested on the JVM. These thin delegations keep the facade stable; the
    // typed results are the NovaParsers data classes (NovaParsers.NodeInfo,
    // NovaParsers.HierarchyRow, NovaParsers.EngineStats).

    fun parseNodeInfo(line: String): NovaParsers.NodeInfo? = NovaParsers.parseNodeInfo(line)
    fun parseTree(dump: String): List<NovaParsers.HierarchyRow> = NovaParsers.parseTree(dump)
    fun parseStats(packed: Long): NovaParsers.EngineStats = NovaParsers.parseStats(packed)
}
