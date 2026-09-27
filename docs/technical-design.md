# Real-Time Parametric Voice Morpher

**Status:** Technical Design v0.1  
**Primary objective:** Ultra-high-quality, low-latency real-time male-to-female voice morphing for speech and singing.  
**Deployment target:** Local desktop application, Windows first.  
**Primary use case:** Streaming, voice chat, recording, karaoke / singing.  
**Out of scope for v1:** neural speaker-identity conversion, cloud inference, TTS, phoneme regeneration, voice cloning.

---

# 1. Product definition

The application is a **real-time parametric voice morphing engine**.

It does not generate speech. It does not replace the speaker's pronunciation, timing, emotion, or linguistic content.

Instead, it continuously transforms the incoming acoustic signal so that:

- speech timing is preserved;
- articulation is preserved;
- source prosody is substantially preserved;
- pitch behavior is mapped into a target range;
- apparent vocal-tract length and formant structure are transformed;
- source spectral characteristics are adjusted toward a target acoustic profile;
- unvoiced consonants, transients and sibilants remain natural;
- latency remains low enough for interactive use.

The system is divided conceptually into:

```text
OFFLINE / CALIBRATION
        ↓
Source Speaker Profile
Target Speaker/Profile
        ↓
Compiled Morph Preset
        ↓
REAL-TIME DSP ENGINE
        ↓
Virtual microphone / monitor / recorder
```

The machine-learning component in v1 is deliberately minimal.

ML may be used offline for analysis or automatic parameter fitting, but the production audio path must remain deterministic DSP.

---

# 2. Hard constraints and known limits

## 2.1 What this system should be able to do extremely well

The intended output is:

> the source speaker, preserving timing and expression, but acoustically transformed toward a convincing feminine vocal configuration.

This is technically realistic.

Experimental work on voice-gender perception consistently identifies **F0 and vocal-tract/formant cues as two of the most important acoustic variables**. Manipulating both is far more effective than changing pitch alone. Spectral characteristics provide additional useful cues.

Therefore v1 prioritizes:

```text
1. Pitch behavior
2. Vocal-tract / spectral-envelope geometry
3. Spectral tilt / timbral shaping
4. Voiced/unvoiced preservation
5. Dynamics and transients
```

rather than simply performing `pitch +N semitones`.

## 2.2 What this system cannot guarantee

DSP-only conversion should **not** be expected to reproduce the identity of a particular target speaker.

For example:

```text
"make me convincingly feminine"
```

is in scope.

```text
"make every phoneme sound indistinguishable from speaker X"
```

is not.

Speaker identity also resides in phoneme realization, glottal behavior, coarticulation, microprosody, consonant production and other characteristics that cannot be reconstructed perfectly by a global parametric transform.

A future neural residual stage may solve part of this problem, but it is deliberately excluded from this design.

## 2.3 Latency reality

Sub-20 ms total latency should **not** be treated as a v1 requirement for adaptive baritone-to-female speech conversion.

For a baritone around 80–120 Hz, one glottal cycle itself lasts approximately:

```text
80 Hz  → 12.5 ms
100 Hz → 10.0 ms
120 Hz →  8.3 ms
```

Reliable F0 estimation normally needs information spanning more than one cycle.

Consequently:

```text
adaptive speech mode:
    realistic target ≈ 30–50 ms end-to-end

excellent implementation:
    potentially lower

fixed-ratio singing mode:
    potentially significantly lower
```

The final number must be measured rather than inferred from buffer sizes.

---

# 3. Design principles

The system follows several architectural rules.

### Real-time path

No:

- filesystem access;
- network access;
- GPU dependency;
- memory allocation;
- blocking locks;
- logging to disk;
- Python;
- garbage collection;
- UI code;
- IPC waits.

may occur in the realtime processing path.

### Offline analysis

Offline processing may be arbitrarily expensive.

It may use:

- Python;
- Praat;
- Parselmouth;
- WORLD;
- numerical optimization;
- ML models;
- GPU acceleration.

### Profiles describe acoustics, not effects

A `SpeakerProfile` describes measured acoustic characteristics.

A `MorphPreset` describes how one profile should be mapped into another.

These must remain separate.

---

# 4. System architecture

```text
┌──────────────────────────────────────────────────────┐
│                    DESKTOP UI                        │
│                 Tauri + React/TS                     │
│                                                      │
│ profile management                                   │
│ calibration wizard                                   │
│ device selection                                     │
│ controls / meters                                    │
│ presets                                              │
└──────────────────────┬───────────────────────────────┘
                       │ control IPC
                       │ never carries audio
                       ▼
┌──────────────────────────────────────────────────────┐
│                 MORPH ENGINE                         │
│                     C++                              │
│                                                      │
│ AudioDeviceManager                                   │
│ StreamClockManager                                   │
│ InputConditioner                                     │
│ RealtimeAnalyzer                                     │
│ PitchMapper                                          │
│ MorphDSP                                             │
│ OutputConditioner                                    │
│ MonitorRouter                                        │
│ Telemetry                                            │
└──────────┬─────────────────────────────┬─────────────┘
           │                             │
           ▼                             ▼
 Physical microphone            Virtual audio endpoint
                                       │
                            OBS / Discord / game / DAW


                       OFFLINE
                         │
                         ▼
┌──────────────────────────────────────────────────────┐
│               PROFILE TOOLKIT                        │
│                    Python                            │
│                                                      │
│ segmentation                                         │
│ Praat analysis                                       │
│ WORLD analysis                                       │
│ statistics                                           │
│ fitting                                              │
│ profile compiler                                     │
└──────────────────────────────────────────────────────┘
```

The audio engine should be a separate process from the UI.

A UI crash therefore does not need to terminate the audio engine.

IPC only transports small control messages such as:

```json
{
  "preset": "source-ja-to-female-a",
  "mode": "speech",
  "strength": 0.94,
  "bypass": false
}
```

Audio never crosses that boundary.

---

# 5. Repository structure

