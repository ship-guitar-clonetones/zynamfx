import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import { DESIGN_WIDTH, DESIGN_HEIGHT } from './useUiScale';
import { BANNER_HEIGHT, type AppBannerSpec } from '../components/AppBanner';
import { HINT_HEIGHT } from '../components/HintBar';

/** Duration of the banner slide (wrapper + root height animate in lockstep). */
export const BANNER_ANIM_MS = 180;

/**
 * Phase machine choreographing the top banner with the native window resize.
 *
 * The chrome strips grow the window instead of squishing the 578px core UI,
 * which means three systems have to agree on the height: the React layout,
 * the native window (setExtraContentHeight -> JUCE setSize, async across the
 * bridge), and the UI scale (recomputed from the viewport). Naively mounting
 * the banner shifts everything down a frame before the window grows; this
 * hook orders the steps so existing content never jumps:
 *
 *   hidden --banner appears--> waiting: window grows first (the new space is
 *     at the bottom edge, black-on-black, invisible; content stays put)
 *   waiting --viewport grew (or timeout)--> entering: banner slides down into
 *     place over BANNER_ANIM_MS; the root height animates in lockstep so the
 *     flex middle keeps its exact size throughout
 *   entering --> shown (animation done; transitions disabled again)
 *   shown --banner clears--> leaving: reverse slide (last spec kept rendered)
 *   leaving --> hidden: unmount, then the window shrinks back
 *
 * The bottom strip (hint bar) needs no phases: the window's bottom edge is
 * where it appears, so mounting it in the same commit as the height report
 * just means it's briefly clipped below the edge until the window grows
 * (grow), or the gap it leaves is black-on-black until the window shrinks
 * (shrink). Content above it never moves either way.
 *
 * The height report itself runs in useLayoutEffect (before paint) and passes
 * the session-persistent portion (the hint bar) separately, so native can
 * pre-size the next session's window for the first paint.
 */
type BannerPhase = 'hidden' | 'waiting' | 'entering' | 'shown' | 'leaving';

export interface ChromeChoreography {
  /** Banner to render (the last spec is kept alive during the exit slide). */
  renderedBanner: AppBannerSpec | null;
  /** Target height of the banner wrapper (0 or BANNER_HEIGHT). */
  bannerSlotHeight: number;
  /** True while the slide runs: enables the height transitions on the
      wrapper and the root so they move in lockstep. */
  animating: boolean;
  /** Extra design-space height the root div should add to DESIGN_HEIGHT. */
  rootExtraHeight: number;
}

export function useChromeChoreography(
  banner: AppBannerSpec | null,
  hintsVisible: boolean,
  setExtraContentHeight: (total: number, persistent: number) => Promise<unknown>
): ChromeChoreography {
  const [phase, setPhase] = useState<BannerPhase>('hidden');
  // Freshest non-null spec, kept so the exit slide has content to show. A
  // ref, deliberately: useAppBanner builds a fresh spec object every render,
  // so holding it in state (set from an effect) would re-render forever.
  const lastBannerRef = useRef<AppBannerSpec | null>(null);
  useLayoutEffect(() => {
    if (banner) lastBannerRef.current = banner;
  });

  const hintExtra = hintsVisible ? HINT_HEIGHT : 0;
  // The window keeps the banner's space through the whole exit slide; it only
  // shrinks back once the strip is gone ('hidden').
  const windowExtra = (phase !== 'hidden' ? BANNER_HEIGHT : 0) + hintExtra;

  // Before paint, so the resize request is in flight by the time the commit
  // shows. The second argument is the portion that persists across sessions.
  useLayoutEffect(() => {
    void setExtraContentHeight(windowExtra, hintExtra);
  }, [windowExtra, hintExtra, setExtraContentHeight]);

  // Drive the phase machine off banner *presence* only: the spec object has
  // a fresh identity every render, and depending on it would loop.
  const bannerPresent = banner !== null;
  useLayoutEffect(() => {
    if (bannerPresent) {
      // From 'leaving' the window still has the banner's space, so re-enter
      // directly; from cold the window has to grow first.
      if (phase === 'hidden') setPhase('waiting');
      else if (phase === 'leaving') setPhase('entering');
    } else {
      if (phase === 'waiting') setPhase('hidden');
      else if (phase === 'entering' || phase === 'shown') setPhase('leaving');
    }
  }, [bannerPresent, phase]);

  // waiting -> entering once the window has actually grown, so the slide
  // plays into space that exists. The expected height mirrors the native
  // aspect lock: viewport = design box * the width-derived scale.
  useEffect(() => {
    if (phase !== 'waiting') return;
    const target = DESIGN_HEIGHT + BANNER_HEIGHT + hintExtra;
    const check = () => {
      const el = document.documentElement;
      const scale = Math.max(1, el.clientWidth / DESIGN_WIDTH);
      // 2px tolerance for setSize/scale rounding.
      if (el.clientHeight >= target * scale - 2) setPhase('entering');
    };
    check();
    const observer = new ResizeObserver(check);
    observer.observe(document.documentElement);
    // A host may refuse or delay the resize; slide anyway after a beat (the
    // webview scrolls in that case).
    const timeout = window.setTimeout(() => setPhase('entering'), 400);
    return () => {
      observer.disconnect();
      window.clearTimeout(timeout);
    };
  }, [phase, hintExtra]);

  // Let the slide finish, then settle (transitions off in the steady states).
  useEffect(() => {
    if (phase !== 'entering' && phase !== 'leaving') return;
    const timeout = window.setTimeout(
      () => setPhase((current) => (current === 'entering' ? 'shown' : current === 'leaving' ? 'hidden' : current)),
      BANNER_ANIM_MS + 40
    );
    return () => window.clearTimeout(timeout);
  }, [phase]);

  const bannerSlotHeight = phase === 'entering' || phase === 'shown' ? BANNER_HEIGHT : 0;
  return {
    // While 'shown', spec changes (rule swaps) render directly; during exit
    // the cached spec keeps the content alive under the reverse slide.
    renderedBanner: phase === 'hidden' ? null : (banner ?? lastBannerRef.current),
    bannerSlotHeight,
    animating: phase === 'entering' || phase === 'leaving',
    rootExtraHeight: bannerSlotHeight + hintExtra,
  };
}
