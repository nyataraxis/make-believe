from __future__ import annotations

import argparse
from copy import deepcopy
import hashlib
from pathlib import Path
from urllib.request import urlopen

import numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly

from voiceprof.analysis import build_profile
from voiceprof.compile import compile_preset
from voiceprof.fit import fit_preset, fit_warp
from voiceprof.io import read_json, write_json
from voiceprof.render import render_comparison


REPOSITORY = Path(__file__).resolve().parents[1]
INPUTS = REPOSITORY / "docs" / "public-vowel-inputs.json"


def fetch_and_prepare(directory: Path, expected: dict[str, str]) -> None:
    for vowel in "AEIOU":
        for speaker in ("male", "female"):
            name = f"{vowel}_{speaker}.wav"
            url = f"https://raw.githubusercontent.com/SpAA-LAB/Vowels-Recordings/main/{name}"
            with urlopen(url, timeout=30) as response:
                content = response.read()
            if hashlib.sha256(content).hexdigest() != expected[name]:
                raise ValueError(f"download hash changed for {name}")
            raw_path = directory / name
            raw_path.write_bytes(content)
            sample_rate, audio = wavfile.read(raw_path)
            if sample_rate != 44100 or audio.dtype != np.int16 or audio.ndim != 2 or audio.shape[1] != 2:
                raise ValueError(f"unexpected audio format for {name}")
            channel_rms = np.sqrt(np.mean(audio.astype(np.float64) ** 2, axis=0))
            if channel_rms[1] < 100 * channel_rms[0]:
                raise ValueError(f"right channel is not clearly the voice channel for {name}")
            voice = audio[:, 1].astype(np.float64) / 32768.0
            output = resample_poly(voice, 160, 147).astype(np.float32)
            wavfile.write(directory / "48k" / speaker / name, 48000, output)


def main() -> None:
    parser = argparse.ArgumentParser(description="Reproduce the public matched-vowel research pilot")
    parser.add_argument("output_directory", type=Path)
    parser.add_argument("--rubberband", type=Path)
    args = parser.parse_args()
    if args.output_directory.exists():
        raise ValueError("choose a new output directory; existing files are never overwritten")
    source_info = read_json(INPUTS)
    corpus = args.output_directory / "corpus"
    for speaker in ("male", "female"):
        (corpus / "48k" / speaker).mkdir(parents=True)
    fetch_and_prepare(corpus, source_info["rawWavSha256"])
    profiles = {}
    for speaker, ceiling in (("male", 5000), ("female", 5500)):
        directory = corpus / "48k" / speaker
        manifest_path = directory / "manifest.json"
        write_json(manifest_path, {
            "id": f"spaa-{speaker}-public", "displayName": f"SpAA LAB {speaker} public vowels",
            "language": "und", "mode": "speech", "formantCeilingHz": ceiling,
            "recordings": [{"path": f"{vowel}_{speaker}.wav", "vowel": vowel.lower()}
                           for vowel in "AEIOU"],
        })
        profiles[speaker] = build_profile(manifest_path)
        write_json(args.output_directory / f"{speaker}-profile.json", profiles[speaker])
    preset = fit_preset(profiles["male"], profiles["female"], "spaa-male-to-female")
    write_json(args.output_directory / "preset.json", preset)
    (args.output_directory / "preset.vmp").write_bytes(compile_preset(preset))
    cases = {}
    for vowel in "AEIOU":
        key = vowel.lower()
        result = render_comparison(corpus / "48k" / "male" / f"{vowel}_male.wav",
                                   args.output_directory / f"{key}-comparison", preset,
                                   args.rubberband if vowel == "A" else None,
                                   profiles["female"], key, profiles["male"])
        held_out_source = deepcopy(profiles["male"])
        held_out_target = deepcopy(profiles["female"])
        held_out_source["vocalTract"]["vowels"].pop(key)
        held_out_target["vocalTract"]["vowels"].pop(key)
        held_out_warp, _ = fit_warp(held_out_source, held_out_target)
        held_out_preset = deepcopy(preset)
        held_out_preset["id"] = f"{preset['id']}-formant-heldout-{key}"
        held_out_preset["vocalTract"]["warp"] = held_out_warp
        held_out_result = render_comparison(corpus / "48k" / "male" / f"{vowel}_male.wav",
                                            args.output_directory / f"{key}-heldout-formant",
                                            held_out_preset, None, profiles["female"], key)
        cases[key] = {
            "targetMedianHz": result["formantReference"]["targetMedianHz"],
            "meanFormantAbsoluteErrorPercent": {
                name: variant["meanFormantAbsoluteErrorPercent"]
                for name, variant in result["variants"].items()
            },
            "report": str(args.output_directory / f"{key}-comparison" / "report.json"),
            "leaveOneVowelOutFormantErrorPercent": held_out_result["variants"]["pitch-formant"]["meanFormantAbsoluteErrorPercent"],
            "leaveOneVowelOutReport": str(args.output_directory / f"{key}-heldout-formant" / "report.json"),
        }
    write_json(args.output_directory / "pilot-report.json", {
        "source": source_info["source"],
        "sourceMedianF0Hz": profiles["male"]["pitch"]["medianHz"],
        "targetMedianF0Hz": profiles["female"]["pitch"]["medianHz"],
        "fitWarnings": preset["quality"]["warnings"],
        "vowels": cases,
        "limitations": ["The dataset language is unspecified",
                        "Praat formant estimates require visual and listening review",
                        "The vowel oracle uses a known label unavailable to live speech",
                        "No source or target recording from the intended user was evaluated"],
    })
    print(args.output_directory / "pilot-report.json")


if __name__ == "__main__":
    main()
