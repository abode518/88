// server.js — جسر بين ComfyUI وواجهة الفيديو العامة اللي يتوقعها server/engines/video.js
// POST /generate {prompt, duration} -> {job_id}
// GET  /status/:id -> {status: 'pending'|'done'|'failed', video_url?, error?}
require('dotenv').config();
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const express = require('express');
const fetch = require('node-fetch');

const PORT = process.env.PORT || 9100;
const COMFY_URL = (process.env.COMFY_URL || 'http://127.0.0.1:8188').replace(/\/$/, '');
const WORKFLOW_PATH = process.env.WORKFLOW_PATH || './workflow.json';
const PROMPT_NODE_ID = process.env.PROMPT_NODE_ID;
const PROMPT_FIELD = process.env.PROMPT_FIELD || 'text';
const FRAMES_NODE_ID = process.env.FRAMES_NODE_ID || '';
const FRAMES_FIELD = process.env.FRAMES_FIELD || '';
const FPS = Number(process.env.FRAMES_PER_SECOND) || 8;

if (!PROMPT_NODE_ID) {
  console.error('ضبط PROMPT_NODE_ID في .env لازم — رقم عنصر البرومبت بالـ workflow المصدّر');
  process.exit(1);
}

let workflowTemplate;
try {
  workflowTemplate = JSON.parse(fs.readFileSync(path.resolve(WORKFLOW_PATH), 'utf8'));
} catch (err) {
  console.error(`فشل قراءة ${WORKFLOW_PATH}: ${err.message}`);
  console.error('صدّر الـ workflow من ComfyUI بصيغة API (Settings -> Enable Dev mode options, بعدها زر "Save (API Format)")');
  process.exit(1);
}

if (!workflowTemplate[PROMPT_NODE_ID]) {
  console.error(`لا يوجد عنصر برقم ${PROMPT_NODE_ID} في ${WORKFLOW_PATH} — راجع أرقام العناصر بالملف`);
  process.exit(1);
}

const app = express();
app.use(express.json({ limit: '2mb' }));

function buildWorkflow(prompt, duration) {
  const wf = JSON.parse(JSON.stringify(workflowTemplate)); // نسخة عشان ما نعدّل الأصل
  wf[PROMPT_NODE_ID].inputs[PROMPT_FIELD] = prompt; // البرومبت يُحقن كما هو، بدون أي فلترة
  if (FRAMES_NODE_ID && FRAMES_FIELD && duration) {
    wf[FRAMES_NODE_ID].inputs[FRAMES_FIELD] = Math.round(duration * FPS);
  }
  return wf;
}

app.post('/generate', async (req, res) => {
  const { prompt, duration } = req.body || {};
  if (!prompt) return res.status(400).json({ error: 'prompt مطلوب' });

  const clientId = crypto.randomUUID();
  try {
    const wf = buildWorkflow(prompt, duration);
    const r = await fetch(`${COMFY_URL}/prompt`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ prompt: wf, client_id: clientId }),
    });
    const data = await r.json();
    if (!r.ok || !data.prompt_id) {
      return res.status(502).json({ error: `رفض ComfyUI الطلب: ${JSON.stringify(data.node_errors || data)}` });
    }
    res.json({ job_id: data.prompt_id });
  } catch (err) {
    res.status(502).json({ error: `تعذر الاتصال بـ ComfyUI على ${COMFY_URL}: ${err.message}` });
  }
});

// يبحث بكل مخرجات العناصر عن أي ملف فيديو/gif/webp ناتج (VHS_VideoCombine, SaveAnimatedWEBP, إلخ)
function findVideoOutput(outputs) {
  for (const nodeId of Object.keys(outputs)) {
    const node = outputs[nodeId];
    for (const key of ['gifs', 'videos', 'images']) {
      const items = node[key];
      if (Array.isArray(items) && items.length) {
        const last = items[items.length - 1];
        if (last.filename) return last;
      }
    }
  }
  return null;
}

app.get('/status/:id', async (req, res) => {
  try {
    const r = await fetch(`${COMFY_URL}/history/${encodeURIComponent(req.params.id)}`);
    if (!r.ok) return res.json({ status: 'pending' });
    const data = await r.json();
    const entry = data[req.params.id];
    if (!entry) return res.json({ status: 'pending' });

    if (entry.status && entry.status.status_str === 'error') {
      return res.json({ status: 'failed', error: 'فشل تنفيذ الـ workflow في ComfyUI — راجع لوق ComfyUI' });
    }
    if (!entry.outputs) return res.json({ status: 'pending' });

    const file = findVideoOutput(entry.outputs);
    if (!file) return res.json({ status: 'pending' });

    const qs = new URLSearchParams({
      filename: file.filename,
      subfolder: file.subfolder || '',
      type: file.type || 'output',
    });
    res.json({ status: 'done', video_url: `${COMFY_URL}/view?${qs.toString()}` });
  } catch (err) {
    res.status(502).json({ error: err.message });
  }
});

app.listen(PORT, () => console.log(`جسر ComfyUI يعمل على http://localhost:${PORT} (يتصل بـ ${COMFY_URL})`));
