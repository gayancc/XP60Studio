import { describe, expect, it } from 'vitest';
import { playPreviewNote } from './previewNote';

describe('Library preview note release', () => {
  it('plays briefly, releases the note, then clears the channel', async () => {
    const events: string[] = [];
    await playPreviewNote(
      async (on) => { events.push(on ? 'on' : 'off'); },
      async () => { events.push('all off'); },
      async (ms) => { events.push(`wait ${ms}`); },
    );
    expect(events).toEqual(['on', 'wait 650', 'off', 'all off']);
  });

  it('still clears the channel when Note Off fails', async () => {
    const events: string[] = [];
    await expect(playPreviewNote(
      async (on) => { events.push(on ? 'on' : 'off'); if (!on) throw new Error('no reply'); },
      async () => { events.push('all off'); },
      async () => {},
    )).rejects.toThrow('no reply');
    expect(events).toEqual(['on', 'off', 'all off']);
  });

  it('still clears the channel when Note On reports an error', async () => {
    const events: string[] = [];
    await expect(playPreviewNote(
      async (on) => { events.push(on ? 'on' : 'off'); if (on) throw new Error('no reply'); },
      async () => { events.push('all off'); },
      async () => { events.push('wait'); },
    )).rejects.toThrow('no reply');
    expect(events).toEqual(['on', 'off', 'all off']);
  });
});
