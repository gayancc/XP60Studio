import { defineConfig } from 'vitest/config';
import { svelte } from '@sveltejs/vite-plugin-svelte';

export default defineConfig({
  plugins: [svelte()],
  // The preview may read the golden bank fixture one level up.
  server: { fs: { allow: ['..'] } },
  resolve: process.env.VITEST ? { conditions: ['browser'] } : undefined,
  test: {
    environment: 'jsdom',
    include: ['src/**/*.test.ts'],
    // The randomize-over-many-seeds test is heavy and times out when the machine is busy building.
    testTimeout: 20000,
  },
});
