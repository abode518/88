// image.js — عميل صور يدعم وضعين: sdwebui (A1111/ComfyUI shim) أو openai-compatible
const fetch = require('node-fetch');
const fs = require('fs');
const path = require('path');
const cfg = require('../config');
const { MEDIA_DIR } = require('../db');
const { GENDER_IMAGE_TAGS, describeTraitsImageTags } = require('../traits');

function stylePrefix(kind) {
  return kind === 'anime'
    ? 'anime style, high quality, detailed illustration, '
    : 'photorealistic, high detail, realistic lighting, ';
}

// برومبت افتراضي للأفاتار يُبنى بالكامل من اختيارات الشخصية إذا المستخدم ما كتب تفاصيل خاصة
function autoAvatarPrompt(character) {
  const traits = describeTraitsImageTags(character);
  return `${GENDER_IMAGE_TAGS[character.gender] || ''} portrait, looking at viewer, upper body, ${character.appearance}${traits ? ', ' + traits : ''}`;
}

async function generateImage({ character, prompt, negativePrompt, width, height, steps }) {
  if (!cfg.image.baseUrl) throw new Error('IMAGE_BASE_URL غير مضبوط — اضبطه في .env');

  const traits = describeTraitsImageTags(character);
  const userPrompt = prompt && prompt.trim() ? prompt.trim() : autoAvatarPrompt(character);
  // البرومبت يُنفَّذ كما هو بدون أي فلترة أو حذف من جهة الموقع — القيد الوحيد هو ما يفرضه محرك الصور المربوط
  const fullPrompt = `${stylePrefix(character.kind)}${character.appearance}${traits ? ', ' + traits : ''}, ${userPrompt}`;
  width = width || 768;
  height = height || 768;
  steps = steps || 28;

  let base64Images = [];

  if (cfg.image.mode === 'sdwebui') {
    const res = await fetch(`${cfg.image.baseUrl.replace(/\/$/, '')}/sdapi/v1/txt2img`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        ...(cfg.image.apiKey ? { Authorization: `Bearer ${cfg.image.apiKey}` } : {}),
      },
      body: JSON.stringify({
        prompt: fullPrompt,
        negative_prompt: negativePrompt || 'low quality, blurry, deformed',
        width,
        height,
        steps,
      }),
    });
    if (!res.ok) {
      const t = await res.text().catch(() => '');
      throw new Error(`فشل طلب الصورة (${res.status}): ${t.slice(0, 500)}`);
    }
    const data = await res.json();
    base64Images = data.images || [];
  } else if (cfg.image.mode === 'openai') {
    const res = await fetch(`${cfg.image.baseUrl.replace(/\/$/, '')}/images/generations`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        ...(cfg.image.apiKey ? { Authorization: `Bearer ${cfg.image.apiKey}` } : {}),
      },
      body: JSON.stringify({
        prompt: fullPrompt,
        size: `${width}x${height}`,
        n: 1,
        response_format: 'b64_json',
      }),
    });
    if (!res.ok) {
      const t = await res.text().catch(() => '');
      throw new Error(`فشل طلب الصورة (${res.status}): ${t.slice(0, 500)}`);
    }
    const data = await res.json();
    base64Images = (data.data || []).map((d) => d.b64_json).filter(Boolean);
  } else {
    throw new Error(`IMAGE_API_MODE غير معروف: ${cfg.image.mode}`);
  }

  if (!base64Images.length) throw new Error('لم يرجع محرك الصور أي نتيجة');

  const fileName = `img_${Date.now()}_${Math.random().toString(36).slice(2, 8)}.png`;
  const filePath = path.join(MEDIA_DIR, fileName);
  fs.writeFileSync(filePath, Buffer.from(base64Images[0], 'base64'));
  return fileName; // نسبي إلى MEDIA_DIR
}

module.exports = { generateImage };
