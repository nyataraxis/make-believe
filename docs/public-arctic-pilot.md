# Public speech pilot: BDL to SLT pitch mapping

This is a reproducible engineering check on real speech, not a completed voice calibration. The samples come from the [CMU ARCTIC dataset mirror](https://huggingface.co/datasets/MikhailT/cmu-arctic); the [CMU paper](https://www.cs.cmu.edu/~awb/papers/ssw5/arctic.pdf) describes the corpus. No corpus WAV is committed. [Input hashes](public-arctic-pilot-inputs.json) pin the exact 16 kHz WAVs used here.

Three utterances per speaker (`a0001`–`a0003`) were resampled to 48 kHz and profiled without vowel labels. The `--pitch-only-pilot` fit maps source BDL log-F0 quantiles to target SLT quantiles and explicitly disables the formant and spectral transforms. Utterances `a0004`–`a0008` were held out. The two speakers read matching text, but their recordings are not time aligned, so framewise pitch error is measured against the preset's mapped source contour rather than the target recording.

Reproduce from a Rubber Band enabled native build and the locked Python environment:

```powershell
.\.venv\Scripts\python.exe scripts\public_arctic_pilot.py outputs\my-arctic-pilot --morph-raw .\build-rubberband\Release\morph_raw.exe
```

The script downloads only the 16 pinned examples, verifies every SHA-256 hash, builds source and target profiles, compiles a native preset, renders five held-out native WAVs, renders WORLD comparisons for `a0004`, and writes `pilot-report.json`. Add `--rubberband C:\path\to\rubberband.exe` for the optional utility comparison. Choose a new output directory for each run; the script does not overwrite previous audio. On the development host the result is in `outputs/public-arctic-reproduction/`.

## Measured on the development host

WORLD StoneMask F0, 5 ms frames, 48 kHz mono. Median absolute mapped-pitch error uses frames where both the expected mapped source and native output are voiced. A global pitch-only preset was fitted from `a0001`–`a0003`; the held-out table follows. These metrics do not rate naturalness or gender perception.

| Utterance | Source median F0 | Paired target median F0 | Native median F0 | Native mapped-pitch error | Voicing mismatch |
| --- | ---: | ---: | ---: | ---: | ---: |
| a0004 | 119.4 Hz | 187.6 Hz | 161.5 Hz | 87.6 cents | 8.3% |
| a0005 | 126.6 Hz | 192.6 Hz | 174.2 Hz | 65.5 cents | 3.1% |
| a0006 | 122.2 Hz | 187.5 Hz | 173.8 Hz | 53.4 cents | 5.9% |
| a0007 | 129.7 Hz | 189.4 Hz | 177.0 Hz | 73.9 cents | 7.9% |
| a0008 | 122.7 Hz | 188.0 Hz | 166.1 Hz | 103.5 cents | 5.9% |

The original native tracker threshold reported F0 on only 87 of 242 blocks in `a0004` and produced a 310.5-cent median mapped-pitch error. A 0.35 YIN threshold raised that to 152 blocks and reduced the error to 87.6 cents. The wider threshold passed a deterministic broadband-noise rejection check. A fixed-length YIN difference was also tried and reverted: it increased mapped-pitch error on all five held-out utterances, though it improved some voicing-mismatch scores. The committed estimator uses the measured better setting for this corpus; broader voices and noise conditions still need evaluation.

The native render of `a0004` had 2,513 samples of reported startup delay (52.35 ms at the initial ratio), zero nonfinite samples, and zero samples reaching its hard output clamp. The WORLD pitch-only variant had an 8.7-cent mapped-pitch error on `a0004`. That gap and the lower native median F0 show that the causal tracker and baseline backend need more work. The public pilot cannot test the required matched-vowel formant fit, subjective feminine quality, live dropouts, or physical round-trip latency.

## Short versus medium analysis window

The same pinned pilot was repeated with `--window medium`. Rubber Band reported 2,513 samples (52.35 ms) of startup delay for short and 3,025 samples (63.02 ms) for medium at this preset's initial ratio. Mean held-out median absolute mapped-pitch error was 76.8 cents for short and 81.9 cents for medium; mean voicing mismatch was 6.2% and 5.8%, respectively. Both rendered all five files with zero nonfinite or clamped output samples. These objective results favor short for latency and pitch error, but a listening comparison is still required to judge vowel quality and artifacts. The live diagnostic defaults to short; medium is available in the offline native renderer for research.
