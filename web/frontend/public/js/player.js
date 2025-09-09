import { loadFragment } from './loadFragment.js';

class SOTBPlayer {
  constructor(rootSelector = 'body') {
    this.root = document.querySelector(rootSelector) || document.body;
    this.playlist = []; // {wavUrl, index, metadata}
    this.current = -1;
    this.audio = null;
    this.ui = {};
  this.STORAGE_KEY = 'sotb.playlist.v1';
  this._init();
  }

  async _init() {
    // load player HTML into document body
    const container = document.createElement('div');
    this.root.appendChild(container);
    container.innerHTML = await (await fetch('./components/player.html')).text();

    this.el = container.querySelector('#sotb-player');
    this.audio = this.el.querySelector('#sotb-audio');
    this.ui = {
      playBtn: this.el.querySelector('#sotb-play'),
      prevBtn: this.el.querySelector('#sotb-prev'),
      nextBtn: this.el.querySelector('#sotb-next'),
      addBtn: this.el.querySelector('#sotb-add'),
  playlistToggle: this.el.querySelector('#sotb-playlist-toggle'),
  playlistDrawer: this.el.querySelector('#sotb-playlist-drawer'),
  playlistEl: this.el.querySelector('#sotb-playlist'),
  playlistClose: this.el.querySelector('#sotb-playlist-close'),
  clearPlaylist: this.el.querySelector('#sotb-clear-playlist'),
      title: this.el.querySelector('#sotb-track-title'),
      meta: this.el.querySelector('#sotb-track-meta'),
      progress: this.el.querySelector('#sotb-progress'),
      progressBar: this.el.querySelector('#sotb-progress-bar'),
      time: this.el.querySelector('#sotb-time'),
    };

    this.ui.playBtn.addEventListener('click', () => this.togglePlay());
    this.ui.prevBtn.addEventListener('click', () => this.prev());
    this.ui.nextBtn.addEventListener('click', () => this.next());
    this.ui.addBtn.addEventListener('click', () => this.addCurrent());
    this.ui.progress.addEventListener('click', (e) => this._seekFromEvent(e));

  // playlist UI handlers
  if (this.ui.playlistToggle) this.ui.playlistToggle.addEventListener('click', () => this._togglePlaylist(true));
  if (this.ui.playlistClose) this.ui.playlistClose.addEventListener('click', () => this._togglePlaylist(false));
  if (this.ui.clearPlaylist) this.ui.clearPlaylist.addEventListener('click', () => { if (confirm('Clear playlist?')) this._clearPlaylist(); });

    this.audio.addEventListener('timeupdate', () => this._updateProgress());
    this.audio.addEventListener('ended', () => this.next());
    this.audio.addEventListener('loadedmetadata', () => this._updateProgress());

    // try to restore saved playlist from localStorage (non-blocking)
    try { await this._loadState(); } catch (e) { console.warn('Failed to load saved playlist', e); }

    // reflect state in UI but don't auto-play on load
    this._updateButtons();
  this._renderPlaylist();
  }

  _isHttpUrl(url) {
    return typeof url === 'string' && /^https?:\/\//i.test(url);
  }

  async _fetchWavFromIndex(indexBase64) {
    // Reconstruct server call - mirrors index.html reconstruction flow
    try {
      const resp = await fetch('/reconstruct?metadata=1', { method: 'POST', headers: { 'Content-Type': 'application/json', Accept: 'application/json' }, body: JSON.stringify({ format: 'base64', data: indexBase64 }) });
      if (!resp.ok) throw new Error('Server error ' + resp.status);
      const j = await resp.json();
      if (!j.wavBase64) throw new Error('No wav returned');
      // convert base64 to blob URL
      const bytes = atob(j.wavBase64);
      const ab = new Uint8Array(bytes.length);
      for (let i = 0; i < bytes.length; ++i) ab[i] = bytes.charCodeAt(i);
      const blob = new Blob([ab], { type: 'audio/wav' });
      const url = URL.createObjectURL(blob);
      return { url, metadata: j.metadata };
    } catch (e) { console.warn('Failed to restore wav from index', e); throw e; }
  }

  _updateButtons() {
    this.ui.prevBtn.disabled = this.playlist.length === 0;
    this.ui.nextBtn.disabled = this.playlist.length === 0;
    this.ui.addBtn.disabled = false;
  }

  _updateProgress() {
    const cur = this.audio.currentTime || 0;
    const dur = this.audio.duration || 0;
    const pct = dur ? Math.max(0, Math.min(1, cur / dur)) * 100 : 0;
    this.ui.progressBar.style.width = pct + '%';
    this.ui.time.textContent = SOTBPlayer._formatTime(cur) + (dur ? ' / ' + SOTBPlayer._formatTime(dur) : '');
  }

