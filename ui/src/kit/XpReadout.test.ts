import { cleanup, render } from '@testing-library/svelte';
import { afterEach, describe, expect, it } from 'vitest';
import XpReadout from './XpReadout.svelte';

afterEach(cleanup);

const base = { title: 'PATCH', badge: 'EDITED', main: 'Warm Pad', status: 'XP-60 ID 17' };

describe('the status readout', () => {
  it('says what it shows for assistive technology', () => {
    const { getByRole } = render(XpReadout, { props: base });
    expect(getByRole('status').getAttribute('aria-label')).toBe('Status: PATCH, EDITED, Warm Pad. XP-60 ID 17');
  });

  it('shows the tag and the second status only when they are given', async () => {
    const { container, rerender } = render(XpReadout, { props: base });
    expect(container.querySelector('.tag')).toBeNull();
    expect(container.querySelector('.tx')).toBeNull();
    await rerender({ ...base, tag: '#012', status2: 'TX 3' });
    expect(container.querySelector('.tag')?.textContent).toBe('#012');
    expect(container.querySelector('.tx')?.textContent).toBe('TX 3');
  });

  it('leaves the state out when there is none, as on the Library and Bank screens', () => {
    const { container } = render(XpReadout, { props: { ...base, badge: '' } });
    expect(container.querySelector('.badge')).toBeNull();
  });

  it('turns the big line to the accent colour while it shows a control value', async () => {
    const { container, rerender } = render(XpReadout, { props: base });
    expect(container.querySelector('.main.live')).toBeNull();
    await rerender({ ...base, hovering: true });
    expect(container.querySelector('.main.live')).not.toBeNull();
  });

  it('lights the link only when the instrument is connected and nothing is being read out', async () => {
    const { container, rerender } = render(XpReadout, { props: { ...base, linked: true } });
    expect(container.querySelector('.link.on')).not.toBeNull();
    await rerender({ ...base, linked: true, hovering: true });
    expect(container.querySelector('.link.on')).toBeNull();
    await rerender({ ...base, linked: false });
    expect(container.querySelector('.link.on')).toBeNull();
  });

  it('pulses the state while sends are waiting', () => {
    const { container } = render(XpReadout, { props: { ...base, busy: true } });
    expect(container.querySelector('.badge.busy')).not.toBeNull();
  });

  it('keeps the size it is asked for', () => {
    const { getByRole } = render(XpReadout, { props: { ...base, width: 260, height: 52 } });
    const el = getByRole('status') as HTMLElement;
    expect(el.style.width).toBe('260px');
    expect(el.style.height).toBe('52px');
  });
});
