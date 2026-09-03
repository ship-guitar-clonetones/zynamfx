import React, { useCallback, useEffect, useRef, useState } from 'react';
import { useSortable } from '@dnd-kit/react/sortable';
import {
  ArrowLeft,
  ArrowLeftRight,
  ArrowRight,
  ClipboardPaste,
  Copy,
  Ellipsis,
  File,
  FolderClosed,
  PlusCircle,
  Power,
  Trash2,
  Upload,
} from './icons';
import { BlockEnergyBorder, BlockLed } from './BlockLed';
import { ToneImage } from './GearIcon';
import { LoadingDots } from './LoadingDots';
import { RetryLoadBadge } from './RetryLoadBadge';
import { meterId } from '../hooks/useMeters';
import { useChainActions } from '../hooks/useChainActions';
import { HELP, helpProps, toneTileHelp } from './helpText';
import type { ChainSide, ToneBlock } from '../types/chain';
import { ChromeIconButton } from './ChromeIconButton';
import { TileMenu } from './TileMenu';
import type { TileMenuAnchor, TileMenuItem } from './TileMenu';
import type { ChainActions } from '../hooks/useChainActions';
import { useToast } from './Toast';
import { GRAY, ICON_SIZE, SURFACE, SURFACE_RAISED } from './theme';
import { IS_IOS } from '../hooks/useUiScale';

/**
 * Gallery view of a chain block: a square tone image with quick actions
 * (power / swap / trash) overlaid along the top edge and a simplified
 * horizontal output level + clip strip along the bottom. Tap/click opens
 * the detail card; dragging the tile reorders it.
 *
 * While dragging, the tile itself travels with the pointer (dnd-kit's
 * Feedback plugin lifts it out of the lane and leaves a hidden placeholder
 * holding its slot, which reveals the ghost rail behind it), dimmed like the
 * old chain's dragged card.
 */

/** Opacity of the tile while it travels with the pointer. */
const DRAG_GHOST_OPACITY = 0.75;

/** File-drag drop-target chrome (tone tiles + add tile). */
const FILE_DROP_BORDER = '2rem dashed rgba(0, 209, 59, 0.50)';
const ADD_TILE_BORDER_WIDTH = 2;
const ADD_TILE_BORDER = `${ADD_TILE_BORDER_WIDTH}rem dashed rgba(141, 141, 147, 0.65)`;
const FILE_DROP_ICON_SIZE = 36;

const isFileDrag = (e: React.DragEvent) => e.dataTransfer.types.includes('Files');

// The global drop swallow (main.tsx) only stops the webview navigating away;
// accepting a drop also needs every dragover cancelled with the file-copy
// effect, or the OS shows a rejection cursor.
const armFileDrag = (e: React.DragEvent, setArmed: (v: boolean) => void) => {
  if (!isFileDrag(e)) return;
  e.preventDefault();
  e.dataTransfer.dropEffect = 'copy';
  setArmed(true);
};

const disarmFileDrag = (e: React.DragEvent, setArmed: (v: boolean) => void) => {
  // Crossing into a child still fires dragLeave on the parent; ignore those
  // or the upload icon / border flicker as the pointer moves across the tile.
  if (e.currentTarget.contains(e.relatedTarget as Node)) return;
  setArmed(false);
};

/** Keep tile buttons from taking focus on press: the webview scrolls the
    focused element into view, which nudges the whole lane by a pixel. */
const preventFocus = (e: React.MouseEvent) => e.preventDefault();

/** Right-click → tile-local anchor for the tile's action sheet (suppresses
    the OS context menu; macOS ctrl-click lands here too). Ctrl-click also
    fires a synthetic `click` after `contextmenu`; `shouldIgnoreClick`
    swallows that so the tile doesn't navigate away under the menu. */
/** Design-px of travel before a drag engages, so a tap/click stays a click.
    Scaled to real px per gesture so the feel tracks the rendered tile size.
    Consumed by ChainView's PointerSensor and by the long-press slop below. */
export const GALLERY_DRAG_DISTANCE_PX = 6;

