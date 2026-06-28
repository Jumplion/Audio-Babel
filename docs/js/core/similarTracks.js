/**
 * similarTracks.js
 *
 * Generates a handful of "Similar Tracks" for a reconstructed audio result:
 * new indexes whose decoded audio is a close variation of the original
 * (slight sample jitter, added silence, sped up/slowed down, or a few of
 * those combined). Each variant is a real index produced by re-running the
 * C++/WASM encodeIndex bijection over a transformed copy of the original
 * PCM payload — never a fabricated string.
 *
 * Index::encode (cpp/src/Index.cpp) always treats the payload as a stream of
 * 16-bit little-endian words regardless of the bitDepth/sampleRate arguments
 * (those only affect the cosmetic WAV header), so transforms here operate on
 * an Int16Array view of the payload to match that real sample unit.
 */

import {
  DEFAULT_SAMPLE_RATE,
  DEFAULT_BIT_DEPTH,
  DEFAULT_NUM_CHANNELS,
} from '../utils/audioConstants.js';

const INT16_MAX = 32767;
const INT16_MIN = -32768;

function clampInt16(value) {
  return Math.max(INT16_MIN, Math.min(INT16_MAX, Math.round(value)));
}

/**
 * Split raw PCM bytes into whole 16-bit words plus any trailing odd byte
 * (matching Index::encode's "stray trailing byte" handling).
 * @param {Uint8Array} pcmData
 * @returns {{words: Int16Array, trailingByte: Uint8Array}}
 */
function toWords(pcmData) {
  const wholeByteLength = pcmData.length - (pcmData.length % 2);
  const words = new Int16Array(pcmData.slice(0, wholeByteLength).buffer);
  const trailingByte = pcmData.slice(wholeByteLength);
  return { words, trailingByte };
}

/**
 * Reassemble a transformed word array back into raw PCM bytes.
 * @param {Int16Array} words
 * @param {Uint8Array} trailingByte
 * @returns {Uint8Array}
 */
function fromWords(words, trailingByte) {
  const wordBytes = new Uint8Array(words.buffer, words.byteOffset, words.byteLength);
  const out = new Uint8Array(wordBytes.length + trailingByte.length);
  out.set(wordBytes, 0);
  out.set(trailingByte, wordBytes.length);
  return out;
}

/**
 * Nudge every sample by a small random amount, proportional to full scale.
 * @param {Int16Array} words
 * @param {number} magnitude - Fraction of full scale (e.g. 0.01 = ~1%)
 * @returns {Int16Array}
 */
function jitter(words, magnitude) {
  const out = new Int16Array(words.length);
  const range = INT16_MAX * magnitude;
  for (let i = 0; i < words.length; i++) {
    out[i] = clampInt16(words[i] + (Math.random() * 2 - 1) * range);
  }
  return out;
}

/**
 * Prepend and/or append a run of silent (zero) samples.
 * @param {Int16Array} words
 * @param {number} leadSamples - Silent samples to add at the start
 * @param {number} trailSamples - Silent samples to add at the end
 * @returns {Int16Array}
 */
function addSilence(words, leadSamples, trailSamples) {
  const out = new Int16Array(leadSamples + words.length + trailSamples);
  out.set(words, leadSamples);
  return out;
}

/**
 * Resample by a constant rate factor via linear interpolation — the same
 * effect as playing the audio back faster/slower (rateFactor > 1 = sped up
 * and shorter; rateFactor < 1 = slowed down and longer).
 * @param {Int16Array} words
 * @param {number} rateFactor
 * @returns {Int16Array}
 */
function changeSpeed(words, rateFactor) {
  if (words.length < 2) return words.slice();

  const newLength = Math.max(1, Math.floor(words.length / rateFactor));
  const out = new Int16Array(newLength);
  const lastIndex = words.length - 1;

  for (let i = 0; i < newLength; i++) {
    const srcPos = Math.min(lastIndex, i * rateFactor);
    const lo = Math.floor(srcPos);
    const hi = Math.min(lastIndex, lo + 1);
    const frac = srcPos - lo;
    out[i] = clampInt16(words[lo] * (1 - frac) + words[hi] * frac);
  }
  return out;
}

/**
 * Pick a value uniformly between min and max.
 */
function randomBetween(min, max) {
  return min + Math.random() * (max - min);
}

/**
 * Convert a duration in milliseconds to a sample count at the default rate.
 */
function samplesForMs(ms) {
  return Math.round((ms / 1000) * DEFAULT_SAMPLE_RATE);
}

// Single-effect transforms, each randomized a bit per call.
const subtleJitter = (words) => jitter(words, randomBetween(0.004, 0.012));
const strongJitter = (words) => jitter(words, randomBetween(0.03, 0.07));
const silenceAtStart = (words) => addSilence(words, samplesForMs(randomBetween(80, 250)), 0);
const silenceAtEnd = (words) => addSilence(words, 0, samplesForMs(randomBetween(80, 250)));
const silenceBothEnds = (words) =>
  addSilence(words, samplesForMs(randomBetween(50, 150)), samplesForMs(randomBetween(50, 150)));
const spedUp = (words) => changeSpeed(words, randomBetween(1.06, 1.18));
const slowedDown = (words) => changeSpeed(words, randomBetween(0.82, 0.94));

/**
 * Chain transforms left to right.
 * @param {...Function} transforms
 * @returns {Function}
 */
function compose(...transforms) {
  return (words) => transforms.reduce((acc, transform) => transform(acc), words);
}

// Variant recipes: a mix of single effects and a few combined together.
// Fixed at 9 so every result lands in the requested 5-10 range.
const VARIANT_TRANSFORMS = [
  subtleJitter,
  strongJitter,
  silenceAtStart,
  silenceAtEnd,
  silenceBothEnds,
  spedUp,
  slowedDown,
  compose(spedUp, subtleJitter),
  compose(slowedDown, silenceAtEnd),
];

/**
 * Generate "similar track" indexes by applying small, audible variations
 * (sample jitter, silence padding, speed change, or combinations of those)
 * to the decoded PCM payload and re-encoding each variant through the real
 * WASM bijection.
 * @param {Object} wasm - Initialized IndexWasm instance (see core/wasmModule.js)
 * @param {Uint8Array} pcmData - Raw PCM payload decoded from the source index
 * @returns {string[]} Generated index strings (skips any that fail to encode)
 */
export function buildSimilarTracks(wasm, pcmData) {
  if (!pcmData || pcmData.length === 0) return [];

  const { words, trailingByte } = toWords(pcmData);
  const results = [];

  for (const transform of VARIANT_TRANSFORMS) {
    try {
      const transformedWords = transform(words);
      const transformedBytes = fromWords(transformedWords, trailingByte);
      const indexBase64 = wasm.encodeIndexFromPcm(
        transformedBytes,
        DEFAULT_SAMPLE_RATE,
        DEFAULT_BIT_DEPTH,
        DEFAULT_NUM_CHANNELS
      );
      results.push(indexBase64);
    } catch (error) {
      console.error('Failed to generate a similar track variant:', error);
    }
  }

  return results;
}
