// routes/chat.js — شات مع شخصية: تاريخ محفوظ بقاعدة البيانات + استدعاء محرك LLM
const express = require('express');
const { nanoid } = require('nanoid');
const { db } = require('../db');
const llm = require('../engines/llm');

const router = express.Router();

router.get('/:characterId/messages', (req, res) => {
  const rows = db
    .prepare('SELECT * FROM messages WHERE character_id = ? ORDER BY created_at ASC')
    .all(req.params.characterId);
  res.json(rows);
});

router.post('/:characterId/messages', async (req, res) => {
  const character = db.prepare('SELECT * FROM characters WHERE id = ?').get(req.params.characterId);
  if (!character) return res.status(404).json({ error: 'الشخصية غير موجودة' });

  const { message } = req.body || {};
  if (!message || !message.trim()) return res.status(400).json({ error: 'الرسالة مطلوبة' });

  const history = db
    .prepare('SELECT role, content FROM messages WHERE character_id = ? ORDER BY created_at ASC LIMIT 40')
    .all(character.id);

  const now = new Date().toISOString();
  const userMsgId = nanoid();
  db.prepare('INSERT INTO messages (id, character_id, role, content, created_at) VALUES (?, ?, ?, ?, ?)').run(
    userMsgId,
    character.id,
    'user',
    message,
    now
  );

  try {
    const reply = await llm.chat(character, history, message);
    const assistantId = nanoid();
    const now2 = new Date().toISOString();
    db.prepare('INSERT INTO messages (id, character_id, role, content, created_at) VALUES (?, ?, ?, ?, ?)').run(
      assistantId,
      character.id,
      'assistant',
      reply,
      now2
    );
    res.json({ id: assistantId, role: 'assistant', content: reply, created_at: now2 });
  } catch (err) {
    res.status(502).json({ error: err.message });
  }
});

router.delete('/:characterId/messages', (req, res) => {
  db.prepare('DELETE FROM messages WHERE character_id = ?').run(req.params.characterId);
  res.json({ ok: true });
});

module.exports = router;
