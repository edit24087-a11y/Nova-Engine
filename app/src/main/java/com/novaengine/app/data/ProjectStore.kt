package com.novaengine.app.data

import android.content.Context
import com.novaengine.app.engine.NovaEngineBridge
import java.io.File

/**
 * High-level project management: creates real project directories on disk,
 * registers them in SQLite, opens them in the native engine.
 */
class ProjectStore(context: Context) {

    private val db = NovaDatabase(context)
    private val rootDir = File(context.filesDir, "NovaEngine/projects")

    val templates = listOf(
        Template("Blank", "blank", "Empty scene with a root node"),
        Template("2D Platformer", "platformer", "Sprite2D player, tilemap, gravity"),
        Template("3D Scene", "3d", "Camera + lights + sample meshes"),
    )

    data class Template(val label: String, val id: String, val description: String)

    init { rootDir.mkdirs() }

    fun listProjects(): List<ProjectRow> {
        val rows = mutableListOf<ProjectRow>()
        db.readableDatabase.query(
            "projects", null, null, null, null, null, "last_opened_at DESC"
        ).use { c ->
            while (c.moveToNext()) {
                rows.add(
                    ProjectRow(
                        id = c.getLong(0), name = c.getString(1), path = c.getString(2),
                        template = c.getString(3), createdAt = c.getLong(4),
                        lastOpenedAt = c.getLong(5),
                    )
                )
            }
        }
        return rows
    }

    /** Creates the full project/ directory tree + project.json + registers in SQLite. */
    fun createProject(name: String, template: String = "blank"): Boolean {
        if (name.isBlank() || name.contains('/')) return false
        val path = File(rootDir, name).absolutePath
        // Native side creates directories + project.json.
        val ok = NovaEngineBridge.nativeCreateProject(rootDir.absolutePath, name)
        if (!ok) return false

        val now = System.currentTimeMillis()
        db.writableDatabase.insert(
            "projects", null,
            android.content.ContentValues().apply {
                put("name", name)
                put("path", path)
                put("template", template)
                put("created_at", now)
                put("last_opened_at", now)
            }
        )
        return true
    }

    fun deleteProject(row: ProjectRow): Boolean {
        File(row.path).deleteRecursively()
        db.writableDatabase.delete("projects", "name = ?", arrayOf(row.name))
        return true
    }

    fun openProject(row: ProjectRow): Boolean {
        val ok = NovaEngineBridge.nativeOpenProject(row.path)
        if (ok) {
            db.writableDatabase.update(
                "projects",
                android.content.ContentValues().apply { put("last_opened_at", System.currentTimeMillis()) },
                "name = ?", arrayOf(row.name),
            )
        }
        return ok
    }

    fun projectRoot(): String = rootDir.absolutePath
}
