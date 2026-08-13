import React, { useEffect, useLayoutEffect, useRef, useState } from 'react';
import { ChevronDown, ChevronUp, ExternalLink } from 'lucide-react';
import type { Tone } from '../types/tone';
import { HELP, helpProps } from './helpText';
import { BORDER, GRAY, WHITE, filledPillButtonStyle, pillButtonStyle } from './theme';

/** Collapsed description before MORE expands it. */
const DESC_CLAMP_LINES = 3;

const sectionTitleStyle: React.CSSProperties = {
  fontSize: '14px',
  fontWeight: 700,
  color: WHITE,
  lineHeight: 1.4,
};

const sectionBodyStyle: React.CSSProperties = {
  fontSize: '14px',
  fontWeight: 400,
  color: GRAY,
  lineHeight: 1.4,
};

// Makes/tags come straight off the network; narrow instead of trusting the
// declared types (entries may be bare strings or objects with a name).
const nameOf = (item: unknown): string | null => {
  if (typeof item === 'string') {
    const trimmed = item.trim();
    return trimmed || null;
  }
  if (item && typeof item === 'object' && 'name' in item) {
    const name = (item as { name: unknown }).name;
    if (typeof name === 'string') {
      const trimmed = name.trim();
      return trimmed || null;
    }
  }
  return null;
};

const InfoSection: React.FC<{ title: string; children: React.ReactNode }> = ({
  title,
  children,
}) => (
  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', alignSelf: 'stretch' }}>
    <div style={sectionTitleStyle}>{title}</div>
    {children}
  </div>
);

const Hairline: React.FC = () => (
  <div style={{ alignSelf: 'stretch', borderTop: BORDER, flexShrink: 0 }} />
);

/** Sign-in / retry: centered white copy + filled pill, same as ToneBrowser. */
const promptStyle: React.CSSProperties = {
  display: 'flex',
  flexDirection: 'column',
  alignItems: 'center',
  gap: '16px',
  padding: '8px 0',
  textAlign: 'center',
};

const promptTextStyle: React.CSSProperties = {
  fontSize: '14px',
  fontWeight: 400,
  color: WHITE,
  lineHeight: 1.4,
};

/** Description with a 3-line clamp; MORE reveals the rest (LESS collapses). */
const DescriptionBlock: React.FC<{ text: string }> = ({ text }) => {
  const [expanded, setExpanded] = useState(false);
  const [overflows, setOverflows] = useState(false);
  const textRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    setExpanded(false);
  }, [text]);

  useLayoutEffect(() => {
    const el = textRef.current;
    if (!el || expanded) return;
    // Measure the clamped height; expanding unclamps, so only check when collapsed.
    setOverflows(el.scrollHeight > el.clientHeight + 1);
  }, [text, expanded]);

  return (
    <InfoSection title="Description">
      <div
        ref={textRef}
        style={{
          ...sectionBodyStyle,
          // pre-line keeps blank lines / breaks in both states. Clamp via
          // max-height (webkit-line-clamp collapses whitespace).
          whiteSpace: 'pre-line',
          ...(expanded
            ? {}
            : {
                maxHeight: `${14 * 1.4 * DESC_CLAMP_LINES}px`,
                overflow: 'hidden',
              }),
        }}
      >
        {text}
      </div>
      {overflows && (
        <button
          type="button"
          onClick={() => setExpanded((v) => !v)}
          style={{
            display: 'inline-flex',
            alignItems: 'center',
            gap: '8px',
            padding: 0,
            border: 'none',
            background: 'transparent',
            color: WHITE,
            cursor: 'pointer',
            fontFamily: 'monospace',
            fontSize: '14px',
            fontWeight: 400,
            lineHeight: 1.4,
            textTransform: 'uppercase',
          }}
        >
          {expanded ? 'Less' : 'More'}
          {expanded ? <ChevronUp size={16} /> : <ChevronDown size={16} />}
        </button>
      )}
    </InfoSection>
  );
};