/** How long a touch has to be held before releasing it opens the tile menu.
    Matches the lift delay in ChainView's touch activation constraint, so the
    two halves of the same gesture agree: past this point the tile is lifted,
    and letting go without moving is a request for the menu rather than a tap. */
const LONG_PRESS_MS = 250;

/** How far the finger may drift before the long press is abandoned. The lane's
    touch sensor lifts on Delay(250 ms, tolerance 5 real px, see ChainView), so
    the same 5 px is the boundary: drift under it and the hold still stands,
    drift over it and the tile is being dragged, not held, and the menu must
    not fire. Real px, matching the tolerance it mirrors. */
const LONG_PRESS_SLOP_PX = 5;

const useTileMenu = () => {
  const [menuAnchor, setMenuAnchor] = useState<TileMenuAnchor | null>(null);
  const suppressClickRef = useRef(false);
  const openMenu = useCallback((e: React.MouseEvent) => {
    e.preventDefault();
    e.stopPropagation();
    suppressClickRef.current = true;
    // Viewport coords: TileMenu portals to body and positions with
    // position:fixed at these real-px coordinates.
    setMenuAnchor({ clientX: e.clientX, clientY: e.clientY });
  }, []);
  const closeMenu = useCallback(() => setMenuAnchor(null), []);

  /** Open the same sheet from a visible control rather than a gesture.
      HIG: "Always make context menu items available in the main interface,
      too" - a hidden long press must never be the only route. Anchored to the
      button's own bottom-left so the sheet hangs off the chrome that opened
      it, instead of at a pointer position the user never sees on touch. */
  const openMenuAtElement = useCallback((e: React.MouseEvent) => {
    e.preventDefault();
    e.stopPropagation();
    suppressClickRef.current = true;
    const rect = (e.currentTarget as HTMLElement).getBoundingClientRect();
    setMenuAnchor({ clientX: rect.left, clientY: rect.bottom });
  }, []);

  // Touch: the iOS Home screen rule. A hold lifts the tile (dnd-kit's Delay
  // constraint, see ChainView); moving after that reorders and no menu ever
  // appears, while releasing the hold without moving opens the tile's menu at
  // that point. A quick swipe is neither: it scrolls the lane.
  //
  // The menu therefore fires on pointerUP after a hold, not on a timer. Firing
  // on a timer would race the lift: both would trigger at their own moment and
  // the sheet would open over a tile that is already travelling.
  //
  // Gated on pointerType 'touch', so mouse and trackpad behaviour on every
  // desktop platform is untouched.
  const pressStart = useRef<{ x: number; y: number; at: number; id: number } | null>(null);
  const releaseListener = useRef<((e: PointerEvent) => void) | null>(null);

  const cancelLongPress = useCallback(() => {
    pressStart.current = null;
    if (releaseListener.current) {
      window.removeEventListener('pointerup', releaseListener.current, true);
      window.removeEventListener('pointercancel', releaseListener.current, true);
      releaseListener.current = null;
    }
  }, []);

  // The release listener lives on window; a tile can unmount mid-press (undo,
  // a preset load), so drop it on unmount.
  useEffect(() => cancelLongPress, [cancelLongPress]);

  const longPressProps = {
    onPointerDown: (e: React.PointerEvent) => {
      if (!IS_IOS || e.pointerType !== 'touch') return;
      cancelLongPress();
      const start = { x: e.clientX, y: e.clientY, at: Date.now(), id: e.pointerId };
      pressStart.current = start;

      // The release is watched on window, in the capture phase, not on the
      // tile. Once the hold elapses, the sensor lifts the tile and takes
      // pointer capture, after which no pointerup reaches this element at all
      // - the first version of this listened on the tile and the menu simply
      // never opened, while the swallowed click fell through and opened the
      // block's detail view instead.
      const onRelease = (ev: PointerEvent) => {
        const held = pressStart.current;
        if (held != null && ev.pointerId !== held.id) return;
        cancelLongPress();
        if (held == null || ev.type !== 'pointerup') return;
        if (Date.now() - held.at < LONG_PRESS_MS) return;
        if (
          Math.abs(ev.clientX - held.x) > LONG_PRESS_SLOP_PX ||
          Math.abs(ev.clientY - held.y) > LONG_PRESS_SLOP_PX
        )
          return;
        // The release that opens the sheet also fires a click; swallow it
        // exactly as the ctrl-click path does.
        suppressClickRef.current = true;
        setMenuAnchor({ clientX: held.x, clientY: held.y });
      };
      releaseListener.current = onRelease;
      window.addEventListener('pointerup', onRelease, true);
      window.addEventListener('pointercancel', onRelease, true);
    },
    onPointerMove: (e: React.PointerEvent) => {
      const start = pressStart.current;
      if (start == null) return;
      if (
        Math.abs(e.clientX - start.x) > LONG_PRESS_SLOP_PX ||
        Math.abs(e.clientY - start.y) > LONG_PRESS_SLOP_PX
      )
        cancelLongPress();
    },
  };
  /** True when a tile click should be ignored (followed a contextmenu, is a
      modifier-click, or the menu is already open, in which case it closes). */
  const shouldIgnoreClick = useCallback(
    (e: React.MouseEvent) => {
      if (suppressClickRef.current) {
        suppressClickRef.current = false;
        return true;
      }
      if (e.ctrlKey || e.metaKey) return true;
      if (menuAnchor) {
        closeMenu();
        return true;
      }
      return false;
    },
    [menuAnchor, closeMenu]
  );
  return {
    menuAnchor,
    openMenu,
    openMenuAtElement,
    closeMenu,
    shouldIgnoreClick,
    longPressProps,
  };
};

