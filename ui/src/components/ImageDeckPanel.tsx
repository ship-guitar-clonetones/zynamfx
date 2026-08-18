import React, { useCallback } from 'react';
import { Power } from './icons';
import { KnobControl } from './KnobControl';
import { crossoverHzScale, percentScale } from './knobScale';
import type { KnobScale } from './knobScale';
import { useParameter } from '../hooks/useParameter';
import { rem } from '../hooks/useUiScale';
import { useCorrelation } from '../hooks/useMeters';
import { HELP, helpProps } from './helpText';
import { ChromeIconButton } from './ChromeIconButton';
import {
  BORDER,
  BRAND_RED,
  BRAND_YELLOW,
  GRAY,
  ICON_BOX_SIZE,
  ICON_SIZE,
  KNOB_SIZE_SECONDARY,
  KNOB_LABEL_GAP,
  SUBTLE,
} from './theme';

/**
 * The shared advanced panel for the stereo-image slot: Spread (mono chain
 * mode) and Align (stereo chain mode) mount the same deck sections (see
 * native ImageDeck.h), so this one panel serves both, keyed on the feature
 * (which selects the parameter ids and help strings):
 *  - Wobble knob + power: humanizing drift of the delay (absolute, up to
 *    ±1.2 ms around the dialed offset).
 *  - Crossover knob + power: lows below the cutoff skip the deck (dual-mono
 *    in spread, untouched in align).
 *  - Diffuse power: the phase-decorrelation cascade on the delayed side.
 *  - Mono-safety LED: live L/R output correlation.
 * Spread's sections default on (the deck is its core sound); Align's all
 * default off (purely corrective until the deck is asked for).
 */

const WOBBLE_DEFAULT = 0.25;
/** Center of the log 32.5-520 Hz map = 130 Hz. */
const CROSSOVER_DEFAULT = 0.5;
const POWER_DEFAULTS = {
  spread: { wobbleOn: true, crossoverOn: true, diffuseOn: true },
  align: { wobbleOn: false, crossoverOn: false, diffuseOn: false },
} as const;

/** Equal section columns so the flex gap between Wobble / Crossover / Diffuse
    reads evenly. Sized for the longest label ("Crossover" at the shared
    14px / 1px letter-spacing); shorter labels center inside. */
const SECTION_WIDTH = 82;

interface DeckStrings {
  wobble: string;
  wobblePower: string;
  crossover: string;
  crossoverPower: string;
  diffuse: string;
}

const DECK_HELP: Record<'spread' | 'align', DeckStrings> = {
  spread: {
    wobble: HELP.spreadWobble,
    wobblePower: HELP.spreadWobblePower,
    crossover: HELP.spreadCrossover,
    crossoverPower: HELP.spreadCrossoverPower,
    diffuse: HELP.spreadDiffuse,
  },
  align: {
    wobble: HELP.alignWobble,
    wobblePower: HELP.alignWobblePower,
    crossover: HELP.alignCrossover,
    crossoverPower: HELP.alignCrossoverPower,
    diffuse: HELP.alignDiffuse,
  },
};

/** Restores a feature's whole deck to its defaults. Wired to Alt/Option-click
    on the feature's Offset knob (KnobControl onReset), so one gesture resets
    the feature, not just the knob. */
export function useImageDeckReset(feature: 'spread' | 'align'): () => void {
  const [, setWobble] = useParameter(`${feature}Wobble`, 'slider');
  const [, setWobbleOn] = useParameter(`${feature}WobbleEnabled`, 'toggle');
  const [, setCrossover] = useParameter(`${feature}Crossover`, 'slider');
  const [, setCrossoverOn] = useParameter(`${feature}CrossoverEnabled`, 'toggle');
  const [, setDiffuseOn] = useParameter(`${feature}DiffuseEnabled`, 'toggle');
  return useCallback(() => {
    const powers = POWER_DEFAULTS[feature];
    setWobble(WOBBLE_DEFAULT);
    setWobbleOn(powers.wobbleOn);
    setCrossover(CROSSOVER_DEFAULT);
    setCrossoverOn(powers.crossoverOn);
    setDiffuseOn(powers.diffuseOn);
  }, [feature, setWobble, setWobbleOn, setCrossover, setCrossoverOn, setDiffuseOn]);
}

/** Mono-safety LED. Below 0.5 correlation a mono fold-down audibly thins;
    below 0 it actively cancels. */
const CorrelationLed: React.FC = () => {
  const correlation = useCorrelation();
  const color = correlation < 0 ? BRAND_RED : correlation < 0.5 ? BRAND_YELLOW : SUBTLE;
  return (
    <div
      {...helpProps(HELP.imageCorrelation)}
      style={{
        position: 'absolute',
        top: '8rem',
        right: '8rem',
        width: '6rem',
        height: '6rem',
        borderRadius: '50%',
        background: color,
      }}
    />
  );
};

