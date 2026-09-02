import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// The UI has one build target: static assets in ../plugin/webview, which the
// plugin embeds as JUCE binary data.
export default defineConfig({
  plugins: [react()],
  build: {
    // The plugin webview runs the system WebKit on macOS and Linux, which
    // can be years old (macOS 10.15 tops out at Safari 15.6, or 13.x if
    // Safari was never updated). Vite's default target assumes a current
    // browser, and one construct it leaves untranspiled is a parse error
    // that kills the whole bundle before it runs: a silent black window.
    // safari13 keeps the bundle parseable on every supported WebKit; the
    // runtime floor is Safari 13.1 (ResizeObserver), macOS 10.15.4.
    target: ['safari13'],
    outDir: '../plugin/webview',
    emptyOutDir: true,
    assetsDir: 'assets',
    // Served from the plugin binary via JUCE's resource provider, so chunk
    // size has no network cost; splitting would only add loading states.
    chunkSizeWarningLimit: 1000,
    rollupOptions: {
      input: {
        main: './index.html',
      },
      output: {
        format: 'es',
        entryFileNames: '[name].js',
        assetFileNames: 'assets/[name]-[hash][extname]',
      },
    },
    cssCodeSplit: true,
  },
  base: './',
});
