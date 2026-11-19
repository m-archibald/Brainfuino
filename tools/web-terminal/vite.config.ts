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
        const docWebTerminal = path.resolve(__dirname, '../../documentation/docs/web-terminal.html');
        const docToolsTerminal = path.resolve(__dirname, '../../documentation/docs/tools/web-terminal.html');
        if (fs.existsSync(distFile)) {
          fs.copyFileSync(distFile, rootIndex);
          if (fs.existsSync(path.dirname(docWebTerminal))) {
            fs.copyFileSync(distFile, docWebTerminal);
          }
          if (fs.existsSync(path.dirname(docToolsTerminal))) {
            fs.copyFileSync(distFile, docToolsTerminal);
          }
          console.log('[copy-bundles] Updated production bundles at tools/web-terminal/index.html and documentation/docs/');
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
