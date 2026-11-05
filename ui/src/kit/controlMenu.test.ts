import { describe, expect, it, vi } from 'vitest';
import { controlMenuItems } from './controlMenu';

const base = { range: { min: 0, max: 127, step: 1 }, get: () => 5, set: () => {} };
const labels = (items: ReturnType<typeof controlMenuItems>): string[] => items.map((i) => i.label);

describe('the control menu', () => {
  it('shows the MIDI link disabled when there is no instrument connection to listen on', () => {
    const item = controlMenuItems(base).find((i) => i.label.startsWith('Link to MIDI CC'));
    expect(item?.disabled).toBe(true);
  });

  it('offers to link a controller and runs the learn action', () => {
    const learn = vi.fn();
    const items = controlMenuItems({ ...base, midi: { linked: null, learn, unlink: () => {} } });
    const link = items.find((i) => i.label === 'Link to MIDI CC…');
    expect(link?.disabled).toBeFalsy();
    link?.action?.();
    expect(learn).toHaveBeenCalled();
    expect(labels(items).some((l) => l.startsWith('Unlink'))).toBe(false);
  });

  it('names the linked controller and offers to unlink it', () => {
    const unlink = vi.fn();
    const items = controlMenuItems({ ...base, midi: { linked: 'CC 74', learn: () => {}, unlink } });
    expect(labels(items)).toContain('Link to another controller… (now CC 74)');
    items.find((i) => i.label === 'Unlink CC 74')?.action?.();
    expect(unlink).toHaveBeenCalled();
  });
});