/** The tile menus' native-picker rows (Load File / Load Folder). Local
    loading must not depend on drag-and-drop alone: Linux never delivers OS
    file drags to the embedded webview, so there these rows are the only way
    local files get in. An insert slot adds; a tone tile swaps in place
    (same targeting as a drop). */
const localLoadMenuItems = (
  targetBlockId: string,
  actions: ChainActions,
  toast: ReturnType<typeof useToast>
): TileMenuItem[] => {
  const pick = async (kind: 'file' | 'folder') => {
    const error = await actions.pickLocalFile(targetBlockId, kind);
    if (error) toast.show(error);
  };
  return [
    {
      label: 'Load File',
      icon: <File size={16} />,
      help: HELP.loadFileTile,
      onSelect: () => void pick('file'),
    },
    {
      label: 'Load Folder',
      icon: <FolderClosed size={16} />,
      help: HELP.loadFolderTile,
      onSelect: () => void pick('folder'),
    },
  ];
};

/** Interactive wiring for a tile's chrome. */
interface TileActions {
  onOpen: (e: React.MouseEvent) => void;
  onTogglePower: (e: React.MouseEvent) => void;
  onSwap: (e: React.MouseEvent) => void;
  onRemove: (e: React.MouseEvent) => void;
  /** Retry a failed model download (shown when block.loadFailed). */
  onRetryLoad: () => void;
}

/**
 * The complete tile visual: artwork, loading scrim, top action strip and
 * bottom meter. `dragging` pins the action strip visible while the tile
 * travels with the pointer (hover state can't reach it mid-drag).
 */
