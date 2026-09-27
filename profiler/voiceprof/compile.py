from __future__ import annotations

import math
import struct
import zlib

import numpy as np

MAGIC = b"MBVMP1\0\0"
VERSION = 1
PAYLOAD_FORMAT = "<22d"
HEADER_FORMAT = "<8sII32s32s"


def compile_preset(preset: dict) -> bytes:
    if preset.get("schemaVersion") != "1.0" or preset["pitch"]["mode"] != "quantile":
        raise ValueError("compiled preset requires schema 1.0 quantile pitch mode")
    source = preset["pitch"]["sourceKnotsLog2Hz"]
    target = preset["pitch"]["targetKnotsLog2Hz"]
    if len(source) != 7 or len(target) != 7 or np.any(np.diff(source) <= 0) or np.any(np.diff(target) <= 0):
        raise ValueError("compiled pitch knots must be strictly increasing")
    warp = preset["vocalTract"]["warp"]
    if preset["sampleRate"] != 48000:
        raise ValueError("native baseline requires 48 kHz profiles")
    smoothing = preset["pitch"]["smoothing"]
    if (smoothing["confidenceThreshold"] != 0.65 or smoothing["attackMs"] != 30 or
            smoothing["releaseMs"] != 80):
        raise ValueError("native baseline supports only its current pitch smoothing settings")
    if len(warp) < 2 or np.any(np.diff([point["inputHz"] for point in warp]) <= 0) or np.any(np.diff([point["outputHz"] for point in warp]) <= 0):
        raise ValueError("compiled formant warp must be strictly monotonic")
    ratios = [point["outputHz"] / point["inputHz"] for point in warp if 500 <= point["inputHz"] <= 4000]
    if not ratios:
        raise ValueError("formant warp has no middle-frequency control points")
    formant_ratio = float(np.median(ratios))
    if not 0.5 <= formant_ratio <= 1.5:
        raise ValueError("fitted formant ratio outside native baseline safety limits")
    values = [*source, *target,
              preset["pitch"]["strength"],
              preset["pitch"]["limits"]["minOutputHz"],
              preset["pitch"]["limits"]["maxOutputHz"],
              preset["pitch"]["limits"]["maxShiftSemitones"],
              preset["pitch"]["extrapolation"]["minSlope"],
              preset["pitch"]["extrapolation"]["maxSlope"],
              formant_ratio, preset["vocalTract"]["strength"]]
    if len(values) != 22 or not all(math.isfinite(value) for value in values):
        raise ValueError("compiled preset contains nonfinite values")
    if not (0 <= values[14] <= 1 and 0 < values[15] < values[16] and
            values[17] > 0 and 0 < values[18] <= values[19] and 0 <= values[21] <= 1):
        raise ValueError("compiled preset pitch/formant limits invalid")
    payload = struct.pack(PAYLOAD_FORMAT, *values)
    source_digest = bytes.fromhex(preset["sourceProfileSha256"])
    target_digest = bytes.fromhex(preset["targetProfileSha256"])
    if len(source_digest) != 32 or len(target_digest) != 32:
        raise ValueError("profile SHA-256 digests must be 32 bytes")
    header = struct.pack(HEADER_FORMAT, MAGIC, VERSION, len(payload), source_digest, target_digest)
    content = header + payload
    return content + struct.pack("<I", zlib.crc32(content))
