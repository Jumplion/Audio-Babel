/**
 * apiAdapter.js
 * 
 * Client-side adapter that replaces server API calls with direct
 * JavaScript processing using audioIndex.js module.
 * 
 * This module provides the same interface as the server API endpoints,
 * but processes everything in the browser.
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
            throw new Error('Format: ' + format + ' - Only base64 format is supported');
        }

        // Convert from standard base64 to bytes
        const indexBytes = base64ToBytes(data);
        
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
 * Fetch wrapper that intercepts API calls and routes to client-side functions
 * This can be used as a drop-in replacement for fetch() in existing code
 */
export async function clientSideFetch(url, options = {}) {
    // Parse the URL
    const urlObj = new URL(url, window.location.origin);
    const pathname = urlObj.pathname;
    
    // Route to appropriate handler
    if (pathname === '/reconstruct' || pathname.endsWith('/reconstruct')) {
        if (options.method === 'POST' && options.body) {
            const requestBody = JSON.parse(options.body);
            const result = await reconstruct(requestBody);
            
            // Return a Response-like object
            return {
                ok: true,
                status: 200,
                json: async () => result,
                text: async () => JSON.stringify(result)
            };
        }
    } else if (pathname === '/search_by_file' || pathname.endsWith('/search_by_file')) {
        if (options.method === 'POST' && options.body) {
            const result = await searchByFile(options.body);
            
            return {
                ok: true,
                status: 200,
                json: async () => result,
                text: async () => JSON.stringify(result)
            };
        }
    }
    
    // Fallback to real fetch for other URLs
    return fetch(url, options);
}

/**
 * Initialize client-side API by replacing global fetch
 * Call this once at app startup to enable client-side processing
 */
export function enableClientSideAPI() {
    // Store original fetch
    window._originalFetch = window.fetch;
    
    // Replace with our adapter
    window.fetch = clientSideFetch;
    
    console.log('Client-side API enabled - no server required!');
}

/**
 * Restore original fetch (for debugging)
 */
export function disableClientSideAPI() {
    if (window._originalFetch) {
        window.fetch = window._originalFetch;
        console.log('Client-side API disabled - using server');
    }
}
