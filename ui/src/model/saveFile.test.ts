import { describe, expect, it } from 'vitest';
import { savedMessage } from './saveFile';

describe('the message after saving a file', () => {
  it('names the file and never shows the folder', () => {
    expect(savedMessage(String.raw`C:\Users\Gayan\AppData\Local\Temp\ALL RIGHT.syx`)).toBe('Saved ALL RIGHT.syx');
    expect(savedMessage('/home/gayan/exports/bank.mid')).toBe('Saved bank.mid');
    expect(savedMessage('plain.syx')).toBe('Saved plain.syx');
  });
});