```text
voice-morpher/
│
├── apps/
│   └── desktop/
│       ├── src/
│       └── src-tauri/
│
├── engine/
│   ├── include/
│   ├── src/
│   │   ├── audio/
│   │   ├── analysis/
│   │   ├── morph/
│   │   ├── profiles/
│   │   ├── runtime/
│   │   ├── telemetry/
│   │   └── utils/
│   │
│   ├── tests/
│   └── benchmarks/
│
├── profiler/
│   ├── voiceprof/
│   │   ├── recording/
│   │   ├── segmentation/
│   │   ├── praat/
│   │   ├── world/
│   │   ├── pitch/
│   │   ├── formants/
│   │   ├── spectral/
│   │   ├── fitting/
│   │   └── reports/
│   │
│   └── tests/
│
├── schemas/
│   ├── speaker-profile.schema.json
│   ├── morph-preset.schema.json
│   ├── calibration-session.schema.json
│   └── engine-config.schema.json
│
├── fixtures/
│   ├── synthetic/
│   ├── regression/
│   └── calibration/
│
├── tools/
│   ├── latency-test/
│   ├── profile-inspector/
│   └── audio-comparator/
│
└── docs/
    ├── architecture.md
    ├── calibration.md
    ├── dsp.md
    └── measurements.md
```

Build systems:

```text
C++       CMake
Python    uv
Frontend  pnpm
Desktop   Tauri
CI        GitHub Actions
```

---

# 6. Core data model

The project uses three principal persistent entities.

```text
SpeakerProfile
MorphPreset
CalibrationSession
```

## 6.1 SpeakerProfile

A `SpeakerProfile` describes observed speaker acoustics.

It MUST NOT contain transformation parameters.

Conceptually:

```typescript
interface SpeakerProfile {
  schemaVersion: "1.0";

  id: string;
  displayName: string;

  language: "ja-JP" | "ru-RU" | "en-US" | string;
  mode: "speech" | "singing";

  recording: {
    sampleRate: number;
    channels: 1;
    usableDurationSeconds: number;
  };

  pitch: PitchProfile;

  vocalTract: VocalTractProfile;

  sourceQuality: SourceQualityProfile;

  spectral: SpectralProfile;

  dynamics: DynamicsProfile;

  sibilance: SibilanceProfile;

  quality: MeasurementQuality;
}
```

### PitchProfile

```typescript
interface PitchProfile {
  voicedFraction: number;

  log2HzQuantiles: {
    q05: number;
    q10: number;
    q25: number;
    q50: number;
    q75: number;
    q90: number;
    q95: number;
  };

  medianHz: number;

  rangeCents: {
    p10ToP90: number;
    p05ToP95: number;
  };

  contourVelocity: {
    medianCentsPerSecond: number;
    p95CentsPerSecond: number;
  };

  phraseDeclination?: number;

  vibrato?: {
    rateHzMedian: number;
    depthCentsMedian: number;
  };
}
```

Pitch statistics are stored primarily in log-frequency space because pitch perception and musical intervals are approximately multiplicative rather than linear in Hz.

---

# 7. Vocal-tract profile

```typescript
interface VocalTractProfile {
  formantSpacingHzMedian: number;

  vowels: Record<string, {
    count: number;

    F1: Distribution;
    F2: Distribution;
    F3: Distribution;
    F4?: Distribution;

    bandwidths?: {
      B1?: Distribution;
      B2?: Distribution;
      B3?: Distribution;
    };
  }>;

  estimatedGlobalWarp: number;

  confidence: number;
}
```

`estimatedGlobalWarp` is a descriptive measurement relative to the profiler's reference model.

Runtime should **not continuously detect individual F1/F2/F3 tracks and independently move them**.

That strategy is too unstable.

Instead:

> formants are used offline to estimate vocal-tract geometry; runtime processing warps the spectral envelope as a continuous function.

This distinction is important.

---

# 8. Voice-source and spectral profile

```typescript
interface SourceQualityProfile {
  hnrDb: Distribution;

  cppDb?: Distribution;

  spectralTiltDbPerOctave: Distribution;

  periodicity: Distribution;

  voicedAperiodicityBands?: number[];
}
```

```typescript
interface SpectralProfile {
  analysisBandsHz: number[];

  meanEnvelopeDb: number[];

  envelopeStdDb: number[];

  melCepstrumMean?: number[];
  melCepstrumStd?: number[];
}
```

Breathiness must **not** be blindly increased because the target is female.

Research does not support aperiodicity as a universally reliable gender cue.

Instead, voice-source characteristics should move only toward measurements from the selected target profile.

---

# 9. MorphPreset

A `MorphPreset` is generated from:

```text
SourceProfile
       +
TargetProfile
       +
Transformation policy
       ↓
MorphPreset
```

Schema:

```typescript
interface MorphPreset {
  schemaVersion: "1.0";

  id: string;
  sourceProfileId: string;
  targetProfileId: string;

  language: string;
  mode: "speech" | "singing";

  pitch: PitchTransform;

  vocalTract: VocalTractTransform;

  spectral: SpectralTransform;

  sourceQuality: SourceQualityTransform;

  dynamics: DynamicsTransform;

  safety: SafetyLimits;
}
```

---

# 10. Speech pitch transformation

A fixed semitone shift is explicitly rejected for normal speech.

Instead, source F0 is mapped into the statistical distribution of the target.

Let:

```text
x = log2(F0)
```

and source pitch percentile:

```text
p = CDF_source(x)
```

Then:

```text
target = inverseCDF_target(p)
```

In practice the CDF is represented as a monotonic spline using stored quantiles.

Example:

```text
source pitch:

q05 ─ q10 ─ q25 ─ q50 ─ q75 ─ q90 ─ q95
 │      │      │      │      │      │      │
 ▼      ▼      ▼      ▼      ▼      ▼      ▼
target pitch:

q05 ─ q10 ─ q25 ─ q50 ─ q75 ─ q90 ─ q95
```

This gives desirable behavior:

- low source intonation remains relatively low;
- expressive peaks remain expressive peaks;
- pitch movement is preserved;
- the output occupies the natural target range;
- the mapping is language-specific.

Transformation:

```text
mappedF0 = targetQuantile(sourcePercentile(F0))
```

Final transformation strength:

```text
F0_output =
    exp2(
        lerp(
            log2(F0_input),
            log2(mappedF0),
            pitchStrength
        )
    )
```

This allows:

```text
pitchStrength = 0.0 → original
pitchStrength = 1.0 → full target mapping
```

---

# 11. Pitch mapper schema

