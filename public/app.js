// app.js — واجهة SPA بسيطة بدون فريمورك
let currentChar = null;
let videoPollTimers = {};

async function api(path, opts) {
  const res = await fetch(path, {
    headers: { 'Content-Type': 'application/json' },
    ...opts,
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || `خطأ ${res.status}`);
  return data;
}

async function loadStatus() {
  try {
    const s = await api('/api/status');
    const line = (label, ok) => `<div>${label}: <span class="${ok ? 'ok' : 'bad'}">${ok ? 'مضبوط' : 'غير مضبوط'}</span></div>`;
    document.getElementById('statusBox').innerHTML =
      line('الشات', s.llm_configured) + line('الصور', s.image_configured) + line('الفيديو', s.video_configured);
  } catch (e) {}
}

async function loadCharacters() {
  const chars = await api('/api/characters');
  const list = document.getElementById('charList');
  list.innerHTML = '';
  chars.forEach((c) => {
    const el = document.createElement('div');
    el.className = 'char-item' + (currentChar && currentChar.id === c.id ? ' active' : '');
    el.innerHTML = `<span>${escapeHtml(c.name)}</span><span class="kind">${c.kind === 'anime' ? 'أنمي' : 'حقيقية'}</span>`;
    el.onclick = () => selectCharacter(c);
    list.appendChild(el);
  });
}

function escapeHtml(s) {
  return s.replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
}

async function selectCharacter(c) {
  currentChar = c;
  document.getElementById('emptyState').style.display = 'none';
  document.getElementById('charView').style.display = 'flex';
  document.getElementById('charName').textContent = c.name;
  await loadCharacters();
  await loadMessages();
  await loadMedia();
}

async function loadMessages() {
  const msgs = await api(`/api/chat/${currentChar.id}/messages`);
  const log = document.getElementById('chatLog');
  log.innerHTML = '';
  msgs.forEach((m) => appendMessage(m.role, m.content));
}

function appendMessage(role, content) {
  const log = document.getElementById('chatLog');
  const div = document.createElement('div');
  div.className = `msg ${role}`;
  div.textContent = content;
  log.appendChild(div);
  log.scrollTop = log.scrollHeight;
}

async function loadMedia() {
  const items = await api(`/api/media/${currentChar.id}`);
  const imageGrid = document.getElementById('imageGrid');
  const videoGrid = document.getElementById('videoGrid');
  imageGrid.innerHTML = '';
  videoGrid.innerHTML = '';
  items.forEach((m) => {
    if (m.type === 'image') imageGrid.appendChild(renderMediaCard(m));
    else {
      videoGrid.appendChild(renderMediaCard(m));
      if (m.status === 'pending') startVideoPoll(m.id);
    }
  });
}

function renderMediaCard(m) {
  const card = document.createElement('div');
  card.className = 'media-card';
  card.dataset.id = m.id;
  let body = '';
  if (m.status === 'done' && m.file_path) {
    body = m.type === 'image'
      ? `<img src="/media/${m.file_path}" alt="" />`
      : `<video src="/media/${m.file_path}" controls></video>`;
  } else if (m.status === 'failed') {
    body = `<div class="meta failed">فشل: ${escapeHtml(m.error || '')}</div>`;
  } else {
    body = `<div class="meta pending">جارٍ التوليد...</div>`;
  }
  card.innerHTML = body + `<div class="meta">${escapeHtml(m.prompt)}</div>`;
  return card;
}

function startVideoPoll(mediaId) {
  if (videoPollTimers[mediaId]) return;
  videoPollTimers[mediaId] = setInterval(async () => {
    try {
      const m = await api(`/api/media/${currentChar.id}/video/${mediaId}/poll`);
      if (m.status !== 'pending') {
        clearInterval(videoPollTimers[mediaId]);
        delete videoPollTimers[mediaId];
        await loadMedia();
      }
    } catch (e) {
      clearInterval(videoPollTimers[mediaId]);
      delete videoPollTimers[mediaId];
    }
  }, 3000);
}

// ---- تبويبات ----
document.querySelectorAll('.tab-btn[data-tab]').forEach((btn) => {
  btn.onclick = () => {
    document.querySelectorAll('.tab-btn[data-tab]').forEach((b) => b.classList.remove('active'));
    document.querySelectorAll('.tab-panel').forEach((p) => p.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById(`tab-${btn.dataset.tab}`).classList.add('active');
  };
});

// ---- نموذج إنشاء شخصية ----
const modalOverlay = document.getElementById('modalOverlay');
document.getElementById('newCharBtn').onclick = () => (modalOverlay.style.display = 'flex');
document.getElementById('cancelCharBtn').onclick = () => (modalOverlay.style.display = 'none');

document.getElementById('charForm').onsubmit = async (e) => {
  e.preventDefault();
  const payload = {
    name: document.getElementById('f_name').value.trim(),
    kind: document.getElementById('f_kind').value,
    appearance: document.getElementById('f_appearance').value.trim(),
    personality: document.getElementById('f_personality').value.trim(),
    scenario: document.getElementById('f_scenario').value.trim(),
  };
  try {
    const c = await api('/api/characters', { method: 'POST', body: JSON.stringify(payload) });
    modalOverlay.style.display = 'none';
    e.target.reset();
    await loadCharacters();
    await selectCharacter(c);
  } catch (err) {
    alert(err.message);
  }
};

document.getElementById('deleteCharBtn').onclick = async () => {
  if (!currentChar) return;
  if (!confirm(`حذف "${currentChar.name}" نهائياً؟`)) return;
  await api(`/api/characters/${currentChar.id}`, { method: 'DELETE' });
  currentChar = null;
  document.getElementById('charView').style.display = 'none';
  document.getElementById('emptyState').style.display = 'block';
  await loadCharacters();
};

// ---- الشات ----
document.getElementById('chatForm').onsubmit = async (e) => {
  e.preventDefault();
  const input = document.getElementById('chatInput');
  const text = input.value.trim();
  if (!text || !currentChar) return;
  input.value = '';
  appendMessage('user', text);
  try {
    const reply = await api(`/api/chat/${currentChar.id}/messages`, {
      method: 'POST',
      body: JSON.stringify({ message: text }),
    });
    appendMessage('assistant', reply.content);
  } catch (err) {
    appendMessage('assistant', `[خطأ] ${err.message}`);
  }
};

// ---- الصور ----
document.getElementById('imageForm').onsubmit = async (e) => {
  e.preventDefault();
  const prompt = document.getElementById('imagePrompt').value.trim();
  const negativePrompt = document.getElementById('imageNeg').value.trim();
  if (!prompt || !currentChar) return;
  try {
    await api(`/api/media/${currentChar.id}/image`, {
      method: 'POST',
      body: JSON.stringify({ prompt, negativePrompt }),
    });
    document.getElementById('imagePrompt').value = '';
    await loadMedia();
  } catch (err) {
    alert(err.message);
  }
};

// ---- الفيديو ----
document.getElementById('videoForm').onsubmit = async (e) => {
  e.preventDefault();
  const prompt = document.getElementById('videoPrompt').value.trim();
  const duration = Number(document.getElementById('videoDuration').value) || 4;
  if (!prompt || !currentChar) return;
  try {
    await api(`/api/media/${currentChar.id}/video`, {
      method: 'POST',
      body: JSON.stringify({ prompt, duration }),
    });
    document.getElementById('videoPrompt').value = '';
    await loadMedia();
  } catch (err) {
    alert(err.message);
  }
};

loadStatus();
loadCharacters();
