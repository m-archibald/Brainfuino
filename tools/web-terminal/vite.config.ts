/// <reference types="vitest" />
import { defineConfig } from 'vite';
import { viteSingleFile } from 'vite-plugin-singlefile';
import fs from 'fs';
import path from 'path';

export default defineConfig({
  root: path.resolve(__dirname, 'src'),
  plugins: [
    viteSingleFile(),
    {
      name: 'copy-bundles',
      closeBundle() {
        const distFile = path.resolve(__dirname, 'dist/index.html');
        const rootIndex = path.resolve(__dirname, 'index.html');
        if (fs.existsSync(distFile)) {
          fs.copyFileSync(distFile, rootIndex);
          console.log('[copy-bundles] Updated single production index.html at tools/web-terminal/index.html');
        }
      }
    }
  ],
  build: {
    target: 'esnext',
    assetsInlineLimit: 100000000,
    chunkSizeWarningLimit: 100000000,
    cssCodeSplit: false,
    outDir: path.resolve(__dirname, 'dist'),
    emptyOutDir: true,
  },
  test: {
    root: path.resolve(__dirname),
    environment: 'happy-dom',
  },
  server: {
    port: 5173,
    open: false,
  },
});
