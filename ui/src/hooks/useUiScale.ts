import { useLayoutEffect } from 'react';

/** Design-space width of the plugin UI. Native sizes the window to this
 * times the user's scale factor (see TONE3000Editor in plugin/include). */
export const DESIGN_WIDTH = 1024;
/** Design-space height of the plugin UI (content only). Figma's 600px
 * artboard includes a 22px mock OS title bar outside JUCE setSize. */
export const DESIGN_HEIGHT = 578;

/** Current UI scale (real viewport px per design px). Grow-only, clamped at
 * 1x, matching the native size floor. */
export const getUiScale = (): number =>
  Math.max(1, document.documentElement.clientWidth / DESIGN_WIDTH);

/** A design-space length as a CSS value. The root font-size is `scale`px
 * (useUiScale), so `1rem` is exactly one design px at the current scale:
 * rem(13) at 1.5x renders 19.5 real px. */
export const rem = (designPx: number): string => `${designPx}rem`;

/**
 * Proportional UI scaling: sets the root font-size to `scale`px (with
 * scale = viewportWidth / 1024) so every rem-denominated length in the UI
 * (knobs, fonts, spacing — the whole design space is authored in rem, one
 * rem per design px) grows with the window. Unlike the CSS `zoom` this
 * replaced, layout runs directly at the real size: text and SVG rasterize
 * crisp, scrolling composites normally, and pointer coordinates, element
 * rects, and position:fixed all agree in real viewport px on every engine
 * (no legacy-zoom pointer patching, no portal special cases).
 *
 * The scale follows the *actual* viewport, never a requested size: a host
 * that refuses a resize leaves the page width (and therefore the scale)
 * untouched.
 *
 * Deliberately imperative (no React state): a live window drag retunes the
 * font-size every frame without re-rendering the tree. ResizeObserver on
 * <html> rather than window `resize`, which fires inconsistently across
 * WebView2 / WKWebView / WebKitGTK. clientWidth is unaffected by font-size,
 * so no feedback loop is possible.
 *
 * External pages (the TONE3000 select flow) replace this document entirely
 * and render with their own styles, using the larger viewport responsively.
 */
export function useUiScale(): void {
  useLayoutEffect(() => {
    const apply = () => {
      document.documentElement.style.setProperty('font-size', `${getUiScale()}px`);
    };

    apply();
    const observer = new ResizeObserver(apply);
    observer.observe(document.documentElement);
    return () => observer.disconnect();
  }, []);
}