```typescript
interface PitchTransform {
  mode:
    | "quantile"
    | "fixed_semitones"
    | "relative";

  strength: number;

  sourceKnotsLog2Hz?: number[];
  targetKnotsLog2Hz?: number[];

  fixedSemitones?: number;

  extrapolation: {
    minSlope: number;
    maxSlope: number;
  };

  limits: {
    minOutputHz: number;
    maxOutputHz: number;
    maxShiftSemitones: number;
  };

  smoothing: {
    confidenceThreshold: number;
    attackMs: number;
    releaseMs: number;
  };
}
```

The mapping must be monotonic.

No pitch mapping may create:

```text
F0_a < F0_b
```

but:

```text
mapped(F0_a) > mapped(F0_b)
```

---

# 12. Singing pitch mode

Singing mode supports a different mapping strategy.

The first supported mode is:

```text
OCTAVE_UP
```

defined as:

```text
F0_output = F0_input × 2
```

or:

```text
shiftSemitones = +12
```

Critically, this transform is applied to the instantaneous pitch rather than quantizing detected notes.

Therefore:

```text
vibrato
slides
portamento
microtonal deviations
attack pitch
```

are preserved.

For example:

```text
220 Hz ± vibrato
        ↓ ×2
440 Hz ± equivalent vibrato in cents
```

No autotune is applied.

### Singing mode controls

```typescript
interface SingingControls {
  pitchMode:
    | "fixed_semitones"
    | "adaptive";

  semitones: number;

  preserveVibrato: true;

  preservePortamento: true;

  noteQuantization: false;
}
```

A later feature may optionally introduce note quantization, but it is explicitly out of scope for the initial version.

---

# 13. Important latency optimization for singing

Fixed `+12 semitone` mode does **not require F0 estimation to determine the shift ratio**.

Therefore:

```text
speech:

input
 ↓
F0 estimation
 ↓
adaptive mapping
 ↓
pitch shift


singing +12:

input
 ↓
fixed ratio = 2.0
 ↓
pitch shift
```

Pitch tracking can still run asynchronously for telemetry and voiced/unvoiced decisions.

This gives fixed-octave singing mode a lower potential latency than adaptive speech mode.

---

# 14. Vocal-tract transformation

Pitch and formants MUST be independently controllable.

The system defines:

```text
pitch ratio:
Rp

spectral-envelope / formant ratio:
Rf
```

with:

```text
Rp ≠ Rf
```

in the general case.

The target formant transform is estimated from matched vowels:

```text
F1_target / F1_source
F2_target / F2_source
F3_target / F3_source
```

across the calibration corpus.

A robust fit generates a frequency-dependent warp rather than one universal scalar.

Example conceptual function:

```text
frequency
   │
   ▼
┌───────────────┐
│ Warp function │
└───────┬───────┘
        ▼

500 Hz  → 560 Hz
1 kHz   → 1.11 kHz
2 kHz   → 2.18 kHz
4 kHz   → 4.22 kHz
8 kHz   → mostly unchanged
```

The exact values are learned from profiles rather than hard-coded.

---

# 15. VocalTractTransform schema

```typescript
interface VocalTractTransform {
  strength: number;

  representation:
    | "global_scale"
    | "piecewise_warp";

  globalScale?: number;

  warp?: Array<{
    inputHz: number;
    outputHz: number;
  }>;

  voicedStrength: number;

  unvoicedStrength: number;

  maxWarpRatio: number;
}
```

The warp function MUST:

- remain monotonic;
- remain smooth;
- avoid folding frequencies;
- taper toward unity in the highest bands.

High-frequency consonant information must not be shifted as aggressively as low/mid vocal resonances.

---

# 16. Unvoiced speech is a separate problem

A naive pitch shifter processes:

```text
vowels
s
sh
f
t
k
breath
transients
```

identically.

That is undesirable.

The engine therefore treats speech as a continuously varying mixture:

```text
Periodic / voiced component
+
Aperiodic / noise component
```

with:

```text
voicing ∈ [0,1]
```

rather than a binary switch.

Conceptually:

```text
                 ┌── harmonic transform ──┐
input ─ analysis ┤                        ├─ blend → output
                 └── noise transform ─────┘
```

When:

```text
voicing ≈ 1
```

the pitch/formant transformation dominates.

When:

```text
voicing ≈ 0
```

pitch transformation approaches zero while spectral shaping remains limited.

This protects:

```text
/s/
/ʃ/
/f/
/h/
/t/
/k/
/p/
```

from becoming metallic or cartoon-like.

---

# 17. Realtime analysis

The minimal realtime feature vector is:

```cpp
struct AnalysisFrame {
    float f0Hz;
    float pitchConfidence;

    float voicingProbability;

    float rmsDb;

    float spectralTilt;

    float transientProbability;
};
```

No ASR is required.

No language recognition is required.

The user explicitly chooses the active language preset.

---

# 18. Pitch tracker

For the main realtime path, use a causal classical tracker.

Candidates:

```text
YIN
autocorrelation + candidate scoring
McLeod Pitch Method
custom hybrid
```

The source speaker calibration provides useful priors:

```text
expected pitch floor
expected pitch ceiling
octave-jump penalties
```

which significantly reduces ambiguity.

RMVPE remains useful as an **offline benchmark**, particularly for singing/noisy recordings: the published model targets robust vocal pitch estimation and has demonstrated resistance to accompaniment/noise.

It is not selected as the primary realtime tracker because the reference architecture includes a bidirectional recurrent component and is not designed around our strict causal-latency requirement.

---

# 19. Runtime F0 stabilization

Raw F0 must never directly control pitch shift.

Pipeline:

```text
raw pitch candidates
      ↓
confidence rejection
      ↓
octave-error correction
      ↓
short causal median filter
      ↓
predictive smoother
      ↓
pitch mapping
```

Octave correction uses:

```text
previous accepted F0
source profile
candidate confidence
allowed acceleration
```

Candidates near:

```text
0.5 × F0_prev
2.0 × F0_prev
```

receive additional penalties unless supported by sufficient evidence.

This avoids sudden:

```text
+12 / -12 semitone glitches.
```

---

# 20. DSP engine strategy

Two transform implementations should be developed behind the same interface.

```cpp
class IMorphEngine {
public:
    virtual void prepare(const EngineSpec&) = 0;

    virtual void process(
        const float* input,
        float* output,
        size_t frames,
        const RuntimeControls&
    ) noexcept = 0;
};
```

## Engine A — Reference realtime implementation

Use Rubber Band Live Pitch Shifter.

