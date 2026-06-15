package eu.lastviking.android.smtool.db

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper

class DatabaseHelper(context: Context) : SQLiteOpenHelper(context, DATABASE_NAME, null, DATABASE_VERSION) {

    companion object {
        private const val DATABASE_NAME = "smtool.db"
        private const val DATABASE_VERSION = 1

        const val TABLE_IDEAS = "ideas"
        const val COLUMN_ID = "id"
        const val COLUMN_NAME = "name"
        const val COLUMN_CREATED_AT = "created_at"
        const val COLUMN_TRANSCRIBED_AT = "transcribed_at"
        const val COLUMN_STATUS = "status"
        const val COLUMN_EXPORTED = "exported"
        const val COLUMN_TRANSCRIPT = "transcript"
    }

    override fun onCreate(db: SQLiteDatabase) {
        val createTable = ("CREATE TABLE $TABLE_IDEAS (" +
                "$COLUMN_ID INTEGER PRIMARY KEY AUTOINCREMENT, " +
                "$COLUMN_NAME TEXT, " +
                "$COLUMN_CREATED_AT INTEGER, " +
                "$COLUMN_TRANSCRIBED_AT INTEGER, " +
                "$COLUMN_STATUS TEXT, " +
                "$COLUMN_EXPORTED INTEGER DEFAULT 0, " +
                "$COLUMN_TRANSCRIPT TEXT)")
        db.execSQL(createTable)
    }

    override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) {
        db.execSQL("DROP TABLE IF EXISTS $TABLE_IDEAS")
        onCreate(db)
    }

    fun insertIdea(idea: Idea): Long {
        val db = this.writableDatabase
        val values = ContentValues().apply {
            put(COLUMN_NAME, idea.name)
            put(COLUMN_CREATED_AT, idea.createdAt)
            put(COLUMN_STATUS, idea.status)
            put(COLUMN_EXPORTED, if (idea.exported) 1 else 0)
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

    fun getPendingIdeas(): List<Idea> {
        val ideas = mutableListOf<Idea>()
        val db = this.readableDatabase
        val cursor = db.query(TABLE_IDEAS, null, "$COLUMN_STATUS = ?", arrayOf("pending"), null, null, "$COLUMN_CREATED_AT ASC")
        
        with(cursor) {
            while (moveToNext()) {
                val idea = Idea(
                    id = getLong(getColumnIndexOrThrow(COLUMN_ID)),
                    name = getString(getColumnIndexOrThrow(COLUMN_NAME)),
                    createdAt = getLong(getColumnIndexOrThrow(COLUMN_CREATED_AT)),
                    transcribedAt = if (isNull(getColumnIndexOrThrow(COLUMN_TRANSCRIBED_AT))) null else getLong(getColumnIndexOrThrow(COLUMN_TRANSCRIBED_AT)),
                    status = getString(getColumnIndexOrThrow(COLUMN_STATUS)),
                    exported = getInt(getColumnIndexOrThrow(COLUMN_EXPORTED)) == 1,
                    transcript = getString(getColumnIndexOrThrow(COLUMN_TRANSCRIPT))
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
                    name = getString(getColumnIndexOrThrow(COLUMN_NAME)),
                    createdAt = getLong(getColumnIndexOrThrow(COLUMN_CREATED_AT)),
                    transcribedAt = if (isNull(getColumnIndexOrThrow(COLUMN_TRANSCRIBED_AT))) null else getLong(getColumnIndexOrThrow(COLUMN_TRANSCRIBED_AT)),
                    status = getString(getColumnIndexOrThrow(COLUMN_STATUS)),
                    exported = getInt(getColumnIndexOrThrow(COLUMN_EXPORTED)) == 1,
                    transcript = getString(getColumnIndexOrThrow(COLUMN_TRANSCRIPT))
                )
                ideas.add(idea)
            }
        }
        cursor.close()
        return ideas
    }
}
