import * as Juce from '@juce-framework/webview';
import { isNativeFunctionRegistered } from './backend/JuceBackend';

/**
 * Space and Enter passthrough to the host DAW.
 *
 * The webview owns keyboard focus once the user interacts with the plugin,
 * so the host's transport keys, Space (play/stop) and Enter (return to
 * start / stop, host-dependent), land in web content and either beep or
 * scroll instead of reaching the transport. Any press outside an editable
 * or interactive element is swallowed here and handed to the host via
 * `forwardKeyToHost` (plugin builds; in the dev browser the suppression
 * alone stops the beep/scroll).
 *
 * Interactive includes the chain tiles: dnd-kit marks them role="button",
 * and Space or Enter on a focused tile starts a keyboard drag (intentional,
 * since it requires tabbing to the tile first). Either key with focus
 * anywhere else still reaches the transport.
 *
 * Installed only on the plugin UI's own origin: the OAuth flows navigate
 * this webview to remote tone3000.com pages, which load without the UI
 * bundle, so they keep full keyboard behavior (the site's search box
 * needs Enter).
 */
export function installKeyPassthrough(): void {
  window.addEventListener(
    'keydown',
    (e) => {
      if (e.code !== 'Space' && e.code !== 'Enter') return;
      // Editable and interactive elements keep the key (typing in preset
      // names/search, Enter committing a value edit, activating a focused
      // button).
      if (
        e.target instanceof Element &&
        e.target.closest(
          'input, textarea, select, button, a[href], [contenteditable], [role="button"]'
        )
      ) {
        return;
      }
      // Leave keyboard drags alone (Space/Enter/Escape end them; a focus
      // change mid-gesture would cancel the drag).
      if (document.querySelector('[data-dnd-dragging]') != null) return;
      // Always suppress: stops the caret scroll / system beep even where
      // there is no host to forward to (standalone, dev browser).
      e.preventDefault();
      // Forward the initial press only; the native side resigns the
      // webview's keyboard focus, so genuine repeats go to the host
      // directly, and any stragglers here must not spam the transport.
      if (e.repeat) return;
      if (isNativeFunctionRegistered('forwardKeyToHost'))
        void Juce.getNativeFunction('forwardKeyToHost')(e.code);
    },
    { capture: true }
  );
}
