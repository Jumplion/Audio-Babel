/**
 * apiAdapter.js
 * 
 * Client-side adapter that provides the same interface as server API endpoints,
 * but processes everything in the browser using direct function calls.
 * 
 * NO FETCH INTERCEPTION - just direct function calls for simplicity and reliability.
 */

import {
    decodeBase64Url,
    encodeBase64Url,
    extractMetadataFromIndex,
    generateWavFromBytes,
    encodeWavFile
} from './audioIndex.js';
import { base64ToBytes, bytesToBase64 } from './utils.js';

/**
 * Reconstruct endpoint replacement
 * Mimics POST /reconstruct?metadata=1
 * 
 * Request body: { format: 'base64', data: <standard base64 of index bytes> }
 * Response: { 
 *   indexBase64: <URL-safe base64 index>,
 *   wavBase64: <standard base64 of WAV file>,
 *   metadata: { genre, artist, album, track, cover }
 * }
 */
export async function reconstruct(requestBody) {
    try {
        // Parse request
        const { format, data } = requestBody;
        
        if (format !== 'base64' && format !== 'base64url') {
            throw new Error('Format: ' + format + ' - Only base64 and base64url formats are supported');
        }

        // Convert to bytes based on format
        let indexBytes;
        if (format === 'base64url') {
            // Use decodeBase64Url for URL-safe base64
            indexBytes = decodeBase64Url(data);
        } else {
            // Use base64ToBytes for standard base64
            indexBytes = base64ToBytes(data);
        }
        
        // Convert bytes to URL-safe base64 (the actual index format)
        const urlSafeIndex = encodeBase64Url(indexBytes);
        
        // Extract metadata
        const metadata = extractMetadataFromIndex(urlSafeIndex);
        
        // Generate WAV file
        const wavBlob = generateWavFromBytes(indexBytes);
        
        // Convert WAV blob to base64
        const wavArrayBuffer = await wavBlob.arrayBuffer();
        const wavBytes = new Uint8Array(wavArrayBuffer);
        const wavBase64 = bytesToBase64(wavBytes);
        
        // Convert SVG cover to data URL if present
        let coverDataUrl = '';
        if (metadata.cover) {
            const svgBlob = new Blob([metadata.cover], { type: 'image/svg+xml' });
            coverDataUrl = URL.createObjectURL(svgBlob);
        }
        
        return {
            indexBase64: urlSafeIndex,
            wavBase64: wavBase64,
            metadata: {
                genre: metadata.genre,
                artist: metadata.artist,
                album: metadata.album,
                track: metadata.track,
                cover: coverDataUrl
            }
        };
    } catch (error) {
        console.error('Error in reconstruct:', error);
        throw error;
    }
}

/**
 * Search by file endpoint replacement
 * Mimics POST /search_by_file
 * 
 * Request: FormData with 'file' field containing WAV file
 * Response: {
 *   indexBase64: <URL-safe base64 index>,
 *   wavBase64: <standard base64 of WAV file>,
 *   metadata: { genre, artist, album, track, cover },
 *   audioFormat, numChannels, sampleRate, bitDepth, dataSize
 * }
 */
export async function searchByFile(formData) {
    try {
        const file = formData.get('file');
        
        if (!file) {
            throw new Error('No file provided');
        }
        
        // Read file as ArrayBuffer
        const arrayBuffer = await file.arrayBuffer();
        
        // Encode the WAV file
        const result = encodeWavFile(arrayBuffer);
        
        // Generate WAV blob from the extracted PCM data
        const pcmBytes = decodeBase64Url(result.base64Index);
        const wavBlob = generateWavFromBytes(
            pcmBytes,
            result.sampleRate,
            result.bitDepth,
            result.numChannels
        );
        
        // Convert WAV blob to base64
        const wavArrayBuffer = await wavBlob.arrayBuffer();
        const wavBytes = new Uint8Array(wavArrayBuffer);
        const wavBase64 = bytesToBase64(wavBytes);
        
        // Convert SVG cover to data URL
        let coverDataUrl = '';
        if (result.metadata.cover) {
            const svgBlob = new Blob([result.metadata.cover], { type: 'image/svg+xml' });
            coverDataUrl = URL.createObjectURL(svgBlob);
        }
        
        return {
            indexBase64: result.base64Index,
            wavBase64: wavBase64,
            metadata: {
                genre: result.metadata.genre,
                artist: result.metadata.artist,
                album: result.metadata.album,
                track: result.metadata.track,
                cover: coverDataUrl
            },
            audioFormat: result.audioFormat,
            numChannels: result.numChannels,
            sampleRate: result.sampleRate,
            bitDepth: result.bitDepth,
            dataSize: result.dataSize
        };
    } catch (error) {
        console.error('Error in searchByFile:', error);
        throw error;
    }
}

/**
 * Client-side reconstruct function - call this directly instead of fetch('/reconstruct')
 * 
 * @param {string} base64Data - Base64 or base64url encoded index data
 * @param {string} format - Either 'base64' or 'base64url'
 * @returns {Promise<Object>} Result object with indexBase64, wavBase64, and metadata
 */
export async function clientReconstruct(base64Data, format = 'base64') {
    try {
        const requestBody = { format, data: base64Data };
        return await reconstruct(requestBody);
    } catch (error) {
        console.error('Error in clientReconstruct:', error);
        throw error;
    }
}

/**
 * Client-side search by file function - call this directly instead of fetch('/search_by_file')
 * 
 * @param {File} file - WAV file to process
 * @returns {Promise<Object>} Result object with index, wav, metadata, and audio properties
 */
export async function clientSearchByFile(file) {
    try {
        const formData = new FormData();
        formData.append('file', file);
        return await searchByFile(formData);
    } catch (error) {
        console.error('Error in clientSearchByFile:', error);
        throw error;
    }
}

console.log('✅ apiAdapter.js loaded - direct function call mode (no fetch interception)');
