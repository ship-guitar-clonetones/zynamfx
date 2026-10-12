# External hardware effects in the zynamfx chain: research

Status: **research only; not implemented**. Reviewed against zynamfx main at
[`6dbdefb`](https://github.com/zackyusof/zynamfx/tree/6dbdefb3daa95014d9b2b81bcc2de22e9acc3e8d)
on 2026-10-12. The proposed block is a hardware send/return, distinct from
the existing `EFFECT` block that hosts an installed AU/VST3 plug-in.

## Feasibility and current limits

An external effect can be placed in the signal chain if zynamfx sends audio
from that position to a spare interface output and receives it on a spare
input before the later chain blocks. The standalone app is the first practical
target because it owns its audio device. A compatible interface needs separate
ports for the instrument, external return, main monitors, and external send.
The exact channel count depends on whether the insert is mono or stereo and
which ports the interface can address independently.

Today, [`ChainBlockType`](../plugin/include/ChainBlock.h) contains NAM, IR,
INSERT (an empty UI slot), and EFFECT (a hosted plug-in). The processor has
one stereo main input and output bus in
[`Processor.cpp`](../plugin/src/Processor.cpp), and the standalone settings
select at most two input channels and one output pair in
[`StandaloneAudioSettings.cpp`](../plugin/src/StandaloneAudioSettings.cpp).
The normal chain runs through `processChainOnBuffer`, inside a 48 kHz chain
domain with optional oversampling and a host-rate boundary. There is no
hardware insert path today.

JUCE exposes selectable input/output channel masks through
[`AudioDeviceManager::AudioDeviceSetup`](https://docs.juce.com/master/structjuce_1_1AudioDeviceManager_1_1AudioDeviceSetup.html),
and its device callback can read and write the enabled channels. The device
reports input and output latency in samples, but those numbers do not include
the whole external unit's processing delay. A measured round trip and a manual
offset are therefore needed. See
[`AudioIODevice`](https://docs.juce.com/master/classjuce_1_1AudioIODevice.html)
and [`AudioProcessorPlayer`](https://docs.juce.com/master/classjuce_1_1AudioProcessorPlayer.html).

## Recommended first implementation: one standalone insert

1. Add a persistent hardware-insert block with a stable ID, enabled/bypass
   state, mono or stereo mode, send and return channel choices, send/return
   trims, mix, measured latency, and optional manual latency offset. Preserve
   its position in presets, undo/redo, duplication, and session state. When
   the configured device or ports are absent, show the reason and pass dry
   audio rather than reading an unrelated input.
2. Extend standalone device settings to activate the normal input and monitor
   channels **plus** the selected send/return channels. Validate that a send
   is not the monitor pair, a return is not the guitar input, and the chosen
   interface actually exposes each port. Persist requested masks and choices;
   current settings explicitly track masks because device readback can differ
   from the requested mask. Check whether JUCE's `StandalonePluginHolder` /
   `AudioProcessorPlayer` can carry the chosen channel layout correctly; a
   dedicated multichannel device callback may be needed.
3. Split rendering at the hardware block. The callback can output this
   callback's pre-insert signal to the hardware send and process an already
   returned signal through the post-insert chain. The return cannot represent
   the signal just sent in the same callback: the physical round trip crosses
   callback boundaries. Use preallocated buffers and a defined pipeline;
   never wait for the interface or allocate on the audio thread. Reconcile
   this with the existing 48 kHz chain boundary, oversampling, block slicing,
   and parallel lane processing before coding.
4. Add a safe ping/loopback measurement and manual offset. Align the bypass
   and dry/wet paths with the returned signal, and align the other lane when
   chains run in parallel. Re-measure or invalidate the offset after device,
   sample-rate, or buffer-size changes. JUCE's
   [`setLatencySamples`](https://docs.juce.com/master/classjuce_1_1AudioProcessor.html)
   can report processor delay where a host can compensate; standalone still
   needs its own alignment. A wet effect may mask a ping, so a direct loopback
   measurement or manual fallback should be supported.
5. Add an insert tile and editor using the existing chain UI. Explain the
   required cabling and signal level: many guitar pedals need a reamp send and
   suitable return gain, while rack gear commonly accepts line level. Avoid
   routing the return to hardware direct monitoring as well as the software
   chain, which would double the signal.

Start with **one insert**. Multiple serial inserts each need their own
send/return and pipeline delay; parallel lanes require latency alignment at
their merge. Offline rendering has no live hardware return and should state
that clearly and use an explicit bypass policy.

Acceptance checks: mono and stereo loopback on a multichannel interface;
send/return port isolation; impulse alignment at 44.1/48/96 kHz and several
buffer sizes; bypass and dry/wet alignment; device disconnect/reconnect;
presets and undo; parallel lanes; no audio-thread waits or allocations; and
no audible feedback from invalid routing. Then test a real external unit.

## AU and VST3 builds

In a DAW the host, not the plug-in, owns the physical interface. The current
processor only exposes a main input and output. JUCE and Audio Units allow
additional buses, so an optional external-send output bus and external-return
input bus are technically possible. The host must expose and route both buses
to hardware; this must be checked per DAW and plug-in format. A working
standalone insert does **not** imply that AU/VST3 hardware routing works.
See [JUCE `AudioProcessor` bus handling](https://docs.juce.com/master/classjuce_1_1AudioProcessor.html)
and [Apple's Audio Unit bus model](https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/AudioUnitProgrammingGuide/TheAudioUnit/TheAudioUnit.html).

For Logic Pro, the supported immediate route is Logic's own
[I/O utility](https://support.apple.com/en-ca/guide/logicpro/lgcef2d8c7d2/mac),
which selects hardware send/return ports and has a latency ping. That places
the hardware effect in Logic's channel routing, not inside a zynamfx block.
Research the target DAW's bus routing and offline-bounce behavior before
promising an in-chain AU/VST3 insert.

## Open decisions for implementation

- Confirm the standalone holder's actual channel mapping with a 4-in/4-out
  loopback; choose between extending it and a custom callback on evidence.
- Define where the hardware-rate send/return crosses the 48 kHz chain domain
  and how much fixed callback delay the pipeline adds.
- Decide whether the first insert is mono-only or mono/stereo; keep the data
  model extensible but validate only a supported configuration.
- Test AU and VST3 extra-bus routing in each intended host before scoping
  format-specific support or reporting latency to those hosts.
