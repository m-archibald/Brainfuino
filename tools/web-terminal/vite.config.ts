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
        const targets = [
          path.resolve(__dirname, 'index.html'),
          path.resolve(__dirname, '../../documentation/docs/tools/terminal/web-terminal.html'),
          path.resolve(__dirname, '../../documentation/docs/web-terminal.html'),
          path.resolve(__dirname, '../../documentation/docs/tools/web-terminal.html'),
        ];
        if (fs.existsSync(distFile)) {
          for (const target of targets) {
            const dir = path.dirname(target);
            if (fs.existsSync(dir)) {
              fs.copyFileSync(distFile, target);
            }
          }
          console.log('[copy-bundles] Updated root index.html and all documentation web-terminal.html targets');
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
