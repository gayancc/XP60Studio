import { describe, expect, it } from 'vitest';
import { bankNote, boardName, EXPANSION_NOTE, expansionBoards, FACTORY_BANKS, factoryBank, factoryPatchName, internalWaveCounts, isFixedBank, PATCH_GROUPS, patchGroupOf, presetPatchName, waveLabel, waveName } from './factory';

describe('the factory lists', () => {
  it('holds only printable ASCII in every patch, wave and board name (the instrument has no other characters)', () => {
    const names = [
      ...FACTORY_BANKS.flatMap((b) => [...factoryBank(b)]),
      ...expansionBoards.flatMap((b) => [b.name, ...(b.waves ?? [])]),
    ];
    for (let n = 1; n <= 255; n++) for (const w of [waveName(0, 1, n), waveName(0, 2, n)]) if (w) names.push(w);
    const bad = names.filter((n) => /[^ -~]/.test(n));
    expect(bad).toEqual([]);
  });

  it("writes the quote in 60's, 90's, '70s and feet marks as a plain apostrophe", () => {
    expect(factoryPatchName('PR-A', 57)).toBe("60's LeadORG");
    expect(factoryPatchName('USER', 69)).toBe("Singing'Mini");
    expect(waveName(2, 8, 133)).toBe("Farf 8'+2'");
    expect(waveName(2, 8, 185)).toBe("'70s Piano A");
    expect(waveName(2, 9, 134)).toBe("90's Snare");
  });

  it('holds all five banks of 128 patch names, none blank except where Roland leaves one empty', () => {
    for (const b of FACTORY_BANKS) expect(factoryBank(b), b).toHaveLength(128);
    const blanks = FACTORY_BANKS.flatMap((b) => factoryBank(b).filter((n) => n.trim() === ''));
    expect(blanks.length).toBeLessThan(5);
  });

  it('names patches from Roland’s list', () => {
    expect(factoryPatchName('USER', 1)).toBe('Temple of XP');
    expect(factoryPatchName('USER', 2)).toBe('Power Split');
    expect(factoryPatchName('PR-A', 1)).toBe('64voicePiano');
    expect(factoryPatchName('PR-D', 1)).toBe('Piano 1');
    expect(factoryPatchName('PR-A', 0)).toBeUndefined();
    expect(factoryPatchName('PR-A', 129)).toBeUndefined();
  });

  it('holds the internal waves, INT-A and INT-B', () => {
    expect(internalWaveCounts).toEqual({ 'INT-A': 255, 'INT-B': 193 });
    expect(waveName(0, 1, 1)).toBe('Ac Piano1 A');
    expect(waveName(0, 1, 26)).toBe('E.Piano 2C');
    expect(waveName(0, 2, 1)).toBe('Kalimba');
    expect(waveName(0, 2, 156)).toBe('REV Cup');
  });

  it('names expansion waves by board number and wave number, for the boards whose lists are held', () => {
    expect(waveName(2, 1, 1)).toBe('Grand sft 1A');
    expect(waveName(2, 1, 154)).toBeTruthy();
    expect(waveName(2, 2, 174)).toBeTruthy();
    expect(waveName(2, 1, 155)).toBeUndefined();
  });

  it('never guesses: an unknown group, board or number has no name', () => {
    expect(waveName(0, 3, 1)).toBeUndefined();
    expect(waveName(0, 1, 0)).toBeUndefined();
    expect(waveName(0, 1, 256)).toBeUndefined();
    expect(waveName(1, 1, 1)).toBeUndefined();
    expect(waveName(2, 6, 1)).toBeUndefined(); // Dance: Roland publishes no waveform list for it
    expect(waveName(2, 12, 112)).toBeUndefined(); // Hip Hop: the one name Roland's list leaves out
    expect(waveName(2, 12, 111)).toBe('Voice MENU 1');
  });

  it('lists the SR-JV80 boards with their names', () => {
    expect(expansionBoards).toHaveLength(23);
    expect(boardName(14)).toBe('Asia');
    expect(boardName(97)).toBe('Experience III');
    expect(boardName(50)).toBeUndefined();
    // Roland publishes a waveform list for every board but 6 (Dance) and 19 (House); promotional boards have none
    expect(expansionBoards.filter((b) => b.waves).map((b) => b.number)).toEqual([1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18]);
  });

  it('treats Preset memory and expansion boards as fixed, and the User bank as the owner’s to change', () => {
    expect(FACTORY_BANKS.filter(isFixedBank)).toEqual(['PR-A', 'PR-B', 'PR-C', 'PR-D']);
    expect(isFixedBank('USER')).toBe(false);
    expect(bankNote('PR-A')).toMatch(/every XP-60/);
    expect(bankNote('PR-A')).toMatch(/cannot be overwritten/);
    expect(bankNote('USER')).toMatch(/may hold something else/);
    expect(EXPANSION_NOTE).toMatch(/read-only/);
  });

  it('always has something to show for a wave: its name, or where it lives and its number', () => {
    expect(waveLabel(0, 1, 26)).toBe('E.Piano 2C');
    expect(waveLabel(2, 6, 12)).toBe('Dance · 012'); // board 6: no list published
    expect(waveLabel(2, 12, 112)).toBe('Hip Hop Collection · 112');
    expect(waveLabel(2, 14, 7)).toBe('Santur 2 B');
    expect(waveLabel(0, 2, 999)).toBe('Internal B · 999');
    expect(waveLabel(2, 55, 1)).toBe('Board 55 · 001');
    expect(waveLabel(1, 1, 1)).toBe('Wave 001');
  });

  it('knows where each Patch group lives, as Roland tabulates it for the family (USER 1, PR-A to PR-D 3 to 6; ID 2 is the absent CARD bank)', () => {
    expect(PATCH_GROUPS.map((g) => [g.key, g.type, g.id])).toEqual([['USER', 0, 1], ['PR-A', 0, 3], ['PR-B', 0, 4], ['PR-C', 0, 5], ['PR-D', 0, 6]]);
    expect(patchGroupOf(0, 3)?.key).toBe('PR-A');
    expect(patchGroupOf(0, 2)).toBeUndefined(); // CARD: not on this instrument, and its absence does not shift the others
    expect(patchGroupOf(2, 9)).toBeUndefined(); // expansion boards are type 2 and are looked up by board number
  });

  it('names Patches in the fixed banks only: User memory is the owner’s own, and expansions are chosen by number', () => {
    expect(presetPatchName(0, 3, 0)).toBe('64voicePiano'); // PR-A 001
    expect(presetPatchName(0, 6, 0)).toBe('Piano 1'); // PR-D (GM) 001
    expect(presetPatchName(0, 1, 0)).toBeUndefined(); // USER
    expect(presetPatchName(2, 3, 0)).toBeUndefined(); // EXP
    expect(presetPatchName(0, 3, 128)).toBeUndefined(); // beyond the bank
  });
});
