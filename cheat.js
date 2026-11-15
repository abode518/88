// Rob Run cheat — browser inject (Phaser 2/3 + vanilla canvas)
// inject via console, bookmarklet, or tampermonkey
// targets: window.game, window.Game, Phaser scene registry, common score/lives vars

(function() {
  'use strict';

  // ── config ──────────────────────────────────────────────────────────────────
  const CFG = {
    scoreAdd:    99999,
    speedMult:   0.5,    // <1 = slow, >1 = fast
    autoJump:    false,
    jumpKey:     ' ',
    jumpIntervalMs: 300,
  };

  // ── utils ────────────────────────────────────────────────────────────────────
  const log = (msg) => console.log(`%c[RobRun] ${msg}`, 'color:#0ff;font-weight:bold');

  function findGame() {
    // Phaser 3
    if (window.game && window.game.scene) return { type: 'phaser3', ref: window.game };
    // Phaser 2 / CE
    if (window.game && window.game.state) return { type: 'phaser2', ref: window.game };
    // generic
    if (window.Game) return { type: 'generic', ref: window.Game };
    return null;
  }

  // ── Phaser 3 helpers ─────────────────────────────────────────────────────────
  function getActiveScene(game) {
    const scenes = game.scene.scenes;
    return scenes.find(s => s.sys.isActive()) || scenes[0];
  }

  function patchPhaser3Lives(scene) {
    // try common prop names
    const keys = ['lives', 'health', 'hp', 'lifes', 'hearts'];
    for (const k of keys) {
      if (scene[k] !== undefined) {
        scene[k] = 999;
        log(`lives patched via scene.${k} = 999`);
        return true;
      }
    }
    return false;
  }

  function patchPhaser3Score(scene, add) {
    const keys = ['score', 'points', 'coins'];
    for (const k of keys) {
      if (scene[k] !== undefined) {
        scene[k] += add;
        log(`score bumped via scene.${k} += ${add} → ${scene[k]}`);
        return true;
      }
    }
    return false;
  }

  // ── global var scanner ───────────────────────────────────────────────────────
  function scanGlobals() {
    const scoreKeys = ['score','Score','points','Points','coins','Coins'];
    const livesKeys = ['lives','Lives','hp','HP','health','Health'];
    const found = {};
    for (const k of scoreKeys) if (window[k] !== undefined && typeof window[k] === 'number') found[k] = 'score';
    for (const k of livesKeys) if (window[k] !== undefined && typeof window[k] === 'number') found[k] = 'lives';
    return found;
  }

  // ── RAF speed hook ───────────────────────────────────────────────────────────
  let _origRAF = null;
  let _speedActive = false;

  function hookSpeed(mult) {
    if (_speedActive) return;
    _speedActive = true;
    _origRAF = window.requestAnimationFrame;
    let last = performance.now();
    window.requestAnimationFrame = function(cb) {
      return _origRAF.call(window, function(ts) {
        const delta = ts - last;
        last = ts;
        cb(ts - delta * (1 - mult)); // shift timestamp to change perceived dt
      });
    };
    log(`RAF speed hook active (×${mult})`);
  }

  function unhookSpeed() {
    if (!_speedActive || !_origRAF) return;
    window.requestAnimationFrame = _origRAF;
    _origRAF = null;
    _speedActive = false;
    log('RAF speed hook removed');
  }

  // ── auto-jump ────────────────────────────────────────────────────────────────
  let _jumpTimer = null;

  function startAutoJump() {
    if (_jumpTimer) return;
    _jumpTimer = setInterval(() => {
      const ev = new KeyboardEvent('keydown', { key: CFG.jumpKey, code: 'Space', bubbles: true });
      document.dispatchEvent(ev);
      window.dispatchEvent(ev);
      const focused = document.activeElement;
      if (focused && focused !== document.body) focused.dispatchEvent(ev);
    }, CFG.jumpIntervalMs);
    log(`auto-jump ON (every ${CFG.jumpIntervalMs}ms)`);
  }

  function stopAutoJump() {
    clearInterval(_jumpTimer);
    _jumpTimer = null;
    log('auto-jump OFF');
  }

  // ── invincibility ─────────────────────────────────────────────────────────────
  // Phaser 3: patch physics body overlap/collider callbacks via scene
  function tryInvincibility() {
    const g = findGame();
    if (!g || g.type !== 'phaser3') {
      log('invincibility: manual — set player.active=false to skip collisions or patch your own damage fn');
      return;
    }
    const scene = getActiveScene(g.ref);
    // freeze lives at high value via Object.defineProperty on scene
    const lk = ['lives','health','hp'].find(k => scene[k] !== undefined);
    if (lk) {
      Object.defineProperty(scene, lk, { get: () => 999, set: () => {}, configurable: true });
      log(`invincibility: scene.${lk} frozen at 999`);
    } else {
      log('invincibility: no lives prop found — open issue');
    }
  }

  function removeInvincibility() {
    const g = findGame();
    if (!g || g.type !== 'phaser3') return;
    const scene = getActiveScene(g.ref);
    ['lives','health','hp'].forEach(k => {
      try { delete scene[k]; scene[k] = 999; } catch {}
    });
    log('invincibility removed');
  }

  // ── panel UI ─────────────────────────────────────────────────────────────────
  function buildPanel() {
    if (document.getElementById('rr-cheat-panel')) return;

    const panel = document.createElement('div');
    panel.id = 'rr-cheat-panel';
    Object.assign(panel.style, {
      position: 'fixed', top: '10px', right: '10px', zIndex: '999999',
      background: 'rgba(0,0,0,0.85)', border: '1px solid #0ff',
      borderRadius: '6px', padding: '12px', color: '#fff',
      fontFamily: 'monospace', fontSize: '13px', userSelect: 'none',
      minWidth: '220px', cursor: 'move',
    });

    panel.innerHTML = `
      <div style="color:#0ff;font-weight:bold;margin-bottom:8px">⚡ Rob Run Cheat</div>
      <button id="rr-score">+Score (${CFG.scoreAdd})</button>
      <button id="rr-lives">∞ Lives</button>
      <button id="rr-invinc">Invincible ON</button>
      <br><br>
      <label>Speed ×
        <input id="rr-speed-val" type="number" value="${CFG.speedMult}" step="0.1" min="0.1" max="5" style="width:50px">
      </label>
      <button id="rr-speed-on">Hook</button>
      <button id="rr-speed-off">Unhook</button>
      <br><br>
      <button id="rr-jump-on">AutoJump ON</button>
      <button id="rr-jump-off">AutoJump OFF</button>
      <br><br>
      <button id="rr-scan">Scan Globals</button>
      <div id="rr-log" style="margin-top:8px;color:#0f0;font-size:11px;max-height:80px;overflow:auto"></div>
      <button id="rr-close" style="position:absolute;top:4px;right:6px;background:none;border:none;color:#f44;cursor:pointer;font-size:14px">✕</button>
    `;

    // minimal button style
    panel.querySelectorAll('button').forEach(b => Object.assign(b.style, {
      background: '#1a1a2e', color: '#0ff', border: '1px solid #0ff',
      borderRadius: '3px', cursor: 'pointer', margin: '2px', padding: '3px 7px',
      fontSize: '12px',
    }));

    document.body.appendChild(panel);

    const logEl = document.getElementById('rr-log');
    const uiLog = (msg) => { logEl.textContent = msg + '\n' + logEl.textContent; log(msg); };

    // drag
    let dx = 0, dy = 0, dragging = false;
    panel.addEventListener('mousedown', e => { dragging = true; dx = e.clientX - panel.offsetLeft; dy = e.clientY - panel.offsetTop; });
    document.addEventListener('mousemove', e => { if (dragging) { panel.style.left = (e.clientX - dx) + 'px'; panel.style.top = (e.clientY - dy) + 'px'; panel.style.right = 'auto'; } });
    document.addEventListener('mouseup', () => { dragging = false; });

    document.getElementById('rr-close').onclick = () => panel.remove();

    document.getElementById('rr-score').onclick = () => {
      const g = findGame();
      if (g && g.type === 'phaser3') {
        const sc = getActiveScene(g.ref);
        if (patchPhaser3Score(sc, CFG.scoreAdd)) { uiLog(`+${CFG.scoreAdd} score`); return; }
      }
      // fallback: global vars
      const globals = scanGlobals();
      let hit = false;
      for (const [k, t] of Object.entries(globals)) {
        if (t === 'score') { window[k] += CFG.scoreAdd; hit = true; uiLog(`window.${k} += ${CFG.scoreAdd}`); }
      }
      if (!hit) uiLog('score: no prop found — open devtools and patch manually');
    };

    document.getElementById('rr-lives').onclick = () => {
      const g = findGame();
      if (g && g.type === 'phaser3') {
        const sc = getActiveScene(g.ref);
        if (patchPhaser3Lives(sc)) { uiLog('lives → 999'); return; }
      }
      const globals = scanGlobals();
      let hit = false;
      for (const [k, t] of Object.entries(globals)) {
        if (t === 'lives') { window[k] = 999; hit = true; uiLog(`window.${k} = 999`); }
      }
      if (!hit) uiLog('lives: no prop found');
    };

    document.getElementById('rr-invinc').onclick = () => tryInvincibility();

    document.getElementById('rr-speed-on').onclick = () => {
      CFG.speedMult = parseFloat(document.getElementById('rr-speed-val').value) || 0.5;
      hookSpeed(CFG.speedMult);
      uiLog(`speed ×${CFG.speedMult}`);
    };
    document.getElementById('rr-speed-off').onclick = () => { unhookSpeed(); uiLog('speed normal'); };

    document.getElementById('rr-jump-on').onclick  = () => { startAutoJump(); uiLog('auto-jump ON'); };
    document.getElementById('rr-jump-off').onclick = () => { stopAutoJump();  uiLog('auto-jump OFF'); };

    document.getElementById('rr-scan').onclick = () => {
      const g = findGame();
      uiLog(`engine: ${g ? g.type : 'none'}`);
      const globals = scanGlobals();
      if (Object.keys(globals).length) uiLog('globals: ' + JSON.stringify(globals));
      else uiLog('no score/lives globals found');
    };

    uiLog('panel ready');
  }

  // ── bookmarklet export ───────────────────────────────────────────────────────
  buildPanel();

  // expose API for console use
  window.RobRunCheat = {
    addScore:          (n = CFG.scoreAdd) => { const g = findGame(); if (g && g.type === 'phaser3') patchPhaser3Score(getActiveScene(g.ref), n); },
    infiniteLives:     () => { const g = findGame(); if (g && g.type === 'phaser3') patchPhaser3Lives(getActiveScene(g.ref)); },
    invincible:        tryInvincibility,
    invincibleOff:     removeInvincibility,
    speedHook:         hookSpeed,
    speedUnhook:       unhookSpeed,
    autoJump:          startAutoJump,
    autoJumpOff:       stopAutoJump,
    scan:              scanGlobals,
    panel:             buildPanel,
  };

  log('loaded — window.RobRunCheat ready, panel injected');
})();
