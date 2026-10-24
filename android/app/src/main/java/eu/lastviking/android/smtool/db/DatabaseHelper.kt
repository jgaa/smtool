package eu.lastviking.android.smtool.db

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import java.util.UUID

class DatabaseHelper(private val context: Context) : SQLiteOpenHelper(context, DATABASE_NAME, null, DATABASE_VERSION) {

    companion object {
        private const val DATABASE_NAME = "smtool.db"
        private const val DATABASE_VERSION = 3

        const val TABLE_IDEAS = "ideas"
        const val COLUMN_ID = "id"
        const val COLUMN_UUID = "uuid"
        const val COLUMN_NAME = "name"
        const val COLUMN_CREATED_AT = "created_at"
        const val COLUMN_TRANSCRIBED_AT = "transcribed_at"
        const val COLUMN_STATUS = "status"
        const val COLUMN_EXPORTED = "exported"
        const val COLUMN_TRANSCRIPT = "transcript"
        const val COLUMN_TARGET_ID = "target_id"

        const val TABLE_TARGETS = "targets"
        const val COLUMN_TARGET_NAME = "name"
        const val COLUMN_SERVER_ADDRESS = "server_address"
        const val COLUMN_SERVER_PORT = "server_port"
        const val COLUMN_TRANSFER_ENABLED = "transfer_enabled"
        const val COLUMN_DELETE_AFTER_TRANSFER = "delete_after_transfer"
    }

