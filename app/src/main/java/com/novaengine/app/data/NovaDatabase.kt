package com.novaengine.app.data

import android.content.Context
import android.database.sqlite.SQLiteOpenHelper

/**
 * SQLite registry for NOVA ENGINE projects.
 * Stores project metadata (name, path, template, created date, last opened).
 * The actual project content lives on disk in filesDir/NovaEngine/projects/<name>/.
 */
class NovaDatabase(context: Context) :
    SQLiteOpenHelper(context, "nova_engine.db", null, 1) {

    override fun onCreate(db: android.database.sqlite.SQLiteDatabase) {
        db.execSQL(
            """
            CREATE TABLE projects (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL UNIQUE,
                path TEXT NOT NULL,
                template TEXT NOT NULL DEFAULT 'blank',
                created_at INTEGER NOT NULL,
                last_opened_at INTEGER NOT NULL,
                thumbnail TEXT
            )
            """.trimIndent()
        )
    }

    override fun onUpgrade(db: android.database.sqlite.SQLiteDatabase, oldV: Int, newV: Int) {
        db.execSQL("DROP TABLE IF EXISTS projects")
        onCreate(db)
    }
}

data class ProjectRow(
    val id: Long,
    val name: String,
    val path: String,
    val template: String,
    val createdAt: Long,
    val lastOpenedAt: Long,
)
