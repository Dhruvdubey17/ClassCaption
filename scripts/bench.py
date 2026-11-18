# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "jiwer==4.0.0",
#     "numpy==2.5.5",
#     "pyarrow==26.0.1",
#     "soundfile==0.14.0",
#     "whisper-normalizer==1.0.1",
# ]
# ///
"""ClassCaption benchmarks. Every number in the README comes from a file this script writes.

    uv run scripts/bench.py fetch       extract TED-LIUM long-form talks into bench/data
    uv run scripts/bench.py accuracy    word error rate, live and after-class
    uv run scripts/bench.py latency     speech-to-caption delay, played in real time
    uv run scripts/bench.py speed       live captioning seconds per audio minute, per backend
    uv run scripts/bench.py power LOG   energy per audio minute from a powermetrics log
    uv run scripts/bench.py table       render bench/results/tables.md
    uv run scripts/bench.py selftest    check the parsing and math in this file

Energy needs sudo for powermetrics, so run it through scripts/measure_power.sh.
"""

import io
import json
import platform
import plistlib
import re
import shutil
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT / "build-release"
BINARY = BUILD_DIR / "classcaption"
DATA_DIR = ROOT / "bench" / "data"
TALKS_DIR = DATA_DIR / "tedlium"
RUNS_DIR = DATA_DIR / "runs"
RESULTS_DIR = ROOT / "bench" / "results"
PARQUET = DATA_DIR / "tedlium-long-form-test.parquet"
PARQUET_URL = (
    "https://huggingface.co/datasets/distil-whisper/tedlium-long-form/resolve/main/"
    "data/test-00000-of-00001-7a1bb92f62e929b8.parquet"
)

SAMPLE_RATE = 16_000
MIN_TALK_SECONDS = 60
# The three shortest full talks in the test split, about 19 minutes together.
ACCURACY_TALKS = ("RobertGupta", "GaryFlake", "EricMead_2009P_EricMead")
LATENCY_TALK = "RobertGupta"
SPEED_TALK = "AimeeMullins"
SPEED_SECONDS = 600
BACKENDS = ("coreml", "metal", "cpu")
LIVE_MODEL = ROOT / "models" / "ggml-large-v3-turbo-q5_0.bin"
FINAL_MODEL = ROOT / "models" / "ggml-large-v3-q5_0.bin"
IDLE_SECONDS = 30
# A draft and a final word are the same word if their start times are this close.
SAME_WORD_START_MS = 1000
RAILS = ("cpu", "gpu", "ane")


# ---------------------------------------------------------------------------- shared helpers


def run(command: list[str | Path], **kwargs) -> subprocess.CompletedProcess:
    printable = " ".join(str(part) for part in command)
    print(f"$ {printable}", file=sys.stderr)
    return subprocess.run([str(part) for part in command], check=True, **kwargs)


def build_release() -> None:
    run(["cmake", "-S", ROOT, "-B", BUILD_DIR, "-DCMAKE_BUILD_TYPE=Release"],
        stdout=subprocess.DEVNULL)
    run(["cmake", "--build", BUILD_DIR, "--target", "classcaption", "--parallel"],
        stdout=subprocess.DEVNULL)


def command_output(command: list[str]) -> str:
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout.strip()


def environment() -> dict:
    """Recorded with every result, so a number can always be traced to the machine and code."""
    return {
        "chip": command_output(["sysctl", "-n", "machdep.cpu.brand_string"]),
        "memory_gb": int(command_output(["sysctl", "-n", "hw.memsize"])) // 1024**3,
        "macos": platform.mac_ver()[0],
        "classcaption_commit": command_output(["git", "-C", str(ROOT), "rev-parse", "--short", "HEAD"]),
        "uncommitted_changes": bool(command_output(["git", "-C", str(ROOT), "status", "--porcelain",
                                                    "--untracked-files=no"])),
        "whisper_cpp": command_output(["git", "-C", str(ROOT / "third_party" / "whisper.cpp"),
                                       "describe", "--tags"]),
        "measured_at": datetime.now().astimezone().isoformat(timespec="seconds"),
    }


