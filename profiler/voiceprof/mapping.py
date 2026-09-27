from __future__ import annotations

import numpy as np

QUANTILES = np.array([0.05, 0.10, 0.25, 0.50, 0.75, 0.90, 0.95])


def pitch_knots(f0_hz: np.ndarray) -> list[float]:
    voiced = np.asarray(f0_hz, dtype=np.float64)
    voiced = voiced[np.isfinite(voiced) & (voiced > 0)]
    if len(voiced) < 20:
        raise ValueError("at least 20 voiced F0 frames required")
    knots = np.quantile(np.log2(voiced), QUANTILES)
    if np.any(np.diff(knots) <= 1e-5):
        raise ValueError("pitch distribution has repeated quantiles; need varied voiced speech")
    return knots.tolist()


def map_pitch(f0_hz: np.ndarray, transform: dict) -> np.ndarray:
    f0 = np.asarray(f0_hz, dtype=np.float64)
    result = np.zeros_like(f0)
    voiced = np.isfinite(f0) & (f0 > 0)
    if not np.any(voiced):
        return result
    if transform["strength"] == 0:
        result[voiced] = f0[voiced]
        return result
    x = np.log2(f0[voiced])
    source = np.asarray(transform["sourceKnotsLog2Hz"], dtype=np.float64)
    target = np.asarray(transform["targetKnotsLog2Hz"], dtype=np.float64)
    if len(source) != 7 or len(target) != 7 or np.any(np.diff(source) <= 0) or np.any(np.diff(target) <= 0):
        raise ValueError("pitch knots must be seven strictly increasing values")
    mapped = np.interp(x, source, target)
    min_slope = transform["extrapolation"]["minSlope"]
    max_slope = transform["extrapolation"]["maxSlope"]
    low_slope = np.clip((target[1] - target[0]) / (source[1] - source[0]), min_slope, max_slope)
    high_slope = np.clip((target[-1] - target[-2]) / (source[-1] - source[-2]), min_slope, max_slope)
    mapped = np.where(x < source[0], target[0] + low_slope * (x - source[0]), mapped)
    mapped = np.where(x > source[-1], target[-1] + high_slope * (x - source[-1]), mapped)
    shift = np.clip(mapped - x, -transform["limits"]["maxShiftSemitones"] / 12, transform["limits"]["maxShiftSemitones"] / 12)
    out = np.exp2(x + transform["strength"] * shift)
    result[voiced] = np.clip(out, transform["limits"]["minOutputHz"], transform["limits"]["maxOutputHz"])
    return result


def warp_frequencies(frequencies_hz: np.ndarray, knots: list[dict]) -> np.ndarray:
    x = np.asarray([k["inputHz"] for k in knots], dtype=np.float64)
    y = np.asarray([k["outputHz"] for k in knots], dtype=np.float64)
    if np.any(np.diff(x) <= 0) or np.any(np.diff(y) <= 0):
        raise ValueError("vocal-tract warp must be strictly monotonic")
    return np.interp(frequencies_hz, x, y, left=y[0], right=y[-1])
