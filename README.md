# ClassCaption

[![CI](https://github.com/Dhruvdubey17/ClassCaption/actions/workflows/ci.yml/badge.svg)](https://github.com/Dhruvdubey17/ClassCaption/actions/workflows/ci.yml)

Live lecture captions that run entirely on a Mac, with Wi-Fi off, for students who are hard of hearing or still building their English.

![Captions appearing live: grey words are drafts that may still change, white words are final](docs/demo.gif)

The demo plays the 11-second clip that ships with whisper.cpp through the live pipeline at real-time speed, on the Neural Engine. Each line shows in grey first and turns white once two passes agree on it.

## How to run

Needs an Apple Silicon Mac, Homebrew, and about 3 GB for models.

```
brew install cmake llvm
git clone --recurse-submodules https://github.com/Dhruvdubey17/ClassCaption.git
cd ClassCaption
scripts/fetch_models.sh all
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
build-release/classcaption
```

That opens the caption window and listens to the microphone. macOS asks for microphone access the first time. Close the window or press Ctrl+C to stop.

Each session writes two files to `lectures/`: `lecture-<date>-<time>.vtt` with the live captions and `.wav` with the recording. After class, a slower and more accurate pass makes a clean transcript next to the recording:

```
build-release/classcaption finalize lectures/lecture-2026-11-17-0905.wav
```

The `.vtt` files open in any video player or text editor.

### Settings

Copy `config.example.ini` to `config.ini` and edit it, or pass any setting as a flag, for example `--backend metal` or `--save_audio false`. Flags win over the file. The main ones:

| Setting | Default | What it does |
|---|---|---|
| `backend` | `coreml` | `coreml` runs Whisper's encoder on the Neural Engine, `metal` runs everything on the GPU, `cpu` on the CPU only |
| `live_model` | large-v3-turbo q5_0 | Model for live captions |
| `final_model` | large-v3 q5_0 | Model for the after-class pass |
| `save_audio` | `true` | Set to `false` where recording is not allowed. Captions are still saved |
| `show_window` | `true` | Set to `false` for captions in the terminal only |
| `input` | microphone | A 16 kHz mono WAV file to caption instead |

In the window, `A-` and `A+` (or Cmd - and Cmd +) change the text size, and there is a dark or light high-contrast theme and an always-on-top switch. Words that may still change are grey. Scrolling up to reread stops the auto-scroll until you scroll back to the bottom.

## Results

Measured on an Apple M3 with 8 GB of memory. Every number comes from `scripts/bench.py` and is traceable to a file in `bench/results/`, which also records the macOS version and commit.

<!-- RESULTS -->

## How it works

```
microphone ──> ring buffer ──> Silero VAD ──> utterance ──> Whisper ──> stabilizer ──> window
 (audio thread)  (lock-free)     (32 ms frames)  (cut at       (every       (two passes    .vtt file
                                                  pauses)       second)      must agree)    terminal
```

1. The microphone callback copies audio into a lock-free single-producer ring buffer. It never locks or allocates, so it cannot stall.
2. The inference thread scores each 32 ms frame with Silero VAD. A speech gate with hysteresis decides when someone is talking; a pause of about half a second ends an utterance. Silence never reaches Whisper.
3. About once a second, Whisper re-transcribes the whole utterance so far. Utterances are cut at 25 s because Whisper sees at most 30 s at once.
4. The stabilizer compares each pass with the previous one. Words both passes agree on become final and never change; the rest is a grey draft. This is LocalAgreement-2 from the whisper_streaming work.
5. Final words go to the window, to the `.vtt` file (flushed after every cue), and to the terminal.

Source layout: `include/classcaption/` has one header per stage, `src/` the implementations, `src/window/` the caption window, `tests/` mirrors `src/`.

## Design decisions

### Whisper through whisper.cpp, not NVIDIA Parakeet

Parakeet streams natively and is fast on the Neural Engine, but its working Neural Engine ports are Swift libraries. whisper.cpp is C++, has a proven Core ML path, and Whisper large-v3-turbo is MIT licensed.

### Two models

Live captions use large-v3-turbo (q5_0, 574 MB) for speed. The after-class pass uses large-v3 (q5_0, 1.1 GB), which is slower and more accurate, on the saved recording.

### Core ML only for the encoder

whisper.cpp runs the encoder through Core ML on the Neural Engine and the decoder on Metal. whisper.cpp v1.9.5 has no runtime switch to turn Core ML off, so the `metal` and `cpu` backends load the model through a stream, which leaves whisper.cpp without a file path to find the encoder. The `coreml` backend refuses to start if the encoder is missing, instead of falling back to the CPU without saying so.

### Re-transcribe and stabilize, instead of captioning fixed chunks

Fixed 2 to 3 second chunks cut words at the chunk edges and give Whisper little context. Re-transcribing the utterance gives it context; the stabilizer keeps final text from flickering. Whisper's word timings drift a few hundred ms between passes, so final words are tracked by time and by text.

### Rejected: Qt and native Cocoa for the window

Qt is large for one window and adds a licensing question. Native Cocoa would mean Objective-C in a C++ project. Dear ImGui with GLFW is small, C++, and runs on Windows too. It redraws at most 30 times a second to save battery.

### The after-class pass reuses the live path

The first version gave Whisper the whole recording at once. On two of the three test talks it got stuck repeating one sentence, 97 and 88 times, and WER on those talks went to 43% and 119%. Skipping silence moved the loop from one talk to the other instead of removing it. The after-class pass now runs the same utterance-by-utterance pipeline as live captions, with the larger model and one pass per utterance, and the repeats are gone.

### Crash safety

The `.vtt` is flushed after every cue and the `.wav` header is rewritten after every append, so a crash or a closed laptop keeps everything up to that moment. The after-class pass never overwrites a `.final.vtt`, which may have been corrected by hand.

### One config struct

Every tunable lives in `Config`, read from `config.ini` with flags on top. Changing model or backend is a config edit, never a code change. whisper.cpp is used in only two files, `src/whisper_transcriber.cpp` and `src/whisper_vad.cpp`.

## Limitations

- macOS on Apple Silicon only. There is no Windows or Linux build yet.
- English only in practice. `language` can be changed, but nothing else has been tested.
- The caption window draws its own text, so screen readers cannot read it. The `.vtt` file can be read with any screen reader.
- No speaker labels and no translation.
- If a speaker repeats a word right where a caption is finalized ("the the"), the repeat is dropped. That check is what stops timing drift from duplicating words.
- The first launch on a Mac is slow while macOS compiles the Core ML encoder for the Neural Engine. Later launches reuse it.
- Only the live model has a Core ML encoder downloaded, so the after-class pass runs on the GPU.
- With `save_audio = false` there is no recording, so there is no after-class pass.
- There is no signed, ready-to-run download yet. It has to be built from source.
- Accuracy is measured on TED talks, which are cleaner than a classroom recording.
- The caption window is checked by hand. Everything behind it is covered by unit tests.

## Development

`scripts/check.sh` builds, runs the tests, and runs clang-format and clang-tidy; CI runs the same script on the CPU path, then the tests again under ThreadSanitizer. Tests that need a microphone or the full-size models are skipped by default:

```
build/classcaption_tests --no-skip -tc="the microphone*"
build/classcaption_tests --no-skip -tc="the live model*"
```

Benchmarks: `uv run scripts/bench.py fetch`, then `accuracy`, `latency`, and `scripts/measure_power.sh` for speed and energy (needs sudo).

## Data and credits

Accuracy and latency use talks from the TED-LIUM release 3 test set (CC BY-NC-ND 3.0), through the `distil-whisper/tedlium-long-form` copy on Hugging Face. The audio is downloaded locally and never committed. The test clip `jfk.wav` ships with whisper.cpp.

Built on [whisper.cpp](https://github.com/ggml-org/whisper.cpp), [miniaudio](https://github.com/mackron/miniaudio), [Dear ImGui](https://github.com/ocornut/imgui), [GLFW](https://github.com/glfw/glfw) and [doctest](https://github.com/doctest/doctest).
