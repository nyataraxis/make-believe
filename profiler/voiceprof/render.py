from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
import parselmouth
import pyworld

from .analysis import world_analysis
from .fit import fit_labeled_vowel_warp
from .io import read_wav, sha256, write_json, write_wav
from .mapping import map_pitch
from .rubberband import render_rubberband, write_pitchmap


def _warp_spectrum(spectrum: np.ndarray, sample_rate: int, knots: list[dict]) -> np.ndarray:
    frequencies = np.linspace(0, sample_rate / 2, spectrum.shape[1])
    input_knots = np.array([point["inputHz"] for point in knots])
    output_knots = np.array([point["outputHz"] for point in knots])
    source_frequency = np.interp(frequencies, output_knots, input_knots)
    source_frequency = np.where((frequencies < output_knots[0]) | (frequencies > output_knots[-1]), frequencies, source_frequency)
    result = np.empty_like(spectrum)
    for index, row in enumerate(spectrum):
        result[index] = np.interp(source_frequency, frequencies, row)
    return result


def _spectral_correction(spectrum: np.ndarray, sample_rate: int, preset: dict) -> np.ndarray:
    frequencies = np.linspace(0, sample_rate / 2, spectrum.shape[1])
    bands = preset["spectral"]["analysisBandsHz"]
    correction = preset["spectral"]["correctionDb"]
    db = np.interp(frequencies, bands, correction, left=correction[0], right=0.0)
    return spectrum * np.power(10.0, db[None, :] / 20.0)


def _vowel_formants(audio: np.ndarray, sample_rate: int, ceiling_hz: float) -> dict[str, float]:
    sound = parselmouth.Sound(audio, sampling_frequency=sample_rate)
    formants = sound.to_formant_burg(time_step=0.01, max_number_of_formants=5,
                                    maximum_formant=ceiling_hz)
    duration = len(audio) / sample_rate
    times = np.arange(duration * 0.2, duration * 0.8, 0.01)
    medians = {}
    for index in range(1, 4):
        values = [formants.get_value_at_time(index, float(time)) for time in times]
        usable = [value for value in values if np.isfinite(value) and 150 < value < 5500]
        if len(usable) < 10:
            raise ValueError(f"F{index}: too few reliable formant frames")
        medians[f"F{index}"] = float(np.median(usable))
    return medians


def _render_variant(audio: np.ndarray, sample_rate: int, f0: np.ndarray, spectrum: np.ndarray,
                    aperiodicity: np.ndarray, preset: dict, variant: str,
                    vowel_warp: list[dict] | None = None) -> np.ndarray:
    if variant == "fixed-pitch":
        source_median = preset["pitch"]["sourceKnotsLog2Hz"][3]
        target_median = preset["pitch"]["targetKnotsLog2Hz"][3]
        mapped_f0 = np.where(f0 > 0, np.clip(f0 * 2 ** (target_median - source_median), 70, 1000), 0)
    else:
        mapped_f0 = map_pitch(f0, preset["pitch"])
    adjusted_spectrum = spectrum.copy()
    voiced = f0 > 0
    if variant == "pitch-global-formant":
        knots = preset["vocalTract"]["warp"]
        ratios = [point["outputHz"] / point["inputHz"] for point in knots if 500 <= point["inputHz"] <= 4000]
        ratio = float(np.median(ratios))
        global_knots = [{"inputHz": 100.0, "outputHz": 100.0}, {"inputHz": 4000.0, "outputHz": 4000 * ratio}, {"inputHz": 8000.0, "outputHz": 8000.0}]
        adjusted_spectrum[voiced] = _warp_spectrum(adjusted_spectrum[voiced], sample_rate, global_knots)
    elif variant in ("pitch-formant", "full", "full-unprotected"):
        selection = np.ones(len(f0), dtype=bool) if variant == "full-unprotected" else voiced
        adjusted_spectrum[selection] = _warp_spectrum(adjusted_spectrum[selection], sample_rate, preset["vocalTract"]["warp"])
    elif variant == "vowel-oracle" and vowel_warp is not None:
        adjusted_spectrum[voiced] = _warp_spectrum(adjusted_spectrum[voiced], sample_rate, vowel_warp)
    if variant in ("full", "full-unprotected"):
        selection = np.ones(len(f0), dtype=bool) if variant == "full-unprotected" else voiced
        adjusted_spectrum[selection] = _spectral_correction(adjusted_spectrum[selection], sample_rate, preset)
    result = pyworld.synthesize(mapped_f0, adjusted_spectrum, aperiodicity, sample_rate, frame_period=5.0)
    if len(result) < len(audio):
        result = np.pad(result, (0, len(audio) - len(result)))
    result = result[: len(audio)]
    if not np.all(np.isfinite(result)):
        raise ValueError(f"{variant}: WORLD synthesis returned nonfinite audio")
    return result