Current Rubber Band 4 exposes a realtime `RubberBandLiveShifter` specifically intended for block-by-block pitch shifting with minimum available processing delay. It provides independently controllable pitch and formant scales and exposes its required block size and startup delay.

This makes it an excellent MVP baseline.

Pipeline:

```text
input
 ↓
input conditioner
 ↓
F0 / voicing analysis
 ↓
dynamic pitch ratio
 ↓
RubberBandLiveShifter
 ↓
formant scale
 ↓
spectral correction
 ↓
output
```

Important threading detail:

Rubber Band documents that `setPitchScale()` must not execute concurrently with `shift()`. Therefore target ratios are published atomically but applied by the DSP thread itself before processing the relevant block.

### Licensing

Rubber Band Live is GPL unless an appropriate commercial licence is obtained. This is acceptable for private/research use but becomes a product consideration if the project is later distributed as proprietary software.

Therefore the DSP abstraction MUST NOT make Rubber Band irreplaceable.

---

# 21. Engine B — custom high-quality morph backend

The long-term target is a custom source/filter-aware engine.

Conceptually:

```text
Input
  ↓
STFT / pitch-synchronous analysis
  ↓
Periodic/noise decomposition
  ↓
Spectral-envelope extraction
  ↓
Residual/excitation extraction
  │
  ├── pitch modification
  │
  └── spectral-envelope warp
  ↓
Recombination
  ↓
Transient/noise restoration
  ↓
Overlap-add
```

Candidate implementations should be benchmarked:

### B1. Pitch-synchronous overlap-add

Advantages:

- excellent transient preservation;
- particularly appropriate for monophonic voice;
- natural time-domain waveform behavior;
- low computational cost.

Challenges:

- robust epoch placement;
- voiced/unvoiced transitions;
- extremely large pitch shifts.

### B2. Phase-locked STFT

Advantages:

- controllable envelope manipulation;
- convenient harmonic/noise decomposition;
- frequency-dependent formant warping.

Challenges:

- phase coherence;
- transient smearing;
- minimum useful analysis window.

### B3. Hybrid

Preferred long-term option:

```text
strongly voiced:
    pitch-synchronous processing

mixed:
    phase-coherent STFT

unvoiced/transient:
    minimally processed original path
```

The algorithm is selected frame by frame with smooth crossfading.

---

# 22. WORLD as analysis oracle

WORLD should be integrated into the offline research toolkit.

WORLD explicitly represents voice using:

```text
F0
spectral envelope
aperiodicity
```

and can resynthesize speech from those parameters. The upstream project describes itself as a high-quality speech analysis/manipulation/synthesis system and includes sequential realtime synthesis work. The current repository also lists a February 2026 formal release.

Use WORLD for:

- offline reference transformations;
- inspecting F0;
- CheapTrick spectral envelope;
- D4C aperiodicity;
- building synthetic expected outputs;
- comparing custom DSP behavior.

Example development workflow:

```text
input.wav
   │
   ├──────── custom realtime engine ────── candidate.wav
   │
   └──────── WORLD offline transform ───── oracle.wav
```

The realtime engine does not need to sound identical to WORLD.

WORLD exists as an experimental reference.

---

# 23. Offline profiler

Recommended stack:

```text
Python
NumPy
SciPy
Praat / Parselmouth
WORLD
optional ML
```

Parselmouth exposes Praat's actual underlying analysis algorithms rather than merely approximating them in Python.

Praat supplies useful measurements including:

- pitch;
- formants;
- harmonicity/HNR;
- intensity;
- voice-quality measurements.


---

# 24. Calibration workflow

A calibration session consists of:

```text
Universal acoustic calibration
+
Language-specific speech calibration
+
optional singing calibration
```

Recommended recording conditions:

```text
48 kHz
24-bit input if available
mono
dry microphone
no EQ
no compressor
no noise suppression
no automatic gain control
no reverb
consistent microphone distance
```

Calibration should reject recordings containing:

```text
clipping
large background noise
strong reflections/reverb
dropouts
aggressive denoising
automatic gain jumps
```

---

# 25. Universal acoustic calibration

## Block U1 — room/noise

```text
5 seconds silence
```

Measures:

```text
noise floor
hum
stationary noise
```

## Block U2 — sustained vowels

Hold each for approximately three seconds:

```text
/a/
/i/
/u/
/e/
/o/
```

Repeat:

```text
comfortable
soft
loud
```

Purpose:

```text
F1–F4
bandwidths
spectral envelope
HNR
spectral tilt
source characteristics
```

## Block U3 — pitch sweeps

On `/a/`:

```text
comfortable → low → comfortable
comfortable → high → comfortable
low → high → low
```

Repeat on `/i/`.

## Block U4 — dynamics

Hold `/a/` while moving:

```text
soft → loud → soft
```

without intentionally changing pitch.

## Block U5 — transient attacks

Repeat:

```text
pa pa pa pa
ta ta ta ta
ka ka ka ka

ba ba ba ba
da da da da
ga ga ga ga
```

---

# 26. Japanese calibration set

The Japanese profile is independent from Russian and English.

## JP-A — mora inventory

Speak naturally:

```text
あ い う え お

か き く け こ
が ぎ ぐ げ ご

さ し す せ そ
ざ じ ず ぜ ぞ

た ち つ て と
だ ぢ づ で ど

な に ぬ ね の

は ひ ふ へ ほ
ば び ぶ べ ぼ
ぱ ぴ ぷ ぺ ぽ

ま み む め も

や ゆ よ

ら り る れ ろ

わ を ん
```

## JP-B — contracted morae

```text
きゃ きゅ きょ
しゃ しゅ しょ
ちゃ ちゅ ちょ

にゃ にゅ にょ
ひゃ ひゅ ひょ
みゃ みゅ みょ
りゃ りゅ りょ

ぎゃ ぎゅ ぎょ
じゃ じゅ じょ

びゃ びゅ びょ
ぴゃ ぴゅ ぴょ
```

## JP-C — gemination / long vowels / nasals

```text
きって
きっぷ
がっこう
ざっし
ちょっと

おばさん
おばあさん

ここ
こうこう

びょういん
りょうり
しゅうまつ
```

## JP-D — natural sentences

