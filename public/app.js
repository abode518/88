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
    if (c.avatar_path) el.style.backgroundImage = `url(/media/${c.avatar_path})`;
    el.innerHTML = `<span class="char-item-text"><span>${escapeHtml(c.name)}</span><span class="kind">${kindLabel} · ${genderLabels[c.gender] || ''}</span></span>`;
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
  const bgEl = document.getElementById('chatBackground');
  if (c.avatar_path) {
    avatarEl.src = `/media/${c.avatar_path}`;
    avatarEl.style.display = 'block';
    bgEl.style.backgroundImage = `url(/media/${c.avatar_path})`;
  } else {
    avatarEl.style.display = 'none';
    bgEl.style.backgroundImage = 'none';
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

// ---- نموذج إنشاء/تعديل شخصية: معالج خطوات (wizard) ----
const modalOverlay = document.getElementById('modalOverlay');
let editingCharacterId = null; // null = إنشاء جديد، غير ذلك = تعديل
let selectedGender = 'female';
let selectedKind = 'realistic';

const WIZARD_STEPS = ['step0', 'step-role', 'step-voice', 'step-relationship', 'step-attrs', 'step-details'];

function closeCharModal() {
  modalOverlay.style.display = 'none';
}
document.querySelectorAll('.modal-cancel').forEach((b) => (b.onclick = closeCharModal));

function showStep(stepId) {
  WIZARD_STEPS.forEach((id) => {
    document.getElementById(id).style.display = id === stepId ? 'flex' : 'none';
  });
}
document.querySelectorAll('.step-next').forEach((b) => (b.onclick = () => showStep(b.dataset.next)));
document.querySelectorAll('.back-to-step0').forEach((b) => (b.onclick = () => showStep(b.dataset.back)));

// ---- تابات الجنس + كروت النمط (خطوة ٠) ----
document.getElementById('genderToggle').addEventListener('click', (e) => {
  const btn = e.target.closest('.gtab');
  if (!btn) return;
  selectedGender = btn.dataset.value;
  document.querySelectorAll('.gtab').forEach((b) => b.classList.toggle('active', b === btn));
  updatePenisFieldVisibility();
});
document.getElementById('kindCards').addEventListener('click', (e) => {
  const btn = e.target.closest('.style-card');
  if (!btn) return;
  selectedKind = btn.dataset.value;
  document.querySelectorAll('.style-card').forEach((b) => b.classList.toggle('active', b === btn));
});
document.getElementById('step0NextBtn').onclick = () => showStep('step-role');

// ---- صفوف تابات نصية عامة (الدور، الصوت) + خيار "أخرى" ----
function renderTabRow(groupId) {
  const group = document.getElementById(groupId);
  const options = JSON.parse(group.dataset.options);
  group.innerHTML =
    options.map((o) => `<button type="button" class="gtab" data-value="${o}">${o}</button>`).join('') +
    `<button type="button" class="gtab" data-value="other">أخرى...</button>`;
}
function wireTabRowOther(groupId, otherInputId) {
  const group = document.getElementById(groupId);
  const other = document.getElementById(otherInputId);
  group.addEventListener('click', (e) => {
    const btn = e.target.closest('.gtab');
    if (!btn) return;
    group.querySelectorAll('.gtab').forEach((b) => b.classList.toggle('active', b === btn));
    other.style.display = btn.dataset.value === 'other' ? 'block' : 'none';
  });
}
renderTabRow('roleTabs');
renderTabRow('voiceTabs');
wireTabRowOther('roleTabs', 'f_role_other');
wireTabRowOther('voiceTabs', 'f_voice_other');

function getTabRowValue(groupId, otherInputId) {
  const group = document.getElementById(groupId);
  const active = group.querySelector('.gtab.active');
  if (!active) return '';
  if (active.dataset.value === 'other') return document.getElementById(otherInputId).value.trim();
  return active.dataset.value;
}
function setTabRowValue(groupId, otherInputId, value) {
  const group = document.getElementById(groupId);
  const other = document.getElementById(otherInputId);
  const options = Array.from(group.querySelectorAll('.gtab'));
  const match = options.find((o) => o.dataset.value === value);
  options.forEach((b) => b.classList.remove('active'));
  if (match) {
    match.classList.add('active');
    other.style.display = 'none';
    other.value = '';
  } else if (value) {
    const otherBtn = options.find((o) => o.dataset.value === 'other');
    if (otherBtn) otherBtn.classList.add('active');
    other.style.display = 'block';
    other.value = value;
  } else {
    other.style.display = 'none';
    other.value = '';
  }
}

// ---- كروت نوع العلاقة بأيقونات + خيار "أخرى" ----
document.getElementById('relationshipCards').addEventListener('click', (e) => {
  const btn = e.target.closest('.icon-card');
  if (!btn) return;
  document.querySelectorAll('.icon-card').forEach((b) => b.classList.toggle('active', b === btn));
  document.getElementById('f_relationship_other').style.display = btn.dataset.value === 'other' ? 'block' : 'none';
});
function getRelationshipValue() {
  const active = document.querySelector('.icon-card.active');
  if (!active) return '';
  if (active.dataset.value === 'other') return document.getElementById('f_relationship_other').value.trim();
  return active.dataset.value;
}
function setRelationshipValue(value) {
  const cards = Array.from(document.querySelectorAll('.icon-card'));
  const other = document.getElementById('f_relationship_other');
  const match = cards.find((c) => c.dataset.value === value);
  cards.forEach((c) => c.classList.remove('active'));
  if (match) {
    match.classList.add('active');
    other.style.display = 'none';
    other.value = '';
  } else if (value) {
    const otherBtn = cards.find((c) => c.dataset.value === 'other');
    if (otherBtn) otherBtn.classList.add('active');
    other.style.display = 'block';
    other.value = value;
  } else {
    other.style.display = 'none';
    other.value = '';
  }
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
  document.getElementById('penisSizeLabel').style.display = HAS_PENIS_GENDERS.includes(selectedGender) ? 'flex' : 'none';
}

function setGenderTab(value) {
  selectedGender = value;
  document.querySelectorAll('.gtab').forEach((b) => {
    if (b.closest('#genderToggle')) b.classList.toggle('active', b.dataset.value === value);
  });
}
function setKindCard(value) {
  selectedKind = value;
  document.querySelectorAll('.style-card').forEach((b) => b.classList.toggle('active', b.dataset.value === value));
}

function openCharForm(character) {
  editingCharacterId = character ? character.id : null;
  document.getElementById('charFormTitle').textContent = character ? `تعديل ${character.name}` : 'التفاصيل النهائية';
  document.getElementById('charFormSubmitBtn').textContent = character ? 'حفظ التعديلات' : 'إنشاء';
  document.getElementById('charForm').reset();
  clearAllChoices();

  setGenderTab(character ? character.gender : 'female');
  setKindCard(character ? character.kind : 'realistic');
  setTabRowValue('roleTabs', 'f_role_other', character ? character.role : '');
  setTabRowValue('voiceTabs', 'f_voice_other', character ? character.voice : '');
  setRelationshipValue(character ? character.relationship_type : '');

  if (character) {
    document.getElementById('f_name').value = character.name;
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
  showStep(character ? 'step-details' : 'step0'); // تعديل يدخل مباشرة للتفاصيل، إنشاء يبدأ من اختيار الجنس/النمط
  modalOverlay.style.display = 'flex';
}

document.getElementById('newCharBtn').onclick = () => openCharForm(null);
document.getElementById('editCharBtn').onclick = () => openCharForm(currentChar);

document.getElementById('charForm').onsubmit = async (e) => {
  e.preventDefault();
  const payload = {
    name: document.getElementById('f_name').value.trim(),
    kind: selectedKind,
    gender: selectedGender,
    role: getTabRowValue('roleTabs', 'f_role_other'),
    voice: getTabRowValue('voiceTabs', 'f_voice_other'),
    relationshipType: getRelationshipValue(),
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
    document.getElementById('chatBackground').style.backgroundImage = `url(/media/${updated.avatar_path})`;
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

// ---- أزرار إجراءات سريعة (تُدرج نص بالصندوق، ما ترسل تلقائي) ----
const QUICK_ACTIONS = [
  'يقبّلها', 'يعانقها', 'يمص', 'من الخلف', 'وضعية فوق', 'وضعية فوق مقلوبة',
  'قذف', 'استمناء', 'بين الصدر', 'التبشيري', 'تقييد', 'إمناء باليد', 'سيلفي', 'وضعية تصوير',
];
function renderQuickActions() {
  const box = document.getElementById('quickActions');
  box.innerHTML = QUICK_ACTIONS.map((a) => `<button type="button" class="pill-btn" data-action="${escapeHtml(a)}">${escapeHtml(a)}</button>`).join('');
  box.querySelectorAll('button').forEach((btn) => {
    btn.onclick = () => {
      const input = document.getElementById('chatInput');
      const text = `*${btn.dataset.action}*`;
      input.value = input.value ? `${input.value} ${text}` : text;
      input.focus();
    };
  });
}
renderQuickActions();

// ---- ساعدني أرد: يقترح رسالة للمستخدم بدون إرسالها ----
document.getElementById('suggestReplyBtn').onclick = async () => {
  if (!currentChar) return;
  const btn = document.getElementById('suggestReplyBtn');
  btn.disabled = true;
  try {
    const { suggestion } = await api(`/api/chat/${currentChar.id}/suggest-reply`, { method: 'POST', body: '{}' });
    document.getElementById('chatInput').value = suggestion;
    document.getElementById('chatInput').focus();
  } catch (err) {
    alert(err.message);
  } finally {
    btn.disabled = false;
  }
};

// ---- بوابة العمر (مرة واحدة لكل متصفح) ----
(function ageGate() {
  let confirmed = false;
  try { confirmed = localStorage.getItem('age_confirmed') === 'yes'; } catch (e) {}
  if (confirmed) return;
  const overlay = document.getElementById('ageGateOverlay');
  overlay.style.display = 'flex';
  document.getElementById('ageGateConfirmBtn').onclick = () => {
    try { localStorage.setItem('age_confirmed', 'yes'); } catch (e) {}
    overlay.style.display = 'none';
  };
  document.getElementById('ageGateLeaveBtn').onclick = () => {
    window.location.href = 'about:blank';
  };
})();

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
