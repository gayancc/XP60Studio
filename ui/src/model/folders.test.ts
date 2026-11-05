import { describe, expect, it } from 'vitest';
import { buildFolders, findFolder, normaliseTag, parentOf, pathParts, topOf } from './folders';

describe('library folders from tags', () => {
  it('splits a tag into parts, dropping empty ones and stray spaces', () => {
    expect(pathParts('Pads/Warm')).toEqual(['Pads', 'Warm']);
    expect(pathParts(' Pads // Warm /')).toEqual(['Pads', 'Warm']);
    expect(pathParts('/')).toEqual([]);
    expect(pathParts('')).toEqual([]);
    expect(normaliseTag(' Pads // Warm /')).toBe('Pads/Warm');
  });

  it('builds the tree the tags imply, sorted, with every level reachable by its path', () => {
    const tree = buildFolders(['Pads/Warm', 'Bass', 'Pads/Bright/Airy', 'pads/Cold', 'Leads']);
    expect(tree.map((f) => f.name)).toEqual(['Bass', 'Leads', 'Pads']);
    const pads = tree.find((f) => f.name === 'Pads');
    expect(pads?.children.map((c) => c.name)).toEqual(['Bright', 'Cold', 'Warm']);
    expect(pads?.children.find((c) => c.name === 'Bright')?.children[0]?.path).toBe('Pads/Bright/Airy');
  });

  it('treats folder names that differ only in case as one folder', () => {
    const tree = buildFolders(['Pads/Warm', 'PADS/Cold']);
    expect(tree).toHaveLength(1);
    expect(tree[0]?.children).toHaveLength(2);
  });

  it('a tag that is both a folder and a patch tag is still one folder', () => {
    const tree = buildFolders(['Pads', 'Pads/Warm']);
    expect(tree).toHaveLength(1);
    expect(tree[0]?.children.map((c) => c.path)).toEqual(['Pads/Warm']);
  });

  it('finds a folder by path in any case, and says so when there is none', () => {
    const tree = buildFolders(['Pads/Warm/Dark']);
    expect(findFolder(tree, 'pads/warm')?.path).toBe('Pads/Warm');
    expect(findFolder(tree, 'Pads/Nope')).toBeUndefined();
    expect(findFolder(tree, '')).toBeUndefined();
  });

  it('knows the top folder and the parent of a path', () => {
    expect(topOf('Pads/Warm/Dark')).toBe('Pads');
    expect(topOf('Pads')).toBe('Pads');
    expect(topOf('')).toBe('');
    expect(parentOf('Pads/Warm/Dark')).toBe('Pads/Warm');
    expect(parentOf('Pads')).toBe('');
  });

  it('ignores tags that have no usable parts', () => {
    expect(buildFolders(['', '/', '  '])).toEqual([]);
  });
});
