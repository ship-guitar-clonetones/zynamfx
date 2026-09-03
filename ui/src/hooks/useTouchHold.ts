import { useCallback, useEffect, useRef } from 'react';
import type { MouseEvent, PointerEvent } from 'react';
import { getUiScale, IS_IOS } from './useUiScale';

/**
 * Touch and hold on a region that answers a right-click on desktop.
 *
 * WKWebView does not deliver a `contextmenu` event for a long press, so every
 * desktop affordance built on `onContextMenu` is simply unreachable on iOS.
 * This hook is the touch half: it spreads onto the same element, fires the
 * same callback, and renders nothing.
 *
 * It is deliberately not the gallery tile's gesture. A tile competes with
 * dnd-kit's lift and with lane scrolling, so there the menu has to wait for
 * the release (see GalleryBlock). Nothing competes here, so the hold fires on
 * its own timer, which is what the platform does and what feels immediate.
 *
 * Gated on `pointerType === 'touch'` and on iOS, so mouse, pen and every
 * desktop build behave exactly as before.
 */

/** Matches the system's own touch-and-hold delay for a context menu. */
const HOLD_MS = 500;
/** Design px of travel that turns the hold into a drag (a knob under the
    finger owns the gesture from that point on). */
const HOLD_SLOP_DESIGN_PX = 8;

export interface TouchHoldProps {
  onPointerDown?: (e: PointerEvent) => void;
  onPointerMove?: (e: PointerEvent) => void;
  onPointerUp?: (e: PointerEvent) => void;
  onPointerCancel?: (e: PointerEvent) => void;
  onClickCapture?: (e: MouseEvent) => void;
}

export const useTouchHold = (onHold: () => void): TouchHoldProps => {
  const timer = useRef<number | undefined>(undefined);
  const start = useRef<{ x: number; y: number; id: number } | null>(null);
  // A hold that fired must not also be read as a tap by whatever sits under
  // the finger (the Spread/Align advert button enables the feature on click).
  const suppressClick = useRef(false);
  const onHoldRef = useRef(onHold);
  onHoldRef.current = onHold;

  const cancel = useCallback(() => {
    if (timer.current !== undefined) window.clearTimeout(timer.current);
    timer.current = undefined;
    start.current = null;
  }, []);

  useEffect(() => cancel, [cancel]);

  if (!IS_IOS) return {};

  return {
    onPointerDown: (e: PointerEvent) => {
      if (e.pointerType !== 'touch') return;
      // A hold on an icon-only control belongs to that control: the section
      // power button, and everything in the panel this opens. ponytail: "no
      // text" is the discriminator because the advert button is the only
      // labelled control in the group, and it must stay holdable since it is
      // the whole group while the feature is off. If a labelled control is
      // ever added there, mark the ones to skip instead.
      const control =
        e.target instanceof Element ? e.target.closest('button, input, [role="button"]') : null;
      if (control && !control.textContent?.trim()) return;
      cancel();
      start.current = { x: e.clientX, y: e.clientY, id: e.pointerId };
      timer.current = window.setTimeout(() => {
        cancel();
        suppressClick.current = true;
        onHoldRef.current();
      }, HOLD_MS);
    },
    onPointerMove: (e: PointerEvent) => {
      const from = start.current;
      if (from == null || e.pointerId !== from.id) return;
      const slop = HOLD_SLOP_DESIGN_PX * getUiScale();
      if (Math.abs(e.clientX - from.x) > slop || Math.abs(e.clientY - from.y) > slop) cancel();
    },
    onPointerUp: cancel,
    onPointerCancel: cancel,
    onClickCapture: (e: MouseEvent) => {
      if (!suppressClick.current) return;
      suppressClick.current = false;
      e.preventDefault();
      e.stopPropagation();
    },
  };
};
