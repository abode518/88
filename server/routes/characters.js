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

// futa: جسد أنثوي + قضيب — خيار جنس مستقل
const GENDERS = ['male', 'female', 'femboy', 'tomboy', 'futa'];
// الأجناس اللي تملك قضيب (يتحكم بإظهار حقل penis_size بالواجهة)
const HAS_PENIS = ['male', 'femboy', 'futa'];

function extractBodyFields(body) {
  return {
    hairColor: body.hairColor || '',
    hairstyle: body.hairstyle || '',
    eyeColor: body.eyeColor || '',
    breastSize: body.breastSize || '',
    buttSize: body.buttSize || '',
    penisSize: body.penisSize || '',
  };
}

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
  const body = extractBodyFields(req.body || {});
  if (!HAS_PENIS.includes(gender)) body.penisSize = ''; // تجاهل القيمة لو الجنس ما يناسبها

  const id = nanoid();
  db.prepare(
    `INSERT INTO characters
       (id, name, kind, gender, role, voice, relationship_type,
        hair_color, hairstyle, eye_color, breast_size, butt_size, penis_size,
        appearance, personality, scenario, unrestricted, created_at)
     VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`
  ).run(
    id,
    name,
    kind,
    gender,
    role || '',
    voice || '',
    relationshipType || '',
    body.hairColor,
    body.hairstyle,
    body.eyeColor,
    body.breastSize,
    body.buttSize,
    body.penisSize,
    appearance,
    personality,
    scenario || '',
    unrestricted === false ? 0 : 1,
    new Date().toISOString()
  );

  res.status(201).json(db.prepare('SELECT * FROM characters WHERE id = ?').get(id));
});

// تعديل شخصية موجودة — كل الحقول اختيارية، يحدّث فقط ما يُرسل
router.patch('/:id', (req, res) => {
  const existing = db.prepare('SELECT * FROM characters WHERE id = ?').get(req.params.id);
  if (!existing) return res.status(404).json({ error: 'الشخصية غير موجودة' });

  const b = req.body || {};
  if (b.kind && !['anime', 'realistic'].includes(b.kind)) {
    return res.status(400).json({ error: "kind يجب أن يكون 'anime' أو 'realistic'" });
  }
  if (b.gender && !GENDERS.includes(b.gender)) {
    return res.status(400).json({ error: `gender يجب أن يكون واحد من: ${GENDERS.join(', ')}` });
  }

  const nextGender = b.gender || existing.gender;
  const body = extractBodyFields(b);

  const merged = {
    name: b.name ?? existing.name,
    kind: b.kind ?? existing.kind,
    gender: nextGender,
    role: b.role ?? existing.role,
    voice: b.voice ?? existing.voice,
    relationship_type: b.relationshipType ?? existing.relationship_type,
    hair_color: b.hairColor !== undefined ? body.hairColor : existing.hair_color,
    hairstyle: b.hairstyle !== undefined ? body.hairstyle : existing.hairstyle,
    eye_color: b.eyeColor !== undefined ? body.eyeColor : existing.eye_color,
    breast_size: b.breastSize !== undefined ? body.breastSize : existing.breast_size,
    butt_size: b.buttSize !== undefined ? body.buttSize : existing.butt_size,
    penis_size: b.penisSize !== undefined ? body.penisSize : existing.penis_size,
    appearance: b.appearance ?? existing.appearance,
    personality: b.personality ?? existing.personality,
    scenario: b.scenario ?? existing.scenario,
    unrestricted: b.unrestricted === false ? 0 : b.unrestricted === true ? 1 : existing.unrestricted,
  };
  if (!HAS_PENIS.includes(merged.gender)) merged.penis_size = '';

  db.prepare(
    `UPDATE characters SET
       name=@name, kind=@kind, gender=@gender, role=@role, voice=@voice, relationship_type=@relationship_type,
       hair_color=@hair_color, hairstyle=@hairstyle, eye_color=@eye_color,
       breast_size=@breast_size, butt_size=@butt_size, penis_size=@penis_size,
       appearance=@appearance, personality=@personality, scenario=@scenario, unrestricted=@unrestricted
     WHERE id=@id`
  ).run({ ...merged, id: existing.id });

  res.json(db.prepare('SELECT * FROM characters WHERE id = ?').get(existing.id));
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
