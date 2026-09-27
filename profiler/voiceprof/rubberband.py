from __future__ import annotations

import subprocess
from pathlib import Path

import numpy as np

from .mapping import map_pitch


def write_pitchmap(path: Path, f0: np.ndarray, times: np.ndarray, sample_rate: int, pitch_transform: dict) -> None:
    mapped = map_pitch(f0, pitch_transform)
    median_shift = 12 * (pitch_transform["targetKnotsLog2Hz"][3] - pitch_transform["sourceKnotsLog2Hz"][3])
    last_shift = float(np.clip(median_shift, -pitch_transform["limits"]["maxShiftSemitones"], pitch_transform["limits"]["maxShiftSemitones"]))
    lines = []
    previous_frame = -1
    for source_f0, output_f0, time in zip(f0, mapped, times):
        if source_f0 > 0 and output_f0 > 0:
            last_shift = float(12 * np.log2(output_f0 / source_f0))
        frame = int(round(float(time) * sample_rate))
        if frame > previous_frame:
            lines.append(f"{frame} {last_shift:.6f}")
            previous_frame = frame
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


def render_rubberband(executable: Path, input_path: Path, output_path: Path, pitchmap_path: Path) -> None:
    if not executable.is_file():
        raise FileNotFoundError(f"Rubber Band executable not found: {executable}")
    subprocess.run([str(executable), "-R", "-2", "-F", "--pitchmap", str(pitchmap_path), str(input_path), str(output_path)], check=True, timeout=120)