```text
今日は朝から冷たい雨が降っています。

新しいカメラを持って、駅の近くまで歩きました。

小さな猫が白いソファの上で眠っています。

切符を買って、特急で東京へ向かいます。

週末は友達と一緒に料理を作る予定です。

昨日買った雑誌をまだ読んでいません。

この部屋は静かなので、とても話しやすいです。

急いでいたので、朝ご飯を食べずに家を出ました。

旅行の写真を見ながら、去年の夏を思い出しました。

電車が遅れているため、到着は少し遅くなりそうです。
```

## JP-E — intonation

Neutral statement:

```text
今日はここで終わります。
```

Question:

```text
今日はここで終わりますか？
```

Surprise:

```text
えっ、本当にこれで大丈夫なんですか？
```

Certainty:

```text
はい、これで間違いありません。
```

Uncertainty:

```text
たぶん大丈夫だと思います。
```

## JP-F — contrastive focus

Speak the same sentence three times.

```text
私は明日、東京に行きます。
```

Emphasize respectively:

```text
私
明日
東京
```

This captures the speaker's natural Japanese focus strategy without requiring realtime linguistic analysis.

---

# 27. Russian calibration set

## RU-A — vowels

Sustain:

```text
а
э
и
о
у
ы
```

## RU-B — voiced/unvoiced pairs

```text
па ба
та да
ка га
фа ва
са за
ша жа
```

## RU-C — hard/soft consonants

```text
ла ля
лу лю
на ня
ну ню
та тя
са ся
за зя
ра ря
```

## RU-D — difficult consonants

```text
ша шо шу
жа жо жу
ща щё щу
ча чё чу
ца цо цу

ра ро ру ры
ла ло лу лы
```

## RU-E — natural sentences

```text
Сегодня утром на улице было прохладно и тихо.

Я положил новую книгу рядом с чашкой горячего чая.

Маленькая кошка быстро пробежала через тёмную комнату.

После работы мы решили немного прогуляться по городу.

На следующей неделе погода, возможно, снова изменится.

Я внимательно посмотрел на экран и заметил небольшую ошибку.

Поезд отправляется через несколько минут с третьей платформы.

Вчера вечером мы долго разговаривали о путешествиях и музыке.

Сначала эта задача показалась сложной, но решение оказалось простым.

Если всё получится, завтра я попробую сделать это ещё раз.
```

## RU-F — prosody

```text
Сегодня мы закончим раньше.
Сегодня мы закончим раньше?
```

Surprise:

```text
Серьёзно? Это действительно произошло?
```

Certainty:

```text
Да, я совершенно в этом уверен.
```

Uncertainty:

```text
Наверное, это всё-таки возможно.
```

Contrastive focus:

```text
Я завтра поеду в Москву.
```

Emphasize:

```text
Я
завтра
Москву
```

---

# 28. English calibration set

Default calibration dialect:

```text
en-US
```

Alternative dialects require separate profiles.

## EN-A — sustained vowels

Use diagnostic words:

```text
heed
hid
head
had
hod
hawed
hood
who'd
hut
```

Then sustain representative vowels independently.

## EN-B — consonant contrasts

```text
pack / back
ten / den
coat / goat

fan / van
sip / zip
thigh / thy

ship / measure
cheap / jeep
```

## EN-C — approximants and nasals

```text
right / light

ray
lay

me
no
sing
```

## EN-D — natural sentences

```text
The morning train arrived a few minutes earlier than expected.

I left the camera beside the window before going outside.

A small cat quietly crossed the room and jumped onto the chair.

We should probably finish this work before the end of the day.

The weather changed quickly after the clouds moved over the city.

I noticed a strange sound coming from somewhere behind the door.

Tomorrow we can try another route if this one takes too long.

The bright screen reflected softly from the surface of the table.

I thought the problem would be difficult, but the answer was surprisingly simple.

After dinner we stayed outside and talked about music for a while.
```

## EN-E — prosody

```text
We're finishing here today.
We're finishing here today?
```

Surprise:

```text
Really? Did that actually happen?
```

Certainty:

```text
Yes, I'm completely sure about that.
```

Uncertainty:

```text
Maybe that's still possible.
```

Contrastive focus:

```text
I am going to Tokyo tomorrow.
```

Emphasize:

```text
I
Tokyo
tomorrow
```

---

# 29. Singing calibration

Singing profiles are separate from speech profiles.

Record:

```text
comfortable sustained note
comfortable note ± minor third
comfortable note ± perfect fifth
comfortable note ± octave where possible
```

on:

```text
a
i
u
```

Then:

```text
ascending five-note scale
descending five-note scale
one-octave scale
```

Then sustained notes with:

```text
no deliberate vibrato
natural vibrato
strong vibrato
```

Finally record approximately 30–60 seconds of a comfortable song without accompaniment.

The singing profiler measures:

```text
F0 range
vibrato rate
vibrato depth
pitch-transition speed
onset characteristics
spectral behavior vs pitch
```

---

# 30. CalibrationSession schema

```typescript
interface CalibrationSession {
  schemaVersion: "1.0";

  id: string;

  speakerId: string;

  language: string;

  mode: "speech" | "singing";

  device: {
    name: string;
    sampleRate: number;
  };

  takes: Array<{
    promptId: string;
    path: string;

    accepted: boolean;

    metrics: {
      peakDbFS: number;
      rmsDbFS: number;
      noiseEstimateDbFS?: number;
      clippingSamples: number;
    };
  }>;

  completedAt: string;
}
```

Raw audio is retained.

Derived profiles can therefore be regenerated when the profiler improves.

---

# 31. Target profile acquisition

There are two modes.

## Matched calibration

Best case:

```text
source speaker
target speaker
```

record the same calibration set.

This allows direct comparison of:

```text
vowel formants
spectral envelopes
F0 distributions
prosody
source quality
```

## Free-corpus target

Alternatively:

```text
2–10 minutes clean target speech
```

can be automatically segmented and analyzed.

Requirements:

```text
minimal music
minimal reverb
minimal background speakers
minimal codec damage
```

This provides a useful acoustic target but less reliable phoneme-conditioned matching.

---

# 32. Profile fitting

The fitter constructs transformations independently.

```text
Pitch fitter
Vocal-tract fitter
Spectral fitter
Source-quality fitter
Dynamics fitter
```

Then combines them.

Never perform one black-box optimization over every parameter simultaneously at the beginning.

That makes failures impossible to diagnose.

---

# 33. Vocal-tract fitting

For each usable vowel:

```text
source formants
target formants
```

are compared in log-frequency space.

Estimate a smooth mapping:

```text
log(F_target) =
    warp(log(F_source))
```

