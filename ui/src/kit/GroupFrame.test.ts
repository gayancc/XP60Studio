import { afterEach, describe, expect, it } from 'vitest';
import { cleanup, render } from '@testing-library/svelte';
import { createRawSnippet } from 'svelte';
import GroupFrame from './GroupFrame.svelte';

afterEach(cleanup);
const body = createRawSnippet(() => ({ render: () => '<span>x</span>' }));

describe('GroupFrame pointing out', () => {
  it('lights the border for a group pointed out just now', () => {
    const { container } = render(GroupFrame, { props: { title: 'Chorus', flash: Date.now(), children: body } });
    expect(container.querySelector('fieldset')?.classList.contains('lit')).toBe(true);
  });
  it('does not light for no jump, or a jump long ago', () => {
    const none = render(GroupFrame, { props: { title: 'Chorus', children: body } });
    expect(none.container.querySelector('fieldset')?.classList.contains('lit')).toBe(false);
    cleanup();
    const old = render(GroupFrame, { props: { title: 'Chorus', flash: Date.now() - 60000, children: body } });
    expect(old.container.querySelector('fieldset')?.classList.contains('lit')).toBe(false);
  });
});
