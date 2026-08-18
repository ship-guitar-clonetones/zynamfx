import React from 'react';
import { RotateCcw } from './icons';
import { HELP, helpProps } from './helpText';
import { filledPillButtonStyle } from './theme';

/**
 * Failed-download state for a chain block (`block.loadFailed`): replaces the
 * loading dots with a short message and a retry button. Rendered over the
 * dimmed tone artwork on both the gallery tile and the detail card. Uses the
 * filled primary pill so Retry reads as the one recovery action.
 */
export const RetryLoadBadge: React.FC<{ onRetry: () => void }> = ({ onRetry }) => (
  <div
    style={{
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'center',
      gap: '8rem',
      textAlign: 'center',
    }}
  >
    <span style={{ fontSize: '11rem', fontWeight: 400, color: '#ffffff', opacity: 0.9 }}>
      Download failed
    </span>
    <button
      type="button"
      onClick={(e) => {
        e.stopPropagation();
        onRetry();
      }}
      {...helpProps(HELP.retryLoad)}
      style={{
        ...filledPillButtonStyle,
        gap: '6rem',
        padding: '5rem 12rem',
        fontSize: '12rem',
      }}
    >
      <RotateCcw size={12} />
      Retry
    </button>
  </div>
);
