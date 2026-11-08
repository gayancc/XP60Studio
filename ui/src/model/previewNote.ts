/** One short Library preview note, with a channel panic even when Note Off fails. */
export async function playPreviewNote(
  note: (on: boolean) => Promise<void>,
  allNotesOff: () => Promise<void>,
  sleep: (ms: number) => Promise<void> = (ms) => new Promise((resolve) => setTimeout(resolve, ms)),
): Promise<void> {
  await note(true);
  try {
    await sleep(650);
  } finally {
    try {
      await note(false);
    } finally {
      await allNotesOff();
    }
  }
}
