# Roadmap and status

Source of truth for the complete technical scope: [Technical design v0.1](docs/technical-design.md), especially sections 57–61. Status here distinguishes code from measurements and listening evidence.

Last updated: 2026-09-27.

## Phase 0 — acoustic research harness (in progress)

- [x] Preserve the full technical design and establish an executable Python project.
- [x] Extract Praat pitch, formants, and harmonicity from labeled calibration WAVs.
- [x] Extract WORLD F0, spectral envelope, and aperiodicity offline.
- [x] Build separate source and target acoustic profiles with WAV hashes and basic input checks.
- [x] Fit monotonic quantile pitch mapping and matched-vowel vocal-tract warp.
- [x] Render offline pitch, formant, and spectral comparison audio.
- [x] Produce signal, pitch, and optional labeled-vowel F1–F3 reports plus listening comparisons for experiments A–D.
- [x] Add an optional Rubber Band offline pitch baseline for experiment E; local 4.0.0 utility passed synthetic integration test.
- [x] Run a reproducible public-speech pitch-only pilot with pinned input hashes and five held-out utterances; see [pilot evidence](docs/public-arctic-pilot.md).
- [x] Run a hash-verified public five-vowel male→female fit, an A–D objective comparison, and leave-one-vowel-out formant checks; see [vowel evidence](docs/public-vowel-pilot.md).
- [x] Compare Rubber Band Live short/medium window delay and held-out pitch metrics as two experiment-F engineering points.
- [ ] Listen to Rubber Band versus WORLD on real speech for experiment E.
- [ ] Measure the quality/latency frontier for experiment F on the realtime engine.
- [ ] Record real source and target calibration material and evaluate Phase 0 exit criterion: convincing feminine configuration without gross artifacts.

## Phase 1 — realtime baseline (in progress)

- [x] Native C++20 processing interface, optional Rubber Band Live adapter, and fixed-block tests.
- [x] WASAPI endpoint enumeration, mix-format inspection, and a compiled three-thread capture/DSP/render diagnostic path.
- [ ] Run a controlled physical-microphone → virtual-cable session and verify routing, dropouts, and output.
- [x] Classical causal YIN F0 tracker, octave-error stabilization, and quantile mapping in the C++ core.
- [x] Rubber Band Live baseline with independently controlled pitch and formant ratios.
- [x] Compile profile-derived pitch knots and a global formant approximation to a checksummed native preset, and load it outside the DSP loop.
- [x] Share the preset-driven processing path between offline native WAV rendering and the WASAPI diagnostic.
- [x] Add provisional confidence-based morph fading, output sample safety, and bounded output-clock drift correction.
- [ ] Add aligned periodic/noise or dry-path protection, transparent post-processing, and precise control/audio alignment.
- [ ] Add a runtime bypass and verified virtual-microphone output.
- [ ] Measure latency, xruns, and 30-minute operation on reference hardware.

Exit: stable realtime operation, no dropouts, acceptable conversion, measured latency.

## Phase 2 — full calibration (not started)

- [ ] Recording wizard and Japanese, Russian, and English phrase sets.
- [ ] Integrate the existing source and target profiling, preset compilation, and automatic fitting into the recording workflow.
- [ ] New-speaker calibration without manual DSP editing.

## Phase 3 — custom morph engine (not started)

- [ ] Periodicity and harmonic/noise decomposition.
- [ ] Custom envelope warp, pitch processing, and transient preservation.
- [ ] A/B against Rubber Band; replace it only if quality, latency, and stability all improve.

## Phase 4 — singing (not started)

- [ ] Fixed octave and custom semitone modes without note quantization.
- [ ] Vibrato and onset preservation, singing calibration, high-F0 handling.
- [ ] Comfortable live monitoring and streaming of the transformed upper octave.

## Phase 5 — polish (not started)

- [ ] Hotkeys, safe live preset switching, device recovery, and automatic latency measurement.
- [ ] Session recording, advanced diagnostic UI, and crash recovery.

## Cross-phase experiments and gates

