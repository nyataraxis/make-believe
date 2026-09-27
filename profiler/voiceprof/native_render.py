from __future__ import annotations

import math
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from scipy.signal import resample_poly

from .io import read_wav, write_wav


def render_native(input_wav: Path, preset: Path, output_wav: Path, executable: Path,
                  window: str = "short") -> dict[str, int | float]:
    if window not in ("short", "medium"):
        raise ValueError("native analysis window must be short or medium")
    if output_wav.exists():
        raise ValueError(f"output already exists: {output_wav}")
    sample_rate, audio = read_wav(input_wav)
    if len(audio) > 60 * sample_rate:
        raise ValueError("native comparison input must be at most 60 seconds")
    divisor = math.gcd(sample_rate, 48000)
    native_audio = resample_poly(audio, 48000 // divisor, sample_rate // divisor)
    native_audio = np.asarray(native_audio, dtype=np.float32)
    if np.max(np.abs(native_audio)) > 1.0:
        raise ValueError("resampled native input exceeds full scale")
    with tempfile.TemporaryDirectory() as directory:
        raw_input = Path(directory) / "input.f32"
        raw_output = Path(directory) / "output.f32"
        native_audio.tofile(raw_input)
        result = subprocess.run(
            [str(executable), str(raw_input), str(preset), str(raw_output), "--window", window],
            check=True,
            capture_output=True,
            text=True,
            timeout=120,
        )
        metrics = dict(part.split("=", 1) for part in result.stdout.strip().split())
        delay = int(metrics["start_delay_samples"])
        processed = np.fromfile(raw_output, dtype=np.float32)
        if len(processed) < delay + len(native_audio):
            raise RuntimeError("native output is shorter than the aligned input")
        write_wav(output_wav, 48000, processed[delay : delay + len(native_audio)])
    return {key: float(value) if key == "mean_pitch_ratio" else int(value)
            for key, value in metrics.items()}
