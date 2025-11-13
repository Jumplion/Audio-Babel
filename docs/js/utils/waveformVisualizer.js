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
 * Normalize audio samples to a target number of bars for visualization
 * @param {Float32Array} samples - Raw audio samples
 * @param {number} targetBars - Number of bars to display in waveform
 * @returns {Float32Array} Normalized bar heights (0-1 range)
 */
function normalizeToBarHeights(samples, targetBars) {
  const samplesPerBar = Math.ceil(samples.length / targetBars);
  const bars = new Float32Array(targetBars);
  
  // Calculate RMS (root mean square) for each bar segment
  for (let i = 0; i < targetBars; i++) {
    const start = i * samplesPerBar;
    const end = Math.min(start + samplesPerBar, samples.length);
    
    let sumSquares = 0;
    let count = 0;
    
    for (let j = start; j < end; j++) {
      sumSquares += samples[j] * samples[j];
      count++;
    }
    
    // RMS value gives better visual representation than peak
    const rms = count > 0 ? Math.sqrt(sumSquares / count) : 0;
    bars[i] = rms;
  }
  
  // Normalize to 0-1 range
  const maxValue = Math.max(...bars);
  if (maxValue > 0) {
    for (let i = 0; i < bars.length; i++) {
      bars[i] /= maxValue;
    }
  }
  
  return bars;
}

/**
 * Draw waveform visualization on canvas
 * @param {HTMLCanvasElement} canvas - Target canvas element
 * @param {Float32Array} barHeights - Normalized bar heights (0-1)
 * @param {Object} options - Visualization options
 * @param {string} options.color - Bar color (CSS color string)
 * @param {string} options.backgroundColor - Canvas background color
 * @param {number} options.barWidth - Width of each bar in pixels
 * @param {number} options.barGap - Gap between bars in pixels
 */
function drawWaveform(canvas, barHeights, options = {}) {
  const {
    color = '#64b5f6',
    backgroundColor = 'rgba(15, 20, 25, 0.8)',
    barWidth = 3,
    barGap = 1
  } = options;
  
  const ctx = canvas.getContext('2d');
  const width = canvas.width;
  const height = canvas.height;
  
  // Clear canvas with background
  ctx.fillStyle = backgroundColor;
  ctx.fillRect(0, 0, width, height);
  
  // Draw bars
  ctx.fillStyle = color;
  
  const totalBarWidth = barWidth + barGap;
  const numBars = barHeights.length;
  
  for (let i = 0; i < numBars; i++) {
    const x = i * totalBarWidth;
    const barHeight = barHeights[i] * height * 0.9; // Use 90% of height
    const y = (height - barHeight) / 2; // Center vertically
    
    ctx.fillRect(x, y, barWidth, barHeight);
  }
}

/**
 * Generate waveform visualization from audio blob
 * @param {Blob} audioBlob - Audio file as blob (WAV format)
 * @param {HTMLCanvasElement} canvas - Canvas element to draw on
 * @param {Object} options - Visualization options
 * @param {string} [options.color='#64b5f6'] - Bar color
 * @param {string} [options.backgroundColor='rgba(15, 20, 25, 0.8)'] - Background color
 * @param {number} [options.barWidth=3] - Width of each bar in pixels
 * @param {number} [options.barGap=1] - Gap between bars in pixels
 * @returns {Promise<void>}
 */
export async function generateWaveform(audioBlob, canvas, options = {}) {
  if (!audioBlob || !canvas) {
    throw new Error('Audio blob and canvas element are required');
  }
  
  // Calculate number of bars based on canvas width and bar spacing
  const barWidth = options.barWidth || 3;
  const barGap = options.barGap || 1;
  const totalBarWidth = barWidth + barGap;
  const numBars = Math.floor(canvas.width / totalBarWidth);
  
  // Extract and normalize audio data
  const audioSamples = await extractAudioData(audioBlob);
  const barHeights = normalizeToBarHeights(audioSamples, numBars);
  
  // Draw waveform
  drawWaveform(canvas, barHeights, options);
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