def render_comparison(input_path: Path, output_directory: Path, preset: dict,
                      rubberband_executable: Path | None = None,
                      target_profile: dict | None = None, vowel: str | None = None,
                      source_profile: dict | None = None) -> dict:
    if (target_profile is None) != (vowel is None):
        raise ValueError("target profile and vowel label must be supplied together")
    if source_profile is not None and (target_profile is None or vowel is None):
        raise ValueError("vowel oracle requires source and target profiles plus vowel label")
    target_formants = None
    if target_profile is not None:
        digest = hashlib.sha256(json.dumps(target_profile, sort_keys=True, ensure_ascii=False,
                                           allow_nan=False).encode("utf-8")).hexdigest()
        if (digest != preset["targetProfileSha256"] or
                target_profile["id"] != preset["targetProfileId"] or
                vowel not in target_profile["vocalTract"]["vowels"]):
            raise ValueError("target profile or vowel does not match the fitted preset")
        target_formants = {key: target_profile["vocalTract"]["vowels"][vowel][key]["median"]
                           for key in ("F1", "F2", "F3")}
    vowel_warp = None
    if source_profile is not None:
        digest = hashlib.sha256(json.dumps(source_profile, sort_keys=True, ensure_ascii=False,
                                           allow_nan=False).encode("utf-8")).hexdigest()
        if (digest != preset["sourceProfileSha256"] or
                source_profile["id"] != preset["sourceProfileId"] or
                vowel not in source_profile["vocalTract"]["vowels"]):
            raise ValueError("source profile or vowel does not match the fitted preset")
        vowel_warp = fit_labeled_vowel_warp(source_profile["vocalTract"]["vowels"][vowel],
                                             target_profile["vocalTract"]["vowels"][vowel])
    sample_rate, audio = read_wav(input_path)
    if sample_rate != preset["sampleRate"]:
        raise ValueError("input sample rate differs from fitted preset")
    f0, times, spectrum, aperiodicity = world_analysis(audio, sample_rate)
    if np.count_nonzero(f0) < 20:
        raise ValueError("input has too few voiced frames for offline comparison")
    outputs = {"original": audio}
    for variant in ("fixed-pitch", "pitch-only", "pitch-global-formant", "pitch-formant", "full-unprotected", "full"):
        outputs[variant] = _render_variant(audio, sample_rate, f0, spectrum, aperiodicity, preset, variant)
    if vowel_warp is not None:
        outputs["vowel-oracle"] = _render_variant(audio, sample_rate, f0, spectrum,
                                                    aperiodicity, preset, "vowel-oracle", vowel_warp)
    output_directory.mkdir(parents=True, exist_ok=True)
    if rubberband_executable is not None:
        pitchmap_path = output_directory / "rubberband-pitchmap.txt"
        write_pitchmap(pitchmap_path, f0, times, sample_rate, preset["pitch"])
        rubberband_path = output_directory / "rubberband-pitch.wav"
        render_rubberband(rubberband_executable, input_path, rubberband_path, pitchmap_path)
        rubberband_rate, rubberband_audio = read_wav(rubberband_path)
        if rubberband_rate != sample_rate:
            raise ValueError("Rubber Band output sample rate differs from input")
        outputs["rubberband-pitch"] = rubberband_audio
    report = {"input": str(input_path), "inputSha256": sha256(input_path), "presetId": preset["id"], "sampleRate": sample_rate,
              "durationSeconds": len(audio) / sample_rate, "variants": {}}
    if target_formants is not None:
        report["formantReference"] = {"targetProfileId": target_profile["id"], "vowel": vowel,
                                      "targetMedianHz": target_formants}
    if vowel_warp is not None:
        report["vowelOracle"] = {"sourceProfileId": source_profile["id"],
                                 "warp": vowel_warp,
                                 "warning": "Uses known vowel label; unavailable to the live engine"}
    if rubberband_executable is not None:
        report["rubberbandExecutableSha256"] = sha256(rubberband_executable)
    for name, result in outputs.items():
        peak = np.max(np.abs(result))
        gain = min(1.0, 10 ** (-1 / 20) / max(peak, 1e-12))
        result = result * gain
        measured_f0, measured_times = pyworld.dio(result.astype(np.float64), sample_rate, frame_period=5.0)
        measured_f0 = pyworld.stonemask(result.astype(np.float64), measured_f0, measured_times, sample_rate)
        if name == "original":
            desired_f0 = f0
        elif name == "fixed-pitch":
            ratio = 2 ** (preset["pitch"]["targetKnotsLog2Hz"][3] - preset["pitch"]["sourceKnotsLog2Hz"][3])
            desired_f0 = np.where(f0 > 0, np.clip(f0 * ratio, 70, 1000), 0)
        else:
            desired_f0 = map_pitch(f0, preset["pitch"])
        common = min(len(measured_f0), len(desired_f0))
        desired_voiced = desired_f0[:common] > 0
        measured_voiced = measured_f0[:common] > 0
        both_voiced = desired_voiced & measured_voiced
        pitch_error = float(np.median(np.abs(1200 * np.log2(measured_f0[:common][both_voiced] / desired_f0[:common][both_voiced])))) if np.any(both_voiced) else None
        voicing_mismatch = float(np.mean(desired_voiced != measured_voiced))
        if name == "rubberband-pitch":
            pitch_error = None
            voicing_mismatch = None
        path = output_directory / f"{name}.wav"
        write_wav(path, sample_rate, result)
        report["variants"][name] = {"path": str(path), "durationSeconds": len(result) / sample_rate,
                                     "safetyGainDb": float(20 * np.log10(gain)),
                                     "peakDbFS": float(20 * np.log10(max(np.max(np.abs(result)), 1e-12))),
                                     "rmsDbFS": float(20 * np.log10(max(np.sqrt(np.mean(result ** 2)), 1e-12))),
                                     "medianPitchErrorCents": pitch_error,
                                     "voicingMismatchFraction": voicing_mismatch,
                                     "nonfiniteSamples": int(np.count_nonzero(~np.isfinite(result)))}
        if target_formants is not None:
            measured = _vowel_formants(result, sample_rate,
                                       target_profile["vocalTract"]["formantCeilingHz"])
            errors = {key: 100.0 * abs(measured[key] - target_formants[key]) / target_formants[key]
                      for key in target_formants}
            report["variants"][name]["formantMedianHz"] = measured
            report["variants"][name]["formantAbsoluteErrorPercent"] = errors
            report["variants"][name]["meanFormantAbsoluteErrorPercent"] = float(np.mean(list(errors.values())))
    write_json(output_directory / "report.json", report)
    lines = ["# Offline comparison", "", f"Input: `{input_path}`", f"Preset: `{preset['id']}`", "",
             "Listen at matched playback volume and record notes for naturalness, vowel character, consonant clarity, transient quality, and artifacts.", "",
             "| Variant | File | Peak dBFS | RMS dBFS | Median pitch error (cents) | Voicing mismatch |", "| --- | --- | ---: | ---: | ---: | ---: |"]
    for name, metrics in report["variants"].items():
        pitch_text = f"{metrics['medianPitchErrorCents']:.1f}" if metrics["medianPitchErrorCents"] is not None else "n/a"
        voicing_text = f"{metrics['voicingMismatchFraction']:.3f}" if metrics["voicingMismatchFraction"] is not None else "n/a"
        lines.append(f"| {name} | [{name}.wav]({name}.wav) | {metrics['peakDbFS']:.2f} | {metrics['rmsDbFS']:.2f} | {pitch_text} | {voicing_text} |")
    if target_formants is not None:
        lines.extend(["", f"Target vowel `{vowel}` medians: " + ", ".join(f"{key} {value:.0f} Hz" for key, value in target_formants.items()),
                      "", "| Variant | F1 | F2 | F3 | Mean absolute target error |",
                      "| --- | ---: | ---: | ---: | ---: |"])
        for name, metrics in report["variants"].items():
            measured = metrics["formantMedianHz"]
            lines.append(f"| {name} | {measured['F1']:.0f} Hz | {measured['F2']:.0f} Hz | "
                         f"{measured['F3']:.0f} Hz | {metrics['meanFormantAbsoluteErrorPercent']:.1f}% |")
    lines.extend(["", "## Listening notes", "", "- A: pitch-only versus pitch-formant:", "- B: pitch-global-formant versus pitch-formant:", "- C: full-unprotected versus full, focusing on consonants:", "- D: fixed-pitch versus pitch-only on normal speech:", "- E: rubberband-pitch versus full, if Rubber Band was supplied:", "- Vowel oracle versus global warp, if source and target profiles were supplied:", "- Full DSP versus original:", "", "WORLD variants are offline resyntheses. The vowel oracle uses the known input vowel label and cannot run in live speech. The optional Rubber Band CLI uses its realtime algorithm with a pitch map and formant preservation; it cannot independently fit the target formant warp. Its pitch/voicing errors are omitted until output delay is aligned. No file here measures live device latency or demonstrates perceptual success."])
    (output_directory / "listening.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return report
