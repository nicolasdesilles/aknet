/**
 * Format sample rate for display
 * @param rate Sample rate in Hz
 * @returns Formatted string (e.g., "48 kHz", "96 kHz")
 */
export function formatSampleRate(rate: number): string {
  if (rate >= 1000) {
    const kHz = rate / 1000;
    // Show one decimal place for non-integer kHz values (like 44.1 kHz)
    return kHz % 1 === 0 ? `${kHz} kHz` : `${kHz.toFixed(1)} kHz`;
  }
  return `${rate} Hz`;
}

/**
 * Format buffer size for display
 * @param size Buffer size in samples
 * @returns Formatted string (e.g., "256 samples", "512 samples")
 */
export function formatBufferSize(size: number): string {
  return `${size} samples`;
}
