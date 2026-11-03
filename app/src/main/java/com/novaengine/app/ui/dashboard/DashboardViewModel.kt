package com.novaengine.app.ui.dashboard

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.novaengine.app.data.ProjectRow
import com.novaengine.app.data.ProjectStore
import com.novaengine.app.engine.NovaEngineBridge
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

class DashboardViewModel(app: Application) : AndroidViewModel(app) {

    private val store = ProjectStore(app.applicationContext)

    private val _projects = MutableStateFlow<List<ProjectRow>>(emptyList())
    val projects: StateFlow<List<ProjectRow>> = _projects.asStateFlow()

    init { refresh() }

    fun refresh() {
        _projects.value = store.listProjects()
    }

    fun templateNames(): List<String> = store.templates.map { it.id }

    fun createProject(name: String, template: String): Boolean {
        val ok = store.createProject(name.trim(), template)
        if (ok) refresh()
        return ok
    }

    fun openProject(row: ProjectRow): Boolean = store.openProject(row)

    fun deleteProject(row: ProjectRow) {
        store.deleteProject(row)
        refresh()
    }
}
