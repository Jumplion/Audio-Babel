/**
 * wavUtils.js
 * 
 * Shared utilities for WAV file parsing and creation.
 * Eliminates code duplication across fileUpload.js and recorder.js
 */

/**
 * Write a string to a DataView
 * @param {DataView} view - The DataView to write to
 * @param {number} offset - The offset to start writing at
 * @param {string} string - The string to write
 */
export function writeString(view, offset, string) {
    for (let i = 0; i < string.length; i++) {
        view.setUint8(offset + i, string.charCodeAt(i));
    }
}

/**
 * Parse WAV file header and extract PCM data and audio format information
 * @param {ArrayBuffer} arrayBuffer - WAV file data
 * @returns {Object} Object containing pcmData, sampleRate, numChannels, bitDepth
 * @throws {Error} If no data chunk is found or invalid WAV format
 */
export function parseWavFile(arrayBuffer) {
    const view = new DataView(arrayBuffer);
    
    // Verify RIFF header
    const riffId = String.fromCharCode(
        view.getUint8(0),
        view.getUint8(1),
        view.getUint8(2),
        view.getUint8(3)
    );
    if (riffId !== 'RIFF') {
        throw new Error('Invalid WAV file: missing RIFF header');
    }
    
    // Verify WAVE format
    const waveId = String.fromCharCode(
        view.getUint8(8),
        view.getUint8(9),
        view.getUint8(10),
        view.getUint8(11)
    );
    if (waveId !== 'WAVE') {
        throw new Error('Invalid WAV file: missing WAVE format');
    }
    
    // Parse chunks
    let offset = 12; // Skip RIFF/WAVE header
    let pcmData = null;
    let sampleRate = 44100;
    let numChannels = 1;
    let bitDepth = 16;
    
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
            // Parse format chunk
            const audioFormat = view.getUint16(offset, true);
            if (audioFormat !== 1) {
                throw new Error(`Unsupported audio format: ${audioFormat} (only PCM format 1 is supported)`);
            }
            numChannels = view.getUint16(offset + 2, true);
            sampleRate = view.getUint32(offset + 4, true);
            bitDepth = view.getUint16(offset + 14, true);
            offset += chunkSize;
        } else if (chunkId === 'data') {
            // Extract PCM data
            pcmData = new Uint8Array(arrayBuffer, offset, chunkSize);
            break;
        } else {
            // Skip unknown chunks (handle odd-sized chunks)
            offset += chunkSize + (chunkSize & 1);
        }
    }
    
    if (!pcmData) {
        throw new Error('No data chunk found in WAV file');
    }
    
    return {
        pcmData,
        sampleRate,
        numChannels,
        bitDepth
    };
}

/**
 * Create a WAV file from PCM sample data
 * @param {Uint8Array} pcmData - Raw PCM sample data
 * @param {number} sampleRate - Sample rate (default: 44100)
 * @param {number} bitDepth - Bit depth (default: 16)
 * @param {number} numChannels - Number of channels (default: 1)
 * @returns {Blob} WAV file as a Blob
 */
export function createWavFile(pcmData, sampleRate = 44100, bitDepth = 16, numChannels = 1) {
    const bytesPerSample = bitDepth / 8;
    const blockAlign = numChannels * bytesPerSample;
    const byteRate = sampleRate * blockAlign;
    const dataSize = pcmData.length;
    const fileSize = 44 + dataSize;

    // Create WAV buffer
    const buffer = new ArrayBuffer(fileSize);
    const view = new DataView(buffer);

    // Write WAV header
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

    // Write sample data
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

console.log('✅ wavUtils.js loaded - shared WAV utilities ready');
