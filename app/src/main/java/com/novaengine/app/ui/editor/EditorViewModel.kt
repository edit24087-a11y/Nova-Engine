package com.novaengine.app.ui.editor

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import com.novaengine.app.engine.NovaEngineBridge
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Editor state: current project path, scene, selected node, hierarchy dump,
 * console log, stats. All mutations route through the native engine.
 */
class EditorViewModel(app: Application) : AndroidViewModel(app) {

    private val _projectPath = MutableStateFlow("")
    val projectPath: StateFlow<String> = _projectPath.asStateFlow()

    private val _sceneName = MutableStateFlow("")
    val sceneName: StateFlow<String> = _sceneName.asStateFlow()

    private val _hierarchy = MutableStateFlow<List<NovaEngineBridge.HierarchyRow>>(emptyList())
    val hierarchy: StateFlow<List<NovaEngineBridge.HierarchyRow>> = _hierarchy.asStateFlow()

    private val _selectedNode = MutableStateFlow<NovaEngineBridge.NodeInfo?>(null)
    val selectedNode: StateFlow<NovaEngineBridge.NodeInfo?> = _selectedNode.asStateFlow()

    private val _console = MutableStateFlow<List<String>>(emptyList())
    val console: StateFlow<List<String>> = _console.asStateFlow()

    private val _stats = MutableStateFlow(NovaEngineBridge.EngineStats(0, 0, 0, 0))
    val stats: StateFlow<NovaEngineBridge.EngineStats> = _stats.asStateFlow()

    fun open(projectPath: String) {
        _projectPath.value = projectPath
        NovaEngineBridge.nativeOpenProject(projectPath)
        _sceneName.value = NovaEngineBridge.nativeSceneName()
        refreshHierarchy()
    }

    fun refreshHierarchy() {
        _hierarchy.value = NovaEngineBridge.parseTree(NovaEngineBridge.nativeDumpTree())
    }

    fun selectNode(id: Int) {
        _selectedNode.value = NovaEngineBridge.parseNodeInfo(
            NovaEngineBridge.nativeGetNodeInfo(id)
        )
    }

    fun createNode(type: String, name: String, parentId: Int): Int {
        val id = NovaEngineBridge.nativeCreateNode(type, name, parentId)
        refreshHierarchy()
        return id
    }

    fun deleteNode(id: Int) {
        NovaEngineBridge.nativeDeleteNode(id)
        refreshHierarchy()
        if (_selectedNode.value?.id == id) _selectedNode.value = null
    }

    fun saveScene(): Boolean {
        val ok = NovaEngineBridge.nativeSaveScene()
        refreshConsole()
        return ok
    }

    fun newScene(name: String): Boolean {
        val ok = NovaEngineBridge.nativeNewScene(name)
        if (ok) {
            _sceneName.value = NovaEngineBridge.nativeSceneName()
            refreshHierarchy()
        }
        return ok
    }

    fun attachScript(nodeId: Int, source: String): Boolean {
        val ok = NovaEngineBridge.nativeAttachScript(nodeId, source)
        refreshConsole()
        return ok
    }

    fun setTransform(nodeId: Int, x: Float, y: Float, rot: Float, sx: Float, sy: Float) {
        NovaEngineBridge.nativeSetNodeTransform(nodeId, x, y, rot, sx, sy)
    }

    fun refreshConsole() {
        _console.value = NovaEngineBridge.nativeConsoleDump().lineSequence()
            .filter { it.isNotBlank() }.toList()
    }

    fun refreshStats() {
        _stats.value = NovaEngineBridge.parseStats(NovaEngineBridge.nativeStats())
    }
}
