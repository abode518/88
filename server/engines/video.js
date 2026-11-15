// video.js — عميل فيديو بنمط job-polling عام: POST /generate -> {job_id}, GET /status/:id -> {status, video_url}
const fetch = require('node-fetch');
const fs = require('fs');
const path = require('path');
const cfg = require('../config');
const { MEDIA_DIR } = require('../db');

async function startJob({ character, prompt, duration }) {
  if (!cfg.video.baseUrl) throw new Error('VIDEO_BASE_URL غير مضبوط — اضبطه في .env');

  const fullPrompt = `${character.name}, ${character.appearance}, ${prompt}`;
  const res = await fetch(`${cfg.video.baseUrl.replace(/\/$/, '')}/generate`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      ...(cfg.video.apiKey ? { Authorization: `Bearer ${cfg.video.apiKey}` } : {}),
    },
    body: JSON.stringify({ prompt: fullPrompt, duration: duration || 4 }),
  });
  if (!res.ok) {
    const t = await res.text().catch(() => '');
    throw new Error(`فشل بدء مهمة الفيديو (${res.status}): ${t.slice(0, 500)}`);
  }
  const data = await res.json();
  if (!data.job_id) throw new Error('لم يرجع محرك الفيديو job_id');
  return data.job_id;
}

async function checkJob(jobId) {
  const res = await fetch(
    `${cfg.video.baseUrl.replace(/\/$/, '')}/status/${encodeURIComponent(jobId)}`,
    { headers: cfg.video.apiKey ? { Authorization: `Bearer ${cfg.video.apiKey}` } : {} }
  );
  if (!res.ok) {
    const t = await res.text().catch(() => '');
    throw new Error(`فشل التحقق من حالة الفيديو (${res.status}): ${t.slice(0, 500)}`);
  }
  return res.json(); // { status: 'pending'|'done'|'failed', video_url? }
}

async function downloadToMedia(videoUrl) {
  const res = await fetch(videoUrl, {
    headers: cfg.video.apiKey ? { Authorization: `Bearer ${cfg.video.apiKey}` } : {},
  });
  if (!res.ok) throw new Error(`فشل تحميل الفيديو (${res.status})`);
  const buf = await res.buffer();
  const fileName = `vid_${Date.now()}_${Math.random().toString(36).slice(2, 8)}.mp4`;
  fs.writeFileSync(path.join(MEDIA_DIR, fileName), buf);
  return fileName;
}

module.exports = { startJob, checkJob, downloadToMedia };