const TileSurface: React.FC<{
  block: ToneBlock;
  size: number;
  enabled: boolean;
  dragging: boolean;
  /** OS file drag is hovering this tile (upload icon + dashed green border). */
  dropArmed: boolean;
  actions: TileActions;
  /** Opens the tile's action sheet from the visible "..." chrome button.
      Only supplied on touch platforms; desktop reaches the same sheet with a
      right-click and shows no extra button. */
  onMore?: (e: React.MouseEvent) => void;
}> = ({ block, size, enabled, dragging, dropArmed, actions, onMore }) => {
  const { blockId, tone } = block;

  // A model download/prepare is in flight: `modelLoading` covers switches
  // (where the previous model keeps playing, so `loaded` stays true) and
  // `!loaded` covers fresh blocks that have nothing to play yet.
  const busy = block.modelLoading || (!block.loaded && !block.loadFailed);
  const outMeterId = meterId.blockOut(blockId);

  return (
    // Outer shell stays overflow-visible so inset energy glow isn't needed
    // outside the tile; kept for a stable size box around the face.
    <div
      style={{
        width: `${size}rem`,
        height: `${size}rem`,
        position: 'relative',
        flexShrink: 0,
      }}
    >
      <div
        // Header reveals on :hover via CSS (see index.css), since JS hover state
        // dies across drag re-renders. The traveling tile pins it visible.
        className={dragging ? 'gallery-tile tile-chrome-visible' : 'gallery-tile'}
        onClick={actions.onOpen}
        {...helpProps(toneTileHelp(tone.title))}
        style={{
          width: `${size}rem`,
          height: `${size}rem`,
          borderRadius: '16rem',
          backgroundColor: SURFACE,
          position: 'relative',
          overflow: 'hidden',
          cursor: 'pointer',
          boxSizing: 'border-box',
          border: dropArmed ? FILE_DROP_BORDER : undefined,
          // Pointer devices: drag wins on the tile face, since a mouse has no
          // competing scroll gesture there and lanes still pan from the gaps
          // around it.
          //
          // iOS reverses this deliberately. HIG's reorder gesture is
          // touch-and-hold to lift, then drag, which means a plain swipe over
          // a tile has to scroll the lane like a swipe anywhere else. Allowing
          // pan-x hands quick swipes to the browser (dnd-kit sees
          // pointercancel and stands down), while a 250 ms hold elapses before
          // any pan begins, so the sensor captures the pointer and the drag
          // proceeds. See the activation constraints in ChainView.
          touchAction: IS_IOS ? 'pan-x' : 'none',
        }}
      >
        {dropArmed ? (
          <div
            style={{
              position: 'absolute',
              inset: 0,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
            }}
          >
            <Upload size={FILE_DROP_ICON_SIZE} color={GRAY} />
          </div>
        ) : (
          <div
            style={{
              position: 'absolute',
              inset: 0,
              opacity: enabled && !busy && !block.loadFailed ? 1 : 0.35,
              transition: 'opacity 0.2s ease',
            }}
          >
            <ToneImage
              src={tone.images?.[0]}
              alt={tone.title}
              gear={tone.gear}
              local={tone.local}
              boxSize={size}
              iconSize={64}
              draggable={false}
            />
          </div>
        )}

        {/* Busy dots while the model downloads natively; if the download
            failed, a retry affordance instead (dots would spin forever). */}
        {!dropArmed && (busy || block.loadFailed) && (
          <div
            style={{
              position: 'absolute',
              inset: 0,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              // Clicks pass through to the tile except on the retry button.
              pointerEvents: 'none',
            }}
          >
            {block.loadFailed ? (
              <div style={{ pointerEvents: 'auto' }}>
                <RetryLoadBadge onRetry={actions.onRetryLoad} />
              </div>
            ) : (
              <LoadingDots />
            )}
          </div>
        )}

        {/* Translucent strip under the quick actions so they read on any art.
            Fades in with the header (opacity only, never a layout change). */}
        {!dropArmed && (
          <div
            className="tile-chrome"
            style={{
              position: 'absolute',
              top: 0,
              left: 0,
              right: 0,
              height: '32rem',
              background: 'rgba(0, 0, 0, 0.35)',
              pointerEvents: 'none',
            }}
          />
        )}

        {/* Top quick-action bar (hover-revealed): power on the left, swap and
            trash clustered on the right. */}
        {!dropArmed && (
          <div
            className="tile-chrome"
            style={{
              position: 'absolute',
              top: 0,
              left: 0,
              right: 0,
              display: 'flex',
              flexDirection: 'row',
              alignItems: 'center',
              justifyContent: 'space-between',
              padding: '4rem',
            }}
          >
            <ChromeIconButton
              tone="power"
              on={enabled}
              help={HELP.blockPower}
              onClick={actions.onTogglePower}
              onMouseDown={preventFocus}
            >
              <Power size={ICON_SIZE} />
            </ChromeIconButton>
            <div style={{ display: 'flex', gap: '16rem' }}>
              {onMore && (
                <ChromeIconButton help={HELP.tileMenu} onClick={onMore} onMouseDown={preventFocus}>
                  <Ellipsis size={ICON_SIZE} />
                </ChromeIconButton>
              )}
              <ChromeIconButton
                help={HELP.swapTone}
                onClick={actions.onSwap}
                onMouseDown={preventFocus}
              >
                <ArrowLeftRight size={ICON_SIZE} />
              </ChromeIconButton>
              <ChromeIconButton
                help={HELP.removeBlock}
                onClick={actions.onRemove}
                onMouseDown={preventFocus}
              >
                <Trash2 size={ICON_SIZE} />
              </ChromeIconButton>
            </div>
          </div>
        )}

        {/* Clip latch lives outside the overflow:hidden face so it stacks
            above the inset glow; red dot only while clipped. */}
      </div>

      {!dropArmed && <BlockEnergyBorder meterId={outMeterId} borderRadius={16} />}
      {!dropArmed && (
        <div style={{ position: 'absolute', bottom: '8rem', right: '8rem', zIndex: 4 }}>
          <BlockLed meterId={outMeterId} size={10} />
        </div>
      )}
    </div>
  );
};

