#!/usr/bin/env bash
# Captions a recording in the window at real-time speed, saving frames, and turns them into the
# README screenshot (docs/window.png) and demo (docs/demo.gif). Needs ffmpeg and a release build.
# Usage: scripts/make_demo.sh [recording.wav]
set -euo pipefail

cd "$(dirname "$0")/.."
input="${1:-third_party/whisper.cpp/samples/jfk.wav}"
frames="bench/data/frames"
rm -rf "$frames" bench/data/demo-session

build-release/classcaption --input "$input" --frames_dir "$frames" --save_audio false \
    --output_dir bench/data/demo-session

mkdir -p docs
last="$(ls "$frames"/frame-*.ppm | tail -1)"
ffmpeg -loglevel error -y -i "$last" -vf "scale=1000:-1:flags=lanczos" docs/window.png
# Frames are 200 ms apart, so 5 per second plays back at real speed.
ffmpeg -loglevel error -y -framerate 5 -i "$frames/frame-%05d.ppm" \
    -vf "scale=800:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=32[p];[b][p]paletteuse" \
    docs/demo.gif
ls -la docs/window.png docs/demo.gif
