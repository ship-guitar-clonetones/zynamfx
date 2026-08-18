import React, { useCallback, useRef, useState } from 'react';
import { ChevronDown, Equal, Power } from 'lucide-react';
import { KnobControl } from './KnobControl';
import { balanceDbScale, gainDbScale, gateDbScale, toneScale } from './knobScale';
import { SpreadGroup } from './SpreadControls';
import { AlignGroup } from './AlignControls';
import { useParameter } from '../hooks/useParameter';
import type { InputMode } from '../types/chain';
import { useAutoMeasure, type AutoMeasureResult } from '../hooks/useAutoMeasure';
import { useDismissable } from '../hooks/useDismissable';
import { HELP, helpProps } from './helpText';
import { ChromeIconButton } from './ChromeIconButton';
import {
  BORDER,
  HIGHLIGHT,
  ICON_BOX_SIZE,
  ICON_BOX_RADIUS,
  ICON_SIZE,
  KNOB_SIZE_PRIMARY,
  KNOB_SIZE_SECONDARY,
  MUTED,
  SUBTLE,
  WHITE,
  faceplateChromeLift,
} from './theme';

/**
 * Bottom faceplate: main input/output gain, gate and the global 3-band tone
 * stack. Gate + tone stack carry power switches (APVTS bool params, so they
 * automate and persist like everything else on the plate).
 *
 * Input: a single level knob (per-channel trims live on the chain blocks).
 * When a real stereo source feeds the plugin, an input-mode button joins it
 * to pick what feeds the chain: both channels, or just L/R mirrored onto
 * both. The mode is chain state (session-persisted, not a preset value;
 * it's I/O routing, not tone).
 *
 * Output gain: the main level knob plus a small balance knob that trims L/R
 * against each other (±12 dB opposing, center = off). The balance knob only
 * appears when the output actually runs stereo (stereo mode, or mono +
 * spread); DSP forces center when inactive so a leftover setting can't skew
 * a mono bus. All values are host parameters, so presets/undo get them for free.
 */

export const PLATE_HEIGHT = 108;
/** Shared faceplate action-button height: center of the secondary knobs. */
const CHROME_LIFT = faceplateChromeLift(KNOB_SIZE_SECONDARY);

const PowerButton: React.FC<{
  on: boolean;
  help: string;
  onClick: () => void;
}> = ({ on, help, onClick }) => (
  <ChromeIconButton tone="power" on={on} help={help} onClick={onClick} offsetY={CHROME_LIFT}>
    <Power size={ICON_SIZE} />
  </ChromeIconButton>
);

/** Two overlapping circles, the classic stereo glyph (lucide has none). */
const StereoIcon: React.FC<{ style?: React.CSSProperties }> = ({ style }) => (
  <svg
    width={17}
    height={10}
    viewBox="0 0 17 10"
    fill="none"
    xmlns="http://www.w3.org/2000/svg"
    style={{ display: 'block', flexShrink: 0, ...style }}
  >
    <circle cx="5" cy="5" r="4.375" stroke="currentColor" strokeWidth="1.25" />
    <circle cx="11.6665" cy="5" r="4.375" stroke="currentColor" strokeWidth="1.25" />
  </svg>
);

const INPUT_MODE_OPTIONS: { mode: InputMode; label: string }[] = [
  { mode: 'stereo', label: 'Stereo (L+R)' },
  { mode: 'left', label: 'Left' },
  { mode: 'right', label: 'Right' },
];

/** The mode's glyph, shared by the trigger and the menu rows. */
const InputModeGlyph: React.FC<{ mode: InputMode }> = ({ mode }) =>
  mode === 'stereo' ? (
    <StereoIcon />
  ) : (
    <span style={{ fontFamily: 'monospace', fontSize: '11px', fontWeight: 700, lineHeight: 1 }}>
      {mode === 'left' ? 'L' : 'R'}
    </span>
  );

/**
 * Input mode: which channels of a stereo source feed the plugin. The
 * trigger (current glyph + down caret) opens a flat floating menu above the
 * plate (same chrome as the Spread advanced panel) listing the three
 * routings. Stereo (the default) shows the two-circle glyph; L/R take only
 * that channel (mirrored onto both) and the trigger keeps the filled
 * "engaged" look so a non-default routing is obvious at a glance.
 */
