import React, { useMemo } from 'react';
import { useMeter, useMeterClip, meterId } from '../hooks/useMeters';
import { METER_MAX_DB, METER_MIN_DB, getGradientColor } from './meterColor';
import { HELP, helpProps } from './helpText';
import { FONT_MONO, GRAY } from './theme';

interface DbMeterProps {
  type: 'input' | 'output';
  /** Dual L/R columns sharing one dB scale (stereo mode / stereo input). */
  stereo?: boolean;
  height?: number;
  labelsPosition?: 'left' | 'right';
}

const DOT_SIZE = 6;
const DOT_GAP = 10;
/** Gap between the L and R columns in stereo. */
const COLUMN_GAP = 5;
/** Gap between the label rail and the dot column(s). */
const LABEL_GAP = 10;
/** Tighter gap when labels sit to the right of the dots (output meter): the
    right-aligned digits are ragged on the side facing the dots, so the visual
    gap already reads larger there. */
const LABEL_GAP_RIGHT = 6;
const LABEL_WIDTH = 18;
const LABEL_COLOR = GRAY;

/**
 * Main input/output meter: vertical dot column(s) in the block-meter style.
 * The full color scale is always visible (dimmed) and lights to the level.
 * The scale tops out at 0 dBFS; the topmost dot is a clip LED that lights only
 * when the level hits 0 dB, latches red, and clears on click.
 * Each column subscribes to its own meter id, so a channel only re-renders
 * for its own (quantized) changes.
 */
const DotColumn: React.FC<{ id: string; numDots: number }> = ({ id, numDots }) => {
  const db = useMeter(id);
  const [clipped, clearClip] = useMeterClip(id);

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column-reverse',
        gap: `${DOT_GAP}rem`,
        flexShrink: 0,
      }}
    >
      {Array.from({ length: numDots }, (_, index) => {
        // Dot i sits at an exact dB threshold on the label scale; the top dot
        // is exactly 0 dBFS and doubles as the latching clip LED.
        const position = numDots > 1 ? index / (numDots - 1) : 0;
        const dotDb = METER_MIN_DB + position * (METER_MAX_DB - METER_MIN_DB);
        const isClipDot = index === numDots - 1;
        const isActive = isClipDot ? clipped : db >= dotDb;
        return (
          <div
            key={index}
            onClick={isClipDot && clipped ? clearClip : undefined}
            {...(isClipDot && clipped ? helpProps(HELP.clipDot) : {})}
            style={{
              width: `${DOT_SIZE}rem`,
              height: `${DOT_SIZE}rem`,
              borderRadius: '50%',
              backgroundColor: getGradientColor(position),
              opacity: isActive ? 1 : 0.22,
              cursor: isClipDot && clipped ? 'pointer' : undefined,
              flexShrink: 0,
            }}
          />
        );
      })}
    </div>
  );
};

export const DbMeter: React.FC<DbMeterProps> = ({
  type,
  stereo = false,
  height = 200,
  labelsPosition = 'left',
}) => {
  const numDots = useMemo(() => Math.floor(height / (DOT_SIZE + DOT_GAP)), [height]);
  const actualMeterHeight = numDots * DOT_SIZE + (numDots - 1) * DOT_GAP;

  // Label centers align with dot centers: MIN at the bottom dot, MAX (0 dB)
  // at the top (clip) dot.
  const dbToPixelPosition = (db: number): number => {
    const normalized = (db - METER_MIN_DB) / (METER_MAX_DB - METER_MIN_DB);
    return DOT_SIZE / 2 + normalized * (actualMeterHeight - DOT_SIZE);
  };

  const scaleMarks = [-60, -48, -36, -24, -18, -12, -9, -6, -3, 0];

  const labels = (
    <div
      style={{
        position: 'relative',
        height: `${actualMeterHeight}rem`,
        fontSize: '8rem',
        fontWeight: '500',
        color: LABEL_COLOR,
        flexShrink: 0,
        width: `${LABEL_WIDTH}rem`,
      }}
    >
      {scaleMarks.map((db) => (
        <div
          key={db}
          style={{
            position: 'absolute',
            bottom: `${dbToPixelPosition(db)}rem`,
            right: 0,
            transform: 'translateY(50%)',
            textAlign: 'right',
            width: `${LABEL_WIDTH}rem`,
            lineHeight: 1,
            fontFamily: FONT_MONO,
          }}
        >
          {db}
        </div>
      ))}
    </div>
  );

  // One column subscribed to the combined level, or L/R columns per channel.
  const columns = stereo ? [meterId.main(type, 'l'), meterId.main(type, 'r')] : [type];

  const dots = (
    <div
      style={{
        display: 'flex',
        flexDirection: 'row',
        alignItems: 'flex-end',
        gap: `${COLUMN_GAP}rem`,
        flexShrink: 0,
      }}
    >
      {columns.map((id) => (
        <DotColumn key={id} id={id} numDots={numDots} />
      ))}
    </div>
  );

  const labelGap = labelsPosition === 'left' ? LABEL_GAP : LABEL_GAP_RIGHT;

  return (
    // Fixed mono-footprint slot: the meter always occupies its mono width, and
    // the labels+dots row is centered inside it. In stereo the row widens by
    // one column and overflows the slot symmetrically, which lands the dot
    // pair's center exactly where the mono column's center was (over the gain
    // knob) while the labels shift outward by half the growth — keeping the
    // label-to-dots gap constant in every state.
    <div
      style={{
        width: `${LABEL_WIDTH + labelGap + DOT_SIZE}rem`,
        // A tighter-than-default gap shrinks the slot; give the savings back
        // as margin on the label side so the meter's overall footprint (and
        // thus the dots' position over the knob) is gap-independent.
        [labelsPosition === 'left' ? 'marginLeft' : 'marginRight']:
          `${LABEL_GAP - labelGap}rem`,
        display: 'flex',
        flexDirection: 'row',
        alignItems: 'flex-end',
        justifyContent: 'center',
        flexShrink: 0,
      }}
    >
      <div
        style={{
          display: 'flex',
          flexDirection: 'row',
          alignItems: 'flex-end',
          gap: `${labelGap}rem`,
          flexShrink: 0,
        }}
      >
        {labelsPosition === 'left' && labels}
        {dots}
        {labelsPosition === 'right' && labels}
      </div>
    </div>
  );
};
