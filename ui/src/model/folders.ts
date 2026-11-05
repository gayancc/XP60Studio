// Folders for the Library (parity row D2). A folder is a tag with a path: "Pads/Warm" puts a patch in the folder Warm inside
// Pads. The library finds a folder's patches by the path prefix, so no separate folder store exists to fall out of step with the
// tags, and a patch can be filed in several folders at once.

export interface Folder {
  /** The last part: "Warm". */
  name: string;
  /** The whole path, as stored in the tag and as sent to the library to filter: "Pads/Warm". */
  path: string;
  children: Folder[];
}

/** Split a tag into its folder parts: surrounding spaces and empty parts ("Pads//Warm", "/Pads/") are dropped. */
export function pathParts(tag: string): string[] {
  return tag
    .split('/')
    .map((p) => p.trim())
    .filter((p) => p !== '');
}

/** The folders that the tags imply, sorted by name at every level (case-insensitively). */
export function buildFolders(tags: readonly string[]): Folder[] {
  const root: Folder = { name: '', path: '', children: [] };
  for (const tag of tags) {
    let at = root;
    for (const part of pathParts(tag)) {
      let next = at.children.find((c) => c.name.toLowerCase() === part.toLowerCase());
      if (!next) {
        next = { name: part, path: at.path === '' ? part : `${at.path}/${part}`, children: [] };
        at.children.push(next);
      }
      at = next;
    }
  }
  const sort = (f: Folder): void => {
    f.children.sort((a, b) => a.name.localeCompare(b.name, undefined, { sensitivity: 'base' }));
    f.children.forEach(sort);
  };
  sort(root);
  return root.children;
}

/** The folder at `path`, or undefined. */
export function findFolder(folders: readonly Folder[], path: string): Folder | undefined {
  let level: readonly Folder[] = folders;
  let found: Folder | undefined;
  for (const part of pathParts(path)) {
    found = level.find((f) => f.name.toLowerCase() === part.toLowerCase());
    if (!found) return undefined;
    level = found.children;
  }
  return found;
}

/** The path of the top-level folder that `path` is in (or is): "Pads/Warm" gives "Pads". */
export const topOf = (path: string): string => pathParts(path)[0] ?? '';

/** The parent path: "Pads/Warm" gives "Pads", and a top-level folder gives "". */
export function parentOf(path: string): string {
  return pathParts(path).slice(0, -1).join('/');
}

/** A tag as the library stores it: parts joined by single slashes. */
export const normaliseTag = (tag: string): string => pathParts(tag).join('/');