const InputModeButton: React.FC<{
  mode: InputMode;
  /** A chain branch is active: the chain has a single (mono) source, so the
      "Stereo" routing is unavailable (native enforces the same). */
  branched: boolean;
  onChange: (mode: InputMode) => void;
}> = ({ mode, branched, onChange }) => {
  const [open, setOpen] = useState(false);
  const rootRef = useRef<HTMLDivElement | null>(null);
  const close = useCallback(() => setOpen(false), []);
  useDismissable(open, rootRef, close);
  const options = branched
    ? INPUT_MODE_OPTIONS.filter((option) => option.mode !== 'stereo')
    : INPUT_MODE_OPTIONS;

  return (
    // The chrome lift lives on the wrapper (not the button) so the floating
    // menu anchors to the lifted trigger. The transform makes the wrapper a
    // stacking context, which would trap the menu's z-index under the chain
    // view's edge-fade gradients (zIndex 3 in the root context), so the
    // wrapper itself is elevated while the menu is open.
    <div
      ref={rootRef}
      style={{
        position: 'relative',
        transform: `translateY(${CHROME_LIFT}px)`,
        zIndex: open ? 300 : undefined,
      }}
    >
      <style>{`.input-mode-item:hover { background-color: ${HIGHLIGHT}; }`}</style>
      <button
        type="button"
        onClick={() => setOpen((prev) => !prev)}
        {...helpProps(HELP.inputMode)}
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: '3px',
          height: `${ICON_BOX_SIZE}px`,
          padding: '0 2px',
          boxSizing: 'border-box',
          background: mode !== 'stereo' ? HIGHLIGHT : 'transparent',
          border: '1px solid transparent',
          borderRadius: `${ICON_BOX_RADIUS}px`,
          color: WHITE,
          cursor: 'pointer',
          flexShrink: 0,
        }}
      >
        <span style={{ display: 'grid', placeItems: 'center', width: `${ICON_SIZE + 3}px` }}>
          <InputModeGlyph mode={mode} />
        </span>
        <ChevronDown size={10} style={{ display: 'block', flexShrink: 0, color: MUTED }} />
      </button>

      {open && (
        <div
          style={{
            position: 'absolute',
            bottom: 'calc(100% + 14px)',
            left: 0,
            minWidth: '172px',
            backgroundColor: '#141416',
            border: BORDER,
            borderRadius: '14px',
            padding: '12px 8px 8px',
            zIndex: 200,
            boxSizing: 'border-box',
          }}
        >
          <div
            style={{
              padding: '0 12px 8px',
              fontSize: '11px',
              fontWeight: 600,
              letterSpacing: '0.05em',
              color: SUBTLE,
              whiteSpace: 'nowrap',
            }}
          >
            Input Mode
          </div>
          {options.map((option) => (
            <button
              key={option.mode}
              type="button"
              className="input-mode-item"
              onClick={() => {
                onChange(option.mode);
                setOpen(false);
              }}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                width: '100%',
                padding: '9px 12px',
                background: 'transparent',
                border: 'none',
                borderRadius: '8px',
                color: option.mode === mode ? WHITE : MUTED,
                fontSize: '13px',
                fontWeight: 400,
                textAlign: 'left',
                cursor: 'pointer',
                whiteSpace: 'nowrap',
              }}
            >
              <span
                style={{
                  display: 'grid',
                  placeItems: 'center',
                  width: `${ICON_BOX_SIZE}px`,
                  flexShrink: 0,
                }}
              >
                <InputModeGlyph mode={option.mode} />
              </span>
              {option.label}
            </button>
          ))}
        </div>
      )}
    </div>
  );
};

/**
 * Auto balance: one-shot L/R energy match. Click arms a listening
 * measurement on the native side: play for ~2 s and the measured dB
 * difference is written into the outputBalance parameter (the Bal knob
 * visibly moves). Yellow (listening) while armed; click again to cancel;
 * times out after 15 s of silence.
 */
