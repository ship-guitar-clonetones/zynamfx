import React, { useEffect, useRef } from 'react';
import { createPortal } from 'react-dom';
import { helpProps } from './helpText';
import { useDismissable } from '../hooks/useDismissable';
import { BORDER, BRAND_RED, DISABLED_OPACITY, HIGHLIGHT, MUTED, WHITE } from './theme';
import { IS_IOS } from '../hooks/useUiScale';

/**
 * Right-click action sheet for gallery tiles, in the house floating-panel
 * style (see the faceplate's input-mode menu): #141416 panel, hairline
 * border, 14px radius, icon + label rows with the shared hover highlight.
 *
 * Portaled to document.body with position:fixed at the click's viewport
 * coords (numeric left/top = real px; the rem-denominated sizes scale with
 * the UI like everything else). Dismissed on outside press, Escape, or
 * picking a row.
 */

export interface TileMenuItem {
  label: string;
  icon: React.ReactNode;
  /** One-line hint for the faceplate help readout. */
  help: string;
  disabled?: boolean;
  /** Destructive action (Remove). Rendered in the brand red and, by the
      caller's ordering, last in the sheet - the HIG's shape for a context
      menu's dangerous row. */
  destructive?: boolean;
  onSelect: () => void;
}

/** Click point in viewport (client) coordinates. */
export interface TileMenuAnchor {
  clientX: number;
  clientY: number;
}

const MENU_WIDTH = 148;
const PANEL_PADDING = 6;
/** Visual-px nudge so the panel's top-left sits clearly past the cursor tip. */
const CURSOR_OFFSET = 6;

export const TileMenu: React.FC<{
  anchor: TileMenuAnchor;
  items: TileMenuItem[];
  onClose: () => void;
}> = ({ anchor, items, onClose }) => {
  const rootRef = useRef<HTMLDivElement>(null);

  useDismissable(true, rootRef, onClose);

  // A resize reflows the content under the fixed menu: just dismiss;
  // keeping it at the old client point would look wrong anyway.
  useEffect(() => {
    window.addEventListener('resize', onClose);
    return () => window.removeEventListener('resize', onClose);
  }, [onClose]);

  // On iOS every row can be filtered out (an empty slot with nothing copied):
  // an empty panel is worse than no panel.
  const shown = items.filter((item) => !(IS_IOS && item.disabled));
  if (shown.length === 0) return null;

  return createPortal(
    <div
      ref={rootRef}
      // Keep every gesture inside the panel: clicks must not open the tile's
      // detail view, presses must not arm a drag under the menu.
      onClick={(e) => e.stopPropagation()}
      onPointerDown={(e) => e.stopPropagation()}
      onContextMenu={(e) => {
        e.preventDefault();
        e.stopPropagation();
      }}
      style={{
        // Fixed to the viewport at the click point: pointer coords are real
        // px, so left/top stay numeric (px), never rem.
        position: 'fixed',
        left: anchor.clientX + CURSOR_OFFSET,
        top: anchor.clientY + CURSOR_OFFSET,
        width: `${MENU_WIDTH}rem`,
        backgroundColor: '#141416',
        border: BORDER,
        borderRadius: '14rem',
        padding: `${PANEL_PADDING}rem`,
        zIndex: 1000,
        boxSizing: 'border-box',
      }}
    >
      <style>{`.tile-menu-item:hover:not(:disabled) { background-color: ${HIGHLIGHT}; }`}</style>
      {/* HIG: "unavailable items are hidden, not dimmed" in a touch context
          menu, where there is no hover to explain why a row is grey and the
          sheet should stay as short as possible. Desktop keeps the dimmed
          row, which is the platform convention there and the behaviour this
          menu has always had. */}
      {shown.map((item) => (
        <button
          key={item.label}
          type="button"
          className="tile-menu-item"
          disabled={item.disabled}
          {...helpProps(item.help)}
          onClick={() => {
            onClose();
            item.onSelect();
          }}
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '12rem',
            width: '100%',
            padding: '9rem 12rem',
            background: 'transparent',
            border: 'none',
            borderRadius: '8rem',
            color: item.disabled ? MUTED : item.destructive ? BRAND_RED : WHITE,
            opacity: item.disabled ? DISABLED_OPACITY : 1,
            fontSize: '13rem',
            fontWeight: 400,
            textAlign: 'left',
            cursor: item.disabled ? 'not-allowed' : 'pointer',
            whiteSpace: 'nowrap',
            boxSizing: 'border-box',
          }}
        >
          {item.icon}
          {item.label}
        </button>
      ))}
    </div>,
    document.body
  );
};