interface GalleryBlockProps {
  block: ToneBlock;
  /** Position within the lane; keeps the sortable registry in sync. */
  index: number;
  /** The lane this tile sorts in. */
  group: ChainSide;
  /** The lane's full order, as ids. Backs the menu's Move left / Move right,
      the visible alternative to the drag gesture (HIG: never only a gesture). */
  laneIds: string[];
  /** Tile edge, px. */
  size: number;
  /** Open the detail takeover for this block. */
  onOpen: (blockId: string) => void;
}

/** Memoized so a lane re-render (e.g. another tile's optimistic state) only
    reaches tiles whose block snapshot actually changed. Mutations come from
    the ChainActions context, so there are no per-render callback props to
    defeat the memo. */
export const GalleryBlock: React.FC<GalleryBlockProps> = React.memo(
  ({ block, index, group, size, onOpen, laneIds }) => {
    const { blockId, params } = block;
    const actions = useChainActions();
    const toast = useToast();
    const {
      menuAnchor,
      openMenu,
      openMenuAtElement,
      closeMenu,
      shouldIgnoreClick,
      longPressProps,
    } = useTileMenu();

    // Optimistic power state; native converges via the chainChanged resync
    // (same pattern as the detail card).
    const [enabled, setEnabled] = useState(params.enabled);
    useEffect(() => setEnabled(params.enabled), [params.enabled]);
    // True while an OS file drag hovers the tile (upload icon + green dash).
    const [dropArmed, setDropArmed] = useState(false);

    const { ref, isDragging } = useSortable({ id: blockId, index, group });

    /** Swap this tile with its neighbour, committing the whole lane order the
        same way a drop does so undo covers it. */
    const moveBy = useCallback(
      (delta: -1 | 1) => {
        const to = index + delta;
        if (to < 0 || to >= laneIds.length) return;
        const next = [...laneIds];
        [next[index], next[to]] = [next[to], next[index]];
        actions.reorderBlocks(next);
      },
      [actions, index, laneIds]
    );

    const handleTogglePower = useCallback(
      (e: React.MouseEvent) => {
        e.stopPropagation();
        setEnabled((prev) => {
          actions.setBlockParam(blockId, 'enabled', !prev);
          return !prev;
        });
      },
      [actions, blockId]
    );

    const handleDrop = async (e: React.DragEvent) => {
      e.preventDefault();
      setDropArmed(false);
      const item = e.dataTransfer.items[0];
      if (!item) return;
      const error = await actions.loadLocalFile(blockId, item);
      if (error) toast.show(error);
    };

    return (
      <div
        ref={ref}
        onContextMenu={openMenu}
        {...longPressProps}
        onDragOver={(e) => armFileDrag(e, setDropArmed)}
        onDragLeave={(e) => disarmFileDrag(e, setDropArmed)}
        onDrop={handleDrop}
        style={{
          // Dim the tile while it travels with the pointer; the hidden
          // placeholder dnd-kit leaves in the lane reveals the plus-circle
          // rail behind the vacated slot.
          opacity: isDragging ? DRAG_GHOST_OPACITY : 1,
          flexShrink: 0,
          position: 'relative',
          // Above the neighboring tiles while the action sheet is up.
          zIndex: menuAnchor ? 5 : undefined,
        }}
      >
        <TileSurface
          onMore={IS_IOS ? openMenuAtElement : undefined}
          block={block}
          size={size}
          enabled={enabled}
          dragging={isDragging}
          dropArmed={dropArmed}
          actions={{
            onOpen: (e) => {
              if (shouldIgnoreClick(e)) return;
              onOpen(blockId);
            },
            onTogglePower: handleTogglePower,
            onSwap: (e) => {
              e.stopPropagation();
              actions.swapBlock(blockId);
            },
            onRemove: (e) => {
              e.stopPropagation();
              actions.removeBlock(blockId);
            },
            onRetryLoad: () => actions.retryLoad(blockId),
          }}
        />
        {menuAnchor && (
          <TileMenu
            anchor={menuAnchor}
            onClose={closeMenu}
            items={[
              {
                label: 'Copy',
                icon: <Copy size={16} />,
                help: HELP.copyBlock,
                onSelect: () => actions.copyBlock(blockId),
              },
              // Visible alternative to the drag gesture, per the HIG rule
              // that a gesture is never the only route. Hidden at the ends of
              // the lane rather than dimmed, like every other unavailable row
              // on touch. Reordering rides the same reorderBlocks action the
              // drag commits, so it lands in undo history identically.
              ...(IS_IOS
                ? ([
                    {
                      label: 'Move left',
                      icon: <ArrowLeft size={16} />,
                      help: HELP.moveBlockLeft,
                      disabled: index <= 0,
                      onSelect: () => moveBy(-1),
                    },
                    {
                      label: 'Move right',
                      icon: <ArrowRight size={16} />,
                      help: HELP.moveBlockRight,
                      disabled: index >= laneIds.length - 1,
                      onSelect: () => moveBy(1),
                    },
                  ] as TileMenuItem[])
                : []),
              ...localLoadMenuItems(blockId, actions, toast),
              // Destructive row last and red, per the HIG's context-menu
              // shape. Touch only: on desktop the trash button in the tile
              // chrome is always a hover away, and adding a row here would
              // change a menu every existing user knows.
              ...(IS_IOS
                ? [
                    {
                      label: 'Remove',
                      icon: <Trash2 size={16} />,
                      help: HELP.removeBlock,
                      destructive: true,
                      onSelect: () => actions.removeBlock(blockId),
                    },
                  ]
                : []),
            ]}
          />
        )}
      </div>
    );
  }
);
GalleryBlock.displayName = 'GalleryBlock';