/** A deck section: a standard labeled knob centered in the column, its power
    button floated beside it. Centering the knob (not the knob + power
    cluster) keeps the label centered too, so a long one like "Crossover"
    overflows symmetrically instead of spilling into one neighbor's gap. */
const SectionKnob: React.FC<{
  label: string;
  value: number;
  onChange: (value: number) => void;
  scale: KnobScale;
  defaultValue: number;
  help: string;
  on: boolean;
  powerHelp: string;
  onPower: () => void;
  onDragStateChange?: (dragging: boolean) => void;
}> = ({ label, value, onChange, scale, defaultValue, help, on, powerHelp, onPower, onDragStateChange }) => (
  <div
    style={{
      width: rem(SECTION_WIDTH),
      position: 'relative',
      display: 'flex',
      justifyContent: 'center',
    }}
  >
    <KnobControl
      label={label}
      value={value}
      onChange={onChange}
      size={KNOB_SIZE_SECONDARY}
      thumb="secondary"
      scale={scale}
      defaultValue={defaultValue}
      help={help}
      onDragStateChange={onDragStateChange}
    />
    <ChromeIconButton
      tone="power"
      on={on}
      help={powerHelp}
      onClick={onPower}
      style={{
        position: 'absolute',
        top: rem((KNOB_SIZE_SECONDARY - ICON_BOX_SIZE) / 2),
        left: `calc(50% + ${KNOB_SIZE_SECONDARY / 2 + 6}rem)`,
      }}
    >
      <Power size={ICON_SIZE} />
    </ChromeIconButton>
  </div>
);

/** Floats above the plate. When anchored to the group's Offset knob, its
    left edge starts at the knob's left side; otherwise (advert state) it
    flush-rights to the group. */
export const ImageDeckPanel = React.forwardRef<
  HTMLDivElement,
  { feature: 'spread' | 'align'; fromKnob?: boolean }
>(function ImageDeckPanel({ feature, fromKnob = false }, ref) {
  const help = DECK_HELP[feature];
  const [wobble, setWobble, onWobbleDrag] = useParameter(`${feature}Wobble`, 'slider');
  const [wobbleOn, setWobbleOn] = useParameter(`${feature}WobbleEnabled`, 'toggle');
  const [crossover, setCrossover, onCrossoverDrag] = useParameter(`${feature}Crossover`, 'slider');
  const [crossoverOn, setCrossoverOn] = useParameter(`${feature}CrossoverEnabled`, 'toggle');
  const [diffuseOn, setDiffuseOn] = useParameter(`${feature}DiffuseEnabled`, 'toggle');
  return (
    <div
      ref={ref}
      style={{
        position: 'absolute',
        // Splits the difference down toward the knob's top edge instead of
        // floating a full gap above the group.
        bottom: 'calc(100% + 6rem)',
        ...(fromKnob ? { left: 0 } : { right: 0 }),
        backgroundColor: '#141416',
        border: BORDER,
        borderRadius: '14rem',
        padding: '14rem 16rem 8rem',
        zIndex: 200,
        boxSizing: 'border-box',
        display: 'flex',
        alignItems: 'flex-end',
        gap: '18rem',
      }}
    >
      <SectionKnob
        label="Wobble"
        value={wobble}
        onChange={setWobble}
        scale={percentScale}
        defaultValue={WOBBLE_DEFAULT}
        help={help.wobble}
        on={wobbleOn}
        powerHelp={help.wobblePower}
        onPower={() => setWobbleOn(!wobbleOn)}
        onDragStateChange={onWobbleDrag}
      />
      <SectionKnob
        label="Crossover"
        value={crossover}
        onChange={setCrossover}
        scale={crossoverHzScale}
        defaultValue={CROSSOVER_DEFAULT}
        help={help.crossover}
        on={crossoverOn}
        powerHelp={help.crossoverPower}
        onPower={() => setCrossoverOn(!crossoverOn)}
        onDragStateChange={onCrossoverDrag}
      />
      {/* Diffuse has no continuous control, just its power; same column width
          and knob-box/label geometry as the knob sections so the flex gaps
          and label baseline read evenly. */}
      <div
        style={{
          width: rem(SECTION_WIDTH),
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          gap: `${KNOB_LABEL_GAP}rem`,
        }}
      >
        <div
          style={{
            height: `${KNOB_SIZE_SECONDARY}rem`,
            display: 'grid',
            placeItems: 'center',
          }}
        >
          <ChromeIconButton
            tone="power"
            on={diffuseOn}
            help={help.diffuse}
            onClick={() => setDiffuseOn(!diffuseOn)}
          >
            <Power size={ICON_SIZE} />
          </ChromeIconButton>
        </div>
        <span
          style={{
            fontSize: '14rem',
            fontWeight: 400,
            letterSpacing: '1rem',
            color: GRAY,
            lineHeight: 1.2,
            whiteSpace: 'nowrap',
          }}
        >
          Diffuse
        </span>
      </div>
      <CorrelationLed />
    </div>
  );
});
