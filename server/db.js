// db.js — sqlite storage, Node/Express
// يستخدم node:sqlite المدمج بـ Node نفسه (Node 22+) بدل better-sqlite3 —
// صفر اعتماد على تجميع كود C++ وقت التثبيت (كان يفشل على ويندوز بدون Visual Studio)
const path = require('path');
const fs = require('fs');
const { DatabaseSync } = require('node:sqlite');

const DATA_DIR = path.join(__dirname, '..', 'data');
const MEDIA_DIR = path.join(DATA_DIR, 'media');
fs.mkdirSync(MEDIA_DIR, { recursive: true });

const db = new DatabaseSync(path.join(DATA_DIR, 'app.db'));
db.exec('PRAGMA journal_mode = WAL;');

db.exec(`
CREATE TABLE IF NOT EXISTS characters (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  kind TEXT NOT NULL,              -- 'anime' | 'realistic'
  gender TEXT DEFAULT 'female',    -- 'male' | 'female' | 'femboy' | 'tomboy' | 'futa'
  role TEXT DEFAULT '',            -- دور الشخصية: حبيب/صديق/معلم/...
  voice TEXT DEFAULT '',           -- وصف نمط الصوت
  relationship_type TEXT DEFAULT '', -- نوع العلاقة مع المستخدم
  hair_color TEXT DEFAULT '',
  hairstyle TEXT DEFAULT '',
  eye_color TEXT DEFAULT '',
  breast_size TEXT DEFAULT '',     -- '' لو الجنس ما يناسبه
  butt_size TEXT DEFAULT '',
  penis_size TEXT DEFAULT '',      -- يظهر فقط لـ male/femboy/futa
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
addColIfMissing('hair_color', `TEXT DEFAULT ''`);
addColIfMissing('hairstyle', `TEXT DEFAULT ''`);
addColIfMissing('eye_color', `TEXT DEFAULT ''`);
addColIfMissing('breast_size', `TEXT DEFAULT ''`);
addColIfMissing('butt_size', `TEXT DEFAULT ''`);
addColIfMissing('penis_size', `TEXT DEFAULT ''`);

module.exports = { db, MEDIA_DIR, DATA_DIR };
