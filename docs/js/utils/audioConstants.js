/**
 * Default audio format constants (from cpp/include/Constants.h).
 * Single source of truth for the JS layer — import these instead of
 * scattering 44100 / 16 / 1 magic numbers across page modules.
 */
export const DEFAULT_SAMPLE_RATE = 44100;
export const DEFAULT_BIT_DEPTH = 16;
export const DEFAULT_NUM_CHANNELS = 1;
