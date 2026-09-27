from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from scipy.io import wavfile


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")


def read_wav(path: Path) -> tuple[int, np.ndarray]:
    sample_rate, raw = wavfile.read(path)
    if raw.ndim != 1:
        raise ValueError(f"{path}: mono WAV required")
    if sample_rate < 16000:
        raise ValueError(f"{path}: sample rate below 16 kHz")
    if raw.dtype == np.int16:
        audio = raw.astype(np.float64) / 32768.0
    elif raw.dtype == np.int32:
        audio = raw.astype(np.float64) / 2147483648.0
    elif raw.dtype in (np.float32, np.float64):
        audio = raw.astype(np.float64)
    else:
        raise ValueError(f"{path}: unsupported WAV sample format {raw.dtype}")
    if not np.all(np.isfinite(audio)):
        raise ValueError(f"{path}: nonfinite sample")
    if len(audio) < int(sample_rate * 0.3):
        raise ValueError(f"{path}: at least 0.3 seconds required")
    if np.max(np.abs(audio)) >= 0.999:
        raise ValueError(f"{path}: clipped or near-clipped samples")
    return sample_rate, audio


def write_wav(path: Path, sample_rate: int, audio: np.ndarray) -> None:
    if not np.all(np.isfinite(audio)):
        raise ValueError("cannot write nonfinite audio")
    path.parent.mkdir(parents=True, exist_ok=True)
    wavfile.write(path, sample_rate, np.asarray(np.clip(audio, -1.0, 1.0), dtype=np.float32))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()