  _seekFromEvent(e) {
    const rect = this.ui.progress.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const pct = Math.max(0, Math.min(1, x / rect.width));
    if (this.audio.duration) this.audio.currentTime = pct * this.audio.duration;
  }

  static _formatTime(s) {
    if (!isFinite(s)) return '0:00';
    const mins = Math.floor(s / 60);
    const secs = Math.floor(s % 60).toString().padStart(2, '0');
    return `${mins}:${secs}`;
  }

  async setTrack(t) {
    // t: { wavUrl, index, metadata }
    if (!t || !t.wavUrl) return;
    // if current track matches URL, just update metadata
    if (this.current >= 0 && this.playlist[this.current] && this.playlist[this.current].wavUrl === t.wavUrl) {
      this.playlist[this.current] = { ...this.playlist[this.current], ...t };
    } else {
      // push as next and play
      this.playlist.push(t);
      this.current = this.playlist.length - 1;
    }
    this._loadCurrent();
    this.play();
  }

  _loadCurrent() {
    if (this.current < 0 || this.current >= this.playlist.length) {
      this.ui.title.textContent = 'No track';
      this.ui.meta.textContent = '';
      this.audio.src = '';
      this._updateButtons();
      return;
    }
    const t = this.playlist[this.current];
    const setUiForTrack = (tt) => {
      const title = (tt.metadata && (tt.metadata.track || tt.metadata.title)) || (tt.index ? ('Index: ' + (tt.index.slice ? tt.index.slice(0, 60) + (tt.index.length>60? '...':'' ) : '')) : 'Track');
      this.ui.title.textContent = title;
      this.ui.meta.textContent = (tt.metadata && (tt.metadata.artist || tt.metadata.album)) || '';
      this._updateButtons();
    };

    // If there's a usable http(s) URL or data URL, use it. If there's only an index, lazily request reconstructed wav.
    if (t.wavUrl && this._isHttpUrl(t.wavUrl)) {
      this.audio.src = t.wavUrl;
      setUiForTrack(t);
      return;
    }

    if (t.wavUrl && t.wavUrl.startsWith('blob:')) {
      // in-memory blob URL available in this session
      this.audio.src = t.wavUrl;
      setUiForTrack(t);
      return;
    }

    if (t.index) {
      // lazy restore from server, do not block UI thread
      setUiForTrack(t);
      this.ui.title.textContent = (this.ui.title.textContent || '') + ' (restoring...)';
      this._fetchWavFromIndex(t.index).then(({ url, metadata }) => {
        // don't persist blob URLs; assign for this session
        t.wavUrl = url;
        if (!t.metadata && metadata) t.metadata = metadata;
        // update UI and audio
        if (this.current >= 0 && this.playlist[this.current] === t) {
          this.audio.src = url;
          this._updateButtons();
        }
      }).catch(() => {
        // failed to restore - leave UI as-is
      });
      return;
    }

    // fallback: nothing to play
    this.audio.src = '';
    setUiForTrack(t);
  }

  add(track) {
    if (!track || !track.wavUrl) return;
    this.playlist.push(track);
    this._updateButtons();
  this._saveState();
  this._renderPlaylist();
    return this.playlist.length - 1;
  }

  addCurrent() {
    // attempt to grab the currently-displayed result fragment index/audio if present
    try {
      const resultAudio = document.querySelector('#result audio#error, #result audio, audio#audioPlayer');
      // prefer reconstructed player in result fragment
      const audioEl = document.querySelector('#result audio') || document.querySelector('audio#audioPlayer');
      const download = document.querySelector('#downloadLink');
      const idxEl = document.querySelector('#indexDisplay');
      if (audioEl && audioEl.src) {
        const track = { wavUrl: audioEl.src, index: idxEl ? idxEl.textContent : null };
        const pos = this.add(track);
        // if there was nothing playing, start this one
        if (this.current < 0) this.current = pos, this._loadCurrent();
        alert('Added to playlist');
      } else {
        alert('No audio to add');
      }
    } catch (e) { console.warn(e); alert('Add failed'); }
  }

  play() { if (!this.audio.src) return; this.audio.play(); this.ui.playBtn.textContent = '⏸'; }
  pause() { this.audio.pause(); this.ui.playBtn.textContent = '▶'; }
  togglePlay() { if (this.audio.paused) this.play(); else this.pause(); }

  next() {
    if (this.playlist.length === 0) return;
    this.current = (this.current + 1) % this.playlist.length;
  this._loadCurrent();
  this._saveState();
    this.play();
  }

  prev() {
    if (this.playlist.length === 0) return;
    this.current = (this.current - 1 + this.playlist.length) % this.playlist.length;
    this._loadCurrent();
    this._saveState();
    this.play();
  }

