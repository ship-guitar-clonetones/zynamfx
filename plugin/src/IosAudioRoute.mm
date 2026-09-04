#include "IosAudioRoute.h"

#if JUCE_IOS

#import <AVFoundation/AVFoundation.h>

namespace IosAudioRoute {

bool isBluetoothRoute() {
  AVAudioSession* session = [AVAudioSession sharedInstance];
  for (AVAudioSessionPortDescription* output in session.currentRoute.outputs) {
    NSString* type = output.portType;
    if ([type isEqualToString:AVAudioSessionPortBluetoothHFP] ||
        [type isEqualToString:AVAudioSessionPortBluetoothA2DP] ||
        [type isEqualToString:AVAudioSessionPortBluetoothLE])
      return true;
  }
  // A headset mic can be the input while the output is still the speaker.
  for (AVAudioSessionPortDescription* input in session.currentRoute.inputs)
    if ([input.portType isEqualToString:AVAudioSessionPortBluetoothHFP])
      return true;
  return false;
}

void disallowBluetoothHfp() {
  AVAudioSession* session = [AVAudioSession sharedInstance];

  // Only PlayAndRecord carries the option; leave Playback alone.
  if (![session.category isEqualToString:AVAudioSessionCategoryPlayAndRecord])
    return;

  // Take JUCE's own options and clear one bit, rather than rebuilding the
  // set: MixWithOthers, DefaultToSpeaker, AllowAirPlay and A2DP stay exactly
  // as JUCE asked for them, whatever the JUCE version decided.
  const AVAudioSessionCategoryOptions options = session.categoryOptions;

  // Same SDK gate JUCE uses for the same constant (it was renamed in the
  // iOS 26 SDK; the value is unchanged).
 #if JUCE_IOS_API_VERSION_CAN_BE_BUILT (26, 0)
  constexpr auto hfp = AVAudioSessionCategoryOptionAllowBluetoothHFP;
 #else
  constexpr auto hfp = AVAudioSessionCategoryOptionAllowBluetooth;
 #endif

  if ((options & hfp) == 0)
    return;

  NSError* error = nil;
  if (! [session setCategory:session.category withOptions:(options & ~hfp) error:&error])
    DBG ("IosAudioRoute: could not drop the HFP option: "
         << (error != nil ? juce::String::fromUTF8 ([[error localizedDescription] UTF8String])
                          : juce::String ("no error object")));
}

}  // namespace IosAudioRoute

#endif
