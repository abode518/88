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
    const genderLabels = { male: 'ولد', female: 'بنت', femboy: 'فيمبوي', tomboy: 'تومبوي', futa: 'فوتا' };
    const kindLabel = c.kind === 'anime' ? 'أنمي' : 'حقيقية';
    const avatarImg = c.avatar_path
      ? `<img class="avatar-xs" src="/media/${c.avatar_path}" alt="" />`
      : `<div class="avatar-xs"></div>`;
    el.innerHTML = `${avatarImg}<span class="char-item-text"><span>${escapeHtml(c.name)}</span><span class="kind">${kindLabel} · ${genderLabels[c.gender] || ''}</span></span>`;
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
  document.getElementById('app').classList.add('char-open'); // بالجوال: يخفي القائمة ويعرض المحادثة كاملة
  const avatarEl = document.getElementById('charAvatar');
  if (c.avatar_path) {
    avatarEl.src = `/media/${c.avatar_path}`;
    avatarEl.style.display = 'block';
  } else {
    avatarEl.style.display = 'none';
  }
  await loadCharacters();
  await loadMessages();
  await loadMedia();
}

document.getElementById('backToListBtn').onclick = () => {
  document.getElementById('app').classList.remove('char-open');
};

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

// ---- نموذج إنشاء/تعديل شخصية ----
const modalOverlay = document.getElementById('modalOverlay');
let editingCharacterId = null; // null = إنشاء جديد، غير ذلك = تعديل

document.getElementById('cancelCharBtn').onclick = () => (modalOverlay.style.display = 'none');

// حقول select فيها خيار "أخرى..." تكشف input نصي بجانبها
function wireOtherSelect(selectId, otherId) {
  const sel = document.getElementById(selectId);
  const other = document.getElementById(otherId);
  sel.onchange = () => {
    other.style.display = sel.value === 'other' ? 'block' : 'none';
  };
}
wireOtherSelect('f_role', 'f_role_other');
wireOtherSelect('f_voice', 'f_voice_other');
wireOtherSelect('f_relationship', 'f_relationship_other');

function selectValue(selectId, otherId) {
  const sel = document.getElementById(selectId);
  if (sel.value === 'other') return document.getElementById(otherId).value.trim();
  return sel.value;
}

// ---- عناصر اختيار السمات الجسدية (دوائر ألوان / كبسولات) ----
function renderChoiceWidgets() {
  document.querySelectorAll('.swatch-group').forEach((group) => {
    const colors = JSON.parse(group.dataset.colors);
    group.innerHTML = colors
      .map(([label, hex]) => `<button type="button" class="swatch-btn" data-value="${label}" style="background:${hex}" title="${label}"></button>`)
      .join('');
  });
  document.querySelectorAll('.pill-group').forEach((group) => {
    const options = JSON.parse(group.dataset.options);
    const isSize = group.classList.contains('size-group');
    group.innerHTML = options
      .map((o) => `<button type="button" class="pill-btn" data-value="${o}">${isSize ? `<span class="bar"></span>${o}` : o}</button>`)
      .join('');
  });
  document.querySelectorAll('.swatch-group, .pill-group').forEach((group) => {
    group.addEventListener('click', (e) => {
      const btn = e.target.closest('button');
      if (!btn) return;
      const wasSelected = btn.classList.contains('selected');
      group.querySelectorAll('button').forEach((b) => b.classList.remove('selected'));
      if (!wasSelected) btn.classList.add('selected');
    });
  });
}
renderChoiceWidgets();

function getChoiceValue(field) {
  const group = document.querySelector(`[data-field="${field}"]`);
  const sel = group.querySelector('.selected');
  return sel ? sel.dataset.value : '';
}
function setChoiceValue(field, value) {
  const group = document.querySelector(`[data-field="${field}"]`);
  group.querySelectorAll('button').forEach((b) => b.classList.toggle('selected', b.dataset.value === value));
}
function clearAllChoices() {
  document.querySelectorAll('.swatch-group button, .pill-group button').forEach((b) => b.classList.remove('selected'));
}

const HAS_PENIS_GENDERS = ['male', 'femboy', 'futa'];
function updatePenisFieldVisibility() {
  const show = HAS_PENIS_GENDERS.includes(document.getElementById('f_gender').value);
  document.getElementById('penisSizeLabel').style.display = show ? 'flex' : 'none';
}
document.getElementById('f_gender').onchange = updatePenisFieldVisibility;

function setSelectWithOther(selectId, otherId, value) {
  const sel = document.getElementById(selectId);
  const other = document.getElementById(otherId);
  const hasOption = Array.from(sel.options).some((o) => o.value === value);
  if (value && !hasOption) {
    sel.value = 'other';
    other.value = value;
    other.style.display = 'block';
  } else {
    sel.value = value || sel.options[0].value;
    other.style.display = 'none';
    other.value = '';
  }
}

