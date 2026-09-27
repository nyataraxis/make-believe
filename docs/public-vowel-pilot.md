# Public matched-vowel research pilot

This experiment exercises the full Phase 0 formant path with real male and female vowel recordings from the [SpAA-LAB Vowels-Recordings repository](https://github.com/SpAA-LAB/Vowels-Recordings), also archived on [Zenodo](https://zenodo.org/records/17611711). The source language is unspecified, so the research profiles use `und`. No source WAV is committed. [SHA-256 hashes and preprocessing](public-vowel-inputs.json) pin the ten inputs.

Each original is a 44.1 kHz stereo WAV with voice in the right channel and a near-silent left channel. The pilot selects the right channel, converts int16 to float, and resamples to 48 kHz. Five actual vowel labels are retained for each speaker. The five recordings used to fit the preset are also used for the in-sample comparisons below; the separate leave-one-vowel-out column excludes that vowel from the global formant fit. The label-aware oracle uses the known vowel label and is not a proposed live implementation.

Reproduce from the locked Python environment in a new output directory:

```powershell
.\.venv\Scripts\python.exe scripts\public_vowel_pilot.py outputs\my-vowel-pilot
```

The script downloads and verifies ten WAV hashes, profiles both speakers, fits and compiles a preset, renders A–D comparisons for all five vowels, renders a label-aware vowel oracle, runs leave-one-vowel-out formant fitting, and writes `pilot-report.json`. Add `--rubberband C:\path\to\rubberband.exe` to include the optional experiment-E utility baseline for `/a/`. The development-host results are in `outputs/public-vowel-final/`; an earlier `outputs/public-vowel-reproduction/` also includes the Rubber Band baseline.

## Measured F1–F3 error

Numbers are mean absolute percentage error of Praat's F1, F2, and F3 medians against the target profile's measured medians. WORLD resynthesizes all variants at the source duration. Low numerical error is a diagnostic, not a listening judgment. The in-sample global warp was bounded to 0.75–1.35×, kept monotonic, and tapered toward unity above 4 kHz after an earlier interpolated curve exceeded its declared maximum.

| Vowel | Pitch only | Global scale | Global warp | Full warp + EQ | Global warp, vowel held out | Known-vowel oracle |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| /a/ | 19.5% | 14.2% | 10.3% | 13.1% | 5.4% | 9.4% |
| /e/ | 19.6% | 6.3% | 5.0% | 5.4% | 3.9% | 1.3% |
| /i/ | 26.2% | 24.4% | 28.4% | 33.9% | 47.6% | 8.9% |
| /o/ | 10.3% | 11.7% | 5.8% | 4.1% | 5.5% | 4.7% |
| /u/ | 8.5% | 32.7% | 40.0% | 64.4% | 29.6% | 24.2% |

The source and target median F0s were 145.4 and 217.2 Hz. The fit explicitly rejected the measured formant pairs 332→261 Hz (`/i/` F1) and 670→1044 Hz (`/a/` F1) as global-ratio outliers. It also records F1 ratio range 0.78–1.56 and F2 range 0.85–1.25 as consistency warnings. These are real conflicting vowel moves: one global frequency map cannot shift the same low-frequency region upward for `/a/` and downward for `/i/`. The known-vowel oracle sharply improves `/i/`, but `/u/` still worsens. On `/u/`, its intended F2 move from about 797 to 679 Hz was measured by Praat near 1,087 Hz after synthesis, consistent with a merged or misidentified resonance; inspection and listening are needed to distinguish those causes.

The full warp plus EQ greatly worsens measured `/u/` formants, so its spectral correction should not be treated as a validated quality enhancement. The public samples prove the analysis and fitting code runs on real vowels and reveal a concrete model limitation. They do not satisfy the Phase 0 exit criterion: the intended user's voice, connected speech, consonants, JP/RU/EN presets, and subjective feminine quality remain untested.
