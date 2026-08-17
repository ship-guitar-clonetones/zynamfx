import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  ArrowUpDown,
  Check,
  ChevronLeft,
  ChevronRight,
  GripVertical,
  Pencil,
  Save,
  Search,
  Trash2,
} from 'lucide-react';
import { DragDropProvider } from '@dnd-kit/react';
import { isSortable, useSortable } from '@dnd-kit/react/sortable';
import { arrayMove } from '@dnd-kit/helpers';
import type { DragEndEvent } from '@dnd-kit/react';
import type { ActivePreset, PresetInfo } from '../types/chain';
import { useDismissable } from '../hooks/useDismissable';
import { useToast } from './Toast';
import { HELP, helpProps } from './helpText';
import { BORDER, GRAY } from './theme';

/**
 * Top-bar preset controls: ‹ name › pill + save button, with two anchored
 * panels: the save popover (name + save) and the preset browser (search,
 * user section with inline rename/delete, TONE3000 factory section, and a
 * reorder mode that swaps the row actions for a grip and drag-and-drop).
 *
 * Pure view: the list and all mutations come from usePresets, the active
 * preset rides the chain state poll. Prev/next walk the list in its shown
 * order, which is also what MIDI program-change numbers follow, so a custom
 * order set here changes what PC 1, PC 2… load.
 */

const MUTED = GRAY;
const PANEL_BG = '#141416';

const panelStyle: React.CSSProperties = {
  position: 'absolute',
  top: 'calc(100% + 10px)',
  left: '-8px',
  backgroundColor: PANEL_BG,
  border: BORDER,
  borderRadius: '14px',
  padding: '16px',
  zIndex: 200,
  boxSizing: 'border-box',
};

const inputStyle: React.CSSProperties = {
  width: '100%',
  boxSizing: 'border-box',
  backgroundColor: '#1C1C1E',
  border: BORDER,
  borderRadius: '10px',
  color: '#ffffff',
  fontSize: '13px',
  // Typed text and placeholders are body text: reset the global 600 default.
  fontWeight: 400,
  padding: '9px 12px',
  outline: 'none',
};

const sectionHeaderStyle: React.CSSProperties = {
  color: GRAY,
  fontSize: '14px',
  fontWeight: 700,
  padding: '8px 4px',
};

const iconButtonStyle: React.CSSProperties = {
  background: 'transparent',
  border: 'none',
  color: '#ffffff',
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'center',
  cursor: 'pointer',
  borderRadius: '4px',
  padding: '5px',
};

type PresetGroup = 'factory' | 'user';

interface PresetRowProps {
  preset: PresetInfo;
  index: number;
  group: PresetGroup;
  sortable: boolean;
  isActive: boolean;
  isRenaming: boolean;
  renameValue: string;
  onRenameChange: (value: string) => void;
  onCommitRename: () => void;
  onCancelRename: () => void;
  onLoad: () => void;
  onStartRename: () => void;
  onDelete: () => void;
}

