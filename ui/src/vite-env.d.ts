/// <reference types="vite/client" />

interface Window {
  /**
   * Set by main.tsx the moment the bundle executes; read by the boot
   * watchdog inline in index.html. If it never appears, the watchdog logs
   * WebKit feature probes to the forwarded console (-> TONE3000.log).
   */
  __T3K_UI_BOOTED?: boolean;
}
