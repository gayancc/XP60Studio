// How a whole sound is cut up when it is sent to the instrument's temporary patch (parity row A3). Single parameter edits are
// one byte and unaffected. A smaller chunk is gentler on a slow interface; the bytes written are identical.

import { resplitDt1 } from '../wasm/pkg/xppatch.js';

export const MAX_CHUNK = 128;
export const MIN_CHUNK = 16;
const KEY = 'xppatch.chunk.v1';

/** A chunk size from storage or the stepper: a whole number from MIN_CHUNK to MAX_CHUNK, otherwise the maximum. */
export function clampChunk(value: unknown): number {
  const n = typeof value === 'string' ? Number(value) : value;
  return typeof n === 'number' && Number.isInteger(n) && n >= MIN_CHUNK && n <= MAX_CHUNK ? n : MAX_CHUNK;
}

function load(): number {
  try {
    return clampChunk(globalThis.localStorage?.getItem(KEY));
  } catch {
    return MAX_CHUNK;
  }
}

export const sendOptions = $state({ chunk: load() });

export function setChunk(value: number): void {
  sendOptions.chunk = clampChunk(value);
  try {
    globalThis.localStorage?.setItem(KEY, String(sendOptions.chunk));
  } catch {
    // Not remembered, still in effect.
  }
}

/** The frames of a whole-patch send, cut to the chosen chunk size (unchanged at the maximum). */
export function cutFrames(frames: Uint8Array, chunk: number = sendOptions.chunk): Uint8Array {
  return chunk >= MAX_CHUNK ? frames : resplitDt1(frames, chunk);
}
