# Phase 0: offline research harness

This harness measures acoustics from labeled mono WAV files, fits a source-to-target preset, and creates offline WORLD resynthesis comparisons. It requires real, clean source and target recordings for a meaningful voice-quality result.

## Recording manifest

Create separate source and target manifests. Paths are relative to each manifest. Use the same language and mode, and at least three matching vowel labels for formant fitting. Five sustained vowels plus varied speech are preferred. Each WAV must be mono, at least 16 kHz, at least 0.3 seconds, unclipped, and all WAVs in a manifest must share a sample rate. The design recommends 48 kHz dry recordings without automatic gain control, denoising, or compression.

Split recordings longer than 20 seconds into shorter takes for this first harness. WORLD's full-band arrays otherwise grow very large in memory. Automatic long-corpus segmentation is reserved for later calibration work.

```json
{
  "id": "source-ja-speech",
  "displayName": "Source Japanese speech",
  "language": "ja-JP",
  "mode": "speech",
  "formantCeilingHz": 5500,
  "recordings": [
    {"path": "a.wav", "vowel": "a"},
    {"path": "i.wav", "vowel": "i"},
    {"path": "u.wav", "vowel": "u"},
    {"path": "e.wav", "vowel": "e"},
    {"path": "o.wav", "vowel": "o"},
    {"path": "speech.wav"}
  ]
}
```

The `vowel` label must describe the actual sound throughout the usable middle of that recording. Unlabeled speech contributes pitch, spectrum, and voice-source statistics, but never matched vowel formants. Keep source and target recordings and manifests for reproducibility. Profile JSON includes SHA-256 hashes of every source WAV.

`formantCeilingHz` controls Praat's Burg analysis. Inspect vowel tracks and adjust it per speaker if formants are implausible; the default is 5500 Hz. A full preset is rejected unless at least three vowel labels are usable in both profiles. For an explicitly limited pitch-only experiment on unlabeled speech, pass `--pitch-only-pilot` to `fit`. It uses identity formant mapping and zero spectral correction; it is not a substitute for the matched-vowel calibration gate. Reproducible public examples are in the [speech pilot](public-arctic-pilot.md) and [matched-vowel pilot](public-vowel-pilot.md).

## Commands

```powershell
.\.venv\Scripts\voiceprof profile data\source\manifest.json outputs\source.json
.\.venv\Scripts\voiceprof profile data\target\manifest.json outputs\target.json
.\.venv\Scripts\voiceprof fit outputs\source.json outputs\target.json outputs\preset.json --id source-to-target
.\.venv\Scripts\voiceprof compile outputs\preset.json outputs\preset.vmp
.\.venv\Scripts\voiceprof render data\source\speech.wav outputs\preset.json outputs\comparison
.\.venv\Scripts\voiceprof render data\source\a.wav outputs\preset.json outputs\vowel-a --source-profile outputs\source.json --target-profile outputs\target.json --vowel a
.\.venv\Scripts\voiceprof native-render data\source\speech.wav outputs\preset.vmp outputs\native.wav --executable .\build-rubberband\Release\morph_raw.exe
```

The native `preset.vmp` is a 260-byte little-endian research format. It contains seven source and target pitch knots, pitch limits, a global approximation to the fitted formant warp, both profile SHA-256 digests, and a CRC-32. The current Rubber Band baseline does not yet implement the full frequency-dependent warp, spectral correction, source-quality, or dynamics fields from the JSON preset.

For optional experiment E, pass `--rubberband C:\path\to\rubberband.exe` to `render`. The tool writes a per-frame pitch map and adds `rubberband-pitch.wav`. This uses the Rubber Band command-line utility's realtime algorithm with formant preservation, but the utility has no independent target-formant warp control, so it is a pitch backend comparison rather than a complete replacement for the planned Rubber Band Live engine. See the [official command-line options](https://www.breakfastquay.com/rubberband/usage.txt) and [licensing terms](https://www.breakfastquay.com/rubberband/index.html) before redistributing it. The utility is not bundled in this repository.

The render command writes `original.wav`, `fixed-pitch.wav`, `pitch-only.wav`, `pitch-global-formant.wav`, `pitch-formant.wav`, `full-unprotected.wav`, `full.wav`, `report.json`, and `listening.md`. These cover experiments A–D in design section 59. The variants use WORLD resynthesis with identical timing. The formant variant warps voiced spectral envelopes; the full variant also applies a smoothed, bounded spectral correction. The aperiodicity track is preserved, and unvoiced spectral frames are left unwarped in the protected version. With a matching target profile and actual vowel label, the report compares each output's Praat F1–F3 medians with the target's measured medians. Supplying the matching source profile also adds `vowel-oracle.wav`, which uses the known label to estimate a vowel-specific warp and is available only offline. This is an experiment, not a production morph engine.

## Interpretation and limits

- Compare all variants at matched listening volume. Record naturalness, feminine perception, speaker continuity, consonant clarity, and artifacts.
- The fitted formant warp needs matching vowel recordings. It refuses to infer vowel geometry from arbitrary unlabeled speech.
- Praat formant tracks can misidentify a resonance. Inspect the tracks and listen; a low measured error alone does not establish a convincing vocal-tract match.
- The voice-source, dynamics, and sibilance transforms are still unfitted. Their profile fields report that limit explicitly.
- The WORLD report includes measured output F0 error and voicing mismatch. The public pilot also measures native F0 on held-out speech. Formant accuracy, perceptual listening results, and physical latency tests remain open.
- The original input is PCM float WAV in the comparison. Safety gain is applied only when a file would exceed -1 dBFS and is recorded in `report.json`.
