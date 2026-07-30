#!/usr/bin/env python3
"""Generate MIDI + WAV fixtures for PitchDetector accuracy tests.

Creates:
  tests/source4test/pitch/
    scale_c0_c4.mid / .wav          — ascending C0..C4 (MIDI 12..60)
    melody_c_major.mid / .wav       — short C-major melody around C3–C4
    vibrato_a3.mid / .wav           — sustained A3 with ±50 cent vibrato
    detuned_e3.mid / .wav           — E3 held +35 cents off grid

Audio is synthesized as band-limited sine (plus soft harmonics) so f0 is known.
"""

from __future__ import annotations

import math
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "tests" / "source4test" / "pitch"
SAMPLE_RATE = 44100
BPM = 120
TICKS_PER_BEAT = 480


def midi_to_hz(midi: float) -> float:
    return 440.0 * (2.0 ** ((midi - 69.0) / 12.0))


def write_midi(path: Path, notes: list[tuple[float, float, int, int]]) -> None:
    """notes: (start_beat, duration_beats, midi_note, velocity)."""
    events: list[tuple[int, bytes]] = []
    for start_beat, dur_beats, pitch, vel in notes:
        t0 = int(round(start_beat * TICKS_PER_BEAT))
        t1 = int(round((start_beat + dur_beats) * TICKS_PER_BEAT))
        events.append((t0, bytes([0x90, pitch & 0x7F, vel & 0x7F])))
        events.append((t1, bytes([0x80, pitch & 0x7F, 0x00])))
    events.sort(key=lambda e: e[0])

    track = bytearray()
    track += b"\x00\xff\x51\x03" + struct.pack(">I", int(60_000_000 / BPM))[1:]
    last = 0
    for tick, payload in events:
        delta = tick - last
        last = tick
        track += _vlq(delta)
        track += payload
    track += b"\x00\xff\x2f\x00"

    header = struct.pack(">4sIHHH", b"MThd", 6, 0, 1, TICKS_PER_BEAT)
    chunk = struct.pack(">4sI", b"MTrk", len(track)) + track
    path.write_bytes(header + chunk)


def _vlq(value: int) -> bytes:
    buf = [value & 0x7F]
    value >>= 7
    while value:
        buf.append(0x80 | (value & 0x7F))
        value >>= 7
    return bytes(reversed(buf))


def synth_note(
    samples: list[float],
    start: int,
    length: int,
    midi: float,
    *,
    vibrato_cents: float = 0.0,
    vibrato_hz: float = 5.5,
    detune_cents: float = 0.0,
) -> None:
    base_midi = midi + detune_cents / 100.0
    fade = min(SAMPLE_RATE // 100, length // 8)
    for i in range(length):
        t = i / SAMPLE_RATE
        cents = vibrato_cents * math.sin(2.0 * math.pi * vibrato_hz * t) if vibrato_cents else 0.0
        hz = midi_to_hz(base_midi + cents / 100.0)
        phase = 2.0 * math.pi * hz * t
        # Soft saw-ish partials keep autocorrelation stable without changing f0.
        # Keep them quieter for vibrato fixtures to avoid octave jumps.
        h2 = 0.10 if vibrato_cents == 0.0 else 0.0
        h3 = 0.05 if vibrato_cents == 0.0 else 0.0
        sample = (
            0.90 * math.sin(phase)
            + h2 * math.sin(2.0 * phase)
            + h3 * math.sin(3.0 * phase)
        )
        env = 1.0
        if i < fade:
            env = i / fade
        elif i >= length - fade:
            env = (length - 1 - i) / fade
        idx = start + i
        if 0 <= idx < len(samples):
            samples[idx] += sample * env * 0.35


def write_wav(path: Path, samples: list[float]) -> None:
    peak = max((abs(s) for s in samples), default=1.0) or 1.0
    scale = 0.9 / peak
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        frames = bytearray()
        for s in samples:
            v = int(max(-32767, min(32767, round(s * scale * 32767.0))))
            frames += struct.pack("<h", v)
        wf.writeframes(frames)


def render_notes(
    notes: list[tuple[float, float, int, int]],
    *,
    vibrato_cents: float = 0.0,
    detune_cents: float = 0.0,
) -> list[float]:
    beat_sec = 60.0 / BPM
    total_beats = max(start + dur for start, dur, *_ in notes) + 0.25
    n = int(total_beats * beat_sec * SAMPLE_RATE) + SAMPLE_RATE
    samples = [0.0] * n
    for start_beat, dur_beats, pitch, _vel in notes:
        start = int(start_beat * beat_sec * SAMPLE_RATE)
        length = int(dur_beats * beat_sec * SAMPLE_RATE)
        synth_note(
            samples,
            start,
            length,
            float(pitch),
            vibrato_cents=vibrato_cents,
            detune_cents=detune_cents,
        )
    return samples


def emit(name: str, notes: list[tuple[float, float, int, int]], **kwargs) -> None:
    mid = OUT / f"{name}.mid"
    wav = OUT / f"{name}.wav"
    write_midi(mid, notes)
    write_wav(wav, render_notes(notes, **kwargs))
    print(f"wrote {mid.name} + {wav.name}")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)

    scale = [(float(i), 0.9, 12 + i * 12, 96) for i in range(0, 5)]  # C0 C1 C2 C3 C4
    emit("scale_c0_c4", scale)

    melody = [
        (0.0, 0.9, 60, 100),  # C4
        (1.0, 0.9, 62, 100),  # D4
        (2.0, 0.9, 64, 100),  # E4
        (3.0, 0.9, 65, 100),  # F4
        (4.0, 0.9, 67, 100),  # G4
        (5.0, 0.9, 69, 100),  # A4
        (6.0, 0.9, 71, 100),  # B4
        (7.0, 1.8, 72, 100),  # C5
        (9.0, 0.9, 48, 100),  # C3
        (10.0, 0.9, 52, 100),  # E3
        (11.0, 1.8, 55, 100),  # G3
    ]
    emit("melody_c_major", melody)

    vibrato = [(0.0, 4.0, 69, 100)]  # A3 (~220 Hz) with mild vibrato
    emit("vibrato_a3", vibrato, vibrato_cents=20.0)

    detuned = [(0.0, 3.0, 64, 100)]  # E3 +35 cents
    emit("detuned_e3", detuned, detune_cents=35.0)

    meta = OUT / "README.md"
    meta.write_text(
        "# Pitch fixtures\n\n"
        "Generated by `tools/generate_pitch_test_fixtures.py`.\n\n"
        "| File | Content |\n"
        "|---|---|\n"
        "| `scale_c0_c4` | C0..C4 sustained notes |\n"
        "| `melody_c_major` | C-major melody + low C/E/G |\n"
        "| `vibrato_a3` | A3 pure tone with ±20 cent vibrato @ 5.5 Hz |\n"
        "| `detuned_e3` | E3 held +35 cents off equal temperament |\n",
        encoding="utf-8",
    )
    print(f"done -> {OUT}")


if __name__ == "__main__":
    main()
