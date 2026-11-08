# Agent instructions (XPPatchStudio)

Read `CLAUDE.md` for the full project rules. This file highlights standing verification policy.

## Live desktop verification (mandatory)

Green unit/integration tests are **necessary but not sufficient**.

For behavioural UI or MIDI/hardware-facing work:

1. Run the **real desktop app** (not only Vitest / `cargo test`).
2. **Click the actual controls** in the window (pads, menus, tabs, knobs) — the same path a user takes.
3. Confirm the user-visible result (UI state, status text, audible patch/MIDI when connected).
4. Do **not** accept as proof: calling Tauri/`invoke`, store helpers, or other underground APIs without using the window.

When reporting done: state what you clicked and what you observed. If live UI verification was blocked, say so — do not mark the behaviour verified.

Canonical detail: `.cursor/rules/live-ui-verification.mdc` and hard rule 7 in `CLAUDE.md`.
