/**
 * Turn a user-supplied image file (PNG, JPEG, WebP, ... — anything the
 * browser can decode) into the fixed-size RGB pixel grid the WASM
 * `constructByCover` call expects. The image is center-cropped to a square
 * ("cover" fit) and scaled down to pixelsPerSide × pixelsPerSide, matching
 * how the C++ side reads cover bytes: three bytes per tile, reading order.
 */

// Decodes an image File/Blob into something drawImage accepts. Prefers
// createImageBitmap; falls back to an HTMLImageElement + object URL for
// older browsers.
async function decodeImageFile(file) {
  if (typeof createImageBitmap === 'function') {
    return createImageBitmap(file);
  }
  const url = URL.createObjectURL(file);
  try {
    return await new Promise((resolve, reject) => {
      const img = new Image();
      img.onload = () => resolve(img);
      img.onerror = () => reject(new Error('Could not decode image file'));
      img.src = url;
    });
  } finally {
    URL.revokeObjectURL(url);
  }
}

// Quantizes an image file down to the cover mosaic's pixel grid.
// Returns { pixels, imageData }: pixels is packed 8-bit RGB in reading order
// (3 * pixelsPerSide^2 bytes, exactly what constructByCover consumes);
// imageData is the same quantized square as RGBA for a preview canvas.
export async function quantizeImageToCoverPixels(file, pixelsPerSide) {
  const source = await decodeImageFile(file);
  const width = source.width || source.naturalWidth;
  const height = source.height || source.naturalHeight;
  if (!width || !height) {
    throw new Error('Could not decode image file');
  }

  const canvas = document.createElement('canvas');
  canvas.width = pixelsPerSide;
  canvas.height = pixelsPerSide;
  const ctx = canvas.getContext('2d', { willReadFrequently: true });

  // Covers have no alpha channel: composite transparent regions onto black
  // (the same color the mosaic uses for missing bytes).
  ctx.fillStyle = '#000';
  ctx.fillRect(0, 0, pixelsPerSide, pixelsPerSide);

  // Center-crop to a square, then scale down — non-square images keep their
  // middle rather than being squashed.
  const cropSide = Math.min(width, height);
  const sx = (width - cropSide) / 2;
  const sy = (height - cropSide) / 2;
  ctx.drawImage(source, sx, sy, cropSide, cropSide, 0, 0, pixelsPerSide, pixelsPerSide);
  if (typeof source.close === 'function') {
    source.close();
  }

  const imageData = ctx.getImageData(0, 0, pixelsPerSide, pixelsPerSide);
  const rgba = imageData.data;
  const pixels = new Uint8Array(pixelsPerSide * pixelsPerSide * 3);
  for (let i = 0, j = 0; i < rgba.length; i += 4, j += 3) {
    pixels[j] = rgba[i];
    pixels[j + 1] = rgba[i + 1];
    pixels[j + 2] = rgba[i + 2];
  }

  return { pixels, imageData };
}
