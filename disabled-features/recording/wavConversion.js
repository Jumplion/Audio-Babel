/**
 * wavConversion.js
 *
 * WebM-to-WAV conversion used exclusively by the (disabled) recording feature.
 * Extracted from docs/js/utils/wavUtils.js so the shared WAV utilities used by
 * the live site no longer carry recording-only code.
 *
 * Includes local copies of the WAV-writing helpers (writeString, createWavFile)
 * so this file has no dependency on docs/js/utils/wavUtils.js.
 */

/**
 * Write a string to a DataView
 * @param {DataView} view - The DataView to write to
 * @param {number} offset - The offset to start writing at
 * @param {string} string - The string to write
 */
function writeString(view, offset, string) {
    for (let i = 0; i < string.length; i++) {
        view.setUint8(offset + i, string.charCodeAt(i));
    }
}

/**
 * Create a WAV file from PCM sample data
 * @param {Uint8Array} pcmData - Raw PCM sample data
 * @param {number} sampleRate - Sample rate (default: 44100)
 * @param {number} bitDepth - Bit depth (default: 16)
 * @param {number} numChannels - Number of channels (default: 1)
 * @returns {Blob} WAV file as a Blob
 */
function createWavFile(pcmData, sampleRate = 44100, bitDepth = 16, numChannels = 1) {
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

/**
 * Convert a WebM blob to WAV format using Web Audio API
 * @param {Blob} webmBlob - The WebM audio blob
 * @returns {Promise<Blob>} A promise that resolves to a WAV blob
 */
export async function convertWebMToWav(webmBlob) {
    const audioContext = new (window.AudioContext || window.webkitAudioContext)();
    const arrayBuffer = await webmBlob.arrayBuffer();
    const audioBuffer = await audioContext.decodeAudioData(arrayBuffer);

    const numChannels = audioBuffer.numberOfChannels;
    const sampleRate = audioBuffer.sampleRate;

    // Extract interleaved 16-bit PCM into a flat byte array
    const pcmData = new Uint8Array(audioBuffer.length * numChannels * 2);
    const pcmView = new DataView(pcmData.buffer);
    let offset = 0;
    for (let i = 0; i < audioBuffer.length; i++) {
        for (let channel = 0; channel < numChannels; channel++) {
            const sample = Math.max(-1, Math.min(1, audioBuffer.getChannelData(channel)[i]));
            pcmView.setInt16(offset, sample < 0 ? sample * 0x8000 : sample * 0x7FFF, true);
            offset += 2;
        }
    }

    return createWavFile(pcmData, sampleRate, 16, numChannels);
}