  _saveState() {
    try {
      const entries = this.playlist.map(t => ({
        // persist index if available (best for rehydration), otherwise persist remote http(s) urls
        index: t.index || null,
        metadata: t.metadata || null,
        remoteUrl: (t.wavUrl && this._isHttpUrl(t.wavUrl)) ? t.wavUrl : null,
      }));
      const state = { entries, current: this.current };
      localStorage.setItem(this.STORAGE_KEY, JSON.stringify(state));
    } catch (e) { console.warn('Failed to save player state', e); }
  }

  async _loadState() {
    try {
      const raw = localStorage.getItem(this.STORAGE_KEY);
      if (!raw) return;
      const state = JSON.parse(raw);
      if (!state || !Array.isArray(state.entries)) return;
      this.playlist = state.entries.map(e => ({ wavUrl: e.remoteUrl || null, index: e.index || null, metadata: e.metadata || null }));
      this.current = typeof state.current === 'number' ? state.current : -1;
      // reflect current in UI but don't autoplay
      if (this.current >= 0 && this.current < this.playlist.length) this._loadCurrent();
    } catch (e) { console.warn('Failed to parse saved player state', e); }
  }

  _renderPlaylist() {
    if (!this.ui.playlistEl) return;
    const el = this.ui.playlistEl;
    el.innerHTML = '';
    this.playlist.forEach((t, idx) => {
      const li = document.createElement('li');
      li.className = 'sotb-playlist-item';
      li.draggable = true;
      li.dataset.index = String(idx);

      const handle = document.createElement('div'); handle.className = 'handle'; handle.textContent = '⋮';
      const meta = document.createElement('div'); meta.className = 'meta';
      meta.title = (t.metadata && (t.metadata.artist || '')) || '';
      meta.textContent = (t.metadata && (t.metadata.track || t.metadata.title)) || (t.index ? ('Index: ' + (t.index.slice ? t.index.slice(0,60) : t.index)) : (t.wavUrl || 'Track'));
      const playBtn = document.createElement('button'); playBtn.className = 'sotb-btn'; playBtn.textContent = (idx === this.current) ? 'Playing' : 'Play';
      playBtn.addEventListener('click', () => { this.current = idx; this._loadCurrent(); this.play(); this._saveState(); this._renderPlaylist(); });
      const removeBtn = document.createElement('button'); removeBtn.className = 'sotb-btn'; removeBtn.textContent = 'Remove';
      removeBtn.addEventListener('click', () => { this._removeAt(idx); });

      li.appendChild(handle); li.appendChild(meta); li.appendChild(playBtn); li.appendChild(removeBtn);

      // drag/drop
      li.addEventListener('dragstart', (e) => { li.classList.add('dragging'); e.dataTransfer.setData('text/plain', String(idx)); });
      li.addEventListener('dragend', () => { li.classList.remove('dragging'); });
      li.addEventListener('dragover', (e) => { e.preventDefault(); });
      li.addEventListener('drop', (e) => {
        e.preventDefault();
        const from = Number(e.dataTransfer.getData('text/plain'));
        const to = Number(li.dataset.index);
        if (!Number.isFinite(from) || !Number.isFinite(to)) return;
        this._reorder(from, to);
      });

      el.appendChild(li);
    });
  }

  _togglePlaylist(show) {
    if (!this.ui.playlistDrawer) return;
    const s = (typeof show === 'boolean') ? show : (this.ui.playlistDrawer.getAttribute('aria-hidden') === 'true');
    this.ui.playlistDrawer.setAttribute('aria-hidden', String(!s));
  }

  _removeAt(idx) {
    if (idx < 0 || idx >= this.playlist.length) return;
    this.playlist.splice(idx, 1);
    if (this.current >= this.playlist.length) this.current = this.playlist.length - 1;
    this._saveState();
    this._renderPlaylist();
    this._updateButtons();
  }

  _clearPlaylist() {
    this.playlist = [];
    this.current = -1;
    this._saveState();
    this._renderPlaylist();
    this._loadCurrent();
    this._updateButtons();
  }

  _reorder(from, to) {
    if (from === to) return;
    const item = this.playlist.splice(from, 1)[0];
    this.playlist.splice(to, 0, item);
    // adjust current index
    if (this.current === from) this.current = to;
    else if (from < this.current && to >= this.current) this.current -= 1;
    else if (from > this.current && to <= this.current) this.current += 1;
    this._saveState();
    this._renderPlaylist();
  }
}

export async function loadPlayer(rootSelector = 'body') {
  const p = new SOTBPlayer(rootSelector);
  // wait until init completes (poll for audio element)
  for (let i = 0; i < 40; ++i) {
    if (p.audio) break; await new Promise(r => setTimeout(r, 50));
  }
  // expose API
  const api = {
    setTrack: (t) => p.setTrack(t),
    add: (t) => p.add(t),
    play: () => p.play(),
    pause: () => p.pause(),
    next: () => p.next(),
    prev: () => p.prev(),
  };
  return api;
}
