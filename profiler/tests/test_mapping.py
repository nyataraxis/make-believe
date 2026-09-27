import numpy as np
import pytest

from voiceprof.fit import fit_preset
from voiceprof.mapping import map_pitch, warp_frequencies
from voiceprof.rubberband import write_pitchmap


def _profile(identifier: str, pitch_offset: float, formant_ratio: float) -> dict:
    base_knots = np.log2(np.array([90, 100, 115, 130, 150, 175, 195]))
    vowels = {}
    for label, base in (("a", (700, 1300, 2600)), ("i", (300, 2300, 3100)), ("u", (350, 900, 2500))):
        vowels[label] = {f"F{index}": {"median": value * formant_ratio} for index, value in enumerate(base, 1)}
    return {"schemaVersion": "1.0", "id": identifier, "language": "ja-JP", "mode": "speech",
            "recording": {"sampleRate": 48000},
            "pitch": {"log2HzQuantiles": dict(zip(("q05", "q10", "q25", "q50", "q75", "q90", "q95"), base_knots + pitch_offset))},
            "vocalTract": {"vowels": vowels},
            "spectral": {"analysisBandsHz": np.geomspace(150, 8000, 12).tolist(), "meanEnvelopeDb": np.zeros(12).tolist()}}


def test_pitch_mapping_is_monotonic_and_preserves_unvoiced() -> None:
    preset = fit_preset(_profile("source", 0, 1), _profile("target", 0.7, 1.12), "test")
    values = np.array([0, 80, 90, 110, 130, 170, 210, 300], dtype=float)
    result = map_pitch(values, preset["pitch"])
    assert result[0] == 0
    assert np.all(np.diff(result[1:]) > 0)
    assert result[4] > values[4]
    preset["pitch"]["strength"] = 0
    assert np.array_equal(map_pitch(values, preset["pitch"]), values)


def test_formant_warp_monotonic_and_tapers_to_unity() -> None:
    preset = fit_preset(_profile("source", 0, 1), _profile("target", 0.7, 1.12), "test")
    knots = preset["vocalTract"]["warp"]
    values = warp_frequencies(np.linspace(100, 8000, 100), knots)
    assert np.all(np.diff(values) > 0)
    assert values[-1] == pytest.approx(8000)
    assert values[10] > 100


def test_formant_fit_requires_matched_vowels() -> None:
    target = _profile("target", 0.7, 1.12)
    target["vocalTract"]["vowels"] = {"a": target["vocalTract"]["vowels"]["a"]}
    with pytest.raises(ValueError, match="three matched"):
        fit_preset(_profile("source", 0, 1), target, "test")


def test_rubberband_pitchmap_has_increasing_frame_indices(tmp_path) -> None:
    preset = fit_preset(_profile("source", 0, 1), _profile("target", 0.7, 1.12), "test")
    path = tmp_path / "pitchmap.txt"
    write_pitchmap(path, np.array([0, 110, 130, 0, 140], dtype=float), np.arange(5) * 0.005, 48000, preset["pitch"])
    entries = [line.split() for line in path.read_text(encoding="ascii").splitlines()]
    assert [int(entry[0]) for entry in entries] == [0, 240, 480, 720, 960]
    assert all(np.isfinite(float(entry[1])) for entry in entries)