/** Plus glyph: 48 on mono tiles (224), 40 on stereo (160). Half of that is
    the radius the routing lines run edge-to-circle against. */
export const plusIconSize = (tileSize: number) => (tileSize <= 160 ? 40 : 48);

/** Lucide's circle-plus draws its circle at r=10 inside the 24-unit viewBox,
    so the visible ring sits 2/24 of the rendered size in from the icon's
    bounding box (measured to the stroke's centerline). Connector lines must
    overshoot the box by this much to actually meet the ring; stopping half a
    stroke short (at the box edge) reads as a hairline gap. */
export const plusCircleInset = (iconSize: number) => (iconSize * 2) / 24;

/** Which tile edges get a routing line into the plus circle (signal-flow
    continuation of the lane's connector lines). */
export type AddTileRouting = 'left' | 'right' | 'both' | 'none';

/** Face of the insert slot tile. */
const addTileFaceStyle = (size: number): React.CSSProperties => ({
  width: `${size}rem`,
  height: `${size}rem`,
  borderRadius: '16rem',
  backgroundColor: SURFACE_RAISED,
  border: ADD_TILE_BORDER,
  position: 'relative',
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'center',
  color: '#ffffff',
  flexShrink: 0,
  boxSizing: 'border-box',
});

interface AddTileProps {
  /** Insert slot block id. */
  id: string;
  /** Position within the lane; keeps the sortable registry in sync. */
  index: number;
  /** The lane this tile sorts in. */
  group: ChainSide;
  size: number;
  routing: AddTileRouting;
  onClick: () => void;
  /** Paste the copied block into this slot; null while there's nothing valid
      to paste (the action sheet shows Paste disabled). */
  onPaste?: (() => void) | null;
}