function openCharForm(character) {
  editingCharacterId = character ? character.id : null;
  document.getElementById('charFormTitle').textContent = character ? `تعديل ${character.name}` : 'شخصية جديدة';
  document.getElementById('charFormSubmitBtn').textContent = character ? 'حفظ التعديلات' : 'إنشاء';
  document.getElementById('charForm').reset();
  clearAllChoices();

  if (character) {
    document.getElementById('f_name').value = character.name;
    document.getElementById('f_kind').value = character.kind;
    document.getElementById('f_gender').value = character.gender;
    setSelectWithOther('f_role', 'f_role_other', character.role);
    setSelectWithOther('f_voice', 'f_voice_other', character.voice);
    setSelectWithOther('f_relationship', 'f_relationship_other', character.relationship_type);
    document.getElementById('f_appearance').value = character.appearance;
    document.getElementById('f_personality').value = character.personality;
    document.getElementById('f_scenario').value = character.scenario || '';
    setChoiceValue('hairColor', character.hair_color);
    setChoiceValue('hairstyle', character.hairstyle);
    setChoiceValue('eyeColor', character.eye_color);
    setChoiceValue('breastSize', character.breast_size);
    setChoiceValue('buttSize', character.butt_size);
    setChoiceValue('penisSize', character.penis_size);
  }
  updatePenisFieldVisibility();
  modalOverlay.style.display = 'flex';
}

document.getElementById('newCharBtn').onclick = () => openCharForm(null);
document.getElementById('editCharBtn').onclick = () => openCharForm(currentChar);

document.getElementById('charForm').onsubmit = async (e) => {
  e.preventDefault();
  const payload = {
    name: document.getElementById('f_name').value.trim(),
    kind: document.getElementById('f_kind').value,
    gender: document.getElementById('f_gender').value,
    role: selectValue('f_role', 'f_role_other'),
    voice: selectValue('f_voice', 'f_voice_other'),
    relationshipType: selectValue('f_relationship', 'f_relationship_other'),
    hairColor: getChoiceValue('hairColor'),
    hairstyle: getChoiceValue('hairstyle'),
    eyeColor: getChoiceValue('eyeColor'),
    breastSize: getChoiceValue('breastSize'),
    buttSize: getChoiceValue('buttSize'),
    penisSize: getChoiceValue('penisSize'),
    appearance: document.getElementById('f_appearance').value.trim(),
    personality: document.getElementById('f_personality').value.trim(),
    scenario: document.getElementById('f_scenario').value.trim(),
  };
  try {
    const c = editingCharacterId
      ? await api(`/api/characters/${editingCharacterId}`, { method: 'PATCH', body: JSON.stringify(payload) })
      : await api('/api/characters', { method: 'POST', body: JSON.stringify(payload) });
    modalOverlay.style.display = 'none';
    e.target.reset();
    clearAllChoices();
    await loadCharacters();
    await selectCharacter(c);
    if (!editingCharacterId) openAvatarModal(); // بعد الإنشاء فقط
    editingCharacterId = null;
  } catch (err) {
    alert(err.message);
  }
};

// ---- صورة الشخصية (بعد الإنشاء أو عند إعادة التوليد) ----
const avatarModalOverlay = document.getElementById('avatarModalOverlay');

function openAvatarModal() {
  document.getElementById('avatarPrompt').value = '';
  renderAvatarPreview();
  avatarModalOverlay.style.display = 'flex';
}

function renderAvatarPreview() {
  const box = document.getElementById('avatarPreview');
  box.innerHTML = currentChar && currentChar.avatar_path
    ? `<img src="/media/${currentChar.avatar_path}" alt="" />`
    : '<span class="muted">لا توجد صورة بعد</span>';
}

async function generateAvatar(prompt) {
  if (!currentChar) return;
  const box = document.getElementById('avatarPreview');
  box.innerHTML = '<span class="muted">جارٍ التوليد...</span>';
  try {
    const updated = await api(`/api/characters/${currentChar.id}/avatar`, {
      method: 'POST',
      body: JSON.stringify({ prompt: prompt || '' }),
    });
    currentChar = updated;
    renderAvatarPreview();
    const avatarEl = document.getElementById('charAvatar');
    avatarEl.src = `/media/${updated.avatar_path}`;
    avatarEl.style.display = 'block';
    await loadCharacters();
    await loadMedia();
  } catch (err) {
    box.innerHTML = `<span class="muted">فشل: ${escapeHtml(err.message)}</span>`;
  }
}

document.getElementById('avatarAutoBtn').onclick = () => generateAvatar('');
document.getElementById('avatarCustomBtn').onclick = () =>
  generateAvatar(document.getElementById('avatarPrompt').value.trim());
document.getElementById('avatarSkipBtn').onclick = () => (avatarModalOverlay.style.display = 'none');
document.getElementById('regenAvatarBtn').onclick = () => openAvatarModal();

document.getElementById('deleteCharBtn').onclick = async () => {
  if (!currentChar) return;
  if (!confirm(`حذف "${currentChar.name}" نهائياً؟`)) return;
  await api(`/api/characters/${currentChar.id}`, { method: 'DELETE' });
  currentChar = null;
  document.getElementById('charView').style.display = 'none';
  document.getElementById('emptyState').style.display = 'block';
  document.getElementById('app').classList.remove('char-open');
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