const PresetRow: React.FC<PresetRowProps> = ({
  preset,
  index,
  group,
  sortable,
  isActive,
  isRenaming,
  renameValue,
  onRenameChange,
  onCommitRename,
  onCancelRename,
  onLoad,
  onStartRename,
  onDelete,
}) => {
  const { ref, handleRef, isDragging } = useSortable({
    id: preset.id,
    index,
    group,
    disabled: !sortable,
  });

  return (
    <div
      ref={ref}
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        height: '32px',
        padding: '0 4px',
        opacity: isDragging ? 0.75 : 1,
      }}
    >
      <span style={{ width: '16px', flexShrink: 0, display: 'flex', alignItems: 'center' }}>
        {isActive && <Check size={14} color="#ffffff" />}
      </span>
      {isRenaming ? (
        <input
          autoFocus
          value={renameValue}
          onChange={(e) => onRenameChange(e.target.value)}
          onBlur={onCommitRename}
          onKeyDown={(e) => {
            if (e.key === 'Enter') onCommitRename();
            if (e.key === 'Escape') onCancelRename();
          }}
          style={{ ...inputStyle, padding: '4px 8px', borderRadius: '6px', flex: 1 }}
        />
      ) : (
        <button
          onClick={onLoad}
          style={{
            flex: 1,
            background: 'transparent',
            border: 'none',
            textAlign: 'left',
            color: isActive ? '#ffffff' : MUTED,
            fontSize: '14px',
            fontWeight: 400,
            cursor: 'pointer',
            padding: 0,
            overflow: 'hidden',
            textOverflow: 'ellipsis',
            whiteSpace: 'nowrap',
          }}
        >
          {preset.name}
        </button>
      )}
      {sortable ? (
        <button
          ref={handleRef}
          type="button"
          aria-label="Reorder"
          {...helpProps(HELP.presetDrag)}
          style={{
            ...iconButtonStyle,
            padding: '3px',
            flexShrink: 0,
            cursor: isDragging ? 'grabbing' : 'grab',
            // Keep the list from claiming the gesture on touch.
            touchAction: 'none',
          }}
        >
          <GripVertical size={14} />
        </button>
      ) : (
        !preset.factory &&
        !isRenaming && (
          <>
            <button
              onClick={onStartRename}
              {...helpProps(HELP.presetRename)}
              style={{ ...iconButtonStyle, padding: '3px' }}
            >
              <Pencil size={13} />
            </button>
            <button
              onClick={onDelete}
              {...helpProps(HELP.presetDelete)}
              style={{ ...iconButtonStyle, padding: '3px' }}
            >
              <Trash2 size={13} />
            </button>
          </>
        )
      )}
    </div>
  );
};

interface PresetBarProps {
  active: ActivePreset | null;
  presets: PresetInfo[];
  onSave: (name: string) => Promise<{ id: string; name: string } | null>;
  onLoad: (id: string) => void;
  onRename: (id: string, name: string) => void;
  onDelete: (id: string) => void;
  /** N steps within the preset's section (negative = earlier). */
  onMove: (id: string, delta: number) => void;
}

type OpenPanel = 'none' | 'save' | 'browse';

