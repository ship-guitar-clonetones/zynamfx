import React from 'react';
import { rem } from '../hooks/useUiScale';
import { FONT_MONO } from './theme';

/** The web app's A2-architecture mark (gradient "A2" rounded square), a
    touch taller than the format badge. The gradient id is per-instance: a
    browser page renders dozens of badges at once. */
const A2Mark: React.FC<{ size?: number }> = ({ size = 18 }) => {
  const gradientId = React.useId();
  return (
    <svg
      viewBox="0 0 24 24"
      fill="none"
      aria-label="A2"
      role="img"
      style={{ width: rem(size), height: rem(size) }}
    >
      <path
        d="M20 0C22.2091 2.57702e-07 24 1.79086 24 4V20C24 22.2091 22.2091 24 20 24H4C1.79086 24 6.44266e-08 22.2091 0 20V4C2.57706e-07 1.79086 1.79086 6.44256e-08 4 0H20ZM6.3125 6.16406L2.40039 18.1074H4.82812L5.5332 15.6143H9.11816L9.81543 18.1074H12.2441L8.4043 6.16406H6.3125ZM17.0674 6C16.433 6 15.8531 6.10373 15.3281 6.31152C14.8086 6.51387 14.3654 6.79297 13.999 7.14844C13.6272 7.50388 13.34 7.91977 13.1377 8.39551C12.9354 8.86575 12.834 9.3661 12.834 9.89648H15.123C15.123 9.57383 15.1641 9.28672 15.2461 9.03516C15.3281 8.78359 15.4488 8.57285 15.6074 8.40332C15.7605 8.23941 15.9521 8.11616 16.1816 8.03418C16.4167 7.9468 16.6872 7.90334 16.9932 7.90332C17.2281 7.90332 17.4415 7.94114 17.6328 8.01758C17.8296 8.09409 17.9995 8.20366 18.1416 8.3457C18.2783 8.49329 18.3854 8.67121 18.4619 8.87891C18.5385 9.08667 18.5762 9.32488 18.5762 9.59277C18.5762 9.77313 18.5464 9.95911 18.4863 10.1504C18.4317 10.3417 18.3415 10.547 18.2158 10.7656C18.0846 10.9898 17.9148 11.236 17.707 11.5039C17.4992 11.7718 17.2451 12.0704 16.9443 12.3984L13.0801 16.5488V18.1074H21.2266V16.2783H15.9844L18.2324 13.8994C18.6205 13.5004 18.973 13.1205 19.29 12.7598C19.6072 12.3934 19.8807 12.0346 20.1104 11.6846C20.3345 11.3347 20.5067 10.9822 20.627 10.627C20.7527 10.266 20.8164 9.88789 20.8164 9.49414C20.8164 8.97476 20.7343 8.50165 20.5703 8.0752C20.4062 7.64318 20.1682 7.27401 19.8564 6.96777C19.5393 6.66152 19.1451 6.42344 18.6748 6.25391C18.2101 6.08447 17.6742 6.00002 17.0674 6ZM8.57715 13.6533H6.09961L7.35449 9.25684L8.57715 13.6533Z"
        fill={`url(#${gradientId})`}
      />
      <defs>
        <linearGradient
          id={gradientId}
          x1="12"
          y1="0"
          x2="12"
          y2="24"
          gradientUnits="userSpaceOnUse"
        >
          <stop stopColor="white" />
          <stop offset="1" stopColor="#434343" />
        </linearGradient>
      </defs>
    </svg>
  );
};

/** Format tag (NAM / IR / …) matching the web ToneCard badge:
    zinc-400 background, black mono text, 2px corners. `a2` appends the A2
    architecture mark (NAM tones the plugin can actually load). */
export const FormatBadge: React.FC<{ label: string; a2?: boolean }> = ({ label, a2 = false }) => (
  <span
    style={{
      display: 'inline-flex',
      alignItems: 'center',
      gap: '10rem',
      flexShrink: 0,
    }}
  >
    <span
      style={{
        fontFamily: FONT_MONO,
        fontSize: '12rem',
        fontWeight: 400,
        color: '#000000',
        backgroundColor: '#a1a1aa',
        padding: '1rem 6rem',
        borderRadius: '2rem',
        whiteSpace: 'nowrap',
        letterSpacing: 'normal',
      }}
    >
      {label}
    </span>
    {a2 && <A2Mark />}
  </span>
);
