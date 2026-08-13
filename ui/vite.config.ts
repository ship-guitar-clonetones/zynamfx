import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// The UI has one build target: static assets in ../plugin/webview, which the
// plugin embeds as JUCE binary data.
export default defineConfig({
  plugins: [react()],
  build: {
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