using robust regression.

Outliers are rejected according to:

```text
formant confidence
phoneme confidence
bandwidth
frame stability
```

Final warp is regularized toward a smooth monotonic function.

---

# 34. Spectral fitting

After compensating for vocal-tract scaling, compare average envelopes:

```text
target envelope
-
transformed source envelope
```

The remaining difference becomes a low-resolution EQ curve.

Do not generate hundreds of narrow EQ bands.

Use approximately:

```text
8–16 perceptually spaced control points
```

with strong smoothing.

This prevents overfitting the microphone or room response.

---

# 35. Runtime signal graph

Production graph:

```text
                     ┌─────────────────┐
Mic ────────────────►│ Input adapter   │
                     └────────┬────────┘
                              ▼
                     DC / HP conditioning
                              │
                              ▼
                     Analysis tap
                              │
                 ┌────────────┴──────────┐
                 │                       │
                 ▼                       ▼
            F0 tracker             Voicing estimator
                 │                       │
                 └─────────┬─────────────┘
                           ▼
                     Morph controller
                           │
                           ▼
                     Pitch transform
                           │
                           ▼
                    Envelope / VTL warp
                           │
                           ▼
                    Spectral correction
                           │
                           ▼
                    Noise/transient blend
                           │
                           ▼
                       Dynamics
                           │
                           ▼
                        Limiter
                           │
             ┌─────────────┴─────────────┐
             ▼                           ▼
        monitoring                  virtual output
```

---

# 36. Input conditioning

Input processing must remain extremely conservative.

Default:

```text
DC blocker
subsonic high-pass
optional extremely mild noise gate
```

Do NOT default-enable:

```text
noise suppression
dereverberation
heavy compressor
AGC
exciter
```

because these modify acoustic features used by the morpher.

Input should be as close as practical to a clean microphone signal.

---

# 37. Post-processing

Default output chain:

```text
very mild dynamic EQ
optional de-esser
transparent compressor
true-peak / sample limiter
```

Compression must occur **after morphing**.

Otherwise input dynamics interfere with analysis.

---

# 38. Audio format

Internal canonical format:

```text
Sample rate: 48,000 Hz
Channels:    mono
Type:        float32
Range:       -1 .. +1
```

Alternative device sample rates are converted at the I/O boundary.

The DSP graph always runs at 48 kHz.

---

# 39. Buffer sizes

Support:

```text
64
128
256
512
```

but recommend:

```text
128 frames → low latency
256 frames → stable/default
```

At 48 kHz:

```text
128 samples ≈ 2.67 ms
256 samples ≈ 5.33 ms
```

These numbers represent only one buffer duration, not complete end-to-end latency.

---

# 40. Audio I/O architecture

Windows priority:

```text
WASAPI exclusive
WASAPI low-latency shared
optional ASIO
```

A lightweight library such as miniaudio is suitable for an initial WASAPI abstraction and supports WASAPI directly. Its documentation also notes that Windows low-latency shared behavior depends on using the device's native sample rate, reinforcing the decision to handle sample-rate conversion explicitly at the boundary.

ASIO can later be added for suitable audio interfaces.

ASIO licensing must be reviewed before proprietary distribution: the current Steinberg SDK is dual-licensed under GPLv3 or a proprietary Steinberg agreement.

---

# 41. Clock drift

Physical microphone and virtual output are separate devices.

They therefore may not share exactly the same hardware clock.

Even:

```text
48000.0 Hz
vs
47999.5 Hz
```

eventually causes:

```text
buffer underflow
or
buffer overflow.
```

The engine therefore requires:

```text
AdaptiveResampler
+
RingBufferFillController
```

Architecture:

```text
input clock
   ↓
ring buffer
   ↓
fill-level estimator
   ↓
tiny resampling correction
   ↓
output clock
```

Resampling ratio adjustments should stay extremely small and smoothly varying.

---

# 42. Threading model

## Audio callback

Highest-priority system audio callback.

Responsibilities:

```text
receive / provide audio
copy to/from lock-free queues
timestamp
```

Nothing else.

## DSP thread

Realtime priority.

Responsibilities:

```text
analysis
morphing
post processing
```

No locks.

No allocations.

## Control thread

Responsibilities:

```text
IPC
profile switching
configuration
UI state
```

Changes are published as immutable snapshots.

## Telemetry thread

Responsibilities:

```text
meters
latency statistics
CPU statistics
debug counters
```

---

# 43. Runtime parameter publication

UI must never mutate processor state directly.

Instead:

```text
UI
 ↓
Control thread
 ↓
new immutable RuntimeControls
 ↓ atomic pointer swap
DSP thread
 ↓
consume snapshot
```

Example:

```cpp
struct RuntimeControls {
    float morphStrength;
    float pitchStrength;
    float formantStrength;
    float timbreStrength;

    bool bypass;

    Mode mode;
};
```

---

# 44. Memory policy

Realtime code:

```text
no malloc
no new
no vector resize
no strings
no exceptions
```

All buffers are allocated during `prepare()`.

Example:

```cpp
engine.prepare({
    .sampleRate = 48000,
    .maximumBlockSize = 512
});
```

---

# 45. Profile compilation

JSON profiles are editable/debuggable but should not be parsed in the realtime engine.

Pipeline:

```text
speaker.json
target.json
preset.json
     ↓
profile compiler
     ↓
preset.vmp
```

`preset.vmp` contains:

```text
validated transforms
precomputed splines
EQ coefficients
warp tables
limits
metadata
```

Loading a profile occurs outside the DSP thread.

The compiled object is then atomically activated.

---

# 46. User-facing controls

Do not expose 40 DSP parameters in the normal UI.

Primary controls:

```text
Morph Strength
Pitch
Vocal Tract
Timbre
Brightness
Air
Dynamics
```

Advanced page:

```text
pitch mapping
formant warp
spectral tilt
sibilance
aperiodicity
pitch smoothing
voicing threshold
```

Modes:

```text
Speech Adaptive
Speech Fixed
Singing +12
Singing Custom
Bypass
```

---

# 47. Morph Strength

`Morph Strength` is a macro parameter.

For strength `m`:

```text
pitchStrength       = curvePitch(m)
formantStrength     = curveFormant(m)
spectralStrength    = curveSpectral(m)
sourceQuality       = curveSource(m)
```

These curves should not necessarily be linear.

