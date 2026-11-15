// index.js — Node/Express, الموقع الرئيسي: شخصيات + شات + صور + فيديو
const path = require('path');
const express = require('express');
require('./db'); // تهيئة القاعدة والمجلدات
const cfg = require('./config');

const app = express();
app.use(express.json({ limit: '5mb' }));

app.use('/media', express.static(path.join(__dirname, '..', 'data', 'media')));
app.use(express.static(path.join(__dirname, '..', 'public')));

app.use('/api/characters', require('./routes/characters'));
app.use('/api/chat', require('./routes/chat'));
app.use('/api/media', require('./routes/media'));

app.get('/api/status', (req, res) => {
  res.json({
    llm_configured: Boolean(cfg.llm.baseUrl),
    image_configured: Boolean(cfg.image.baseUrl),
    video_configured: Boolean(cfg.video.baseUrl),
  });
});

app.listen(cfg.port, () => {
  console.log(`يعمل على http://localhost:${cfg.port}`);
});
