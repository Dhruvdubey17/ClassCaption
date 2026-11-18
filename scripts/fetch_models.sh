#!/usr/bin/env bash
# Downloads models into models/. With no argument it fetches only what the tests need.
# `fetch_models.sh all` adds the live and after-class models (about 3 GB).
set -euo pipefail

cd "$(dirname "$0")/.."
mkdir -p models

whisper_src="https://huggingface.co/ggerganov/whisper.cpp/resolve/main"
vad_src="https://huggingface.co/ggml-org/whisper-vad/resolve/main"

fetch() {
    local url="$1" file="models/$2"
    if [[ -e "$file" ]]; then
        echo "have $file"
        return
    fi
    echo "fetching $file"
    curl -fL --retry 3 -o "$file.part" "$url"
    mv "$file.part" "$file"
}

fetch_coreml_encoder() {
    local name="$1"
    [[ -d "models/ggml-$name-encoder.mlmodelc" ]] && return
    fetch "$whisper_src/ggml-$name-encoder.mlmodelc.zip" "ggml-$name-encoder.mlmodelc.zip"
    unzip -q "models/ggml-$name-encoder.mlmodelc.zip" -d models
    rm "models/ggml-$name-encoder.mlmodelc.zip"
}

fetch "$whisper_src/ggml-tiny.en.bin" ggml-tiny.en.bin
fetch "$vad_src/ggml-silero-v6.2.0.bin" ggml-silero-v6.2.0.bin

if [[ "${1:-}" == "all" ]]; then
    fetch "$whisper_src/ggml-large-v3-turbo-q5_0.bin" ggml-large-v3-turbo-q5_0.bin
    fetch "$whisper_src/ggml-large-v3-q5_0.bin" ggml-large-v3-q5_0.bin
    # whisper.cpp drops the -q5_0 suffix when it looks for the encoder, so one encoder serves both sizes.
    fetch_coreml_encoder large-v3-turbo
fi