For example, perceptually useful formant movement may saturate earlier than pitch movement.

---

# 48. Language profiles

Language is explicitly user-selected:

```text
Japanese
Russian
English
```

No realtime language identification.

Each language profile contains its own:

```text
pitch distribution
prosodic range
spectral statistics
optional transform
```

Speaker-level anatomical measurements may be shared.

Architecture:

```text
Speaker
 ├── anatomical profile
 │
 ├── ja-JP
 │    ├── speech
 │    └── singing
 │
 ├── ru-RU
 │    ├── speech
 │    └── singing
 │
 └── en-US
      ├── speech
      └── singing
```

---

# 49. Preset switching

Preset changes during audio must crossfade.

Never:

```text
preset A
[instant switch]
preset B
```

Instead:

```text
preset A ───────────╲
                     ╲
                      ╲
                       preset B
```

Crossfade transform parameters over approximately tens of milliseconds.

Actual duration should be empirically tuned.

---

# 50. Failure behavior

If F0 confidence disappears:

```text
do not extrapolate indefinitely.
```

Instead:

```text
hold briefly
 ↓
reduce pitch morph strength smoothly
 ↓
transition toward unvoiced processing
```

On recovery:

```text
reacquire
 ↓
validate octave
 ↓
fade transformation back in
```

No abrupt reset.

---

# 51. Latency instrumentation

Latency is a first-class product metric.

Measure separately:

```text
input device latency
capture buffering
analysis delay
DSP algorithm delay
output buffering
virtual device latency
```

The app should expose:

```text
Algorithmic latency
Reported device latency
Measured loopback latency
```

Rubber Band exposes its own startup/output delay through `getStartDelay()`, which should be incorporated into reported algorithmic latency.

---

# 52. Latency targets

## Speech / Low Latency

Goal:

```text
algorithmic <= 30 ms
end-to-end <= 50 ms
```

## Speech / Quality

Goal:

```text
algorithmic <= 45 ms
end-to-end <= 70 ms
```

## Singing / fixed shift

Goal:

```text
algorithmic <= 25 ms
end-to-end <= 45 ms
```

These are engineering targets, not guaranteed physical limits.

If subjective quality improves substantially at +10 ms latency, the application should expose both profiles.

---

# 53. Realtime performance requirements

At the selected block size:

```text
DSP p99 execution time
<
50% of available block duration
```

Example:

```text
256 @ 48k
block duration ≈ 5.33 ms

desired p99 DSP
< 2.6 ms
```

This leaves scheduling headroom.

Target:

```text
0 xruns during 30-minute normal session
```

under the reference hardware configuration.

---

# 54. Objective quality tests

Every regression corpus should calculate:

### Pitch

```text
desired F0 vs measured output F0
error in cents
```

### Voicing

```text
voiced → unvoiced errors
unvoiced → voiced errors
```

### Formants

On stable vowels:

```text
F1 error
F2 error
F3 error
```

measured in relative/log frequency.

### Spectral

```text
mel-cepstral distance
spectral envelope distance
spectral tilt error
```

### Sibilants

Compare:

```text
spectral centroid
high-frequency energy
duration
```

### Timing

Output duration must remain effectively identical to input duration.

### Glitches

Detect:

```text
discontinuities
dropouts
NaN
inf
clipping
```

---

# 55. Subjective evaluation

Objective metrics are insufficient.

Maintain an AB evaluation harness.

Each test compares:

```text
Original
Pitch-only
Pitch + formant
Full DSP
Alternative engine
```

Listeners evaluate:

```text
Naturalness
Feminine perception
Speaker continuity
Intelligibility
Metallic artifacts
Robotization
Sibilants
Transient quality
Listening fatigue
```

For singing additionally:

```text
pitch stability
vibrato preservation
attack preservation
high-note quality
```

---

# 56. Golden regression corpus

The project should maintain recordings containing:

```text
low male pitch
normal speech
high-energy speech
whisper-like speech
vocal fry
laugh
breath
sibilant-heavy speech
plosives
fast speech
slow speech
Japanese
Russian
English
singing
```

Every DSP change runs automatically against this corpus.

Waveforms and metrics are versioned.

---

# 57. Development phases

## Phase 0 — acoustic research harness

Python only.

Implement:

```text
Praat extraction
WORLD extraction
source profile
target profile
quantile pitch mapping
formant target fitting
offline transformations
reports
```

Goal:

> determine the acoustic transformation that produces the best source→female result before caring about realtime constraints.

Exit criterion:

A prerecorded male sample can be transformed into a convincing female acoustic configuration without gross artifacts.

---

## Phase 1 — realtime baseline

Implement native C++ engine:

```text
audio I/O
Rubber Band Live
classical F0 tracker
pitch quantile mapping
formant scaling
basic post-processing
virtual cable output
```

Exit criterion:

```text
stable realtime operation
no dropouts
acceptable male→female conversion
measured latency
```

---

## Phase 2 — full calibration

Implement:

```text
recording wizard
JP/RU/EN phrase sets
source profiler
target profiler
preset compiler
automatic fitting
```

Exit criterion:

A new speaker can create a usable profile without manual DSP editing.

---

## Phase 3 — custom morph engine

Implement:

```text
periodicity estimator
harmonic/noise decomposition
custom spectral-envelope warp
custom pitch processing
transient preservation
```

A/B against Rubber Band.

Do not replace Rubber Band unless:

```text
quality improves
AND
latency is competitive
AND
stability is maintained.
```

---

## Phase 4 — singing

Implement:

```text
fixed octave mode
custom semitone mode
vibrato preservation
singing calibration
high-F0 artifact handling
```

Exit criterion:

The user can comfortably sing the lower octave while monitoring/streaming the transformed upper-octave voice.

---

## Phase 5 — polish

Implement:

```text
hotkeys
preset switching
device recovery
automatic latency measurement
session recording
advanced diagnostic UI
crash recovery
```

---

# 58. MVP acceptance criteria

MVP is complete when all of the following are true.

### Functional

```text
physical mic → transformed virtual mic
JP speech preset
RU speech preset
EN speech preset
singing +12 preset
runtime bypass
hot preset switching
```

### Quality

The output must:

```text
not sound like a simple pitch shifter;
preserve consonant clarity;
avoid obvious chipmunk effects;
retain source rhythm;
retain source emotional pitch movement;
retain recognizability of the source speaker;
produce a convincingly feminine acoustic configuration.
```

