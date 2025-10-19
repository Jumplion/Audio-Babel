// src/services/audioConstraints.js
/**
 * Audio constraints for the Library of Babel record store
 * Limits based on 2-minute maximum audio duration
 */

// Audio format constants
const SAMPLE_RATE = 44100; // CD quality: 44.1kHz
const BIT_DEPTH = 16; // 16-bit audio
const CHANNELS = 1; // Mono (for simplicity and size)
const BYTES_PER_SAMPLE = BIT_DEPTH / 8; // 2 bytes for 16-bit

// Time constraints
const MAX_DURATION_SECONDS = 120; // 2 minutes
const MAX_DURATION_MS = MAX_DURATION_SECONDS * 1000;

// Calculate maximum sizes
const MAX_SAMPLES = SAMPLE_RATE * MAX_DURATION_SECONDS; // 5,292,000 samples
const MAX_SAMPLE_BYTES = MAX_SAMPLES * BYTES_PER_SAMPLE * CHANNELS; // ~10.6 MB
const MAX_FILE_SIZE_MB = MAX_SAMPLE_BYTES / (1024 * 1024); // ~10.08 MB

// Index size calculations (base64 encoding adds ~33% overhead to raw bytes)
// The C++ library adds a 16-byte header, so total index bytes ≈ sample_bytes + 16
const HEADER_BYTES = 16;
const MAX_INDEX_BYTES = MAX_SAMPLE_BYTES + HEADER_BYTES;
const MAX_INDEX_BASE64_LENGTH = Math.ceil((MAX_INDEX_BYTES * 4) / 3); // Base64 encoding

/**
 * Validate audio duration
 * @param {number} durationSeconds - Duration in seconds
 * @returns {boolean} True if valid
 */
function isValidDuration(durationSeconds) {
    return durationSeconds > 0 && durationSeconds <= MAX_DURATION_SECONDS;
}

/**
 * Validate sample count
 * @param {number} sampleCount - Number of samples
 * @returns {boolean} True if valid
 */
function isValidSampleCount(sampleCount) {
    return sampleCount > 0 && sampleCount <= MAX_SAMPLES;
}

/**
 * Validate file size
 * @param {number} fileSizeBytes - File size in bytes
 * @returns {boolean} True if valid
 */
function isValidFileSize(fileSizeBytes) {
    return fileSizeBytes > 0 && fileSizeBytes <= MAX_SAMPLE_BYTES * 1.5; // Allow some overhead for WAV headers
}

/**
 * Calculate expected index size for given audio duration
 * @param {number} durationSeconds - Duration in seconds
 * @returns {object} Size estimates
 */
function calculateIndexSize(durationSeconds) {
    if (!isValidDuration(durationSeconds)) {
        throw new Error(`Duration must be between 0 and ${MAX_DURATION_SECONDS} seconds`);
    }

    const samples = SAMPLE_RATE * durationSeconds;
    const sampleBytes = samples * BYTES_PER_SAMPLE * CHANNELS;
    const totalIndexBytes = sampleBytes + HEADER_BYTES;
    const base64Length = Math.ceil((totalIndexBytes * 4) / 3);

    return {
        durationSeconds,
        samples,
        sampleBytes,
        headerBytes: HEADER_BYTES,
        totalIndexBytes,
        base64Length,
        estimatedFileSizeMB: sampleBytes / (1024 * 1024)
    };
}

/**
 * Get recommended audio settings
 * @returns {object} Recommended settings
 */
function getRecommendedSettings() {
    return {
        sampleRate: SAMPLE_RATE,
        bitDepth: BIT_DEPTH,
        channels: CHANNELS,
        maxDurationSeconds: MAX_DURATION_SECONDS,
        format: 'WAV (PCM)',
        description: 'CD-quality mono audio, 2-minute maximum'
    };
}

/**
 * Estimate duration from file size
 * @param {number} fileSizeBytes - File size in bytes
 * @returns {number} Estimated duration in seconds
 */
function estimateDurationFromSize(fileSizeBytes) {
    const bytesPerSecond = SAMPLE_RATE * BYTES_PER_SAMPLE * CHANNELS;
    return fileSizeBytes / bytesPerSecond;
}

/**
 * Format file size for display
 * @param {number} bytes - Size in bytes
 * @returns {string} Formatted size
 */
function formatFileSize(bytes) {
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(2)} KB`;
    return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
}

/**
 * Format duration for display
 * @param {number} seconds - Duration in seconds
 * @returns {string} Formatted duration (mm:ss)
 */
function formatDuration(seconds) {
    const minutes = Math.floor(seconds / 60);
    const secs = Math.floor(seconds % 60);
    return `${minutes}:${secs.toString().padStart(2, '0')}`;
}

export {
    // Constants
    SAMPLE_RATE,
    BIT_DEPTH,
    CHANNELS,
    BYTES_PER_SAMPLE,
    MAX_DURATION_SECONDS,
    MAX_DURATION_MS,
    MAX_SAMPLES,
    MAX_SAMPLE_BYTES,
    MAX_FILE_SIZE_MB,
    HEADER_BYTES,
    MAX_INDEX_BYTES,
    MAX_INDEX_BASE64_LENGTH,
    
    // Validation functions
    isValidDuration,
    isValidSampleCount,
    isValidFileSize,
    
    // Utility functions
    calculateIndexSize,
    getRecommendedSettings,
    estimateDurationFromSize,
    formatFileSize,
    formatDuration
};
