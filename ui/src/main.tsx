import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App.tsx';
import { installKeyPassthrough } from './keyPassthrough.ts';
// Self-hosted Roboto Mono (bundled woff2, served from the plugin binary):
// the DAW webview has no network, so no CDN fonts.
import '@fontsource/roboto-mono/400.css';
import '@fontsource/roboto-mono/700.css';
import './index.css';

// Reaching this line means the bundle parsed and executed: tell the boot
// watchdog in index.html, which otherwise logs boot diagnostics to the
// native log after 4s (see the inline script there).
window.__T3K_UI_BOOTED = true;

// A file dropped anywhere on the plugin would make the webview navigate away
// from the UI. Swallow drags globally; components that want drops can handle
// them before the event bubbles here.
window.addEventListener('dragover', (e) => e.preventDefault());
window.addEventListener('drop', (e) => e.preventDefault());

// Space and Enter are the DAW's transport keys, not ours: swallow them and
// hand them to the host (see keyPassthrough.ts).
installKeyPassthrough();

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);

// Breadcrumb in TONE3000.log (console.* is forwarded natively): confirms the
// UI booted and the log-forwarding pipeline works in every session.
console.log('TONE3000 UI booted');
