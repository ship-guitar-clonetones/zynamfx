import { useEffect, useRef } from 'react';
import { IS_IOS } from './useUiScale';

/**
 * iPad navigation shortcuts: swipe in from the left edge to go back, swipe
 * down to dismiss a sheet. Both are extras; every screen keeps its visible
 * 44 pt control.
 *
 * Touch events, not pointer events. The page opts into panning
 * (`touch-action: manipulation`), so WKWebView takes the gesture over and
 * ends it with a `pointercancel` reported at 0,0: no usable pointermove and
 * no pointerup. `touchend` is delivered either way and carries the real end
 * position in `changedTouches`.
 *
 * Lengths are CSS px, which at initial-scale=1 are points.
 */
type Swipe = { edge?: boolean; dx?: number; dy?: number };

/** Longest touch that still counts as a swipe. */
const SWIPE_MAX_MS = 600;

const useSwipe = (active: boolean, { edge = false, dx = 0, dy = 0 }: Swipe, onFire: () => void) => {
  const fire = useRef(onFire);
  fire.current = onFire;

  useEffect(() => {
    if (!IS_IOS || !active) return;
    let from: { x: number; y: number; id: number; at: number } | null = null;

    const start = (e: TouchEvent) => {
      const t = e.touches.length === 1 ? e.touches[0] : null;
      from =
        t && (!edge || t.clientX <= 24)
          ? { x: t.clientX, y: t.clientY, id: t.identifier, at: e.timeStamp }
          : null;
    };
    // A cancel is the system taking the touch over (a call banner, an OS edge
    // gesture, a second finger): never a commit.
    const abort = () => {
      from = null;
    };
    const end = (e: TouchEvent) => {
      const at = from;
      from = null;
      const t = at && [...e.changedTouches].find((c) => c.identifier === at.id);
      if (!at || !t) return;
      // A swipe is quick. A long touch that happens to end past the threshold
      // (a knob turn or a slow scroll that started near the bezel) is not one.
      if (e.timeStamp - at.at > SWIPE_MAX_MS) return;
      const mx = t.clientX - at.x;
      const my = t.clientY - at.y;
      if (dx && (mx < dx || Math.abs(my) > dx / 2)) return;
      if (dy && (my < dy || Math.abs(mx) > dy / 2)) return;
      fire.current();
    };

    window.addEventListener('touchstart', start, true);
    window.addEventListener('touchend', end, true);
    window.addEventListener('touchcancel', abort, true);
    return () => {
      window.removeEventListener('touchstart', start, true);
      window.removeEventListener('touchend', end, true);
      window.removeEventListener('touchcancel', abort, true);
    };
  }, [active, edge, dx, dy]);
};

/** Swipe in from the left screen edge: back. */
export const useEdgeSwipeBack = (active: boolean, onBack: () => void) =>
  useSwipe(active, { edge: true, dx: 72 }, onBack);

/** Swipe down anywhere on a sheet: dismiss. ponytail: fires even if the sheet
    body is mid-scroll; neither sheet that uses it (Tuner, Settings) scrolls
    far. If one grows, gate on the scroller being at scrollTop 0. */
export const useSwipeDownDismiss = (active: boolean, onDismiss: () => void) =>
  useSwipe(active, { dy: 96 }, onDismiss);
