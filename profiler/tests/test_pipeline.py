import json
import os
from copy import deepcopy
from pathlib import Path

import numpy as np
import pytest
from jsonschema import validate
from scipy.io import wavfile
from scipy.signal import lfilter

from voiceprof.analysis import build_profile
from voiceprof.compile import compile_preset
from voiceprof.fit import fit_preset
from voiceprof.render import render_comparison

SCHEMAS = Path(__file__).resolve().parents[2] / "schemas"


def _vowel(sample_rate: int, f0: float, formants: tuple[float, float, float]) -> np.ndarray:
    duration = 1.25
    times = np.arange(int(sample_rate * duration)) / sample_rate
    phase = np.cumsum((f0 + 6 * np.sin(2 * np.pi * 2 * times)) / sample_rate)
    excitation = (np.mod(phase, 1.0) < (f0 / sample_rate)).astype(float)
    excitation -= np.mean(excitation)
    audio = excitation
    for frequency in formants:
        pole = np.exp(-np.pi * 90 / sample_rate)
        audio = lfilter([1.0], [1.0, -2 * pole * np.cos(2 * np.pi * frequency / sample_rate), pole ** 2], audio)
    audio /= max(np.max(np.abs(audio)), 1e-12)
    return (0.4 * audio).astype(np.float32)


def test_profile_fit_and_render(tmp_path) -> None:
    sample_rate = 48000
    profiles = []
    for identifier, f0, scale in (("source", 130, 1.0), ("target", 220, 1.12)):
        folder = tmp_path / identifier
        folder.mkdir()
        recordings = []
        for label, formants in (("a", (700, 1300, 2600)), ("i", (300, 2300, 3100)), ("u", (350, 900, 2500))):
            name = f"{label}.wav"
            wavfile.write(folder / name, sample_rate, _vowel(sample_rate, f0, tuple(value * scale for value in formants)))
            recordings.append({"path": name, "vowel": label})
        manifest = {"id": identifier, "displayName": identifier, "language": "ja-JP", "mode": "speech", "recordings": recordings}
        manifest_path = folder / "manifest.json"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        profiles.append(build_profile(manifest_path))
        validate(profiles[-1], json.loads((SCHEMAS / "speaker-profile.schema.json").read_text(encoding="utf-8")))
    assert len(profiles[0]["provenance"]) == 3
    assert len(profiles[0]["vocalTract"]["vowels"]) == 3
    unlabeled = [{**profile, "vocalTract": {**profile["vocalTract"], "vowels": {}}} for profile in profiles]
    pilot = fit_preset(*unlabeled, "pitch-only-pilot", pitch_only_pilot=True)
    assert pilot["vocalTract"]["strength"] == 0.0
    assert pilot["spectral"]["strength"] == 0.0
    assert compile_preset(pilot)
    invalid_target = deepcopy(profiles[1])
    invalid_target["vocalTract"]["vowels"]["a"]["F1"]["median"] = float("nan")
    with pytest.raises(ValueError, match="medians must be finite and positive"):
        fit_preset(profiles[0], invalid_target, "invalid")
    preset = fit_preset(*profiles, "synthetic-test")
    warp_input = np.array([point["inputHz"] for point in preset["vocalTract"]["warp"]])
    warp_output = np.array([point["outputHz"] for point in preset["vocalTract"]["warp"]])
    assert np.all(np.diff(warp_output) > 0)
    assert np.all(warp_output / warp_input <= preset["vocalTract"]["maxWarpRatio"] + 1e-12)
    assert np.all(warp_output / warp_input >= 0.75 - 1e-12)
    validate(preset, json.loads((SCHEMAS / "morph-preset.schema.json").read_text(encoding="utf-8")))
    compiled = compile_preset(preset)
    assert len(compiled) == 260
    unsupported = deepcopy(preset)
    unsupported["pitch"]["smoothing"]["attackMs"] = 20
    with pytest.raises(ValueError, match="smoothing"):
        compile_preset(unsupported)
    rubberband = Path(os.environ["RUBBERBAND_EXECUTABLE"]) if "RUBBERBAND_EXECUTABLE" in os.environ else None
    report = render_comparison(tmp_path / "source" / "a.wav", tmp_path / "comparison", preset,
                               rubberband, profiles[1], "a", profiles[0])
    assert report["formantReference"]["targetProfileId"] == "target"
    assert (report["variants"]["pitch-formant"]["meanFormantAbsoluteErrorPercent"] <
            report["variants"]["pitch-only"]["meanFormantAbsoluteErrorPercent"])
    expected = {"original", "fixed-pitch", "pitch-only", "pitch-global-formant", "pitch-formant", "full-unprotected", "full", "vowel-oracle"}
    if rubberband is not None:
        expected.add("rubberband-pitch")
    assert set(report["variants"]) == expected
    for variant in report["variants"].values():
        assert variant["nonfiniteSamples"] == 0
        if variant["path"].endswith("rubberband-pitch.wav"):
            assert variant["durationSeconds"] > 0
        else:
            assert variant["durationSeconds"] == report["durationSeconds"]
        assert variant["peakDbFS"] <= -0.9
        assert 0 <= variant["meanFormantAbsoluteErrorPercent"] < 200
