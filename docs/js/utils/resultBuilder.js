/**
 * Builds the standardised result object passed to handleJsonResponse.
 * Metadata always comes from the C++/WASM getMetadata call — never
 * fabricated client-side.
 */

import { DEFAULT_SAMPLE_RATE, DEFAULT_BIT_DEPTH, DEFAULT_NUM_CHANNELS } from './audioConstants.js';

// Reconstructs audio and metadata for an index string. Returns raw PCM bytes
// rather than a base64 WAV — see docs/js/utils/README.md.
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
