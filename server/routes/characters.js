// routes/characters.js — CRUD للشخصيات
const express = require('express');
const { nanoid } = require('nanoid');
const { db } = require('../db');

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

router.post('/', (req, res) => {
  const { name, kind, appearance, personality, scenario, unrestricted } = req.body || {};
  if (!name || !kind || !appearance || !personality) {
    return res.status(400).json({ error: 'الحقول المطلوبة: name, kind, appearance, personality' });
  }
  if (!['anime', 'realistic'].includes(kind)) {
    return res.status(400).json({ error: "kind يجب أن يكون 'anime' أو 'realistic'" });
  }
  const id = nanoid();
  db.prepare(
    `INSERT INTO characters (id, name, kind, appearance, personality, scenario, unrestricted, created_at)
     VALUES (?, ?, ?, ?, ?, ?, ?, ?)`
  ).run(id, name, kind, appearance, personality, scenario || '', unrestricted === false ? 0 : 1, new Date().toISOString());

  res.status(201).json(db.prepare('SELECT * FROM characters WHERE id = ?').get(id));
});

router.delete('/:id', (req, res) => {
  db.prepare('DELETE FROM characters WHERE id = ?').run(req.params.id);
  res.json({ ok: true });
});

module.exports = router;
