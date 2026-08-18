import React from 'react';
import { ShieldAlert, WifiOff } from './icons';
import { isNativeFunctionRegistered } from '../backend/JuceBackend';
import { useAudioBackend } from '../hooks/useAudioBackend';
import type { ConnectionProblem } from '../hooks/useConnectionGate';
import { filledPillButtonStyle, pillButtonStyle } from './theme';

interface ConnectionModalProps {
  problem: ConnectionProblem | null;
  onRetry: () => void;
  onDismiss: () => void;
}

/**
 * Connection gate modal, two variants (see useConnectionGate):
 *
 * - offline: a network-dependent action was attempted while the OS reports
 *   no connection at all (`navigator.onLine === false`).
 * - insecure: the internet is up but the HTTPS probe to TONE3000 failed,
 *   usually a wrong system clock, so we offer a jump to the OS date & time
 *   settings where the native side provides one.
 *
 * Same full-window scrim + button language as OAuthOverlay so the error
 * surfaces read as one system.
 */
export const ConnectionModal: React.FC<ConnectionModalProps> = ({
  problem,
  onRetry,
  onDismiss,
}) => {
  const backend = useAudioBackend();
  if (!problem) return null;

  const offline = problem === 'offline';
  const canOpenClockSettings = !offline && isNativeFunctionRegistered('openDateTimeSettings');

  return (
    <div
      role="alertdialog"
      aria-label={offline ? 'No internet connection' : 'Secure connection failed'}
      style={{
        position: 'absolute',
        inset: 0,
        backgroundColor: 'rgba(0, 0, 0, 0.5)',
        backdropFilter: 'blur(4rem)',
        WebkitBackdropFilter: 'blur(4rem)',
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        gap: '16rem',
        padding: '24rem',
        textAlign: 'center',
        color: '#fff',
        zIndex: 3000,
      }}
    >
      {offline ? (
        <WifiOff size={28} style={{ opacity: 0.9 }} />
      ) : (
        <ShieldAlert size={28} style={{ opacity: 0.9 }} />
      )}
      {/* Body copy: reset the global 600 default. */}
      <div style={{ fontSize: '14rem', fontWeight: 400, opacity: 0.95, maxWidth: '400rem' }}>
        {offline
          ? 'No internet connection. Connect to browse and load tones from TONE3000.'
          : "Couldn't establish a secure connection to TONE3000. This usually means the " +
            "computer's date & time are wrong; a firewall, VPN, or security software can " +
            'also cause it.'}
      </div>
      <div style={{ display: 'flex', gap: '12rem' }}>
        <button type="button" onClick={onRetry} style={filledPillButtonStyle}>
          Try again
        </button>
        {canOpenClockSettings && (
          <button
            type="button"
            onClick={() => void backend.getPluginFunction('openDateTimeSettings')()}
            style={pillButtonStyle}
          >
            Date &amp; time settings
          </button>
        )}
        <button type="button" onClick={onDismiss} style={pillButtonStyle}>
          Dismiss
        </button>
      </div>
    </div>
  );
};
