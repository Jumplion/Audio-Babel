/**
 * waveformVisualizer.js
 * 
 * Generates normalized waveform visualizations from audio data.
 * Creates canvas-based visualizations that fit within a fixed width,
 * regardless of audio duration.
 */

/**
 * Extract audio samples from WAV blob
 * @param {Blob} audioBlob - WAV audio blob
 * @returns {Promise<Float32Array>} Audio sample data
 */
async function extractAudioData(audioBlob) {
  const arrayBuffer = await audioBlob.arrayBuffer();
  const audioContext = new (window.AudioContext || window.webkitAudioContext)();
  const audioBuffer = await audioContext.decodeAudioData(arrayBuffer);
  
  // Get channel data (use first channel if stereo)
  const channelData = audioBuffer.getChannelData(0);
  
  // Close the audio context to free resources
  audioContext.close();
  
  return channelData;
}

/**
 * Downsample audio data to fit target width
 * Uses min/max pairs for each pixel to preserve waveform detail
 * @param {Float32Array} samples - Raw audio samples (-1 to 1 range)
 * @param {number} targetWidth - Target width in pixels
 * @returns {Array<{min: number, max: number}>} Min/max pairs for each pixel
 */
function downsampleAudio(samples, targetWidth) {
  const samplesPerPixel = samples.length / targetWidth;
  const waveformData = [];
  
  for (let i = 0; i < targetWidth; i++) {
    const start = Math.floor(i * samplesPerPixel);
    const end = Math.floor((i + 1) * samplesPerPixel);
    
    let min = 1.0;
    let max = -1.0;
    
    for (let j = start; j < end && j < samples.length; j++) {
      const sample = samples[j];
      if (sample < min) min = sample;
      if (sample > max) max = sample;
    }
    
    waveformData.push({ min, max });
  }
  
  return waveformData;
}

/**
 * Draw waveform visualization on canvas
 * @param {HTMLCanvasElement} canvas - Target canvas element
 * @param {Array<{min: number, max: number}>} waveformData - Min/max pairs for each pixel
 * @param {Object} options - Visualization options
 * @param {string} options.waveColor - Waveform color (CSS color string)
 * @param {string} options.centerLineColor - Center line color
 * @param {string} options.backgroundColor - Canvas background color
 */
function drawWaveform(canvas, waveformData, options = {}) {
  const {
    waveColor = '#64b5f6',
    centerLineColor = 'rgba(100, 181, 246, 0.3)',
    backgroundColor = 'rgba(15, 20, 25, 0.8)'
  } = options;
  
  const ctx = canvas.getContext('2d');
  const width = canvas.width;
  const height = canvas.height;
  const halfHeight = height / 2;
  
  // Clear canvas with background
  ctx.fillStyle = backgroundColor;
  ctx.fillRect(0, 0, width, height);
  
  // Draw center line
  ctx.strokeStyle = centerLineColor;
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, halfHeight);
  ctx.lineTo(width, halfHeight);
  ctx.stroke();
  
  // Draw waveform
  ctx.fillStyle = waveColor;
  ctx.strokeStyle = waveColor;
  ctx.lineWidth = 1;
  
  for (let i = 0; i < waveformData.length; i++) {
    const { min, max } = waveformData[i];
    
    // Convert sample values (-1 to 1) to canvas coordinates
    const yMax = halfHeight - (max * halfHeight * 0.95); // Use 95% of half-height
    const yMin = halfHeight - (min * halfHeight * 0.95);
    
    // Draw vertical line from min to max
    ctx.beginPath();
    ctx.moveTo(i, yMax);
    ctx.lineTo(i, yMin);
    ctx.stroke();
  }
}

/**
 * Generate waveform visualization from audio blob
 * @param {Blob} audioBlob - Audio file as blob (WAV format)
 * @param {HTMLCanvasElement} canvas - Canvas element to draw on
 * @param {Object} options - Visualization options
 * @param {string} [options.waveColor='#64b5f6'] - Waveform color
 * @param {string} [options.centerLineColor='rgba(100, 181, 246, 0.3)'] - Center line color
 * @param {string} [options.backgroundColor='rgba(15, 20, 25, 0.8)'] - Background color
 * @returns {Promise<void>}
 */
export async function generateWaveform(audioBlob, canvas, options = {}) {
  if (!audioBlob || !canvas) {
    throw new Error('Audio blob and canvas element are required');
  }
  
  // Extract audio data and downsample to canvas width
  const audioSamples = await extractAudioData(audioBlob);
  const waveformData = downsampleAudio(audioSamples, canvas.width);
  
  // Draw waveform
  drawWaveform(canvas, waveformData, options);
}

/**
 * Create a waveform visualization from base64-encoded WAV data
 * @param {string} wavBase64 - Base64-encoded WAV data
 * @param {HTMLCanvasElement} canvas - Canvas element to draw on
 * @param {Object} options - Visualization options
 * @returns {Promise<void>}
 */
export async function generateWaveformFromBase64(wavBase64, canvas, options = {}) {
  // Convert base64 to blob
  const bytes = atob(wavBase64);
  const ab = new Uint8Array(bytes.length);
  for (let i = 0; i < bytes.length; i++) {
    ab[i] = bytes.charCodeAt(i);
  }
  const blob = new Blob([ab], { type: 'audio/wav' });
  
  // Generate waveform
  await generateWaveform(blob, canvas, options);
}