export const PresetBar: React.FC<PresetBarProps> = ({
  active,
  presets,
  onSave,
  onLoad,
  onRename,
  onDelete,
  onMove,
}) => {
  const [open, setOpen] = useState<OpenPanel>('none');
  const [saveName, setSaveName] = useState('');
  const [search, setSearch] = useState('');
  const [renamingId, setRenamingId] = useState<string | null>(null);
  const [renameValue, setRenameValue] = useState('');
  // Reorder mode: rows swap rename/delete for a grip handle. Drag only
  // makes sense on the full list, so grips hide while a search filter is on.
  const [reordering, setReordering] = useState(false);
  const toast = useToast();
  const containerRef = useRef<HTMLDivElement | null>(null);
  const closePanels = useCallback(() => setOpen('none'), []);
  useDismissable(open !== 'none', containerRef, closePanels);

  // Optimistic order while a drag is in flight / until native list refresh.
  const [ordered, setOrdered] = useState<PresetInfo[] | null>(null);
  const draggingRef = useRef(false);
  useEffect(() => {
    if (!draggingRef.current) setOrdered(null);
  }, [presets]);

  const openSave = useCallback(() => {
    // Prefill with the active user preset's name: saving it again is the
    // one-click "update" path (same name overwrites in place).
    const activeInfo = active ? presets.find((p) => p.id === active.id) : undefined;
    setSaveName(activeInfo && !activeInfo.factory ? activeInfo.name : '');
    setOpen((prev) => (prev === 'save' ? 'none' : 'save'));
  }, [active, presets]);

  const openBrowse = useCallback(() => {
    setSearch('');
    setRenamingId(null);
    setReordering(false);
    setOrdered(null);
    setOpen((prev) => (prev === 'browse' ? 'none' : 'browse'));
  }, []);

  const handleSave = useCallback(async () => {
    const name = saveName.trim();
    if (!name) return;
    const saved = await onSave(name);
    setOpen('none');
    if (saved) toast.show('Preset Saved');
  }, [saveName, onSave, toast]);

  // Prev/next step through the list in order, wrapping at the ends. With no
  // active preset, › starts at the first and ‹ at the last.
  const step = useCallback(
    (direction: 1 | -1) => {
      if (presets.length === 0) return;
      const index = active ? presets.findIndex((p) => p.id === active.id) : -1;
      const next =
        index < 0
          ? direction === 1
            ? 0
            : presets.length - 1
          : (index + direction + presets.length) % presets.length;
      onLoad(presets[next].id);
    },
    [presets, active, onLoad]
  );

  const commitRename = useCallback(() => {
    if (renamingId && renameValue.trim()) onRename(renamingId, renameValue.trim());
    setRenamingId(null);
  }, [renamingId, renameValue, onRename]);

  const filtered = useMemo(() => {
    const list = ordered ?? presets;
    const q = search.trim().toLowerCase();
    return q ? list.filter((p) => p.name.toLowerCase().includes(q)) : list;
  }, [ordered, presets, search]);
  const factoryPresets = filtered.filter((p) => p.factory);
  const userPresets = filtered.filter((p) => !p.factory);

  const chevronStyle: React.CSSProperties = {
    background: 'transparent',
    border: 'none',
    color: presets.length > 0 ? '#ffffff' : MUTED,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    cursor: presets.length > 0 ? 'pointer' : 'default',
    padding: '0 4px',
    alignSelf: 'stretch',
  };

  // Grips only on the full list: a search filter's indices don't match the
  // persisted order, and moves never cross the factory/user boundary.
  const canDrag = reordering && search.trim() === '';

  const handleDragStart = useCallback(() => {
    draggingRef.current = true;
  }, []);

  const handleDragEnd = useCallback(
    (event: DragEndEvent) => {
      draggingRef.current = false;
      const { source } = event.operation;
      if (event.canceled || !isSortable(source)) return;
      if (source.group !== source.initialGroup) return;
      const from = source.initialIndex;
      const to = source.index;
      if (from === to) return;
      const id = String(source.id);
      setOrdered((prev) => {
        const list = prev ?? presets;
        const item = list.find((p) => p.id === id);
        if (!item) return prev;
        const section = list.filter((p) => p.factory === item.factory);
        const others = list.filter((p) => p.factory !== item.factory);
        const moved = arrayMove(section, from, to);
        // Browser order is user section first, then factory.
        return item.factory ? [...others, ...moved] : [...moved, ...others];
      });
      onMove(id, to - from);
    },
    [presets, onMove]
  );

  const renderRow = (preset: PresetInfo, sectionIndex: number, group: PresetGroup) => {
    const isRenaming = renamingId === preset.id;
    return (
      <PresetRow
        key={preset.id}
        preset={preset}
        index={sectionIndex}
        group={group}
        sortable={canDrag}
        isActive={active?.id === preset.id}
        isRenaming={isRenaming}
        renameValue={renameValue}
        onRenameChange={setRenameValue}
        onCommitRename={commitRename}
        onCancelRename={() => setRenamingId(null)}
        onLoad={() => onLoad(preset.id)}
        onStartRename={() => {
          setRenamingId(preset.id);
          setRenameValue(preset.name);
        }}
        onDelete={() => onDelete(preset.id)}
      />
    );
  };

  return (
    <div
      ref={containerRef}
      style={{ position: 'relative', display: 'flex', alignItems: 'center', gap: '8px' }}
    >
      {/* ‹ name › pill */}
      <div
        style={{
          display: 'flex',
          alignItems: 'stretch',
          height: '36px',
          borderRadius: '8px',
          backgroundColor: '#1C1C1E',
          padding: '0 4px',
          flexShrink: 0,
        }}
      >
        <button onClick={() => step(-1)} {...helpProps(HELP.presetPrev)} style={chevronStyle}>
          <ChevronLeft size={14} />
        </button>
        <button
          onClick={openBrowse}
          {...helpProps(HELP.presetBrowse)}
          style={{
            background: 'transparent',
            border: 'none',
            color: active ? '#ffffff' : MUTED,
            fontSize: '14px',
            fontWeight: 400,
            cursor: 'pointer',
            // Constant width so the pill never resizes with the name; long
            // names ellipsize. Full pill height is the click target.
            width: '150px',
            height: '100%',
            lineHeight: '36px',
            textAlign: 'center',
            overflow: 'hidden',
            textOverflow: 'ellipsis',
            whiteSpace: 'nowrap',
            padding: '0 6px',
          }}
        >
          {active?.name ?? 'Presets'}
        </button>
        <button onClick={() => step(1)} {...helpProps(HELP.presetNext)} style={chevronStyle}>
          <ChevronRight size={14} />
        </button>
      </div>

      {/* Save */}
      <button onClick={openSave} {...helpProps(HELP.presetSave)} style={iconButtonStyle}>
        <Save size={18} />
      </button>

      {/* Save popover */}
      {open === 'save' && (
        <div style={{ ...panelStyle, width: '280px' }}>
          <div
            style={{ color: '#ffffff', fontSize: '14px', fontWeight: 600, marginBottom: '12px' }}
          >
            Save Preset
          </div>
          <input
            autoFocus
            value={saveName}
            onChange={(e) => setSaveName(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === 'Enter') handleSave();
            }}
            placeholder="Name"
            style={inputStyle}
          />
          <button
            onClick={handleSave}
            disabled={!saveName.trim()}
            style={{
              width: '100%',
              marginTop: '12px',
              padding: '9px 0',
              borderRadius: '999px',
              border: '1px solid rgba(235, 235, 245, 0.6)',
              background: 'transparent',
              color: saveName.trim() ? '#ffffff' : MUTED,
              fontSize: '13px',
              cursor: saveName.trim() ? 'pointer' : 'default',
            }}
          >
            Save
          </button>
        </div>
      )}

      {/* Preset browser. Top/bottom spacing lives in the scroll content so
          the list clips at the search row and the panel border. */}
      {open === 'browse' && (
        <div
          style={{
            ...panelStyle,
            width: '360px',
            padding: '12px 12px 0',
            overflow: 'hidden',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <div style={{ position: 'relative', flex: 1, minWidth: 0 }}>
              <Search
                size={14}
                color={MUTED}
                style={{
                  position: 'absolute',
                  left: '12px',
                  top: '50%',
                  transform: 'translateY(-50%)',
                }}
              />
              <input
                autoFocus
                value={search}
                onChange={(e) => setSearch(e.target.value)}
                placeholder="Search presets"
                style={{ ...inputStyle, padding: '8px 12px 8px 32px', borderRadius: '10px' }}
              />
            </div>
            {presets.length > 1 && (
              <button
                onClick={() => setReordering((prev) => !prev)}
                aria-pressed={reordering}
                {...helpProps(HELP.presetReorder)}
                style={{
                  ...iconButtonStyle,
                  padding: '7px',
                  flexShrink: 0,
                  color: reordering ? '#ffffff' : MUTED,
                  background: reordering ? 'rgba(255, 255, 255, 0.12)' : 'transparent',
                }}
              >
                <ArrowUpDown size={15} />
              </button>
            )}
          </div>

          <DragDropProvider onDragStart={handleDragStart} onDragEnd={handleDragEnd}>
            <div className="hide-scrollbar" style={{ maxHeight: '362px', overflowY: 'auto' }}>
              <div style={{ padding: '10px 0 12px' }}>
                {userPresets.length > 0 && (
                  <>
                    <div style={sectionHeaderStyle}>Your Presets</div>
                    {userPresets.map((preset, i) => renderRow(preset, i, 'user'))}
                  </>
                )}
                {factoryPresets.length > 0 && (
                  <>
                    <div style={sectionHeaderStyle}>TONE3000</div>
                    {factoryPresets.map((preset, i) => renderRow(preset, i, 'factory'))}
                  </>
                )}
                {filtered.length === 0 && (
                  <div
                    style={{ color: MUTED, fontSize: '13px', fontWeight: 400, padding: '12px 4px' }}
                  >
                    {presets.length === 0
                      ? 'No presets yet. Save one to get started.'
                      : 'No matches.'}
                  </div>
                )}
              </div>
            </div>
          </DragDropProvider>
        </div>
      )}
    </div>
  );
};