def write_result(name: str, payload: dict) -> Path:
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    path = RESULTS_DIR / f"{name}.json"
    path.write_text(json.dumps({"environment": environment(), **payload}, indent=2) + "\n")
    print(f"wrote {path.relative_to(ROOT)}", file=sys.stderr)
    return path


def talk_wav(talk: str) -> Path:
    path = TALKS_DIR / f"{talk}.wav"
    if not path.exists():
        sys.exit(f"{path} is missing. Run: uv run scripts/bench.py fetch")
    return path


def audio_minutes(path: Path) -> float:
    import soundfile

    return soundfile.info(str(path)).frames / SAMPLE_RATE / 60


def fresh_dir(path: Path) -> Path:
    shutil.rmtree(path, ignore_errors=True)
    path.mkdir(parents=True)
    return path


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    rank = fraction * (len(ordered) - 1)
    low = int(rank)
    high = min(low + 1, len(ordered) - 1)
    return ordered[low] + (ordered[high] - ordered[low]) * (rank - low)


# --------------------------------------------------------------------------------- fetch


def fetch() -> None:
    import numpy
    import pyarrow.parquet
    import soundfile

    if not PARQUET.exists():
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        run(["curl", "-fL", "--retry", "3", "-o", PARQUET, PARQUET_URL])
    TALKS_DIR.mkdir(parents=True, exist_ok=True)
    table = pyarrow.parquet.read_table(PARQUET)
    for row in table.to_pylist():
        audio, rate = soundfile.read(io.BytesIO(row["audio"]["bytes"]), dtype="float32")
        if rate != SAMPLE_RATE or audio.ndim != 1:
            sys.exit(f"{row['speaker_id']}: expected 16 kHz mono, got {rate} Hz, shape {audio.shape}")
        if len(audio) < MIN_TALK_SECONDS * SAMPLE_RATE:
            continue
        talk = row["speaker_id"]
        soundfile.write(str(TALKS_DIR / f"{talk}.wav"), numpy.asarray(audio), SAMPLE_RATE, "PCM_16")
        (TALKS_DIR / f"{talk}.txt").write_text(row["text"].strip() + "\n")
        print(f"{talk}: {len(audio) / SAMPLE_RATE / 60:.1f} min", file=sys.stderr)


# ------------------------------------------------------------------------------ accuracy


def vtt_text(vtt: str) -> str:
    """The caption text of a WebVTT file, cues joined by spaces."""
    lines = []
    for block in vtt.strip().split("\n\n")[1:]:
        cue = block.split("\n")
        timing = next(i for i, line in enumerate(cue) if "-->" in line)
        lines.extend(cue[timing + 1:])
    text = " ".join(lines)
    return text.replace("&lt;", "<").replace("&gt;", ">").replace("&amp;", "&")


def normalized(text: str) -> str:
    from whisper_normalizer.english import EnglishTextNormalizer

    return EnglishTextNormalizer()(text)


def word_errors(reference: str, hypothesis: str) -> tuple[int, int]:
    """Errors and reference words after Whisper's English normalizer, the usual WER setup."""
    import jiwer

    output = jiwer.process_words(normalized(reference), normalized(hypothesis))
    errors = output.substitutions + output.deletions + output.insertions
    return errors, output.substitutions + output.deletions + output.hits


def caption_live(wav: Path, out_dir: Path, *extra: str) -> Path:
    run([BINARY, "--input", wav, "--output_dir", out_dir, "--show_window", "false",
         "--save_audio", "false", "--live_model", LIVE_MODEL, *extra],
        stdout=subprocess.DEVNULL)
    return next(out_dir.glob("lecture-*.vtt"))


def finalize(wav: Path, out_dir: Path, model: Path, backend: str) -> Path:
    recording = out_dir / "recording.wav"
    shutil.copyfile(wav, recording)
    run([BINARY, "finalize", recording, "--final_model", model, "--final_backend", backend])
    return out_dir / "recording.final.vtt"


