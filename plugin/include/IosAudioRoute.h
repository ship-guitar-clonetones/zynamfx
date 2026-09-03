#pragma once

#include <juce_core/juce_core.h>

/**
 * iOS audio-route shim (a no-op everywhere else).
 *
 * JUCE's iOS device opens the session as PlayAndRecord with
 * AllowBluetoothHFP (juce_Audio_ios.cpp, setAudioSessionCategory), so a
 * Bluetooth headset with a microphone becomes the whole route and iOS caps
 * the session at 16 or 24 kHz, refusing the requested 48 kHz. The owner hit
 * exactly that with AirPods: `prepareToPlay: sampleRate=24000` and no
 * explanation in the UI.
 *
 * Two answers, both here: tell the user what happened (isBluetoothRoute
 * feeds the settings tip), and stop asking for the HFP route in the first
 * place (disallowBluetoothHfp). A2DP stays allowed, so Bluetooth output-only
 * listening still works; only the low-rate headset *mic* route goes away.
 *
 * Same platform-shim pattern as Haptics / AudioPermissions: one header, one
 * ObjC++ implementation, a header-only no-op off iOS so desktop links.
 */
namespace IosAudioRoute {

#if JUCE_IOS

/** True when the session's current output route is Bluetooth (HFP, A2DP or
    LE). Cheap enough to call on every state pull. */
bool isBluetoothRoute();

/** Re-set the session category without AllowBluetoothHFP, keeping every other
    option JUCE asked for (including A2DP and MixWithOthers). Idempotent, and
    a no-op unless the option is actually set, so it can be called after every
    device-manager change: JUCE only sets the category when it opens a device,
    never on the route-change restart path, so re-applying is how the override
    survives a reopen. */
void disallowBluetoothHfp();

#else

inline bool isBluetoothRoute() {
  return false;
}
inline void disallowBluetoothHfp() {}

#endif

}  // namespace IosAudioRoute
