from __future__ import annotations

from collections import defaultdict
from pathlib import Path

import numpy as np
import parselmouth
import pyworld
import scipy

from .io import read_json, read_wav, sha256
from .mapping import QUANTILES, pitch_knots

BANDS_HZ = np.geomspace(150.0, 8000.0, 12)


def distribution(values: np.ndarray) -> dict:
    finite = np.asarray(values, dtype=np.float64)
    finite = finite[np.isfinite(finite)]
    if not len(finite):
        raise ValueError("no finite measurements")
    return {"median": float(np.median(finite)), "q10": float(np.quantile(finite, 0.1)), "q90": float(np.quantile(finite, 0.9))}


def world_analysis(audio: np.ndarray, sample_rate: int) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    # DIO and StoneMask are kept together so F0 and the WORLD envelopes share frame times.
    f0, times = pyworld.dio(audio, sample_rate, frame_period=5.0)
    f0 = pyworld.stonemask(audio, f0, times, sample_rate)
    spectrum = pyworld.cheaptrick(audio, f0, times, sample_rate)
    aperiodicity = pyworld.d4c(audio, f0, times, sample_rate)
    return f0, times, spectrum, aperiodicity


def analyze_recording(path: Path, vowel: str | None, formant_ceiling_hz: float = 5500.0) -> tuple[dict, dict]:
    sample_rate, audio = read_wav(path)
    if len(audio) / sample_rate > 20:
        raise ValueError(f"{path}: split recordings longer than 20 seconds into shorter takes before WORLD analysis")
    sound = parselmouth.Sound(audio, sampling_frequency=sample_rate)
    pitch = sound.to_pitch_ac(time_step=0.01, pitch_floor=60, pitch_ceiling=600)
    praat_f0 = pitch.selected_array["frequency"]
    hnr = sound.to_harmonicity_cc(time_step=0.01, minimum_pitch=60).values.ravel()
    hnr = hnr[np.isfinite(hnr) & (hnr > -100)]
    world_f0, times, spectrum, aperiodicity = world_analysis(audio, sample_rate)
    if not np.all(np.isfinite(spectrum)) or not np.all(np.isfinite(aperiodicity)):
        raise ValueError(f"{path}: WORLD returned nonfinite values")
    frequencies = np.linspace(0, sample_rate / 2, spectrum.shape[1])
    if sample_rate / 2 < BANDS_HZ[-1]:
        raise ValueError(f"{path}: sample rate too low for 8 kHz spectral analysis")
    log_envelope = 20 * np.log10(np.maximum(spectrum, 1e-12))
    spectral_bands = np.vstack([np.interp(BANDS_HZ, frequencies, row) for row in log_envelope])
    spectral_bands -= np.mean(spectral_bands, axis=1, keepdims=True)
    voiced = world_f0 > 0
    stable_voiced = voiced & (times > times[-1] * 0.2) & (times < times[-1] * 0.8)
    formants: dict[str, list[float]] = defaultdict(list)
    if vowel:
        formant_object = sound.to_formant_burg(time_step=0.01, max_number_of_formants=5, maximum_formant=formant_ceiling_hz)
        for time in times[stable_voiced][::2]:
            for index in range(1, 4):
                value = formant_object.get_value_at_time(index, float(time))
                if np.isfinite(value) and 150 < value < 5500:
                    formants[f"F{index}"].append(float(value))
    rms_window = max(1, sample_rate // 100)
    frame_rms = np.sqrt(np.mean(audio[: len(audio) // rms_window * rms_window].reshape(-1, rms_window) ** 2, axis=1))
    frame_rms_db = 20 * np.log10(np.maximum(frame_rms, 1e-12))
    details = {
        "sampleRate": sample_rate,
        "durationSeconds": len(audio) / sample_rate,
        "peakDbFS": float(20 * np.log10(max(np.max(np.abs(audio)), 1e-12))),
        "rmsDbFS": float(20 * np.log10(max(np.sqrt(np.mean(audio ** 2)), 1e-12))),
        "praatVoicedFrames": int(np.count_nonzero(praat_f0)),
        "worldVoicedFrames": int(np.count_nonzero(voiced)),
        "worldFrames": len(world_f0),
        "f0Hz": world_f0[voiced].tolist(),
        "contourLog2Step": np.abs(np.diff(np.log2(np.maximum(world_f0, 1.0))))[voiced[1:] & voiced[:-1]].tolist(),
        "praatF0Hz": praat_f0[praat_f0 > 0].tolist(),
        "hnrDb": hnr.tolist(),
        "formants": dict(formants),
        "spectralBandsDb": spectral_bands[voiced].tolist(),
        "aperiodicityBands": [float(np.mean(aperiodicity[voiced, np.argmin(np.abs(frequencies - band))])) for band in BANDS_HZ] if np.any(voiced) else [],
        "frameRmsDbFS": frame_rms_db.tolist(),
    }
    return details, {"f0Hz": world_f0, "times": times, "spectrum": spectrum, "aperiodicity": aperiodicity}


def build_profile(manifest_path: Path) -> dict:
    manifest = read_json(manifest_path)
    required = ("id", "displayName", "language", "mode", "recordings")
    if any(key not in manifest for key in required) or manifest.get("mode") not in ("speech", "singing"):
        raise ValueError("manifest requires id, displayName, language, speech/singing mode, recordings")
    if not manifest["recordings"]:
        raise ValueError("manifest has no recordings")
    formant_ceiling_hz = manifest.get("formantCeilingHz", 5500)
    if not 4000 <= formant_ceiling_hz <= 6500:
        raise ValueError("formantCeilingHz must be between 4000 and 6500")
    measurements = []
    provenance = []
    for item in manifest["recordings"]:
        path = (manifest_path.parent / item["path"]).resolve()
        vowel = item.get("vowel")
        if vowel is not None and vowel not in ("a", "i", "u", "e", "o"):
            raise ValueError(f"{path}: vowel label must be a, i, u, e, or o")
        detail, _ = analyze_recording(path, vowel, formant_ceiling_hz)
        measurements.append((vowel, detail))
        provenance.append({"path": item["path"], "sha256": sha256(path), "vowel": vowel, "durationSeconds": detail["durationSeconds"], "peakDbFS": detail["peakDbFS"]})
    rates = {detail["sampleRate"] for _, detail in measurements}
    if len(rates) != 1:
        raise ValueError("all recordings must share a sample rate")
    all_f0 = np.array([value for _, detail in measurements for value in detail["f0Hz"]])
    knots = pitch_knots(all_f0)
    cents_per_second = [value * 1200 / 0.005 for _, detail in measurements for value in detail["contourLog2Step"]]
    vowels = {}
    for label in ("a", "i", "u", "e", "o"):
        matching = [detail for vowel, detail in measurements if vowel == label]
        if matching and all(sum(len(detail["formants"].get(key, [])) for detail in matching) >= 10 for key in ("F1", "F2", "F3")):
            vowels[label] = {"count": sum(len(detail["formants"].get("F1", [])) for detail in matching), **{key: distribution(np.array([v for detail in matching for v in detail["formants"].get(key, [])])) for key in ("F1", "F2", "F3")}}
    bands = np.array([band for _, detail in measurements for band in detail["spectralBandsDb"]])
    if len(bands) < 20:
        raise ValueError("at least 20 voiced WORLD frames required for spectral profile")
    hnr = np.array([value for _, detail in measurements for value in detail["hnrDb"]])
    if not len(hnr):
        raise ValueError("Praat found no harmonic frames")
    praat_f0 = np.array([value for _, detail in measurements for value in detail["praatF0Hz"]])
    if not len(praat_f0):
        raise ValueError("Praat found no voiced pitch frames")
    frame_rms = np.array([value for _, detail in measurements for value in detail["frameRmsDbFS"]])
    q = np.quantile(np.log2(all_f0), QUANTILES)
    return {
        "schemaVersion": "1.0", "id": manifest["id"], "displayName": manifest["displayName"],
        "language": manifest["language"], "mode": manifest["mode"],
        "recording": {"sampleRate": rates.pop(), "channels": 1, "usableDurationSeconds": sum(detail["durationSeconds"] for _, detail in measurements)},
        "pitch": {"voicedFraction": float(sum(detail["worldVoicedFrames"] for _, detail in measurements) / sum(detail["worldFrames"] for _, detail in measurements)),
                  "log2HzQuantiles": dict(zip(("q05", "q10", "q25", "q50", "q75", "q90", "q95"), map(float, q))),
                  "medianHz": float(2 ** q[3]), "rangeCents": {"p10ToP90": float((q[5] - q[1]) * 1200), "p05ToP95": float((q[6] - q[0]) * 1200)},
                  "contourVelocity": distribution(np.array(cents_per_second)) if cents_per_second else None},
        "vocalTract": {"vowels": vowels, "formantSpacingHzMedian": float(np.median([v["F3"]["median"] - v["F1"]["median"] for v in vowels.values()])) if vowels else None, "estimatedGlobalWarp": None, "formantCeilingHz": formant_ceiling_hz, "confidence": min(1.0, len(vowels) / 5)},
        "sourceQuality": {"hnrDb": distribution(hnr), "voicedAperiodicityBands": np.mean(np.array([d["aperiodicityBands"] for _, d in measurements if d["aperiodicityBands"]]), axis=0).tolist(), "spectralTiltDbPerOctave": distribution((np.diff(bands, axis=1) / np.diff(np.log2(BANDS_HZ))).ravel()), "periodicity": None},
        "spectral": {"analysisBandsHz": BANDS_HZ.tolist(), "meanEnvelopeDb": np.mean(bands, axis=0).tolist(), "envelopeStdDb": np.std(bands, axis=0).tolist()},
        "dynamics": {"frameRmsDbFS": distribution(frame_rms)},
        "sibilance": {"status": "not_measured"},
        "quality": {"status": "research_only", "warnings": (["Fewer than three usable labeled vowels; vocal-tract fit unavailable"] if len(vowels) < 3 else []), "praatWorldMedianF0DifferenceCents": float(abs(np.median(np.log2(praat_f0)) - np.median(np.log2(all_f0))) * 1200)},
        "analysisSoftware": {"numpy": np.__version__, "scipy": scipy.__version__, "parselmouth": parselmouth.__version__, "pyworld": pyworld.__version__},
        "provenance": provenance,
    }
