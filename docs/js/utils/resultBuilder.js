/**
 * resultBuilder.js
 *
 * Builds the standardised result object passed to handleJsonResponse.
 * Metadata always comes from the C++/WASM getMetadata call — never fabricated client-side.
 */

import { DEFAULT_SAMPLE_RATE, DEFAULT_BIT_DEPTH, DEFAULT_NUM_CHANNELS } from './audioConstants.js';

/**
 * Reconstruct audio and metadata for an index string and build the result object
 * consumed by handleJsonResponse.
 *
 * Returns raw PCM bytes instead of a base64-encoded WAV so the display layer can
 * feed the samples directly to WaveSurfer (via AudioBuffer) and only build the WAV
 * container lazily when the user requests a download.
 *
 * @param {Object} wasm - Initialized IndexWasm instance
 * @param {string} indexBase64 - Bijective base64 index (no header)
 * @param {Object} [audioFormat] - PCM format hints (all optional; defaults match C++ constants)
 * @param {number} [audioFormat.sampleRate]
 * @param {number} [audioFormat.bitDepth]
 * @param {number} [audioFormat.numChannels]
 * @returns {Promise<Object>} Result object: { indexBase64, metadata, position, pcm, sampleRate, bitDepth, numChannels }
 */
export async function buildResultForIndex(wasm, indexBase64, audioFormat = {}) {
  const {
    sampleRate = DEFAULT_SAMPLE_RATE,
    bitDepth = DEFAULT_BIT_DEPTH,
    numChannels = DEFAULT_NUM_CHANNELS,
  } = audioFormat;

  const decoded = wasm.module.decodeIndex(indexBase64);
  if (!decoded) throw new Error('Failed to decode index');

  const metadata = JSON.parse(decoded.metadataJson);
  if (metadata.error) throw new Error(metadata.error);

  const position = JSON.parse(decoded.positionJson);

  return { indexBase64, metadata, position, pcm: decoded.pcm, sampleRate, bitDepth, numChannels };
}
