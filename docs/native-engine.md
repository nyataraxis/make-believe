# Native engine foundation

Phase 1 currently contains a C++20 core with a YIN pitch tracker, causal pitch stabilizer, quantile mapper, fixed-capacity SPSC audio rings, a compiled-preset loader, and a separate Rubber Band Live adapter. A standalone Windows process contains an event-driven WASAPI capture → DSP → render diagnostic path. Device enumeration and format inspection ran on the development PC; live microphone-to-cable streaming has not yet been tested. There is no desktop UI or user-facing bypass yet.

## Build

The pure core requires CMake and a C++20 compiler:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

To build the optional Rubber Band adapter, acquire a Rubber Band Library 4 source checkout separately, then set `RUBBERBAND_SOURCE_DIR`:

```powershell
cmake -S . -B build-rubberband -DCMAKE_BUILD_TYPE=Release -DRUBBERBAND_SOURCE_DIR=C:\path\to\rubberband -DBUILD_BENCHMARKS=ON
cmake --build build-rubberband
ctest --test-dir build-rubberband --output-on-failure
.\build-rubberband\offline_dsp_benchmark.exe 1000
```

The adapter compiles Rubber Band's [single-file source](https://github.com/breakfastquay/rubberband/blob/default/COMPILING.md) with built-in FFT and resampler. The dependency is external and the backend remains optional. Check the [upstream licence](https://www.breakfastquay.com/rubberband/index.html) before distributing an application that links it.

The `morph_raw` executable runs the same `RealtimeMorphProcessor` as the Windows diagnostic stream on raw 48 kHz mono float32 blocks. `voiceprof native-render` accepts a WAV, handles resampling and startup-delay alignment, and writes a WAV for objective comparison. See the [public ARCTIC pilot](public-arctic-pilot.md) for a held-out real-speech check.

## Windows diagnostic stream

List active endpoints and their shared-mode formats:

```powershell
.\build-rubberband\audio_devices.exe --formats
```

The live diagnostic accepts explicit endpoint IDs from that output. The first ID must be a capture endpoint and the second a render endpoint, such as the input side of an existing virtual audio cable. It requires 48 kHz, one or two channels, and 32-bit float on both endpoints. The process does not record raw audio to disk.

```powershell
.\build-rubberband\morph_stream.exe "<capture-id>" "<render-id>" --preset outputs\preset.vmp 5
```

`--fixed <pitch-ratio> <formant-ratio> <seconds>` is a diagnostic alternative. Fixed pitch is deliberately not the normal speech strategy. Streams stop after 1–60 seconds and print captured/rendered frame counts, input overflow, output underrun, capture discontinuities, input/output nonfinite and clamp counts, and Rubber Band's reported algorithmic delay. Captured float samples are sanitized before they enter the DSP ring. The output reader applies bounded ±1000 ppm clock-drift compensation around a 1,024-frame target fill. The stream is not yet ready for unattended use: it lacks device recovery, precisely aligned voiced/unvoiced blending, and safe live preset switching. It has not been run against a live endpoint in this session.

## Measured on the development host

Release build, Windows x64, Zig/Clang 20.1.2, 48 kHz mono, 512-frame blocks, Rubber Band source checkout `e4296ac`:

| Initial pitch ratio | Reported startup delay |
| ---: | ---: |
| 1.0 | 44.0 ms |
| 1.5 | 52.875 ms |
| 1.7 | 55.854 ms |
| 2.0 | 60.0 ms |

Three 1,000-block synthetic-tone benchmark runs of the shared `RealtimeMorphProcessor` measured 2.01–2.09 ms p99 DSP time with the short analysis window and 2.02–2.16 ms with the medium window, against a 10.67 ms block duration. For the benchmark preset, reported startup delay was 56.29 ms short and 66.96 ms medium; the exact delay depends on initial pitch ratio. These measurements exclude devices, scheduling, virtual cable buffering, and real speech. The startup delay alone exceeds the design's speech and singing algorithmic latency goals at large pitch ratios, so this backend is a quality baseline while lower latency approaches remain research work. The [upstream API](https://github.com/breakfastquay/rubberband/blob/default/rubberband/RubberBandLiveShifter.h) also warns about substantial startup delay and requires ratio changes to occur outside concurrent `shift()` calls.

## Current signal path and limits

`YinTracker` processes a 2048-sample causal window every block. `PitchStabilizer` rejects low-confidence candidates, corrects isolated octave jumps, applies a short causal median and smoother, and fades morph strength when pitch disappears. `PitchMapper` maps F0 with seven monotonic log-frequency knots. The compiled preset retains source and target profile digests and a coarse global formant ratio. The adapter applies pitch and independent formant ratios before `shift()` on the same DSP thread, with no allocations or blocking calls in its `process()` implementation.

This path fades morph strength when F0 becomes unreliable, but it does not yet separate periodic and noise components or protect consonants with an aligned dry path. It clamps nonfinite and out-of-range output samples, a safety fallback that does not replace a transparent limiter. Clock-drift correction is provisional and untested on live devices. The static benchmark is not a no-xruns or latency acceptance test.
