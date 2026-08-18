import React, { createContext, useContext, useEffect, useMemo, useRef, useState } from 'react';
import { BLACK, WHITE } from './theme';

/**
 * One app-wide toast: a white pill centered above the faceplate, for quick
 * confirmations ("Preset Saved", "Link Copied") and auto-measure status.
 * Only one message shows at a time; a new one replaces whatever is up.
 */

interface ToastControl {
  /** Flash a message; auto-dismisses after a moment. */
  show: (message: string) => void;
  /** Pin a message until the next show/clear (auto-measure "Listening"). */
  pin: (message: string) => void;
  /** Take a pinned message down with no follow-up (cancel, timeout). */
  clear: () => void;
}

const ToastContext = createContext<ToastControl | null>(null);

export function useToast(): ToastControl {
  const control = useContext(ToastContext);
  if (control === null) throw new Error('useToast requires a ToastProvider');
  return control;
}

/** How long a flashed message stays up. */
const SHOW_MS = 1800;

export const ToastProvider: React.FC<{
  /** Pill offset from the bottom of the (position: relative) plugin root. */
  bottom: number;
  children: React.ReactNode;
}> = ({ bottom, children }) => {
  const [message, setMessage] = useState<string | null>(null);
  const timeoutRef = useRef<number | undefined>(undefined);

  // The control is stable, so consumers never re-render with the message;
  // and since a message change re-renders only this provider (the children
  // element identity is unchanged), the tree below skips entirely.
  const control = useMemo<ToastControl>(() => {
    const set = (msg: string | null, dismissMs?: number) => {
      window.clearTimeout(timeoutRef.current);
      setMessage(msg);
      if (msg && dismissMs)
        timeoutRef.current = window.setTimeout(() => setMessage(null), dismissMs);
    };
    return {
      show: (msg) => set(msg, SHOW_MS),
      pin: (msg) => set(msg),
      clear: () => set(null),
    };
  }, []);

  useEffect(() => () => window.clearTimeout(timeoutRef.current), []);

  return (
    <ToastContext.Provider value={control}>
      {children}
      {message !== null && (
        // Keyed by text so a replacement message re-runs the entrance.
        <div
          key={message}
          style={{
            position: 'absolute',
            left: '50%',
            bottom: `${bottom}rem`,
            transform: 'translateX(-50%)',
            backgroundColor: WHITE,
            color: BLACK,
            fontSize: '16rem',
            fontWeight: 700,
            lineHeight: 1,
            padding: '14rem 24rem',
            borderRadius: '16rem',
            whiteSpace: 'nowrap',
            zIndex: 1000,
            pointerEvents: 'none',
            animation: 'toast-in 0.16s ease-out',
          }}
        >
          <style>{`@keyframes toast-in { from { opacity: 0; transform: translate(-50%, 6rem); } }`}</style>
          {message}
        </div>
      )}
    </ToastContext.Provider>
  );
};