def accuracy() -> None:
    build_release()
    talks = []
    for talk in ACCURACY_TALKS:
        wav = talk_wav(talk)
        reference = (TALKS_DIR / f"{talk}.txt").read_text()
        out_dir = fresh_dir(RUNS_DIR / "accuracy" / talk)
        live = caption_live(wav, out_dir, "--realtime_input", "false", "--backend", "coreml")
        final = finalize(wav, out_dir, FINAL_MODEL, "metal")
        live_errors, words = word_errors(reference, vtt_text(live.read_text()))
        final_errors, _ = word_errors(reference, vtt_text(final.read_text()))
        talks.append({"talk": talk, "audio_minutes": round(audio_minutes(wav), 2),
                      "reference_words": words, "live_errors": live_errors,
                      "final_errors": final_errors})

    total_words = sum(t["reference_words"] for t in talks)
    write_result("accuracy", {
        "dataset": "TED-LIUM release 3 test talks (distil-whisper/tedlium-long-form)",
        "normalizer": "whisper-normalizer EnglishTextNormalizer, on reference and captions",
        "live": {"model": LIVE_MODEL.name, "backend": "coreml", "step_ms": 1000},
        "final": {"model": FINAL_MODEL.name, "backend": "metal"},
        "talks": talks,
        "live_wer": round(sum(t["live_errors"] for t in talks) / total_words, 4),
        "final_wer": round(sum(t["final_errors"] for t in talks) / total_words, 4),
    })


# ------------------------------------------------------------------------------- latency


@dataclass(frozen=True)
class Event:
    wall_ms: int
    kind: str
    start_ms: int
    end_ms: int
    text: str


def read_events(tsv: str) -> list[Event]:
    events = []
    for line in tsv.splitlines()[1:]:
        wall, kind, start, end, text = line.split("\t")
        if kind != "pause":
            events.append(Event(int(wall), kind, int(start), int(end), text))
    return events


def comparable(text: str) -> str:
    return re.sub(r"[^a-z0-9]", "", text.lower())


def caption_delays(events: list[Event]) -> list[tuple[int, int]]:
    """For each final word: (ms from the end of the word in the audio until it was first on
    screen, as a draft or final; ms until it was final)."""
    drafts = [e for e in events if e.kind == "draft"]
    delays = []
    for word in (e for e in events if e.kind == "final"):
        first_shown = min(
            [d.wall_ms for d in drafts
             if d.wall_ms <= word.wall_ms and comparable(d.text) == comparable(word.text)
             and abs(d.start_ms - word.start_ms) <= SAME_WORD_START_MS],
            default=word.wall_ms,
        )
        delays.append((first_shown - word.end_ms, word.wall_ms - word.end_ms))
    return delays


def latency() -> None:
    build_release()
    wav = talk_wav(LATENCY_TALK)
    out_dir = fresh_dir(RUNS_DIR / "latency")
    events_path = out_dir / "events.tsv"
    caption_live(wav, out_dir, "--backend", "coreml", "--event_log", events_path)
    delays = caption_delays(read_events(events_path.read_text()))
    shown = [d[0] for d in delays]
    final = [d[1] for d in delays]
    write_result("latency", {
        "talk": LATENCY_TALK,
        "audio_minutes": round(audio_minutes(wav), 2),
        "method": "recording played at real-time speed through the full pipeline; delay is the "
                  "time from the end of each word in the audio to when it reached the screen",
        "live": {"model": LIVE_MODEL.name, "backend": "coreml", "step_ms": 1000},
        "words": len(delays),
        "first_shown_ms": {"median": statistics.median(shown), "p95": percentile(shown, 0.95)},
        "final_ms": {"median": statistics.median(final), "p95": percentile(final, 0.95)},
    })


# --------------------------------------------------------------------------------- speed


def speed_clip() -> Path:
    import soundfile

    clip = DATA_DIR / f"{SPEED_TALK}-first-{SPEED_SECONDS}s.wav"
    if not clip.exists():
        audio, _ = soundfile.read(str(talk_wav(SPEED_TALK)), dtype="int16",
                                  frames=SPEED_SECONDS * SAMPLE_RATE)
        soundfile.write(str(clip), audio, SAMPLE_RATE, "PCM_16")
    return clip


