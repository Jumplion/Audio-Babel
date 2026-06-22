// gatefold-demo.js — standalone prototype for the "gatefold" album/track selector.
// Loads the real WASM module so the cover art is genuine generated art, but is not
// wired into the Browse navigation flow (see docs/gatefold-demo.html).

import { getWasmModule } from '../core/wasmModule.js';

const $ = (id) => document.getElementById(id);

let TRACKS_PER_ALBUM = 15;
let currentAlbum = 0;

/**
 * Cover art only exists at the full-index level (room+wall+shelf+album+track),
 * not per-album. As a stand-in for "what the album looks like", this uses
 * track 0 of the chosen album, room 0 / wall 0 / shelf 0.
 */
function buildStandInIndex(wasm, albumNum) {
    const result = wasm.module.reconstructIndex('', 0, 0, albumNum, 0);
    if (typeof result === 'string' && result.startsWith('{')) {
        const err = JSON.parse(result);
        throw new Error(err.error || 'Failed to build stand-in index');
    }
    return result;
}

function renderTracks(container, count) {
    container.innerHTML = '';
    for (let i = 0; i < count; i++) {
        const btn = document.createElement('button');
        btn.className = 'track-btn';
        btn.textContent = `Track ${i}`;
        btn.addEventListener('click', () => {
            console.log(`Selected track ${i} of album ${currentAlbum} (prototype — no playback wired up)`);
        });
        container.appendChild(btn);
    }
}

async function loadAlbumCover(wasm, albumNum) {
    const statusNote = $('statusNote');
    const coverArt = $('coverArt');
    const coverCaption = $('coverCaption');
    const toggleOpenBtn = $('toggleOpenBtn');

    statusNote.textContent = `Loading album ${albumNum}…`;

    const indexBase64 = buildStandInIndex(wasm, albumNum);
    const metadataJson = wasm.module.getMetadata(indexBase64);
    const metadata = JSON.parse(metadataJson);
    if (metadata.error) {
        throw new Error(metadata.error);
    }

    const svgDataUrl = 'data:image/svg+xml;charset=utf-8,' + encodeURIComponent(metadata.cover);
    coverArt.src = svgDataUrl;
    coverCaption.textContent = `${metadata.artist} — ${metadata.album}`;

    currentAlbum = albumNum;
    renderTracks($('tracksContainer'), TRACKS_PER_ALBUM);

    toggleOpenBtn.disabled = false;
    statusNote.textContent = `Album ${albumNum} loaded. Click the cover (or the button) to open it.`;
}

function setOpen(stage, toggleOpenBtn, open) {
    stage.classList.toggle('open', open);
    toggleOpenBtn.textContent = open ? 'Close album' : 'Open album';
}

document.addEventListener('DOMContentLoaded', async () => {
    const statusNote = $('statusNote');
    const stage = $('gatefoldStage');
    const coverRight = $('coverRight');
    const toggleOpenBtn = $('toggleOpenBtn');
    const reloadCoverBtn = $('reloadCoverBtn');
    const albumPicker = $('albumPicker');

    let wasm;
    try {
        wasm = await getWasmModule();
        const constants = JSON.parse(wasm.module.getLibraryConstants());
        TRACKS_PER_ALBUM = constants.tracksPerAlbum;
    } catch (e) {
        console.error('Failed to initialize WASM module', e);
        statusNote.textContent = 'Failed to load WASM module — see console for details.';
        return;
    }

    toggleOpenBtn.addEventListener('click', () => {
        setOpen(stage, toggleOpenBtn, !stage.classList.contains('open'));
    });

    coverRight.addEventListener('click', () => {
        if (!stage.classList.contains('open')) {
            setOpen(stage, toggleOpenBtn, true);
        }
    });

    reloadCoverBtn.addEventListener('click', async () => {
        const albumNum = Math.min(31, Math.max(0, parseInt(albumPicker.value, 10) || 0));
        albumPicker.value = albumNum;
        setOpen(stage, toggleOpenBtn, false);
        try {
            await loadAlbumCover(wasm, albumNum);
        } catch (e) {
            console.error('Failed to load album cover', e);
            statusNote.textContent = `Failed to load album ${albumNum} — see console for details.`;
        }
    });

    try {
        await loadAlbumCover(wasm, currentAlbum);
    } catch (e) {
        console.error('Failed to load initial album cover', e);
        statusNote.textContent = 'Failed to load initial album cover — see console for details.';
    }
});