/** matchedDb is the measured L/R energy diff (positive = left louder), so
    the correction lifts the quieter side by that amount relative. */
const balanceDoneMessage = ({ matchedDb = 0 }: AutoMeasureResult): string => {
  if (Math.abs(matchedDb) < 0.05) return 'Balanced';
  return `Balanced · ${matchedDb > 0 ? 'R' : 'L'} +${Math.abs(matchedDb).toFixed(1)} dB`;
};

const AutoBalanceButton: React.FC = () => {
  const { listening, toggle } = useAutoMeasure(
    'startAutoBalance',
    'cancelAutoBalance',
    'pollAutoBalance',
    balanceDoneMessage
  );
  return (
    <ChromeIconButton
      tone="armed"
      on={listening}
      help={HELP.autoBalance}
      onClick={toggle}
      offsetY={CHROME_LIFT}
    >
      <Equal size={ICON_SIZE} />
    </ChromeIconButton>
  );
};

/**
 * Output gain knob. When the output runs stereo, a smaller balance knob
 * joins it (center = no effect, off-center trims L/R against each other by
 * up to ±12 dB on top of the main level), plus the auto-balance (=) button
 * when two independent chains are running.
 */
const OutputGainKnob: React.FC<{
  stereo: boolean;
  /** Show the auto-balance (=) button next to the balance knob. */
  autoBalance: boolean;
}> = ({ stereo, autoBalance }) => {
  const [level, setLevel, onLevelDrag] = useParameter('outputLevel', 'slider');
  const [balance, setBalance, onBalanceDrag] = useParameter('outputBalance', 'slider');

  // The (=) button sits on the outer edge, keeping Bal next to the main
  // knob: [=][Bal][knob]. Inactive companions stay mounted but invisible so
  // the group's footprint is constant; toggling stereo/spread must not
  // shift the plate (it's laid out with space-between).
  return (
    <div style={{ display: 'flex', flexDirection: 'row', alignItems: 'flex-end', gap: '10px' }}>
      <div style={{ visibility: autoBalance ? 'visible' : 'hidden' }}>
        <AutoBalanceButton />
      </div>
      <div style={{ visibility: stereo ? 'visible' : 'hidden' }}>
        <KnobControl
          label="Bal"
          value={balance}
          onChange={setBalance}
          size={KNOB_SIZE_SECONDARY}
          variant="bipolar"
          thumb="secondary"
          scale={balanceDbScale}
          defaultValue={0.5}
          help={HELP.outputBalance}
          onDragStateChange={onBalanceDrag}
        />
      </div>
      <KnobControl
        label="Output"
        value={level}
        onChange={setLevel}
        size={KNOB_SIZE_PRIMARY}
        scale={gainDbScale}
        defaultValue={0.5}
        help={HELP.outputLevel}
        onDragStateChange={onLevelDrag}
      />
    </div>
  );
};

interface FaceplateProps {
  /** Output stage runs stereo (stereo mode or mono-mode spread); shows the
      output balance knob. */
  stereoOutput: boolean;
  /** Two independent chains are running (stereo mode); shows the auto
      balance button and swaps the Spread group for the Align group.
      Mono-mode spread doesn't need auto balance: both channels carry the
      same chain, so their energy already matches. */
  stereoChains: boolean;
  /** Plugin is fed a real stereo source; shows the input-mode button. */
  stereoInput: boolean;
  /** A chain branch is active; hides the "Stereo" input routing (the chain
      has a single mono source while branched). */
  branched: boolean;
  inputMode: InputMode;
  onInputModeChange: (mode: InputMode) => void;
}

