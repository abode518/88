// routes/characters.js — CRUD للشخصيات
const express = require('express');
const { nanoid } = require('nanoid');
const { db } = require('../db');
const imageEngine = require('../engines/image');

const router = express.Router();

router.get('/', (req, res) => {
  const rows = db.prepare('SELECT * FROM characters ORDER BY created_at DESC').all();
  res.json(rows);
});

router.get('/:id', (req, res) => {
  const row = db.prepare('SELECT * FROM characters WHERE id = ?').get(req.params.id);
  if (!row) return res.status(404).json({ error: 'الشخصية غير موجودة' });
  res.json(row);
});

const GENDERS = ['male', 'female', 'femboy', 'tomboy'];

router.post('/', (req, res) => {
  const { name, kind, gender, role, voice, relationshipType, appearance, personality, scenario, unrestricted } =
    req.body || {};
  if (!name || !kind || !gender || !appearance || !personality) {
    return res.status(400).json({ error: 'الحقول المطلوبة: name, kind, gender, appearance, personality' });
  }
  if (!['anime', 'realistic'].includes(kind)) {
    return res.status(400).json({ error: "kind يجب أن يكون 'anime' أو 'realistic'" });
  }
  if (!GENDERS.includes(gender)) {
    return res.status(400).json({ error: `gender يجب أن يكون واحد من: ${GENDERS.join(', ')}` });
  }
  const id = nanoid();
  db.prepare(
    `INSERT INTO characters
       (id, name, kind, gender, role, voice, relationship_type, appearance, personality, scenario, unrestricted, created_at)
     VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`
  ).run(
    id,
    name,
    kind,
    gender,
    role || '',
    voice || '',
    relationshipType || '',
    appearance,
    personality,
    scenario || '',
    unrestricted === false ? 0 : 1,
    new Date().toISOString()
  );

  res.status(201).json(db.prepare('SELECT * FROM characters WHERE id = ?').get(id));
});

// توليد/إعادة توليد صورة الشخصية — prompt اختياري: فاضي = تلقائي بناءً على اختيارات الشخصية
router.post('/:id/avatar', async (req, res) => {
  const character = db.prepare('SELECT * FROM characters WHERE id = ?').get(req.params.id);
  if (!character) return res.status(404).json({ error: 'الشخصية غير موجودة' });

  const { prompt, negativePrompt } = req.body || {};
  try {
    const fileName = await imageEngine.generateImage({
      character,
      prompt: prompt || '',
      negativePrompt,
      width: 512,
      height: 512,
    });
    db.prepare('UPDATE characters SET avatar_path = ? WHERE id = ?').run(fileName, character.id);
    // نسجّلها في سجل الوسائط أيضاً حتى تظهر مع باقي الصور
    db.prepare(
      `INSERT INTO media (id, character_id, type, prompt, status, file_path, created_at)
       VALUES (?, ?, 'image', ?, 'done', ?, ?)`
    ).run(nanoid(), character.id, prompt || '(تلقائي)', fileName, new Date().toISOString());

    res.status(201).json(db.prepare('SELECT * FROM characters WHERE id = ?').get(character.id));
  } catch (err) {
    res.status(502).json({ error: err.message });
  }
});

router.delete('/:id', (req, res) => {
  db.prepare('DELETE FROM characters WHERE id = ?').run(req.params.id);
  res.json({ ok: true });
});

module.exports = router;