def speed() -> None:
    build_release()
    clip = speed_clip()
    minutes = audio_minutes(clip)
    print(f"idle for {IDLE_SECONDS} s (baseline for energy)", file=sys.stderr)
    idle_start = time.time()
    time.sleep(IDLE_SECONDS)
    phases = {"idle": [idle_start, time.time()]}
    runs = []
    for backend in BACKENDS:
        out_dir = fresh_dir(RUNS_DIR / "speed" / backend)
        start = time.time()
        caption_live(clip, out_dir, "--realtime_input", "false", "--backend", backend)
        end = time.time()
        phases[backend] = [start, end]
        runs.append({"backend": backend, "seconds": round(end - start, 2),
                     "seconds_per_audio_minute": round((end - start) / minutes, 2)})
    write_result("speed", {
        "clip": f"first {SPEED_SECONDS} s of {SPEED_TALK}",
        "audio_minutes": round(minutes, 2),
        "model": LIVE_MODEL.name,
        "method": "live captioning (a pass every 1 s of audio) fed as fast as it keeps up, "
                  "including model load; below 60 s per audio minute keeps up with a lecture",
        "runs": runs,
        "phases": phases,
    })


# --------------------------------------------------------------------------------- power


@dataclass(frozen=True)
class PowerSample:
    end: float
    seconds: float
    milliwatts: dict[str, float]


def power_samples(log: bytes) -> list[PowerSample]:
    """powermetrics -f plist writes one plist per sample, separated by NUL bytes."""
    documents = [plistlib.loads(chunk.strip(), aware_datetime=True)
                 for chunk in log.split(b"\x00") if chunk.strip()]
    samples = []
    end = documents[0]["timestamp"].timestamp() if documents else 0.0
    for index, document in enumerate(documents):
        seconds = document["elapsed_ns"] / 1e9
        if index:
            end += seconds
        processor = document["processor"]
        samples.append(PowerSample(end, seconds, {rail: float(processor[f"{rail}_power"])
                                                  for rail in RAILS}))
    return samples


def mean_milliwatts(samples: list[PowerSample], phase: list[float]) -> dict[str, float]:
    inside = [s for s in samples if s.end - s.seconds >= phase[0] and s.end <= phase[1]]
    if not inside:
        sys.exit(f"no power samples between {phase[0]:.0f} and {phase[1]:.0f}")
    seconds = sum(s.seconds for s in inside)
    return {rail: sum(s.milliwatts[rail] * s.seconds for s in inside) / seconds for rail in RAILS}


def energy_above_idle(samples: list[PowerSample], phases: dict, run_seconds: float,
                      backend: str, minutes: float) -> dict[str, float]:
    idle = mean_milliwatts(samples, phases["idle"])
    busy = mean_milliwatts(samples, phases[backend])
    joules = {rail: (busy[rail] - idle[rail]) * run_seconds / 1000 / minutes for rail in RAILS}
    return {**{rail: round(j, 1) for rail, j in joules.items()},
            "total": round(sum(joules.values()), 1)}


def power(log_path: Path) -> None:
    speed_result = json.loads((RESULTS_DIR / "speed.json").read_text())
    samples = power_samples(log_path.read_bytes())
    runs = []
    for run_record in speed_result["runs"]:
        backend = run_record["backend"]
        runs.append({"backend": backend,
                     "joules_per_audio_minute_above_idle": energy_above_idle(
                         samples, speed_result["phases"], run_record["seconds"], backend,
                         speed_result["audio_minutes"])})
    write_result("energy", {
        "source": "powermetrics cpu_power, gpu_power, ane_power at 500 ms, during scripts/bench.py speed",
        "speed_measured_at": speed_result["environment"]["measured_at"],
        "runs": runs,
    })


# --------------------------------------------------------------------------------- table