### Reliability

```text
30 minutes continuous operation
0 crashes
0 NaNs
0 persistent buffer underruns
0 unbounded memory growth
```

### Latency

Measured through physical loopback:

```text
speech target < 50 ms end-to-end
singing target < 45 ms end-to-end
```

on the reference setup.

---

# 59. Key experiments that must happen early

Before building extensive UI, run these experiments.

### Experiment A

```text
pitch only
vs
pitch + global formant shift
```

This establishes the minimum benefit of vocal-tract processing.

### Experiment B

```text
global formant shift
vs
frequency-dependent envelope warp
```

### Experiment C

```text
process entire spectrum
vs
protect unvoiced/noise components
```

### Experiment D

```text
fixed pitch shift
vs
quantile F0 mapping
```

for normal speech.

### Experiment E

```text
Rubber Band baseline
vs
offline WORLD target
```

This estimates how much quality remains available by replacing the transform backend.

### Experiment F

Find the actual quality/latency frontier:

```text
15
20
25
30
40
50
60 ms
```

rather than prematurely optimizing for an arbitrary latency number.

---

# 60. Technical risks

## Risk 1 — large pitch shifts expose artifacts

Male→female conversion may require very substantial F0 change.

Mitigation:

```text
formant-independent processing
voiced/unvoiced separation
multiple algorithms
profile-derived mapping
```

## Risk 2 — octave errors

A single pitch-tracker octave error produces a catastrophic audible glitch.

Mitigation:

```text
speaker-specific pitch prior
temporal continuity
confidence
octave candidate correction
```

## Risk 3 — metallic sibilants

Mitigation:

```text
aperiodic preservation
limited high-frequency warping
separate unvoiced path
```

## Risk 4 — vowels sound synthetic

Mitigation:

```text
smooth envelope warp
target vowel measurements
avoid independent unstable formant tracking
```

## Risk 5 — UI/audio coupling

Mitigation:

```text
separate engine process
immutable controls
no UI execution in realtime thread
```

## Risk 6 — different audio clocks

Mitigation:

```text
adaptive ring buffer
low-ratio SRC correction
clock monitoring
```

---

# 61. What must explicitly NOT enter v1

Do not introduce:

```text
Whisper
LLM inference
speech recognition
phoneme recognition in realtime
cloud services
diffusion models
RVC
neural vocoder
speaker embeddings in realtime
custom virtual audio driver
VST plugin
mobile support
automatic language detection
```

unless evidence from testing proves one of them necessary.

They solve different problems and would obscure evaluation of the core DSP system.

---

# 62. Future neural extension boundary

Although neural identity refinement is out of scope, architecture should reserve:

```cpp
class IPostMorphRefiner {
public:
    virtual void process(...) noexcept = 0;
};
```

Default implementation:

```text
NullRefiner
```

Graph:

```text
DSP morph
   ↓
IPostMorphRefiner
   ↓
post processing
```

Therefore a future causal neural residual model can be inserted without redesigning:

```text
calibration
profiles
audio I/O
UI
routing
```

---

# 63. Recommended first implementation stack

### Desktop

```text
Tauri
React
TypeScript
```

### Engine

```text
C++20
CMake
```

### Windows audio

Initial:

```text
WASAPI
```

Optional:

```text
ASIO
```

### Initial morph backend

```text
Rubber Band Live
```

### Offline profiler

```text
Python
NumPy
SciPy
Praat-Parselmouth
WORLD
```

### Tests

```text
Catch2 or GoogleTest
pytest
```

### Inter-process control

Simple local IPC:

```text
named pipe
or
localhost IPC socket
```

JSON is adequate because this path is control-only.

Do not introduce protobuf unless the control protocol becomes significantly more complex.

---

# 64. Development priority

The most important technical priority is not UI and not ML.

It is:

```text
source recording
      ↓
reliable acoustic analysis
      ↓
profile-derived F0 mapping
      ↓
independent vocal-tract transformation
      ↓
high-quality treatment of unvoiced speech
```

If those four pieces work, the product exists.

Everything else is refinement.

---

# 65. Final architecture

```text
                         ┌─────────────────────┐
                         │ Calibration Corpus  │
                         │ JP / RU / EN / Song │
                         └──────────┬──────────┘
                                    │
                                    ▼
                           Offline Profiler
                    Praat + WORLD + statistics
                                    │
                   ┌────────────────┴──────────────┐
                   ▼                               ▼
             Source Profile                  Target Profile
                   │                               │
                   └──────────────┬────────────────┘
                                  ▼
                          Transform Fitter
                                  │
                                  ▼
                           Morph Preset
                                  │
                                  ▼
                       Compiled Runtime Profile


════════════════════════ REALTIME ════════════════════════

Physical Mic
     │
     ▼
Audio Capture
     │
     ▼
Input Conditioning
     │
     ├────────────────────┐
     │                    │
     ▼                    ▼
Pitch Analysis       Voicing Analysis
     │                    │
     └──────────┬─────────┘
                ▼
          Morph Controller
                │
                ▼
       ┌──────────────────┐
       │ Pitch Transform  │
       └────────┬─────────┘
                ▼
       ┌──────────────────┐
       │ Vocal-Tract Warp │
       └────────┬─────────┘
                ▼
       ┌──────────────────┐
       │ Spectral Shaping │
       └────────┬─────────┘
                ▼
       ┌──────────────────┐
       │ Noise / Transient│
       │ Preservation     │
       └────────┬─────────┘
                ▼
          Output Dynamics
                │
                ▼
        Clock Compensation
                │
                ▼
          Virtual Endpoint
          │       │       │
          ▼       ▼       ▼
         OBS   Discord   DAW
```

---

# 66. Core hypothesis

The central technical hypothesis of the project is:

> A source-specific and language-specific transform of F0 distribution, vocal-tract geometry and spectral envelope, combined with explicit preservation of aperiodic/transient speech components, can produce substantially more natural real-time male-to-female conversion than conventional fixed pitch/formant voice changers while remaining far lower-latency and computationally cheaper than neural voice conversion.

Everything in Phase 0–3 exists to test this hypothesis.

If it proves correct, neural identity refinement is an enhancement.

If it proves false and a clearly audible residual “male identity” remains after optimally fitted source/filter morphing, then that result gives us a precise definition of what the later neural residual model actually needs to learn instead of throwing a general voice-conversion model at the entire signal.