// Memoized: Plugin re-renders on every chain poll tick, but the plate only
// depends on these few flags (its knobs subscribe to their own parameters).
export const Faceplate = React.memo(function Faceplate({
  stereoOutput,
  stereoChains,
  stereoInput,
  branched,
  inputMode,
  onInputModeChange,
}: FaceplateProps) {
  const [inputLevel, setInputLevel, onInputDrag] = useParameter('inputLevel', 'slider');
  const [toneBass, setToneBass, onBassDrag] = useParameter('toneBass', 'slider');
  const [toneMid, setToneMid, onMidDrag] = useParameter('toneMid', 'slider');
  const [toneTreble, setToneTreble, onTrebleDrag] = useParameter('toneTreble', 'slider');
  const [noiseGate, setNoiseGate, onGateDrag] = useParameter('gateThreshold', 'slider');
  const [gateEnabled, setGateEnabled] = useParameter('gateEnabled', 'toggle');
  const [toneEqEnabled, setToneEqEnabled] = useParameter('toneEqEnabled', 'toggle');

  return (
    <div
      style={{
        width: '100%',
        height: `${PLATE_HEIGHT}px`,
        display: 'flex',
        // Five peers (Input, Gate, Tone, Spread/Align, Output) share the
        // plate width. flex-end keeps the secondary Gate on the same label
        // baseline as the primary knobs (same pattern as Bal next to Output).
        justifyContent: 'space-between',
        alignItems: 'flex-end',
        flexShrink: 0,
        borderTop: BORDER,
        background: '#1C1C1E',
        padding: '16px 24px',
        boxSizing: 'border-box',
      }}
    >
      <div style={{ display: 'flex', flexDirection: 'row', alignItems: 'flex-end', gap: '10px' }}>
        <KnobControl
          label="Input"
          value={inputLevel}
          onChange={setInputLevel}
          size={KNOB_SIZE_PRIMARY}
          scale={gainDbScale}
          defaultValue={0.5}
          help={HELP.inputLevel}
          onDragStateChange={onInputDrag}
        />
        {stereoInput && (
          <InputModeButton mode={inputMode} branched={branched} onChange={onInputModeChange} />
        )}
      </div>

      <div
        style={{
          display: 'flex',
          flexDirection: 'row',
          alignItems: 'flex-end',
          gap: '10px',
          opacity: gateEnabled ? 1 : 0.55,
        }}
      >
        <KnobControl
          label="Gate"
          value={noiseGate}
          onChange={setNoiseGate}
          size={KNOB_SIZE_SECONDARY}
          thumb="secondary"
          scale={gateDbScale}
          defaultValue={gateDbScale.fromDisplay(-80)}
          help={HELP.gate}
          onDragStateChange={onGateDrag}
        />
        <PowerButton
          on={gateEnabled}
          help={HELP.gatePower}
          onClick={() => setGateEnabled(!gateEnabled)}
        />
      </div>

      <div
        style={{
          display: 'flex',
          flexDirection: 'row',
          alignItems: 'flex-end',
          gap: '10px',
          opacity: toneEqEnabled ? 1 : 0.55,
        }}
      >
        <div style={{ display: 'flex', alignItems: 'flex-end', gap: '24px' }}>
          <KnobControl
            label="Bass"
            value={toneBass}
            onChange={setToneBass}
            size={KNOB_SIZE_PRIMARY}
            scale={toneScale}
            defaultValue={toneScale.fromDisplay(5)}
            help={HELP.toneBass}
            onDragStateChange={onBassDrag}
          />
          <KnobControl
            label="Middle"
            value={toneMid}
            onChange={setToneMid}
            size={KNOB_SIZE_PRIMARY}
            scale={toneScale}
            defaultValue={toneScale.fromDisplay(5)}
            help={HELP.toneMiddle}
            onDragStateChange={onMidDrag}
          />
          <KnobControl
            label="Treble"
            value={toneTreble}
            onChange={setToneTreble}
            size={KNOB_SIZE_PRIMARY}
            scale={toneScale}
            defaultValue={toneScale.fromDisplay(5)}
            help={HELP.toneTreble}
            onDragStateChange={onTrebleDrag}
          />
        </div>
        <PowerButton
          on={toneEqEnabled}
          help={HELP.tonePower}
          onClick={() => setToneEqEnabled(!toneEqEnabled)}
        />
      </div>

      {/* Stereo-image slot: Spread in mono, Align in stereo. Fixed footprint
          (IMAGE_GROUP_WIDTH) so mode switches never shift the plate. */}
      {stereoChains ? <AlignGroup /> : <SpreadGroup />}

      <OutputGainKnob stereo={stereoOutput} autoBalance={stereoChains} />
    </div>
  );
});
