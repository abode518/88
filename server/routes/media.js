// routes/media.js — توليد صور (فوري) وفيديو (job polling) + سجل الوسائط لكل شخصية
const express = require('express');
const { nanoid } = require('nanoid');
const { db } = require('../db');
const imageEngine = require('../engines/image');
const videoEngine = require('../engines/video');

const router = express.Router();

router.get('/:characterId', (req, res) => {
  const rows = db
    .prepare('SELECT * FROM media WHERE character_id = ? ORDER BY created_at DESC')
    .all(req.params.characterId);
  res.json(rows);
});

router.post('/:characterId/image', async (req, res) => {
  const character = db.prepare('SELECT * FROM characters WHERE id = ?').get(req.params.characterId);
  if (!character) return res.status(404).json({ error: 'الشخصية غير موجودة' });

  const { prompt, negativePrompt, width, height, steps } = req.body || {};
  if (!prompt) return res.status(400).json({ error: 'prompt مطلوب' });

  const id = nanoid();
  const now = new Date().toISOString();
  db.prepare(
    `INSERT INTO media (id, character_id, type, prompt, status, created_at) VALUES (?, ?, 'image', ?, 'pending', ?)`
  ).run(id, character.id, prompt, now);

  try {
    const fileName = await imageEngine.generateImage({ character, prompt, negativePrompt, width, height, steps });
    db.prepare(`UPDATE media SET status = 'done', file_path = ? WHERE id = ?`).run(fileName, id);
    res.status(201).json(db.prepare('SELECT * FROM media WHERE id = ?').get(id));
  } catch (err) {
    db.prepare(`UPDATE media SET status = 'failed', error = ? WHERE id = ?`).run(err.message, id);
    res.status(502).json({ error: err.message });
  }
});

router.post('/:characterId/video', async (req, res) => {
  const character = db.prepare('SELECT * FROM characters WHERE id = ?').get(req.params.characterId);
  if (!character) return res.status(404).json({ error: 'الشخصية غير موجودة' });

  const { prompt, duration } = req.body || {};
  if (!prompt) return res.status(400).json({ error: 'prompt مطلوب' });

  const id = nanoid();
  const now = new Date().toISOString();
  db.prepare(
    `INSERT INTO media (id, character_id, type, prompt, status, created_at) VALUES (?, ?, 'video', ?, 'pending', ?)`
  ).run(id, character.id, prompt, now);

  try {
    const jobId = await videoEngine.startJob({ character, prompt, duration });
    db.prepare(`UPDATE media SET job_id = ? WHERE id = ?`).run(jobId, id);
    res.status(202).json(db.prepare('SELECT * FROM media WHERE id = ?').get(id));
  } catch (err) {
    db.prepare(`UPDATE media SET status = 'failed', error = ? WHERE id = ?`).run(err.message, id);
    res.status(502).json({ error: err.message });
  }
});

// يُستدعى بشكل متكرر من الواجهة لمعرفة إن انتهى الفيديو
router.get('/:characterId/video/:mediaId/poll', async (req, res) => {
  const media = db.prepare('SELECT * FROM media WHERE id = ? AND character_id = ?').get(
    req.params.mediaId,
    req.params.characterId
  );
  if (!media) return res.status(404).json({ error: 'غير موجود' });
  if (media.status !== 'pending' || !media.job_id) return res.json(media);

  try {
    const status = await videoEngine.checkJob(media.job_id);
    if (status.status === 'done' && status.video_url) {
      const fileName = await videoEngine.downloadToMedia(status.video_url);
      db.prepare(`UPDATE media SET status = 'done', file_path = ? WHERE id = ?`).run(fileName, media.id);
    } else if (status.status === 'failed') {
      db.prepare(`UPDATE media SET status = 'failed', error = ? WHERE id = ?`).run(
        status.error || 'فشل غير محدد',
        media.id
      );
    }
    res.json(db.prepare('SELECT * FROM media WHERE id = ?').get(media.id));
  } catch (err) {
    res.status(502).json({ error: err.message });
  }
});

router.delete('/:characterId/:mediaId', (req, res) => {
  db.prepare('DELETE FROM media WHERE id = ? AND character_id = ?').run(req.params.mediaId, req.params.characterId);
  res.json({ ok: true });
});

module.exports = router;
