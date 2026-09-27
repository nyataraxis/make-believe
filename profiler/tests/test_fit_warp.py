import numpy as np

from voiceprof.fit import fit_labeled_vowel_warp, fit_warp, formant_consistency


SOURCE = {
    "a": (670, 1154, 2648),
    "e": (467, 1730, 2517),
    "i": (332, 2103, 2686),
    "o": (576, 1037, 2604),
    "u": (372, 797, 2598),
}
TARGET = {
    "a": (1044, 1366, 2779),
    "e": (647, 2168, 2867),
    "i": (261, 2592, 3583),
    "o": (756, 1102, 2766),
    "u": (382, 679, 2620),
}


def _vowels(measurements: dict[str, tuple[int, int, int]]) -> dict:
    return {label: {f"F{index}": {"median": value}
                    for index, value in enumerate(formants, start=1)}
            for label, formants in measurements.items()}


def test_real_vowel_geometry_keeps_final_warp_bounded_and_monotonic() -> None:
    source = {"vocalTract": {"vowels": _vowels(SOURCE)}}
    target = {"vocalTract": {"vowels": _vowels(TARGET)}}
    warp, rejected = fit_warp(source, target)
    ranges, warnings = formant_consistency(source, target)
    frequencies = np.array([point["inputHz"] for point in warp])
    mapped = np.array([point["outputHz"] for point in warp])
    assert len(rejected) == 2
    assert ranges["F1"]["minimum"] < 1 < ranges["F1"]["maximum"]
    assert any("F1" in warning for warning in warnings)
    assert any("F2" in warning for warning in warnings)
    assert np.all(np.diff(mapped) > 0)
    assert np.all(mapped <= 1.35 * frequencies + 1e-10)
    assert np.all(mapped >= 0.75 * frequencies - 1e-10)
    assert abs(mapped[-2] / frequencies[-2] - 1) < abs(mapped[-4] / frequencies[-4] - 1)
    individual = fit_labeled_vowel_warp(source["vocalTract"]["vowels"]["i"],
                                         target["vocalTract"]["vowels"]["i"])
    individual_ratio = np.array([point["outputHz"] / point["inputHz"] for point in individual])
    assert np.min(individual_ratio) < 1.0
    assert np.max(individual_ratio) <= 1.35 + 1e-10
