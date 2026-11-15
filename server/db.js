// db.js — sqlite storage, Node/Express, better-sqlite3
const path = require('path');
const fs = require('fs');
const Database = require('better-sqlite3');

const DATA_DIR = path.join(__dirname, '..', 'data');
const MEDIA_DIR = path.join(DATA_DIR, 'media');
fs.mkdirSync(MEDIA_DIR, { recursive: true });

const db = new Database(path.join(DATA_DIR, 'app.db'));
db.pragma('journal_mode = WAL');

db.exec(`
CREATE TABLE IF NOT EXISTS characters (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  kind TEXT NOT NULL,              -- 'anime' | 'realistic'
  gender TEXT DEFAULT 'female',    -- 'male' | 'female' | 'femboy' | 'tomboy'
  role TEXT DEFAULT '',            -- دور الشخصية: حبيب/صديق/معلم/...
  voice TEXT DEFAULT '',           -- وصف نمط الصوت
  relationship_type TEXT DEFAULT '', -- نوع العلاقة مع المستخدم
  appearance TEXT NOT NULL,        -- وصف المظهر (يُحقن في برومبت الصور)
  personality TEXT NOT NULL,       -- وصف الشخصية (يُحقن في system prompt الشات)
  scenario TEXT DEFAULT '',        -- سياق/عالم القصة
  avatar_path TEXT DEFAULT NULL,
  unrestricted INTEGER DEFAULT 1,  -- بدون قيود محتوى في برومبت هذه الشخصية
  created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS messages (
  id TEXT PRIMARY KEY,
  character_id TEXT NOT NULL,
  role TEXT NOT NULL,              -- 'user' | 'assistant'
  content TEXT NOT NULL,
  created_at TEXT NOT NULL,
  FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS media (
  id TEXT PRIMARY KEY,
  character_id TEXT NOT NULL,
  type TEXT NOT NULL,               -- 'image' | 'video'
  prompt TEXT NOT NULL,
  file_path TEXT,                   -- نسبي إلى /data/media
  status TEXT NOT NULL DEFAULT 'pending', -- pending|done|failed
  job_id TEXT,
  error TEXT,
  created_at TEXT NOT NULL
);
`);

// ترحيل آمن لقواعد بيانات قديمة أُنشئت قبل إضافة هذه الحقول
const existingCols = db.prepare(`PRAGMA table_info(characters)`).all().map((c) => c.name);
const addColIfMissing = (name, decl) => {
  if (!existingCols.includes(name)) db.exec(`ALTER TABLE characters ADD COLUMN ${name} ${decl}`);
};
addColIfMissing('gender', `TEXT DEFAULT 'female'`);
addColIfMissing('role', `TEXT DEFAULT ''`);
addColIfMissing('voice', `TEXT DEFAULT ''`);
addColIfMissing('relationship_type', `TEXT DEFAULT ''`);

module.exports = { db, MEDIA_DIR, DATA_DIR };