/** The insert slot as a dashed add tile, sortable so the insert point can be
    repositioned within its lane, like any other block. Routing lines continue
    the lane's connector line through to the plus circle. Also the drop zone
    for local .nam / IR .wav files (loaded natively, no browser flow). */
export const AddTile: React.FC<AddTileProps> = ({
  id,
  index,
  group,
  size,
  routing,
  onClick,
  onPaste = null,
}) => {
  const { menuAnchor, openMenu, closeMenu, shouldIgnoreClick, longPressProps } = useTileMenu();
  const actions = useChainActions();
  const toast = useToast();
  // True while an OS file drag hovers the tile (drop-target highlight).
  const [dropArmed, setDropArmed] = useState(false);
  const { ref, isDragging } = useSortable({ id, index, group });

  const handleDrop = async (e: React.DragEvent) => {
    e.preventDefault();
    setDropArmed(false);
    // The item (not files[0]): folders only surface through the entry API.
    const item = e.dataTransfer.items[0];
    if (!item) return;
    const error = await actions.loadLocalFile(id, item);
    if (error) toast.show(error);
  };

  // Anchored inside the tile's border (absolute children position against
  // the padding box), so the run to the plus ring is a border-width shorter
  // than measured from the tile's outer edge.
  const routingLine = (edge: 'left' | 'right') => (
    <div
      style={{
        position: 'absolute',
        top: '50%',
        [edge]: 0,
        width: `${size / 2 - plusIconSize(size) / 2 + plusCircleInset(plusIconSize(size)) - ADD_TILE_BORDER_WIDTH}rem`,
        height: '2rem',
        backgroundColor: '#ffffff',
        transform: 'translateY(-50%)',
      }}
    />
  );

  return (
    <div
      ref={ref}
      onClick={(e) => {
        if (shouldIgnoreClick(e)) return;
        onClick();
      }}
      onContextMenu={openMenu}
      {...longPressProps}
      onDragOver={(e) => armFileDrag(e, setDropArmed)}
      onDragLeave={(e) => disarmFileDrag(e, setDropArmed)}
      onDrop={handleDrop}
      {...helpProps(HELP.addTile)}
      style={{
        ...addTileFaceStyle(size),
        ...(dropArmed ? { border: FILE_DROP_BORDER } : {}),
        opacity: isDragging ? DRAG_GHOST_OPACITY : 1,
        cursor: 'pointer',
        // Touch drags need the gesture (see the tone tile face).
        touchAction: 'none',
        // Above the neighboring tiles while the action sheet is up.
        zIndex: menuAnchor ? 5 : undefined,
      }}
    >
      {!dropArmed &&
        !isDragging &&
        (routing === 'left' || routing === 'both') &&
        routingLine('left')}
      {!dropArmed &&
        !isDragging &&
        (routing === 'right' || routing === 'both') &&
        routingLine('right')}
      {dropArmed ? (
        <Upload size={FILE_DROP_ICON_SIZE} color={GRAY} />
      ) : (
        <PlusCircle size={plusIconSize(size)} strokeWidth={1} />
      )}
      {menuAnchor && (
        <TileMenu
          anchor={menuAnchor}
          onClose={closeMenu}
          items={[
            {
              label: 'Paste',
              icon: <ClipboardPaste size={16} />,
              help: HELP.pasteBlock,
              disabled: onPaste == null,
              onSelect: () => onPaste?.(),
            },
            ...localLoadMenuItems(id, actions, toast),
          ]}
        />
      )}
    </div>
  );
};
