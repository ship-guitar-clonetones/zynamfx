import { useCallback, type RefCallback } from 'react';

/**
 * Pan a horizontal-only scroller with a plain vertical wheel.
 *
 * Browsers never remap a vertical wheel onto a sideways-only container;
 * native horizontal scrolling needs Shift+wheel or hardware that emits real
 * horizontal deltas (trackpads, tilt wheels). Mac users all have such
 * hardware, but the standard Windows mouse does not, and the app hides
 * scrollbars, so the chain gallery and gear filter row were wheel-dead on
 * Windows (issue #26). Events with dominant horizontal deltas keep their
 * native handling, and a scroller pushed past either end releases the event
 * to its ancestors, like native scroll chaining (the gear filter row sits in
 * a vertically scrolling page).
 *
 * Returns a callback ref (the chain gallery remounts its scroller around the
 * block-detail takeover, so a mount-once effect would go stale) and attaches
 * the listener natively and non-passive: React registers wheel listeners as
 * passive, which forbids the preventDefault this needs.
 */
export function useHorizontalWheelScroll<T extends HTMLElement>(): RefCallback<T> {
  return useCallback((el: T | null) => {
    if (el == null) return;
    const onWheel = (e: WheelEvent) => {
      if (Math.abs(e.deltaY) <= Math.abs(e.deltaX)) return; // native horizontal input
      const max = el.scrollWidth - el.clientWidth;
      if (max <= 0) return; // everything fits
      // Chromium and WebKit always report pixel deltas; line mode appears
      // only in plain-browser dev (Firefox), where a wheel notch is 3 lines.
      const step = e.deltaMode === WheelEvent.DOM_DELTA_LINE ? e.deltaY * 40 : e.deltaY;
      // At either end, hand the event back to ancestors (scroll chaining).
      // 1px of slack: scroll positions go fractional at non-integer UI scales.
      if (step < 0 ? el.scrollLeft <= 1 : el.scrollLeft >= max - 1) return;
      e.preventDefault();
      el.scrollLeft += step;
    };
    el.addEventListener('wheel', onWheel, { passive: false });
    // Ref cleanup, also run between StrictMode's double-attach in dev.
    return () => el.removeEventListener('wheel', onWheel);
  }, []);
}
