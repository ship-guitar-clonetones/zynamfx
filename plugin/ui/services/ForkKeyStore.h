#pragma once
// Fork-only: bring-your-own TONE3000 API key.
//
// Public builds ship WITHOUT a baked publishable key (see
// cmake/ForkIdentity.cmake and the release workflow): every user pastes their
// own free key — generated at tone3000.com/settings after signing in — into
// Plugin Settings, and all catalog traffic runs under that key. A key saved
// here wins over the baked build key (which may be empty); clearing it falls
// back to the baked key. Changing the key drops the stored tokens and cached
// identity (they belong to the previous key), so the next sign-in starts
// clean. Tone3000Session re-reads the key when a sign-in starts, so a key
// saved at runtime takes effect without reopening the editor.
#include <juce_core/juce_core.h>

#include "UiPrefs.h"

namespace t3k::ui::fork {

inline constexpr const char* kCustomPublishableKey = "fork.customPublishableKey";
// A secret key was pasted and nothing usable is stored: the sign-in screen
// reads this so it can name the reason instead of "no key configured".
inline constexpr const char* kRejectedSecretKey = "fork.customPublishableKey.rejectedSecret";

// Why a value cannot serve as the OAuth client_id. `none` covers the empty
// value too (the caller falls back to the build key, then reports "no key"):
// this only classifies a value the user actually typed.
enum class KeyProblem {
  none,
  secret,     // t3k_cs_… — a server-only credential, never a client_id
  malformed,  // not a t3k_pub_… publishable key (truncated paste, a URL, …)
};

inline const char* keyProblemText(KeyProblem problem) {
  switch (problem) {
    case KeyProblem::none: return "";
    case KeyProblem::secret:
      return "That is a TONE3000 secret key (t3k_cs_…), which only works on a server. Use the publishable "
             "key (t3k_pub_…) from TONE3000 Settings > API Keys.";
    case KeyProblem::malformed:
      return "That doesn't look like a TONE3000 publishable key. It should start with t3k_pub_ and be "
             "copied whole from TONE3000 Settings > API Keys.";
  }
  return "";
}

inline juce::String customPublishableKey(UiPrefs& prefs) { return prefs.get(kCustomPublishableKey).trim(); }

// True while the last thing the user pasted was a secret key and no usable
// publishable key is stored.
inline bool rejectedSecretKey(UiPrefs& prefs) { return prefs.getBool(kRejectedSecretKey, false); }

// The key the session should use: the saved one, else the build's baked key.
inline juce::String effectivePublishableKey(UiPrefs& prefs, const juce::String& buildKey) {
  const auto custom = customPublishableKey(prefs);
  return custom.isNotEmpty() ? custom : buildKey.trim();
}

inline KeyProblem checkPublishableKey(const juce::String& value) {
  const auto key = value.trim();
  if (key.isEmpty()) return KeyProblem::none;
  if (key.startsWithIgnoreCase("t3k_cs_")) return KeyProblem::secret;
  // Publishable keys are `t3k_pub_` plus a long identifier. A truncated paste
  // (or a pasted URL) fails here instead of at the authorize round trip.
  if (!key.startsWithIgnoreCase("t3k_pub_") || key.length() < 16) return KeyProblem::malformed;
  return KeyProblem::none;
}

// A secret key is never stored: it would be sent as a client_id and leak a
// server credential into a client. Everything else is stored as pasted (the
// Settings field shows what the user typed); a malformed value is reported by
// the sign-in flow with the reason, instead of failing at the server.
inline KeyProblem setCustomPublishableKey(UiPrefs& prefs, const juce::String& key) {
  const auto value = key.trim();
  const auto problem = checkPublishableKey(value);
  if (problem == KeyProblem::secret) {
    juce::Logger::writeToLog(
        "[auth] API key not saved: t3k_cs_ is a secret key; a publishable t3k_pub_ key is required");
    // Only worth remembering while there is nothing usable to fall back to,
    // so the sign-in screen can name this instead of "no key configured".
    const auto stored = customPublishableKey(prefs);
    const bool usable = stored.isNotEmpty() && checkPublishableKey(stored) == KeyProblem::none;
    prefs.setBool(kRejectedSecretKey, !usable);
    return problem;
  }
  prefs.setBool(kRejectedSecretKey, false);
  if (problem == KeyProblem::malformed)
    juce::Logger::writeToLog("[auth] API key saved but it does not look like a t3k_pub_ key; sign-in will report it");
  if (value == customPublishableKey(prefs)) return problem;
  if (value.isEmpty())
    prefs.remove(kCustomPublishableKey);
  else
    prefs.set(kCustomPublishableKey, value);
  // Tokens and the cached identity were issued under the previous key.
  prefs.remove(UiPrefs::kTokens);
  prefs.remove(UiPrefs::kCachedUser);
  return problem;
}

}  // namespace t3k::ui::fork
