from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from urllib.parse import urlencode
from urllib.request import urlopen

import numpy as np
import pyworld
from scipy.io import wavfile
from scipy.signal import resample_poly

from voiceprof.analysis import build_profile
from voiceprof.compile import compile_preset
from voiceprof.fit import fit_preset
from voiceprof.io import read_json, write_json
from voiceprof.mapping import map_pitch
from voiceprof.native_render import render_native
from voiceprof.render import render_comparison


REPOSITORY = Path(__file__).resolve().parents[1]
INPUTS = REPOSITORY / "docs" / "public-arctic-pilot-inputs.json"


def fetch_samples(directory: Path, expected: dict[str, str]) -> None:
    for speaker in ("bdl", "slt"):
        query = urlencode({"dataset": "MikhailT/cmu-arctic", "config": "default",
                           "split": speaker, "offset": 0, "length": 8})
        with urlopen("https://datasets-server.huggingface.co/rows?" + query, timeout=30) as response:
            rows = json.load(response)["rows"]
        if len(rows) != 8:
            raise ValueError(f"expected eight {speaker} rows")
        for index, entry in enumerate(rows, start=1):
            row = entry["row"]
            name = f"{speaker}-a{index:04d}.wav"
            if row["speaker"] != speaker or row["file"] != name:
                raise ValueError(f"dataset row order or name changed for {name}")
            with urlopen(row["audio"][0]["src"], timeout=30) as response:
                content = response.read()
            if hashlib.sha256(content).hexdigest() != expected[name]:
                raise ValueError(f"download hash changed for {name}")
            raw_path = directory / name
            raw_path.write_bytes(content)
            sample_rate, audio = wavfile.read(raw_path)
            if sample_rate != 16000 or audio.dtype != np.int16 or audio.ndim != 1:
                raise ValueError(f"unexpected audio format for {name}")
            resampled = resample_poly(audio.astype(np.float64) / 32768.0, 3, 1)
            wavfile.write(directory / "48k" / name, 48000, resampled.astype(np.float32))


def measure_f0(path: Path) -> np.ndarray:
    sample_rate, raw = wavfile.read(path)
    if sample_rate != 48000:
        raise ValueError("evaluation audio must be 48 kHz")
    audio = raw.astype(np.float64)
    if raw.dtype == np.int16:
        audio /= 32768.0
    f0, times = pyworld.dio(audio, sample_rate, frame_period=5.0)
    return pyworld.stonemask(audio, f0, times, sample_rate)


def median_voiced(f0: np.ndarray) -> float:
    voiced = f0[f0 > 0]
    if not len(voiced):
        raise ValueError("evaluation speech has no voiced F0 frames")
    return float(np.median(voiced))


def evaluate(source: Path, target: Path, rendered: Path, preset: dict) -> dict[str, float]:
    source_f0 = measure_f0(source)
    target_f0 = measure_f0(target)
    rendered_f0 = measure_f0(rendered)
    desired_f0 = map_pitch(source_f0, preset["pitch"])
    common = min(len(desired_f0), len(rendered_f0))
    desired = desired_f0[:common]
    measured = rendered_f0[:common]
    both_voiced = (desired > 0) & (measured > 0)
    if not np.any(both_voiced):
        raise ValueError("no matching voiced frames in evaluation")
    cents = np.abs(1200 * np.log2(measured[both_voiced] / desired[both_voiced]))
    return {
        "sourceMedianHz": median_voiced(source_f0),
        "pairedTargetMedianHz": median_voiced(target_f0),
        "nativeMedianHz": median_voiced(rendered_f0),
        "medianAbsoluteMappedPitchErrorCents": float(np.median(cents)),
        "voicingMismatchFraction": float(np.mean((desired > 0) != (measured > 0))),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Reproduce the public pitch-only ARCTIC pilot")
    parser.add_argument("output_directory", type=Path)
    parser.add_argument("--morph-raw", type=Path, required=True)
    parser.add_argument("--window", choices=("short", "medium"), default="short")
    parser.add_argument("--rubberband", type=Path)
    args = parser.parse_args()
    if args.output_directory.exists():
        raise ValueError("choose a new output directory; existing files are never overwritten")
    expected = read_json(INPUTS)["rawWavSha256"]
    args.output_directory.mkdir(parents=True)
    raw_directory = args.output_directory / "corpus"
    (raw_directory / "48k").mkdir(parents=True)
    fetch_samples(raw_directory, expected)
    profiles = []
    for speaker in ("bdl", "slt"):
        manifest_path = raw_directory / "48k" / f"{speaker}-manifest.json"
        write_json(manifest_path, {
            "id": f"cmu-arctic-{speaker}-pilot",
            "displayName": f"CMU ARCTIC {speaker} public pilot",
            "language": "en-US", "mode": "speech",
            "recordings": [{"path": f"{speaker}-a{index:04d}.wav"} for index in range(1, 4)],
        })
        profile = build_profile(manifest_path)
        write_json(args.output_directory / f"{speaker}-profile.json", profile)
        profiles.append(profile)
    preset = fit_preset(*profiles, "arctic-bdl-to-slt-pitch-pilot", pitch_only_pilot=True)
    preset_path = args.output_directory / "pilot-preset.json"
    native_preset_path = args.output_directory / "pilot-preset.vmp"
    write_json(preset_path, preset)
    native_preset_path.write_bytes(compile_preset(preset))
    cases = {}
    for index in range(4, 9):
        name = f"a{index:04d}"
        source = raw_directory / "48k" / f"bdl-{name}.wav"
        target = raw_directory / "48k" / f"slt-{name}.wav"
        rendered = args.output_directory / f"native-{name}.wav"
        native_stats = render_native(source, native_preset_path, rendered, args.morph_raw, args.window)
        cases[name] = {**evaluate(source, target, rendered, preset), "nativeStats": native_stats}
    comparison = render_comparison(raw_directory / "48k" / "bdl-a0004.wav",
                                   args.output_directory / "world-a0004", preset, args.rubberband)
    write_json(args.output_directory / "pilot-report.json", {
        "source": read_json(INPUTS)["source"],
        "nativeAnalysisWindow": args.window,
        "calibrationUtterances": ["a0001", "a0002", "a0003"],
        "heldOutUtterances": cases,
        "worldComparison": str(args.output_directory / "world-a0004" / "report.json"),
        "worldVariants": list(comparison["variants"]),
        "limitations": ["Pitch-only preset; formant and spectral transforms are disabled",
                        "Paired transcripts are not time-aligned",
                        "Objective F0 measurements do not establish perceptual quality"],
    })
    print(args.output_directory / "pilot-report.json")


if __name__ == "__main__":
    main()