def table() -> None:
    lines = []
    accuracy_path = RESULTS_DIR / "accuracy.json"
    if accuracy_path.exists():
        result = json.loads(accuracy_path.read_text())
        minutes = sum(t["audio_minutes"] for t in result["talks"])
        lines += ["| Pass | Model | Backend | WER |", "|---|---|---|---|",
                  f"| Live | {result['live']['model']} | {result['live']['backend']} | "
                  f"{result['live_wer']:.1%} |",
                  f"| After class | {result['final']['model']} | {result['final']['backend']} | "
                  f"{result['final_wer']:.1%} |", "",
                  f"{len(result['talks'])} TED talks, {minutes:.1f} minutes. "
                  f"Source: `bench/results/accuracy.json`.", ""]
    latency_path = RESULTS_DIR / "latency.json"
    if latency_path.exists():
        result = json.loads(latency_path.read_text())
        lines += ["| Speech to caption | Median | p95 |", "|---|---|---|",
                  f"| First shown (draft) | {result['first_shown_ms']['median'] / 1000:.2f} s | "
                  f"{result['first_shown_ms']['p95'] / 1000:.2f} s |",
                  f"| Final | {result['final_ms']['median'] / 1000:.2f} s | "
                  f"{result['final_ms']['p95'] / 1000:.2f} s |", "",
                  f"{result['words']} words, {result['audio_minutes']:.1f} minutes played in real "
                  f"time. Source: `bench/results/latency.json`.", ""]
    speed_path = RESULTS_DIR / "speed.json"
    energy_path = RESULTS_DIR / "energy.json"
    if speed_path.exists():
        result = json.loads(speed_path.read_text())
        energy = {}
        if energy_path.exists():
            energy = {r["backend"]: r["joules_per_audio_minute_above_idle"]["total"]
                      for r in json.loads(energy_path.read_text())["runs"]}
        lines += ["| Backend | Seconds per audio minute | Joules per audio minute, above idle |",
                  "|---|---|---|"]
        for record in result["runs"]:
            joules = energy.get(record["backend"])
            lines.append(f"| {record['backend']} | {record['seconds_per_audio_minute']:.2f} | "
                         f"{'not measured' if joules is None else f'{joules:.1f}'} |")
        lines += ["", f"{result['model']}, {result['clip']}. Sources: `bench/results/speed.json`"
                  + (", `bench/results/energy.json`." if energy else "."), ""]
    out = RESULTS_DIR / "tables.md"
    out.write_text("\n".join(lines))
    print(out.read_text())


# ------------------------------------------------------------------------------ selftest


def selftest() -> None:
    vtt = "WEBVTT\n\n1\n00:00:00.000 --> 00:00:01.000\nIf a &lt; b,\n\n2\n00:00:01.000 --> 00:00:02.000\nthen stop.\n"
    assert vtt_text(vtt) == "If a < b, then stop.", vtt_text(vtt)
    assert word_errors("ask not what your country", "Ask not, what our country!") == (1, 5)

    events = read_events("wall_ms\tkind\tstart_ms\tend_ms\ttext\n"
                         "900\tdraft\t0\t400\task\n"
                         "1900\tdraft\t20\t410\tAsk\n"
                         "1900\tfinal\t20\t410\tAsk\n"
                         "1900\tpause\t\t\t\n"
                         "2500\tfinal\t3000\t3300\tnot\n")
    assert caption_delays(events) == [(490, 1490), (-800, -800)], caption_delays(events)

    assert percentile([1, 2, 3, 4, 5], 0.5) == 3
    assert percentile([10, 20], 0.95) == 19.5

    samples = [PowerSample(end=t, seconds=1.0, milliwatts={"cpu": mw, "gpu": 0.0, "ane": 0.0})
               for t, mw in [(1, 100), (2, 100), (11, 1100), (12, 1100)]]
    phases = {"idle": [0, 2], "coreml": [10, 12]}
    # 1000 mW above idle for 60 s of work on 2 audio minutes = 30 J per audio minute.
    assert energy_above_idle(samples, phases, 60, "coreml", 2)["total"] == 30.0
    print("selftest passed")


def main() -> None:
    commands = {"fetch": fetch, "accuracy": accuracy, "latency": latency, "speed": speed,
                "table": table, "selftest": selftest}
    if len(sys.argv) == 3 and sys.argv[1] == "power":
        power(Path(sys.argv[2]))
    elif len(sys.argv) == 2 and sys.argv[1] in commands:
        commands[sys.argv[1]]()
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
