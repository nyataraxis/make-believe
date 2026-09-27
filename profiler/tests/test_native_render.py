import os
from pathlib import Path

import numpy as np
import pytest
from scipy.io import wavfile
from scipy.signal import periodogram

from voiceprof.native_render import render_native


@pytest.mark.skipif("MORPH_RAW_EXECUTABLE" not in os.environ, reason="native adapter not built")
def test_native_render_tracks_preset_pitch_and_output_bounds(tmp_path: Path) -> None:
    sample_rate = 16000
    time = np.arange(2 * sample_rate) / sample_rate
    source = (0.2 * np.sin(2 * np.pi * 130.0 * time)).astype(np.float32)
    input_wav = tmp_path / "input.wav"
    output_wav = tmp_path / "output.wav"
    wavfile.write(input_wav, sample_rate, source)
    preset = Path(__file__).resolve().parents[2] / "fixtures" / "synthetic" / "preset.vmp"
    metrics = render_native(input_wav, preset, output_wav, Path(os.environ["MORPH_RAW_EXECUTABLE"]))
    rendered_rate, rendered = wavfile.read(output_wav)
    assert rendered_rate == 48000
    assert len(rendered) == 2 * rendered_rate
    assert np.all(np.isfinite(rendered))
    assert np.max(np.abs(rendered)) <= 0.89125
    assert metrics["nonfinite_output_samples"] == 0
    assert metrics["start_delay_samples"] > 0
    frequencies, spectrum = periodogram(rendered[rendered_rate :], rendered_rate)
    fundamental = frequencies[np.argmax(spectrum)]
    assert 190 < fundamental < 240
    medium_wav = tmp_path / "medium.wav"
    medium = render_native(input_wav, preset, medium_wav,
                           Path(os.environ["MORPH_RAW_EXECUTABLE"]), "medium")
    medium_rate, medium_audio = wavfile.read(medium_wav)
    assert medium_rate == rendered_rate and len(medium_audio) == len(rendered)
    assert medium["start_delay_samples"] > metrics["start_delay_samples"]
    assert np.all(np.isfinite(medium_audio))