    override fun onCreate(db: SQLiteDatabase) {
        val createTargetsTable = ("CREATE TABLE $TABLE_TARGETS (" +
                "$COLUMN_ID INTEGER PRIMARY KEY AUTOINCREMENT, " +
                "$COLUMN_TARGET_NAME TEXT, " +
                "$COLUMN_SERVER_ADDRESS TEXT, " +
                "$COLUMN_SERVER_PORT INTEGER, " +
                "$COLUMN_TRANSFER_ENABLED INTEGER, " +
                "$COLUMN_DELETE_AFTER_TRANSFER INTEGER)")
        db.execSQL(createTargetsTable)

        val createIdeasTable = ("CREATE TABLE $TABLE_IDEAS (" +
                "$COLUMN_ID INTEGER PRIMARY KEY AUTOINCREMENT, " +
                "$COLUMN_UUID TEXT, " +
                "$COLUMN_NAME TEXT, " +
                "$COLUMN_CREATED_AT INTEGER, " +
                "$COLUMN_TRANSCRIBED_AT INTEGER, " +
                "$COLUMN_STATUS TEXT, " +
                "$COLUMN_EXPORTED INTEGER DEFAULT 0, " +
                "$COLUMN_TRANSCRIPT TEXT, " +
                "$COLUMN_TARGET_ID INTEGER DEFAULT 1)")
        db.execSQL(createIdeasTable)

        // Insert Default target for new installs
        val values = ContentValues().apply {
            put(COLUMN_TARGET_NAME, "Default")
            put(COLUMN_SERVER_ADDRESS, "")
            put(COLUMN_SERVER_PORT, 45437)
            put(COLUMN_TRANSFER_ENABLED, 1)
            put(COLUMN_DELETE_AFTER_TRANSFER, 0)
        }
        val defaultId = db.insert(TABLE_TARGETS, null, values)

        // Save default target ID in SharedPreferences
        val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)
        prefs.edit().putLong("default_target_id", defaultId).apply()
    }

    override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) {
        if (oldVersion < 2) {
            db.execSQL("ALTER TABLE $TABLE_IDEAS ADD COLUMN $COLUMN_UUID TEXT")
            val cursor = db.query(TABLE_IDEAS, arrayOf(COLUMN_ID), null, null, null, null, null)
            while (cursor.moveToNext()) {
                val id = cursor.getLong(0)
                val uuid = UUID.randomUUID().toString()
                db.execSQL("UPDATE $TABLE_IDEAS SET $COLUMN_UUID = '$uuid' WHERE $COLUMN_ID = $id")
            }
            cursor.close()
        }
        if (oldVersion < 3) {
            val createTargetsTable = ("CREATE TABLE $TABLE_TARGETS (" +
                    "$COLUMN_ID INTEGER PRIMARY KEY AUTOINCREMENT, " +
                    "$COLUMN_TARGET_NAME TEXT, " +
                    "$COLUMN_SERVER_ADDRESS TEXT, " +
                    "$COLUMN_SERVER_PORT INTEGER, " +
                    "$COLUMN_TRANSFER_ENABLED INTEGER, " +
                    "$COLUMN_DELETE_AFTER_TRANSFER INTEGER)")
            db.execSQL(createTargetsTable)

            // Add target_id column to ideas if not present
            try {
                db.execSQL("ALTER TABLE $TABLE_IDEAS ADD COLUMN $COLUMN_TARGET_ID INTEGER DEFAULT 1")
            } catch (e: Exception) {
                // Column might already exist
            }

            // Migrate existing server settings from SharedPreferences
            val prefs = context.getSharedPreferences("settings", Context.MODE_PRIVATE)
            val serverAddress = prefs.getString("server_address", "") ?: ""
            val serverPort = prefs.getInt("server_port", 45437)
            val transferEnabled = prefs.getBoolean("transfer_enabled", true)
            val deleteAfterTransfer = prefs.getBoolean("delete_after_transfer", false)

            val values = ContentValues().apply {
                put(COLUMN_TARGET_NAME, "smtool")
                put(COLUMN_SERVER_ADDRESS, serverAddress)
                put(COLUMN_SERVER_PORT, serverPort)
                put(COLUMN_TRANSFER_ENABLED, if (transferEnabled) 1 else 0)
                put(COLUMN_DELETE_AFTER_TRANSFER, if (deleteAfterTransfer) 1 else 0)
            }
            val smtoolId = db.insert(TABLE_TARGETS, null, values)

            // Update existing ideas to point to smtool target
            db.execSQL("UPDATE $TABLE_IDEAS SET $COLUMN_TARGET_ID = $smtoolId")

            // Set smtool as default target
            prefs.edit()
                .putLong("default_target_id", smtoolId)
                .remove("server_address")
                .remove("server_port")
                .remove("transfer_enabled")
                .remove("delete_after_transfer")
                .apply()
        }
    }

    // Target CRUD operations
    fun insertTarget(target: Target): Long {
        val db = this.writableDatabase
        val values = ContentValues().apply {
            put(COLUMN_TARGET_NAME, target.name)
            put(COLUMN_SERVER_ADDRESS, target.serverAddress)
            put(COLUMN_SERVER_PORT, target.serverPort)
            put(COLUMN_TRANSFER_ENABLED, if (target.transferEnabled) 1 else 0)
            put(COLUMN_DELETE_AFTER_TRANSFER, if (target.deleteAfterTransfer) 1 else 0)
        }
        return db.insert(TABLE_TARGETS, null, values)
    }

    fun updateTarget(target: Target) {
        val db = this.writableDatabase
        val values = ContentValues().apply {
            put(COLUMN_TARGET_NAME, target.name)
            put(COLUMN_SERVER_ADDRESS, target.serverAddress)
            put(COLUMN_SERVER_PORT, target.serverPort)
            put(COLUMN_TRANSFER_ENABLED, if (target.transferEnabled) 1 else 0)
            put(COLUMN_DELETE_AFTER_TRANSFER, if (target.deleteAfterTransfer) 1 else 0)
        }
        db.update(TABLE_TARGETS, values, "$COLUMN_ID = ?", arrayOf(target.id.toString()))
    }

    fun deleteTarget(targetId: Long, defaultTargetId: Long) {
        val db = this.writableDatabase
        // Move affected ideas to default target
        val values = ContentValues().apply {
            put(COLUMN_TARGET_ID, defaultTargetId)
        }
        db.update(TABLE_IDEAS, values, "$COLUMN_TARGET_ID = ?", arrayOf(targetId.toString()))
        // Delete target
        db.delete(TABLE_TARGETS, "$COLUMN_ID = ?", arrayOf(targetId.toString()))
    }

    fun getAllTargets(): List<Target> {
        val targets = mutableListOf<Target>()
        val db = this.readableDatabase
        val cursor = db.query(TABLE_TARGETS, null, null, null, null, null, "$COLUMN_ID ASC")
        with(cursor) {
            while (moveToNext()) {
                val target = Target(
                    id = getLong(getColumnIndexOrThrow(COLUMN_ID)),
                    name = getString(getColumnIndexOrThrow(COLUMN_TARGET_NAME)),
                    serverAddress = getString(getColumnIndexOrThrow(COLUMN_SERVER_ADDRESS)) ?: "",
                    serverPort = getInt(getColumnIndexOrThrow(COLUMN_SERVER_PORT)),
                    transferEnabled = getInt(getColumnIndexOrThrow(COLUMN_TRANSFER_ENABLED)) == 1,
                    deleteAfterTransfer = getInt(getColumnIndexOrThrow(COLUMN_DELETE_AFTER_TRANSFER)) == 1
                )
                targets.add(target)
            }
        }
        cursor.close()
        return targets
    }

    fun getTarget(id: Long): Target? {
        val db = this.readableDatabase
        val cursor = db.query(TABLE_TARGETS, null, "$COLUMN_ID = ?", arrayOf(id.toString()), null, null, null)
        var target: Target? = null
        if (cursor.moveToFirst()) {
            target = Target(
                id = cursor.getLong(cursor.getColumnIndexOrThrow(COLUMN_ID)),
                name = cursor.getString(cursor.getColumnIndexOrThrow(COLUMN_TARGET_NAME)),
                serverAddress = cursor.getString(cursor.getColumnIndexOrThrow(COLUMN_SERVER_ADDRESS)) ?: "",
                serverPort = cursor.getInt(cursor.getColumnIndexOrThrow(COLUMN_SERVER_PORT)),
                transferEnabled = cursor.getInt(cursor.getColumnIndexOrThrow(COLUMN_TRANSFER_ENABLED)) == 1,
                deleteAfterTransfer = cursor.getInt(cursor.getColumnIndexOrThrow(COLUMN_DELETE_AFTER_TRANSFER)) == 1
            )
        }
        cursor.close()
        return target
    }

    // Idea operations
    fun insertIdea(idea: Idea): Long {
        val db = this.writableDatabase
        val values = ContentValues().apply {
            put(COLUMN_UUID, idea.uuid)
            put(COLUMN_NAME, idea.name)
            put(COLUMN_CREATED_AT, idea.createdAt)
            put(COLUMN_STATUS, idea.status)
            put(COLUMN_EXPORTED, if (idea.exported) 1 else 0)
            put(COLUMN_TARGET_ID, idea.targetId)
        }
        return db.insert(TABLE_IDEAS, null, values)
    }

    fun updateIdeaTranscript(id: Long, transcript: String, transcribedAt: Long) {
        val db = this.writableDatabase
        val values = ContentValues().apply {
            put(COLUMN_TRANSCRIPT, transcript)
            put(COLUMN_TRANSCRIBED_AT, transcribedAt)
            put(COLUMN_STATUS, "transcript")
        }
        db.update(TABLE_IDEAS, values, "$COLUMN_ID = ?", arrayOf(id.toString()))
    }

    fun updateIdeaWithTranscriptAndName(id: Long, transcript: String, name: String?, transcribedAt: Long) {
        val db = this.writableDatabase
        val values = ContentValues().apply {
            put(COLUMN_TRANSCRIPT, transcript)
            put(COLUMN_TRANSCRIBED_AT, transcribedAt)
            put(COLUMN_STATUS, "transcript")
            put(COLUMN_NAME, name)
        }
        db.update(TABLE_IDEAS, values, "$COLUMN_ID = ?", arrayOf(id.toString()))
    }

    fun updateIdea(id: Long, name: String, transcript: String) {
        val db = this.writableDatabase
        val values = ContentValues().apply {
            put(COLUMN_NAME, name)
            put(COLUMN_TRANSCRIPT, transcript)
        }
        db.update(TABLE_IDEAS, values, "$COLUMN_ID = ?", arrayOf(id.toString()))
    }

    fun deleteIdea(id: Long) {
        val db = this.writableDatabase
        db.delete(TABLE_IDEAS, "$COLUMN_ID = ?", arrayOf(id.toString()))
    }

    fun deleteAllIdeas() {
        val db = this.writableDatabase
        db.delete(TABLE_IDEAS, null, null)
    }

    fun updateIdeaStatus(id: Long, status: String) {
        val db = this.writableDatabase
        val values = ContentValues().apply {
            put(COLUMN_STATUS, status)
        }
        db.update(TABLE_IDEAS, values, "$COLUMN_ID = ?", arrayOf(id.toString()))
    }

    fun updateExportedStatus(id: Long, exported: Boolean) {
        val db = this.writableDatabase
        val values = ContentValues().apply {
            put(COLUMN_EXPORTED, if (exported) 1 else 0)
        }
        db.update(TABLE_IDEAS, values, "$COLUMN_ID = ?", arrayOf(id.toString()))
    }

    fun getPendingIdeas(): List<Idea> {
        val ideas = mutableListOf<Idea>()
        val db = this.readableDatabase
        val cursor = db.query(TABLE_IDEAS, null, "$COLUMN_STATUS = ?", arrayOf("pending"), null, null, "$COLUMN_CREATED_AT ASC")
        
        with(cursor) {
            while (moveToNext()) {
                val idea = Idea(
                    id = getLong(getColumnIndexOrThrow(COLUMN_ID)),
                    uuid = getString(getColumnIndexOrThrow(COLUMN_UUID)) ?: UUID.randomUUID().toString(),
                    name = getString(getColumnIndexOrThrow(COLUMN_NAME)),
                    createdAt = getLong(getColumnIndexOrThrow(COLUMN_CREATED_AT)),
                    transcribedAt = if (isNull(getColumnIndexOrThrow(COLUMN_TRANSCRIBED_AT))) null else getLong(getColumnIndexOrThrow(COLUMN_TRANSCRIBED_AT)),
                    status = getString(getColumnIndexOrThrow(COLUMN_STATUS)),
                    exported = getInt(getColumnIndexOrThrow(COLUMN_EXPORTED)) == 1,
                    transcript = getString(getColumnIndexOrThrow(COLUMN_TRANSCRIPT)),
                    targetId = getLong(getColumnIndexOrThrow(COLUMN_TARGET_ID))
                )
                ideas.add(idea)
            }
        }
        cursor.close()
        return ideas
    }

    fun getAllIdeas(): List<Idea> {
        val ideas = mutableListOf<Idea>()
        val db = this.readableDatabase
        val cursor = db.query(TABLE_IDEAS, null, null, null, null, null, "$COLUMN_CREATED_AT DESC")
        
        with(cursor) {
            while (moveToNext()) {
                val idea = Idea(
                    id = getLong(getColumnIndexOrThrow(COLUMN_ID)),
                    uuid = getString(getColumnIndexOrThrow(COLUMN_UUID)) ?: UUID.randomUUID().toString(),
                    name = getString(getColumnIndexOrThrow(COLUMN_NAME)),
                    createdAt = getLong(getColumnIndexOrThrow(COLUMN_CREATED_AT)),
                    transcribedAt = if (isNull(getColumnIndexOrThrow(COLUMN_TRANSCRIBED_AT))) null else getLong(getColumnIndexOrThrow(COLUMN_TRANSCRIBED_AT)),
                    status = getString(getColumnIndexOrThrow(COLUMN_STATUS)),
                    exported = getInt(getColumnIndexOrThrow(COLUMN_EXPORTED)) == 1,
                    transcript = getString(getColumnIndexOrThrow(COLUMN_TRANSCRIPT)),
                    targetId = getLong(getColumnIndexOrThrow(COLUMN_TARGET_ID))
                )
                ideas.add(idea)
            }
        }
        cursor.close()
        return ideas
    }

    fun getIdeasForTarget(targetId: Long): List<Idea> {
        val ideas = mutableListOf<Idea>()
        val db = this.readableDatabase
        val cursor = db.query(TABLE_IDEAS, null, "$COLUMN_TARGET_ID = ?", arrayOf(targetId.toString()), null, null, "$COLUMN_CREATED_AT DESC")
        
        with(cursor) {
            while (moveToNext()) {
                val idea = Idea(
                    id = getLong(getColumnIndexOrThrow(COLUMN_ID)),
                    uuid = getString(getColumnIndexOrThrow(COLUMN_UUID)) ?: UUID.randomUUID().toString(),
                    name = getString(getColumnIndexOrThrow(COLUMN_NAME)),
                    createdAt = getLong(getColumnIndexOrThrow(COLUMN_CREATED_AT)),
                    transcribedAt = if (isNull(getColumnIndexOrThrow(COLUMN_TRANSCRIBED_AT))) null else getLong(getColumnIndexOrThrow(COLUMN_TRANSCRIBED_AT)),
                    status = getString(getColumnIndexOrThrow(COLUMN_STATUS)),
                    exported = getInt(getColumnIndexOrThrow(COLUMN_EXPORTED)) == 1,
                    transcript = getString(getColumnIndexOrThrow(COLUMN_TRANSCRIPT)),
                    targetId = getLong(getColumnIndexOrThrow(COLUMN_TARGET_ID))
                )
                ideas.add(idea)
            }
        }
        cursor.close()
        return ideas
    }
}
