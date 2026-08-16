import React from 'react';
import { Equal, Power } from 'lucide-react';
import { KnobControl } from './KnobControl';
import { offsetMsScale } from './knobScale';
import { useParameter } from '../hooks/useParameter';
import { useAutoMeasure, type AutoMeasureResult } from '../hooks/useAutoMeasure';
import { HELP, helpProps } from './helpText';
import { ChromeIconButton } from './ChromeIconButton';
import {
  ICON_SIZE,
  KNOB_SIZE_PRIMARY,
  KNOB_SIZE_SECONDARY,
  KNOB_LABEL_GAP,
  faceplateChromeLift,
  pillButtonStyle,
} from './theme';
import { IMAGE_GROUP_WIDTH } from './SpreadControls';

/**
 * Align (stereo chain mode): a purely corrective alignment delay on one
 * chain (see native StereoOffset.h), e.g. two captures of one performance
 * landing a few ms apart, or NAM models / IRs with different baked-in
 * latency.
 *
 * Faceplate face: an "ALIGN" advert pill while off; clicking it powers
 * align on and reveals the bipolar Offset knob, auto-align button, and
 * power (which collapses back to the advert). Both states share one
 * footprint so the toggle never shifts the plate. Offset center = 0 ms =
 * identity; the sign picks which chain is delayed.
 *
 * The auto (=) button measures the alignment for you: one click mutes the
 * output for under half a second while an internal sweep drives both chains
 * (see native AutoOffset.h), and the measured inter-chain lag is written
 * into the offset (powering align on when there's a real correction). The
 * measurement also catches chains that are polarity-inverted against each
 * other and toggles a chain Ø (the chips on the pan rail) to match.
 *
 * Occupies the same fixed slot as the mono-mode Spread group
 * (IMAGE_GROUP_WIDTH).
 */

const CHROME_LIFT = faceplateChromeLift(KNOB_SIZE_SECONDARY);

const SECONDARY_CENTER_Y = KNOB_LABEL_GAP + 14 + KNOB_SIZE_SECONDARY / 2;
const ADVERT_HEIGHT = KNOB_SIZE_PRIMARY;

/** Same elongated outline triangle as Spread, but the advert flips the
    directions so the arrows point inward (→ ALIGN ←) instead of out. */
const AlignArrow: React.FC<{ direction: 'left' | 'right' }> = ({ direction }) => (
  <svg
    width={20}
    height={7}
    viewBox="0 0 24 10"
    fill="none"
    aria-hidden
    style={{
      flexShrink: 0,
      display: 'block',
      transform: direction === 'left' ? 'scaleX(-1)' : undefined,
    }}
  >
    <path
      d="M2 2 L20.5 5 L2 8 Z"
      stroke="currentColor"
      strokeWidth={1.15}
      strokeLinejoin="miter"
      strokeMiterlimit={10}
    />
  </svg>
);

/** What sits in the slot while align is off: a pill CTA that powers it on. */
const AdvertButton: React.FC<{ onClick: () => void }> = ({ onClick }) => (
  <button
    onClick={onClick}
    {...helpProps(HELP.alignAdvert)}
    style={{
      ...pillButtonStyle,
      height: `${ADVERT_HEIGHT}px`,
      width: `${IMAGE_GROUP_WIDTH}px`,
      marginBottom: `${SECONDARY_CENTER_Y - ADVERT_HEIGHT / 2}px`,
      boxSizing: 'border-box',
      borderRadius: `${ADVERT_HEIGHT / 2}px`,
      padding: 0,
      fontSize: '12px',
      letterSpacing: '0.08em',
      gap: '8px',
    }}
  >
    <AlignArrow direction="right" />
    ALIGN
    <AlignArrow direction="left" />
  </button>
);

/**
 * Auto align: one-shot chain time alignment. Click runs an internal probe
 * on the native side (output muted for ~½ s) and the measured lag is
 * written into alignOffset (the Offset knob visibly moves). Yellow while
 * measuring; click again to cancel. An untrustworthy measurement (e.g. a
 * chain edit landing mid-probe) is discarded silently.
 */
/** matchedMs is the measured inter-chain lag (positive = the right chain
    gets delayed), mirroring the Offset knob move; below the native
    "already aligned" floor no delay correction was applied. polarityFlipped
    reports a chain Ø toggled alongside. Two decimals: the probe measures to
    sub-sample precision. */
const alignDoneMessage = ({ matchedMs = 0, polarityFlipped = false }: AutoMeasureResult): string => {
  const delay =
    Math.abs(matchedMs) < 0.05
      ? null
      : `${matchedMs > 0 ? 'R' : 'L'} +${Math.abs(matchedMs).toFixed(2)} ms`;
  return ['Aligned', delay, polarityFlipped ? 'Ø flipped' : null]
    .filter((part) => part != null)
    .join(' · ');
};

const AutoAlignButton: React.FC = () => {
  const { listening, toggle } = useAutoMeasure(
    'startAutoOffset',
    'cancelAutoOffset',
    'pollAutoOffset',
    alignDoneMessage,
    'Measuring'
  );
  return (
    <ChromeIconButton
      tone="armed"
      on={listening}
      help={HELP.autoAlign}
      onClick={toggle}
      offsetY={CHROME_LIFT}
    >
      <Equal size={ICON_SIZE} />
    </ChromeIconButton>
  );
};

/** Offset knob + auto + power; collapses to the advert when powered off. */
export const AlignGroup: React.FC = () => {
  const [enabled, setEnabled] = useParameter('alignEnabled', 'toggle');
  const [offset, setOffset] = useParameter('alignOffset', 'slider');

  return (
    <div style={{ position: 'relative', width: `${IMAGE_GROUP_WIDTH}px`, boxSizing: 'border-box' }}>
      {enabled ? (
        <div
          style={{
            display: 'flex',
            flexDirection: 'row',
            alignItems: 'flex-end',
            justifyContent: 'center',
            gap: '10px',
          }}
        >
          <AutoAlignButton />
          <KnobControl
            label="Offset"
            value={offset}
            onChange={setOffset}
            variant="bipolar"
            size={KNOB_SIZE_PRIMARY}
            scale={offsetMsScale}
            defaultValue={0.5}
            help={HELP.alignOffset}
          />
          <ChromeIconButton
            tone="power"
            on
            help={HELP.alignPower}
            onClick={() => setEnabled(false)}
            offsetY={CHROME_LIFT}
          >
            <Power size={ICON_SIZE} />
          </ChromeIconButton>
        </div>
      ) : (
        <AdvertButton onClick={() => setEnabled(true)} />
      )}
    </div>
  );
};
