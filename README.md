# Real-Time Parametric Voice Morpher

Windows-first, local voice morphing research and implementation. The production audio path is intended to use deterministic DSP; Python is for offline analysis and experiments.

The complete supplied design is in [docs/technical-design.md](docs/technical-design.md). The implementation sequence and current evidence are in [ROADMAP.md](ROADMAP.md).

## Phase 0 research harness

The first milestone builds source and target acoustic profiles from clean, mono WAV recordings, fits a morph preset, and renders offline comparisons. The separate native foundation includes a live diagnostic executable, but neither path has passed perceptual acceptance or live device validation.

Use Python 3.11 or newer and `uv`. From the repository root:

```powershell
uv sync --extra dev --extra world --locked
.\.venv\Scripts\python -m pytest profiler/tests
```

See [docs/phase-0.md](docs/phase-0.md) for input format and commands. The Python environment is locked in [uv.lock](uv.lock).

## Native foundation

The C++ core, optional Rubber Band Live adapter, and Windows WASAPI diagnostic stream are described in [docs/native-engine.md](docs/native-engine.md). The live stream code builds, but its device path still needs a controlled hardware test.