- [x] Render and measure experiments A–D on the public vowel tokens and speech pilot: pitch only versus formant work, global versus frequency-dependent warp, entire spectrum versus protected unvoiced WORLD bins, and fixed shift versus quantile mapping. These objective results are not perceptual acceptance.
- [ ] Listen to A–D on matching source/target material, including real connected speech, and select a better transform only after the evidence supports it.
- [x] Render an optional Rubber Band pitch baseline and WORLD target for experiment E.
- [ ] Listen to Rubber Band versus WORLD on real speech for experiment E.
- [x] Compare two Rubber Band Live analysis windows for initial delay, DSP time, and held-out pitch/voicing metrics.
- [ ] Measure experiment F's actual quality and end-to-end latency frontier at 15, 20, 25, 30, 40, 50, and 60 ms; record infeasible budgets explicitly.
- [ ] Maintain a versioned golden regression corpus covering low male pitch, normal and high-energy speech, whisper-like speech, vocal fry, laugh, breath, sibilants, plosives, fast and slow speech, Japanese, Russian, English, and singing. Run it after DSP changes and version the metrics and waveforms.
- [ ] Functional MVP: physical mic to transformed virtual mic, JP/RU/EN speech presets, singing +12 preset, runtime bypass, and hot preset switching.
- [ ] Quality MVP: consonant clarity, no obvious pitch-shifter or chipmunk sound, preserved rhythm and emotional pitch movement, source recognizability, and convincing feminine configuration.
- [ ] Reliability MVP: 30 continuous minutes, zero crashes and NaNs, no persistent underruns, and bounded memory.
- [ ] Physical-loopback latency MVP on the reference setup: speech under 50 ms end to end, singing under 45 ms end to end.

## Current evidence and next action

The Phase 0 command-line harness is implemented. `uv sync --extra dev --extra world --locked` resolved 21 packages. Synthetic profile → preset → WORLD comparisons and the optional official Rubber Band 4.0.0 utility passed local tests. A public CMU ARCTIC BDL→SLT pitch-only pilot pins 16 input WAV hashes, fits from three utterances per speaker, and evaluates five held-out utterances. It disables formant/spectral fitting because those recordings lack sustained-vowel labels. Its native held-out mapped-pitch errors were 53.4–103.5 cents; the `a0004` WORLD pitch-only result was 8.7 cents. A separate public five-vowel male/female corpus exercised full formant fitting. Its global warp improved measured `/a/`, `/e/`, and `/o/` F1–F3 error but worsened `/i/` and `/u/`; the leave-one-vowel-out results confirmed the limitation. These are engineering measurements, not perceptual or personal-voice quality results. Phase 0's personal matched-vowel and listening gates remain open.

Phase 1 C++ code built in Release mode and passed six CTest checks with Rubber Band. The core also builds without Rubber Band. A synthetic preset compiled by Python was read by the C++ inspector, with its CRC corruption test passing. The shared native processor passed a synthetic WAV integration check and rendered five held-out public speech files with zero nonfinite or clamped output samples. Its original YIN setting detected F0 on only 87/242 blocks of `a0004`; the wider 0.35 threshold detected 152/242 and lowered mapped-pitch error from 310.5 to 87.6 cents. A deterministic broadband-noise test passed, but broader voiced/unvoiced quality remains open. WASAPI enumeration and format queries ran on this PC, finding 48 kHz float physical-mic and virtual-cable endpoints. The three-thread `morph_stream` executable builds but has not been started on live endpoints while the device choice is pending. The first remote CI run passed the profiler job and failed both native builds: MSVC lacked the property-key macro declaration and Rubber Band's `std::max` calls collided with a Windows macro. The two source fixes pass local builds and tests on `codex/fix-windows-ci`; a remote MSVC rerun is pending publication of that branch.

Latest local verification: seven profiler tests passed, six CTest checks passed with Rubber Band, five core-only CTest checks passed, and Python source compilation and `git diff --check` passed. The profiler test emitted one nonfatal warning for an extra Rubber Band WAV metadata chunk. The separate final review added validation of nonfinite formant medians before fitting; it has a regression assertion in the passing profiler suite.

Three 1,000-block synthetic benchmark runs of the shared processor measured 2.01–2.09 ms p99 DSP time with the short window and 2.02–2.16 ms with medium. At the public pilot's initial ratio, Rubber Band reported 52.35 ms and 63.02 ms startup delay, respectively, before device buffering. A separate ratio sweep reported 55.854 ms at 1.7× and 60.0 ms at 2.0× using short. This backend cannot meet the design's lower algorithmic latency goals at those settings. Continue real-speech listening and lower-latency backend research. Detailed build and measurement notes: [native-engine.md](docs/native-engine.md).

Next acceptance inputs: source and target recordings with matched sustained vowels plus connected speech for the intended voice; listening ratings of the rendered comparisons; and explicit capture/render endpoint IDs for a short physical-mic-to-virtual-cable diagnostic. After those, measure actual loopback latency and sustained operation. The public pilots and static benchmarks are useful research evidence but cannot certify those gates.
