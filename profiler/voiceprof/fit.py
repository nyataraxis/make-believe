from __future__ import annotations

import hashlib
import json

import numpy as np
from scipy.interpolate import PchipInterpolator
from scipy.ndimage import gaussian_filter1d

from .mapping import warp_frequencies

MIN_WARP_RATIO = 0.75
MAX_WARP_RATIO = 1.35
WARP_GRID = np.array([100, 250, 500, 750, 1000, 1500, 2000, 3000, 4000, 5500, 6500, 8000], dtype=float)


def _isotonic(values: np.ndarray, weights: np.ndarray) -> np.ndarray:
    blocks: list[tuple[int, int, float, float]] = []
    for index, (value, weight) in enumerate(zip(values, weights)):
        blocks.append((index, index + 1, float(value * weight), float(weight)))
        while len(blocks) > 1 and blocks[-2][2] / blocks[-2][3] > blocks[-1][2] / blocks[-1][3]:
            left = blocks.pop(-2)
            right = blocks.pop(-1)
            blocks.append((left[0], right[1], left[2] + right[2], left[3] + right[3]))
    result = np.empty_like(values)
    for start, end, total, weight in blocks:
        result[start:end] = total / weight
    return result


def _sample_bounded_warp(curve: PchipInterpolator) -> list[dict]:
    grid = WARP_GRID
    output = np.exp(curve(np.log(grid)))
    high_band_taper = np.clip((8000.0 - grid) / 4000.0, 0.0, 1.0)
    output = grid + (output - grid) * high_band_taper
    output = np.clip(output, grid * MIN_WARP_RATIO, grid * MAX_WARP_RATIO)
    output[0] = 100
    output[-1] = 8000
    if np.any(~np.isfinite(output)) or np.any(np.diff(output) <= 0):
        raise ValueError("fitted warp is not strictly monotonic")
    return [{"inputHz": float(a), "outputHz": float(b)} for a, b in zip(grid, output)]


def fit_labeled_vowel_warp(source_vowel: dict, target_vowel: dict) -> list[dict]:
    formants = ("F1", "F2", "F3")
    source = np.array([100.0, *(source_vowel[key]["median"] for key in formants), 8000.0])
    target = np.array([100.0, *(target_vowel[key]["median"] for key in formants), 8000.0])
    if (np.any(~np.isfinite(source)) or np.any(~np.isfinite(target)) or
            np.any(np.diff(source) <= 0) or np.any(np.diff(target) <= 0)):
        raise ValueError("labeled vowel formants must be finite and strictly increasing")
    target[1:-1] = np.clip(target[1:-1], source[1:-1] * MIN_WARP_RATIO,
                            source[1:-1] * MAX_WARP_RATIO)
    if np.any(np.diff(target) <= 0):
        raise ValueError("bounded vowel warp would fold frequencies")
    return _sample_bounded_warp(PchipInterpolator(np.log(source), np.log(target)))


def formant_consistency(source: dict, target: dict) -> tuple[dict, list[str]]:
    matched = sorted(set(source["vocalTract"]["vowels"]) & set(target["vocalTract"]["vowels"]))
    ranges = {}
    warnings = []
    for formant in ("F1", "F2", "F3"):
        ratios = []
        for label in matched:
            source_hz = source["vocalTract"]["vowels"][label][formant]["median"]
            target_hz = target["vocalTract"]["vowels"][label][formant]["median"]
            if not np.isfinite(source_hz) or not np.isfinite(target_hz) or source_hz <= 0 or target_hz <= 0:
                raise ValueError(f"{label} {formant} medians must be finite and positive")
            ratios.append(target_hz / source_hz)
        if ratios:
            minimum = min(ratios)
            maximum = max(ratios)
            ranges[formant] = {"minimum": float(minimum), "maximum": float(maximum)}
            if maximum / minimum > MAX_WARP_RATIO:
                warnings.append(f"{formant} target/source ratios vary {minimum:.2f}–{maximum:.2f} across vowels; a global warp may be unreliable")
    return ranges, warnings


