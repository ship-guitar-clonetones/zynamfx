# Transpose: low-latency pitch-shift research

Record of the engine choice behind the Transpose effect. The feature
started from Vivek Radhakrishna's contribution
([#133](https://github.com/tone-3000/tone3000-plugin/pull/133)), which ran
Signalsmith Stretch (a phase vocoder) at 30 / 60 / 100 ms windows. Played
live against a commercial input transpose it was late (60 ms; ~20 ms is
the reference feel) and unstable on bass. This document lists the
candidates that were benchmarked, the scorer, the iterations that failed,
and the engine that shipped: a correlation-spliced delay line with onset
re-sync. Listening tests agreed with the numbers (30, 40 and 60 ms splicer
buffers all pass; the 60 ms vocoder does not).

Assets referenced below live in `plugin/docs/transpose/`. The bench harness
(C++ candidates + Python scorer) is described in enough detail to rebuild;
it was developed outside the repo.

## The problem

A real-time pitch shifter for guitar and bass DI has three requirements
that pull against each other:

- **Latency.** A player feels anything over ~20-25 ms; 60 ms is too late
  for tight rhythm parts.
- **Bass.** A low E on a 4-string is 41 Hz, a 24 ms period. Anything that
  needs a few periods of context needs a 50-100 ms window, outside the
  latency budget.
- **Transients.** Palm mutes, pick attacks and slap have to arrive
  unsmeared and on time.

Phase vocoders trade window length for pitch accuracy. At 30 ms the
frequency resolution is too coarse to separate bass partials: the pitch
wanders by 10-15 cents and inter-partial sidebands appear. At 60 ms the
output is cleaner but still smears bass and is already late. No window
size satisfies both constraints.

## Literature and prior art

- Signalsmith, [Four Ways To Write A Pitch-Shifter](https://signalsmith-audio.co.uk/writing/2023/stretch-design/):
  the design write-up behind the library the first version used. A survey
  of delay-line vs. granular vs. vocoder trade-offs; it concludes the
  vocoder wins on quality when latency is not the constraint.
- N. Juillerat, S. Müller Arisona, S. Schubiger-Banz, *Low Latency Audio
  Pitch Shifting in the Time Domain* (ICALIP 2008) and *Low Latency Audio
  Pitch Shifting in the Frequency Domain* ("Ocean", ICALIP 2010, reference
  Java implementation from MIT). The 2010 algorithm is an STFT shifter
  designed to survive very small FFT sizes by moving bins with per-bin phase
  correction rather than phase-vocoder accumulation. Ported and benchmarked
  (see below); eliminated.
- Eventide H949 "de-glitch" (1977): the origin of the idea used here. A
  pitch shifter is a delay line read at a different rate than it is written,
  so the read tap drifts and must periodically jump back. The H949's
  contribution was picking *where* to jump by waveform correlation so the
  splice lands on a matching phase of the signal. The same lineage runs
  through the Digitech Whammy / Drop family.
- Digitech Drop (polyphonic algorithms from the Whammy DT, 44.1 kHz,
  published frequency response 20 Hz-11 kHz when engaged, "should be first
  in the chain"): the band-limited response suggests a time-domain design,
  and "first in chain" is about feeding the shifter a clean DI. The common
  hardware reference for a low-latency transpose.

## Candidates

All candidates share one interface (`process(in, out, n)` at a fixed pitch
ratio, 128-sample blocks, 48 kHz) and are run through the same scorer.

| name | what it is | nominal latency |
|---|---|---|
| `ss30`, `ss60` | Signalsmith Stretch, 30 / 60 ms window, 4x overlap (the shipped v1 engine) | 30 / 60 ms |
| `ocean1024` | Juillerat 2010 "Ocean" STFT shifter, N=1024, ported from the MIT reference | 21 ms |
| `dual20` | classic two-tap crossfading delay-line shifter (Dattorro / Whammy style), 20 ms loop | 10 ms |
| `splice<N>` | correlation-spliced single-tap delay line, N ms buffer (below) | (2 + N) / 2 ms |

### Correlation-spliced delay line (shipped)

One read tap runs through a ring buffer at the pitch ratio, so its delay
behind the write head drifts linearly between `dMin` (2 ms) and `dMax`
(the buffer size: 20/30/40/60 ms). When it reaches the end of that range it
jumps back. Rather than a fixed jump (the periodic flutter of a two-tap
shifter) it scores every candidate delay in the legal range by how well its
recent waveform matches the current one (normalised cross-correlation over
a 20-25 ms window), then crossfades to the chosen position with a 6 ms
raised-cosine fade while both taps keep running at the pitch ratio. With
the splice placed where the two waveforms agree, the crossfade is nearly
inaudible on periodic material.

Two additions to the basic design:

1. **Rate-cost lag selection.** The best-correlated position is usually
   the nearest one, one period away. On an E1+B1 fifth that chose 35 sample
   jumps and spliced 133 times per second, and the fades alone produced
   +12 dB of sidebands. Scoring candidates by damage-per-splice times
   splices-per-second, `(1 - ncc) / jump`, prefers large jumps with good
   enough correlation: 3.3 splices/s and -1.4 dB sidebands on the same test.
2. **Onset re-sync.** On a detected pick attack the tap is spliced to the
   freshest end of the buffer (within `dMin` to `dMin + 4 ms`,
   correlation-chosen, 2 ms fade). The attack arrives 2-7 ms after the dry
   one wherever the tap was, so the felt latency is set by the attacks, not
   by the (2 + N)/2 mean. Steady tone between attacks drifts as usual.

The delay track shows both on a real riff:

![Read-tap delay of the 40 ms splicer on Power - Guitar, -2 st](transpose/guitar_delay_track.png)

Ramps are the tap drifting at the pitch ratio (2^(-2/12) - 1 = -11%
speed, so a ~1.5 s run from 2 to 40 ms); vertical drops to the 2-6 ms
floor line up with the pick attacks in the waveform above; the drops that
land mid-range are correlation splices choosing a big jump.

#### Onset detector iterations

A false trigger on a sustained note is a gratuitous splice; a missed onset
is a late transient. Variants tried, in order:

1. 2 ms / 50 ms energy ratio: false-triggered constantly on bass (142
   splices/s on Downtown).
2. Instant-attack peak follower: never triggered, the follower tracks the
   attack itself.
3. Compare against energy 5 ms earlier: missed palm-muted riffs where the
   previous note is louder than the new attack.
4. High-passed (600 Hz) energy vs. its recent minimum: re-triggered every
   40 ms (refractory period) on steady low notes, because a low note's
   waveform contains periodic HF pulses that look like tiny attacks.
5. Peak-hold only: safe, but caught 36 of 296 attacks in the Power riff.
6. **Final:** 600 Hz HPF energy with 2 ms smoothing, held in 1 ms cells
   for the last 5-49 ms. Trigger when it rises 9 dB over the recent
   *minimum* AND 1 dB over the recent *maximum* (the second condition is
   what rejects periodic HF pulses: they never exceed their own recent
   peaks). 40 ms refractory; the history is reset to the current level on
   trigger. Result: 0-4 false triggers per 3 s steady tone, median attack
   latency 7 ms on Mayer.

## The benchmark

`bench` is a CLI that reads raw float32 mono on stdin, runs one candidate at
one ratio in 128-sample blocks, writes the output to stdout, and reports
`latency cpu% splices onsetSplices` on stderr (plus a per-sample record of
the actual read-tap delay via `DELAY_OUT=<file>`). `metrics.py` drives it
over a corpus and scores the results; `render_listen.py` writes 24-bit
listening sets (`dry` + every candidate, time-aligned by nominal latency)
per DI and interval.

### Corpus

- Synthetic: single plucked notes E1, A1 (bass), E2, A2, D3, G3, E4; a power
  chord on E2; open E major; a major third E2+G#2; bass dyads E1+B1 and
  E1+A1. Synthetic notes give known harmonics so sidebands and pitch can be
  measured exactly.
- Real DIs: all 33 files from the
  [neural-amp-modeler-wasm inputs](https://github.com/tone-3000/neural-amp-modeler-wasm/tree/main/ui/public/inputs)
  (29 guitar, 4 bass; 48 kHz, 24-bit mono). Summary rows below use the
  subset Mayer, Power, Pluck, Metalcore, Brit, Fast Thrash, Progression,
  Smooth (guitar) and Downtown, Frogger, Smokin', Rollin' (bass).
- Intervals: -2, -4, -12 semitones (the drop-tuning use cases), -5 on bass.

### Metrics

All wet signals are aligned to the dry by the candidate's nominal latency
before scoring.

- **warble (dB):** RMS of the detrended dry-to-wet envelope ratio in
  sustained regions. Measures the AM that crossfades or vocoder phase errors
  impose. The dry is first re-timed by the splicer's actual per-sample
  delay so the intentional tap drift is not counted as an artifact.
- **sideband (dB):** energy off the expected shifted harmonics vs. on them
  (harmonics found within +/-26 cents of the ideal), on steady synthetic
  notes and chords. The single most audible number: positive means the junk
  is louder than the note.
- **pitch (cents):** standard deviation of the frame-wise f0 error on steady
  single notes, by autocorrelation over frames of at least four periods
  (shorter frames mis-measured a shifted E1).
- **onset (ms):** arrival of each attack relative to nominal latency, by
  cross-correlating dry and wet onset-strength envelopes within +/-45 ms.
  Median is trustworthy; p90 sits around 37 ms for every candidate including
  constant-delay ones, so it is a measurement artifact and is not reported
  here.
- **atk:** attack-time ratio wet / dry (1.0 = transient intact, >1 =
  smeared).
- **cpu%:** share of one core at 128-sample blocks, single thread.
- **splices/s** for the splicer.

### Results

Means over the corpus. Lower is better for warble, sideband and pitch;
"bass" columns are the E1/A1 synthetic cases; "DI onset" is the median
attack arrival relative to nominal latency (negative = earlier than the mean
latency thanks to onset re-sync).

| algo | st | latency ms | cpu% | synth warble | synth sideband dB | pitch cents | bass warble | bass sideband dB | DI warble | DI onset ms | DI atk |
|---|---|---|---|---|---|---|---|---|---|---|---|
| ss60 | -2 | 60 | 0.34 | 0.48 | -8.2 | 4.8 | 0.71 | **+5.3** | 0.47 | -0.9 | 1.11 |
| ss30 | -2 | 30 | 0.36 | 0.35 | **+2.3** | 11.2 | 0.61 | **+10.9** | 0.41 | -0.3 | 1.09 |
| splice20 | -2 | 11 | 0.65 | 0.18 | -14.6 | 2.0 | 0.26 | -4.8 | 0.19 | -1.5 | 0.95 |
| splice30 | -2 | 16 | 0.68 | 0.15 | -19.7 | 0.8 | 0.27 | -6.3 | 0.14 | -4.9 | 1.00 |
| splice40 | -2 | 21 | 0.62 | 0.13 | -21.0 | 0.7 | 0.18 | -7.2 | 0.11 | -6.7 | 1.00 |
| splice60 | -2 | 31 | 0.54 | 0.91* | -23.1 | 1.1 | 1.26* | -11.4 | 0.10 | -8.9 | 1.01 |
| ss60 | -4 | 60 | 0.33 | 0.56 | -6.8 | 26.3 | 0.82 | **+12.2** | 0.59 | -1.7 | 1.19 |
| ss30 | -4 | 30 | 0.36 | 0.43 | **+8.5** | 30.0 | 0.67 | **+12.5** | 0.48 | -0.7 | 1.14 |
| splice20 | -4 | 11 | 1.09 | 0.28 | -11.9 | 24.1 | 0.45 | -1.3 | 0.24 | -1.4 | 1.08 |
| splice30 | -4 | 16 | 1.37 | 0.15 | -16.5 | 2.7 | 0.28 | -1.7 | 0.19 | -3.2 | 1.09 |
| splice40 | -4 | 21 | 1.33 | 0.21 | -17.8 | 2.1 | 0.30 | -4.2 | 0.18 | -4.8 | 1.09 |
| splice60 | -4 | 31 | 1.17 | 0.84* | -20.0 | 1.5 | 0.11 | -7.7 | 0.14 | -6.2 | 1.08 |
| ss60 | -12 | 60 | 0.32 | 0.76 | -1.0 | 2.6 | 1.03 | +2.5 | 0.96 | -2.2 | 1.38 |
| ss30 | -12 | 30 | 0.34 | 0.64 | +7.2 | 12.6 | 0.97 | +4.7 | 0.74 | -0.2 | 1.24 |
| splice30 | -12 | 16 | 3.21 | 0.27 | -12.5 | 1.3 | 0.41 | -5.8 | 0.32 | -0.6 | 1.31 |
| splice40 | -12 | 21 | 3.35 | 0.27 | -12.3 | 2.3 | 0.60 | -4.2 | 0.25 | -0.7 | 1.32 |
| splice60 | -12 | 31 | 3.48 | 0.18 | -14.1 | 1.5 | 0.22 | -6.0 | 0.25 | -1.6 | 1.31 |

\* `splice60` synthetic warble is an artifact of the warble metric's
detrending window colliding with the long delay ramp; its DI warble (the
number that matters) is the best of the set.

Earlier-round candidates, -2 st, same scorer before the delay-aware warble
fix (so their warble is not comparable to the table above; sidebands and
pitch are):

| algo | latency ms | synth sideband dB | pitch cents | bass sideband dB |
|---|---|---|---|---|
| ocean1024 | 21 | +15.0 | 24.6 | +17.6 |
| dual20 | 10 | -1.3 | 0.6 | +5.4 |

Ocean scored worst on every axis at a size that met the latency budget.
The dual-tap shifter has exact pitch (it never splices) but its constant
crossfading is the classic flutter, worst on bass where a 20 ms loop cannot
hold a period.

Observations:

- The splicer scores better than the vocoder on every metric at every
  interval at a third to a quarter of the latency. At -2 st, `splice40` vs
  `ss60`: pitch 0.7 vs 4.8 cents, sidebands -21 vs -8 dB, bass sidebands -7
  vs +5 dB, DI warble 0.11 vs 0.47 dB.
- Positive sideband levels (the off-harmonic energy louder than the note)
  occur only for the vocoder, and only on bass. The splicer keeps bass
  sidebands negative at every buffer size >= 30 ms.
- Onset re-sync: DI onsets arrive 5-9 ms before the mean latency and the
  attack-time ratio stays at 1.0 (vocoder: 1.1-1.4).
- CPU is higher than the vocoder (0.6% vs 0.3% at -2 st, 3.4% at -12 st)
  because the table's correlation search is exhaustive. The shipped engine
  uses a coarse-to-fine search (see Implementation): 0.3% / 0.9% with
  identical results.
- **20 ms** (`splice20`, 11 ms mean latency) works for guitar: pitch and
  sidebands are a few dB behind 30 ms and it passed listening on the
  guitar sets. It does not work for low bass: a 20 ms buffer cannot hold an
  E1 period (24 ms), so the E1 cases go from -14 dB to +9 dB sidebands and
  the -4 st pitch column fails on that note. 25 ms sits between (E1
  sidebands +1.5 dB). Bass needs 30 ms or more, so the Latency control
  stays, labelled by buffer size, with 20 ms as a guitar-only setting.

### Spectrograms

Downtown - Bass, -2 st, 2.5 s excerpt, 0-1.2 kHz. Dry (top), vocoder
(middle), splicer (bottom). The vocoder fills the space between partials
with noise, most visibly at 400-800 Hz and around each attack; the splicer
keeps each partial as a line, shifted down 2 st:

![Downtown - Bass spectrograms](transpose/bass_spectrogram.png)

Power - Guitar, -2 st, 0-3 kHz:

![Power - Guitar spectrograms](transpose/guitar_spectrogram.png)

### Audio examples

5 s excerpts, 48 kHz 16-bit, peak-normalised as a set so levels match:

- Guitar (Power - Guitar, -2 st):
  [dry](transpose/guitar_-2st_dry.wav),
  [Signalsmith 60 ms](transpose/guitar_-2st_ss60.wav),
  [splicer 40 ms](transpose/guitar_-2st_splice40.wav)
- Bass (Downtown - Bass, -2 st):
  [dry](transpose/bass_-2st_dry.wav),
  [Signalsmith 60 ms](transpose/bass_-2st_ss60.wav),
  [splicer 40 ms](transpose/bass_-2st_splice40.wav)

The wet files are aligned to the dry by nominal latency, so A/B'ing them in
a DAW compares artifacts, not delay.

## Implementation

The shipped engine is `plugin/include/Transpose.h` /
`plugin/src/Transpose.cpp`; Signalsmith Stretch and Linear are not
dependencies. Where it differs from the bench prototype:

- **Coarse-to-fine lag search.** Candidates are scored every 4 samples (at
  48 kHz; the step scales with the rate) and the best is refined to the
  sample. Identical sidebands, pitch and onset figures to the exhaustive
  search at a third of the cost (0.3% vs 0.7% of a core at -2 st, 0.9% vs
  3.0% at -12 st).
- **Spread lag search.** The prototype ran each search as one burst inside
  a single sample (400-800 us at 48 kHz for 30-60 ms buffers), which
  exceeds a 16-sample host buffer's 333 us budget and clicked on sustained
  notes. A drift splice is predictable (the tap approaches the buffer end
  at a known rate), so the search starts 4 ms early and scores a few
  candidates per sample; candidate positions are shifted by the drift the
  tap makes during the lead so the landing range is unchanged. The
  correlation runs on a mirrored ring (every sample written twice) so each
  window is contiguous and the dot product vectorises. Worst block at 16
  samples: 13-100 us; average CPU 0.15-0.35%. Only the onset re-sync (a
  4 ms candidate range) searches at once.
- **Fixed floor.** The prototype moved the 2 ms floor out for upshifts;
  the plugin keeps the reported latency independent of the knob and moves
  the splice trigger out by the distance an upshift tap gains during a fade
  (`max(floor, fade shrink)`; summing them instead landed +12 st on the
  20 ms buffer 9 cents low).
- **Stereo** shares one control path (detector and lag search on the
  channel mean) and one tap position; each channel has its own ring, so
  the image cannot smear.
- **Tonality** keeps its parameter and knob as an LR4 crossover after the
  shifter: the band below the limit from the shifted signal, the band above
  from the dry, delayed by the floor so it lands with the re-synced
  attacks. The crossover output is blended against the plain shift. Both
  filters run whether or not the limit is engaged (four biquads a channel)
  so engaging it does not start them cold. The rings hold full-band audio,
  so the lag search is unaffected.
- **Latency control** = buffer size, four detents 20 / 30 / 40 / 60 ms,
  read out as the latency each reports ((2 + N) / 2 = 11 / 16 / 21 / 31 ms).
  Default 30 ms.
- **Transitions blend** over 25 ms, like the image decks'. Power
  crossfades dry and wet; the engine keeps running until the fade-out
  lands, then the processor bypasses it, so off stays bit-exact. The
  blend-in waits one floor delay after the reset, since the tap reads the
  cleared ring until then and ramping onto that edge stepped at 8% wet. A
  window change keeps the rings (sized for the largest window) and drops
  only the pending search; a tap outside the new range splices back in
  through an ordinary crossfade. Tonality engage relies on the always-warm
  filters above: a cold highpass passed the mid-cycle start into the dry
  ring as a step.
- Verified by wrapping the plugin class as a bench candidate: every metric
  matches the `splice` prototype at the same buffer within noise.

Unit tests (`test/src/transpose_tests.cpp`): latency figure matches the
engine at every window and rate; 0 st is a pure delay at the floor at unity
gain; octave / fine-trim shifts land on frequency; shifted tones keep unity
gain at every window (splices land on whole periods); the tonality limit
passes a 6 kHz partial unshifted while shifting a 440 Hz one; a pick attack
after a drifting sustain arrives within floor + re-sync span + fade
(measured 2.6-7 ms); stereo channels stay exactly proportional through every
splice; +12 st on the 20 ms buffer stays within 3 cents; mono buffers,
rate / block / window changes and ±12 st stay finite; power on/off, a
60 to 20 ms window change and a tonality engage all pass a 220 Hz tone
with no sample step over twice the tone's own and no hole, and a powered-off
engine reports itself stopped within 50 ms.

## Open items

- Upshift attacks: the re-sync target sits at the front of the buffer and
  an upshift tap runs toward the write head, so at +12 st it hits the
  guard within ~4 ms and splices again in the attack's decay. Not yet
  evaluated by listening; a ratio-aware re-sync target is the fix if it is
  audible.
- Long-term: a polyphonic-aware lag search (score per-band) for chords with
  very different fundamentals, which is where the remaining -12 st
  sidebands live.
- The bench itself lives outside the repo (`/tmp/t3k-bench`: `bench.cpp`,
  `shifters.h`, `metrics.py`, `render_listen.py`, `doc_assets.py`, plus a
  `plugin_shifter.h` wrapper that scores the shipped class).