interface BlockInfoPanelProps {
  loading: boolean;
  error: string | null;
  onRetry: () => void;
  /** Signed-out: skip the fetch and show a Log In CTA instead of Try again. */
  authenticated: boolean;
  onLogin: () => void;
  tone: Tone | null;
  /** Public TONE3000 page URL (summary or constructed fallback). */
  pageUrl: string;
}

/**
 * Extra tone metadata in the detail card's right column (below title / gear /
 * counts / creator). Fetched on demand via getTone and never persisted. Empty
 * sections (no description / makes / tags) are omitted entirely.
 */
export const BlockInfoPanel: React.FC<BlockInfoPanelProps> = ({
  loading,
  error,
  onRetry,
  authenticated,
  onLogin,
  tone,
  pageUrl,
}) => {
  const description = tone?.description?.trim() || null;
  const makes = (tone?.makes ?? []).map(nameOf).filter((n): n is string => n != null);
  const tags = (tone?.tags ?? []).map(nameOf).filter((n): n is string => n != null);
  const hasSections = !!(description || makes.length || tags.length);
  const signedOut = !authenticated;
  const showView = !!pageUrl;

  // Fetch in flight: the card body shows BusyOverlay over title/artwork.
  if (loading) return null;

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        alignSelf: 'stretch',
        gap: '24px',
        minWidth: 0,
      }}
    >
      <Hairline />

      {signedOut && (
        <div style={promptStyle}>
          <span style={promptTextStyle}>Sign in to TONE3000 to see tone details.</span>
          <button
            type="button"
            onClick={onLogin}
            {...helpProps(HELP.toneInfoLogin)}
            style={filledPillButtonStyle}
          >
            Log In
          </button>
        </div>
      )}

      {!signedOut && error && (
        <div style={promptStyle}>
          <span style={promptTextStyle}>{error}</span>
          <button type="button" onClick={onRetry} style={filledPillButtonStyle}>
            Try again
          </button>
        </div>
      )}

      {!signedOut && !error && hasSections && (
        <>
          {description && <DescriptionBlock text={description} />}
          {makes.length > 0 && (
            <InfoSection title="Makes & Models">
              <div
                style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'stretch',
                  gap: '4px',
                }}
              >
                {/* A make's name can pack several lines; each gets its own row. */}
                {makes
                  .flatMap((name) => name.split(/\n+/))
                  .map((line, i) => (
                    <div key={`${line}-${i}`} style={sectionBodyStyle}>
                      {line}
                    </div>
                  ))}
              </div>
            </InfoSection>
          )}
          {tags.length > 0 && (
            <InfoSection title="Tags">
              <div
                style={{
                  display: 'flex',
                  flexWrap: 'wrap',
                  alignItems: 'center',
                  gap: '8px',
                }}
              >
                {tags.map((name) => (
                  <span
                    key={name}
                    style={{
                      padding: '8px 16px',
                      borderRadius: 1000,
                      border: BORDER,
                      color: WHITE,
                      fontSize: '12px',
                      fontWeight: 400,
                      lineHeight: 1.4,
                      whiteSpace: 'nowrap',
                    }}
                  >
                    {name}
                  </span>
                ))}
              </div>
            </InfoSection>
          )}
        </>
      )}

      {showView && (
        <>
          {(signedOut || error || hasSections) && <Hairline />}
          <a
            href={pageUrl}
            target="_blank"
            rel="noopener noreferrer"
            {...helpProps(HELP.viewOnT3k)}
            style={{
              ...pillButtonStyle,
              borderRadius: '8px',
              padding: '8px 16px',
              fontSize: '14px',
              textDecoration: 'none',
              width: 'fit-content',
            }}
          >
            <ExternalLink size={16} />
            View on TONE3000
          </a>
        </>
      )}
    </div>
  );
};
