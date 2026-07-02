/**
 * Shared utilities for WAV file parsing and creation, so pages don't
 * duplicate header logic.
 */

import { DEFAULT_SAMPLE_RATE, DEFAULT_BIT_DEPTH, DEFAULT_NUM_CHANNELS } from './audioConstants.js';

export function writeString(view, offset, string) {
  for (let i = 0; i < string.length; i++) {
    view.setUint8(offset + i, string.charCodeAt(i));
  }
}

// Parses a WAV ArrayBuffer into { pcmData, sampleRate, numChannels, bitDepth }.
// Throws on missing RIFF/WAVE headers, non-PCM format, or a missing data chunk.
export function parseWavFile(arrayBuffer) {
  const view = new DataView(arrayBuffer);

  const riffId = String.fromCharCode(
    view.getUint8(0),
    view.getUint8(1),
    view.getUint8(2),
    view.getUint8(3)
  );
  if (riffId !== 'RIFF') {
    throw new Error('Invalid WAV file: missing RIFF header');
  }

  const waveId = String.fromCharCode(
    view.getUint8(8),
    view.getUint8(9),
    view.getUint8(10),
    view.getUint8(11)
  );
  if (waveId !== 'WAVE') {
    throw new Error('Invalid WAV file: missing WAVE format');
  }

  let offset = 12; // Skip RIFF/WAVE header
  let pcmData = null;
  let sampleRate = DEFAULT_SAMPLE_RATE;
  let numChannels = DEFAULT_NUM_CHANNELS;
  let bitDepth = DEFAULT_BIT_DEPTH;

  while (offset < view.byteLength - 8) {
    const chunkId = String.fromCharCode(
      view.getUint8(offset),
      view.getUint8(offset + 1),
      view.getUint8(offset + 2),
      view.getUint8(offset + 3)
    );
    const chunkSize = view.getUint32(offset + 4, true);
    offset += 8;

    if (chunkId === 'fmt ') {
      const audioFormat = view.getUint16(offset, true);
      if (audioFormat !== 1) {
        throw new Error(
          `Unsupported audio format: ${audioFormat} (only PCM format 1 is supported)`
        );
      }
      numChannels = view.getUint16(offset + 2, true);
      sampleRate = view.getUint32(offset + 4, true);
      bitDepth = view.getUint16(offset + 14, true);
      offset += chunkSize;
    } else if (chunkId === 'data') {
      pcmData = new Uint8Array(arrayBuffer, offset, chunkSize);
      break;
    } else {
      offset += chunkSize + (chunkSize & 1); // Skip unknown chunks (odd sizes are padded)
    }
  }

  if (!pcmData) {
    throw new Error('No data chunk found in WAV file');
  }

  return {
    pcmData,
    sampleRate,
    numChannels,
    bitDepth,
  };
}

// Builds a standard 44-byte-header WAV Blob from raw PCM sample data.
export function createWavFile(
  pcmData,
  sampleRate = DEFAULT_SAMPLE_RATE,
  bitDepth = DEFAULT_BIT_DEPTH,
  numChannels = DEFAULT_NUM_CHANNELS
) {
  const bytesPerSample = bitDepth / 8;
  const blockAlign = numChannels * bytesPerSample;
  const byteRate = sampleRate * blockAlign;
  const dataSize = pcmData.length;
  const fileSize = 44 + dataSize;

  const buffer = new ArrayBuffer(fileSize);
  const view = new DataView(buffer);

  writeString(view, 0, 'RIFF');
  view.setUint32(4, fileSize - 8, true);
  writeString(view, 8, 'WAVE');
  writeString(view, 12, 'fmt ');
  view.setUint32(16, 16, true); // fmt chunk size
  view.setUint16(20, 1, true); // PCM format
  view.setUint16(22, numChannels, true);
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, byteRate, true);
  view.setUint16(32, blockAlign, true);
  view.setUint16(34, bitDepth, true);
  writeString(view, 36, 'data');
  view.setUint32(40, dataSize, true);

  const dataView = new Uint8Array(buffer, 44);
  dataView.set(pcmData);

  return new Blob([buffer], { type: 'audio/wav' });
}

console.log('✅ wavUtils.js loaded - shared WAV utilities ready');