def fit_warp(source: dict, target: dict) -> tuple[list[dict], list[str]]:
    source_vowels = source["vocalTract"]["vowels"]
    target_vowels = target["vocalTract"]["vowels"]
    matched = sorted(set(source_vowels) & set(target_vowels))
    if len(matched) < 3:
        raise ValueError("at least three matched vowel labels are required for vocal-tract fitting")
    pairs = []
    for vowel in matched:
        for formant in ("F1", "F2", "F3"):
            src = source_vowels[vowel][formant]["median"]
            dst = target_vowels[vowel][formant]["median"]
            if src > 0 and dst > 0:
                pairs.append((src, dst))
    if len(pairs) < 9:
        raise ValueError("at least nine matched formant measurements required")
    pairs.sort()
    x = np.array([pair[0] for pair in pairs])
    y = np.array([pair[1] for pair in pairs])
    log_ratios = np.log(y / x)
    center = np.median(log_ratios)
    mad = np.median(np.abs(log_ratios - center))
    retained = np.abs(log_ratios - center) <= max(0.16, 3 * mad)
    rejected = [f"formant pair {src:.0f}->{dst:.0f} Hz rejected as ratio outlier" for (src, dst), keep in zip(pairs, retained) if not keep]
    x = x[retained]
    y = y[retained]
    if len(x) < 6:
        raise ValueError("too few plausible formant pairs after outlier rejection")
    x = np.concatenate(([100.0], x, [8000.0]))
    y = np.concatenate(([100.0], np.clip(y, x[1:-1] * MIN_WARP_RATIO,
                                        x[1:-1] * MAX_WARP_RATIO), [8000.0]))
    unique_x, inverse = np.unique(x, return_inverse=True)
    averaged_y = np.array([np.median(y[inverse == index]) for index in range(len(unique_x))])
    weights = np.ones(len(unique_x))
    weights[0] = weights[-1] = 1000
    monotone_y = _isotonic(averaged_y, weights)
    if np.any(np.diff(monotone_y) <= 0):
        monotone_y = np.maximum.accumulate(monotone_y + np.arange(len(monotone_y)) * 1e-4)
    curve = PchipInterpolator(np.log(unique_x), np.log(monotone_y))
    return _sample_bounded_warp(curve), rejected


def fit_preset(source: dict, target: dict, preset_id: str, *, pitch_only_pilot: bool = False) -> dict:
    if source["schemaVersion"] != "1.0" or target["schemaVersion"] != "1.0":
        raise ValueError("unsupported profile schema")
    if source["language"] != target["language"] or source["mode"] != target["mode"]:
        raise ValueError("source and target language/mode must match")
    if source["recording"]["sampleRate"] != target["recording"]["sampleRate"]:
        raise ValueError("source and target sample rates must match")
    ratio_ranges, consistency_warnings = formant_consistency(source, target)
    if pitch_only_pilot:
        warp = [{"inputHz": float(frequency), "outputHz": float(frequency)}
                for frequency in (100, 500, 1000, 2000, 4000, 8000)]
        rejected = ["Pitch-only pilot from unlabeled speech; no vocal-tract or spectral correction fit"]
    else:
        warp, rejected = fit_warp(source, target)
    source_bands = np.array(source["spectral"]["analysisBandsHz"])
    target_bands = np.array(target["spectral"]["analysisBandsHz"])
    if not np.allclose(source_bands, target_bands):
        raise ValueError("source and target spectral bands differ")
    source_envelope = np.array(source["spectral"]["meanEnvelopeDb"])
    target_envelope = np.array(target["spectral"]["meanEnvelopeDb"])
    warped_positions = warp_frequencies(source_bands, warp)
    compensated_source = np.interp(source_bands, warped_positions, source_envelope)
    if pitch_only_pilot:
        eq = np.zeros_like(source_bands)
    else:
        eq = gaussian_filter1d(target_envelope - compensated_source, sigma=1.25)
        eq -= np.mean(eq)
        eq = np.clip(eq, -6.0, 6.0)
    knot_names = ("q05", "q10", "q25", "q50", "q75", "q90", "q95")
    source_knots = [source["pitch"]["log2HzQuantiles"][name] for name in knot_names]
    target_knots = [target["pitch"]["log2HzQuantiles"][name] for name in knot_names]
    source_digest = hashlib.sha256(json.dumps(source, sort_keys=True, ensure_ascii=False, allow_nan=False).encode("utf-8")).hexdigest()
    target_digest = hashlib.sha256(json.dumps(target, sort_keys=True, ensure_ascii=False, allow_nan=False).encode("utf-8")).hexdigest()
    return {
        "schemaVersion": "1.0", "id": preset_id,
        "sourceProfileId": source["id"], "targetProfileId": target["id"],
        "sourceProfileSha256": source_digest, "targetProfileSha256": target_digest,
        "language": source["language"], "mode": source["mode"], "sampleRate": source["recording"]["sampleRate"],
        "pitch": {"mode": "quantile", "strength": 1.0, "sourceKnotsLog2Hz": source_knots, "targetKnotsLog2Hz": target_knots,
                  "extrapolation": {"minSlope": 0.25, "maxSlope": 2.0},
                  "limits": {"minOutputHz": 70, "maxOutputHz": 1000, "maxShiftSemitones": 18},
                  "smoothing": {"confidenceThreshold": 0.65, "attackMs": 30, "releaseMs": 80}},
        "vocalTract": {"strength": 0.0 if pitch_only_pilot else 1.0, "representation": "piecewise_warp", "warp": warp,
                       "voicedStrength": 1.0, "unvoicedStrength": 0.0, "maxWarpRatio": MAX_WARP_RATIO},
        "spectral": {"analysisBandsHz": source_bands.tolist(), "correctionDb": eq.tolist(), "strength": 0.0 if pitch_only_pilot else 1.0},
        "sourceQuality": {"status": "unfitted"}, "dynamics": {"status": "unfitted"},
        "safety": {"peakLimitDbFS": -1.0},
        "quality": {"status": "research_only", "warnings": rejected + consistency_warnings + ["Source-quality, dynamics, and sibilance transforms remain unfitted"],
                    "formantRatioRanges": ratio_ranges},
    }
