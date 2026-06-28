/**
 * resultBuilder.js
 *
 * Builds the standardised result object passed to handleJsonResponse.
 * Metadata always comes from the C++/WASM getMetadata call — never fabricated client-side.
 */

import { DEFAULT_SAMPLE_RATE, DEFAULT_BIT_DEPTH, DEFAULT_NUM_CHANNELS } from './audioConstants.js';

// Re-export so page modules can import everything from one place
export { DEFAULT_SAMPLE_RATE, DEFAULT_BIT_DEPTH, DEFAULT_NUM_CHANNELS };

/**
 * Reconstruct audio and metadata for an index string and build the result object
 * consumed by handleJsonResponse.
 * @param {Object} wasm - Initialized IndexWasm instance
 * @param {string} indexBase64 - Bijective base64 index (no header)
 * @param {Object} [wavOptions] - Output WAV format overrides
 * @param {number} [wavOptions.sampleRate] - Sample rate for the output WAV header
 * @param {number} [wavOptions.bitDepth] - Bit depth for the output WAV header
 * @param {number} [wavOptions.numChannels] - Channel count for the output WAV header
 * @returns {Promise<Object>} Result object with indexBase64, metadata, and wavBase64
 */
export async function buildResultForIndex(wasm, indexBase64, wavOptions = {}) {
  const {
    sampleRate = DEFAULT_SAMPLE_RATE,
    bitDepth = DEFAULT_BIT_DEPTH,
    numChannels = DEFAULT_NUM_CHANNELS,
  } = wavOptions;

  const decoded = wasm.module.decodeIndex(indexBase64);
  if (!decoded) throw new Error('Failed to decode index');

  const metadata = JSON.parse(decoded.metadataJson);
  if (metadata.error) throw new Error(metadata.error);

  const position = JSON.parse(decoded.positionJson);
  const wavBase64 = await wasm.samplesToWavBase64(decoded.pcm, sampleRate, bitDepth, numChannels);

  return { indexBase64, metadata, position, wavBase64 };
}
