from __future__ import annotations

import argparse
from pathlib import Path

from .analysis import build_profile
from .compile import compile_preset
from .fit import fit_preset
from .io import read_json, write_json
from .native_render import render_native
from .render import render_comparison


def main() -> None:
    parser = argparse.ArgumentParser(prog="voiceprof", description="Offline acoustic research harness")
    commands = parser.add_subparsers(dest="command", required=True)
    profile = commands.add_parser("profile", help="analyze a labeled calibration manifest")
    profile.add_argument("manifest", type=Path)
    profile.add_argument("output", type=Path)
    fit = commands.add_parser("fit", help="fit source and target acoustic profiles")
    fit.add_argument("source", type=Path)
    fit.add_argument("target", type=Path)
    fit.add_argument("output", type=Path)
    fit.add_argument("--id", required=True)
    fit.add_argument("--pitch-only-pilot", action="store_true", help="use unlabeled speech for a pitch-only research preset")
    compiled = commands.add_parser("compile", help="compile a validated preset for the native baseline")
    compiled.add_argument("preset", type=Path)
    compiled.add_argument("output", type=Path)
    render = commands.add_parser("render", help="render WORLD A/B comparison files")
    render.add_argument("input", type=Path)
    render.add_argument("preset", type=Path)
    render.add_argument("output_directory", type=Path)
    render.add_argument("--rubberband", type=Path, help="optional path to the Rubber Band command-line utility")
    render.add_argument("--target-profile", type=Path, help="target profile for labeled-vowel formant accuracy")
    render.add_argument("--source-profile", type=Path, help="source profile for optional known-vowel oracle")
    render.add_argument("--vowel", choices=("a", "i", "u", "e", "o"), help="vowel label of the entire input recording")
    native = commands.add_parser("native-render", help="render a WAV through the native realtime baseline")
    native.add_argument("input", type=Path)
    native.add_argument("preset", type=Path, help="compiled .vmp preset")
    native.add_argument("output", type=Path)
    native.add_argument("--executable", type=Path, required=True, help="path to morph_raw")
    native.add_argument("--window", choices=("short", "medium"), default="short")
    args = parser.parse_args()
    if args.command == "profile":
        write_json(args.output, build_profile(args.manifest))
    elif args.command == "fit":
        write_json(args.output, fit_preset(read_json(args.source), read_json(args.target), args.id,
                                               pitch_only_pilot=args.pitch_only_pilot))
    elif args.command == "compile":
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(compile_preset(read_json(args.preset)))
    elif args.command == "render":
        target_profile = read_json(args.target_profile) if args.target_profile else None
        source_profile = read_json(args.source_profile) if args.source_profile else None
        render_comparison(args.input, args.output_directory, read_json(args.preset),
                          args.rubberband, target_profile, args.vowel, source_profile)
    elif args.command == "native-render":
        print(render_native(args.input, args.preset, args.output, args.executable, args.window))


if __name__ == "__main__":
    main()
