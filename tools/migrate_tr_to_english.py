#!/usr/bin/env python3
"""Migrate UI source strings RU -> EN using from_en/source_map.json (+ punctuation-normalized lookup)."""
from __future__ import annotations

import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAP_PATH = ROOT / "translations" / "from_en" / "source_map.json"
CYR = re.compile(r"[А-Яа-яЁё]")

# New / split strings not always present as whole entries in live .ts
EXTRA_RU_TO_EN: dict[str, str] = {
    "Тональность тактов %1–%2": "Key for bars %1–%2",
    "Тональность тактов %1-%2": "Key for bars %1-%2",
    "%1 (такты %2–%3)": "%1 (bars %2–%3)",
    "%1 (такты %2-%3)": "%1 (bars %2-%3)",
    "Не определена": "Undefined",
    "Язык: Русский": "Language: Russian",
    "Language: English": "Language: English",
    "Русский": "Русский",
    "English": "English",
    "Language": "Language",
    # Marker / testgen / pitch UI leftovers (absent or unfinished in live en_US.ts)
    "%1 — импорт аудио": "%1 — audio import",
    "%1 — экспорт WAV": "%1 — WAV export",
    "&Сохранить…": "&Save…",
    "BPM %1 · долей %2 · отклонений %3": "BPM %1 · beats %2 · deviations %3",
    "BPM: %1 — метки на каждой доле": "BPM: %1 — markers on each beat",
    "DONTFLOAT — разметка тестовых файлов": "DONTFLOAT — test file marking",
    "WAV (*.wav);;MP3 — копия исходника (*.mp3);;Все файлы (*)": (
        "WAV (*.wav);;MP3 — source copy (*.mp3);;All files (*)"
    ),
    "stretch применён · %1 сэмплов": "stretch applied · %1 samples",
    "Анализ BPM": "BPM analysis",
    "Аудио (*.wav *.mp3 *.flac);;Все файлы (*)": "Audio (*.wav *.mp3 *.flac);;All files (*)",
    "В аудиофайле есть несохраненные изменения.\n": (
        "The audio file has unsaved changes.\n"
    ),
    "В&ыход": "E&xit",
    "Выровнять доли": "Align beats",
    "Декодирование…": "Decoding…",
    "Для применения растяжения необходимо минимум 2 метки.\n": (
        "At least 2 markers are required to apply stretch.\n"
    ),
    "Загружены метки из %1": "Loaded markers from %1",
    "Импорт WAV…": "Import WAV…",
    "Метки привязаны к тактовой сетке": "Markers snapped to the bar grid",
    "Не удалось декодировать файл: %1": "Failed to decode file: %1",
    "Не удалось определить BPM. Укажите BPM вручную.": (
        "Could not detect BPM. Enter BPM manually."
    ),
    "Не удалось скопировать аудиофайл.": "Failed to copy the audio file.",
    "Откройте аудиофайл с постоянным BPM": "Open an audio file with a constant BPM",
    "Открыть аудио": "Open audio",
    "Ошибка": "Error",
    "Питч / ноты / тональность": "Pitch / notes / key",
    "Привязать метки": "Snap markers",
    "Привязать метки к сетке": "Snap markers to grid",
    "Применить stretch": "Apply stretch",
    "Применить коррекцию": "Apply correction",
    "Размер:": "Size:",
    "Ритм / BPM / stretch": "Rhythm / BPM / stretch",
    "Сдвинуть тактовую сетку на один удар вперёд (Shift — вместе с метками)\n": (
        "Shift the bar grid one beat forward (Shift — with markers)\n"
    ),
    "Сдвинуть тактовую сетку на один удар назад (Shift — вместе с метками)\n": (
        "Shift the bar grid one beat backward (Shift — with markers)\n"
    ),
    "Сетка сдвинута %1 на 1 удар": "Grid shifted %1 by 1 beat",
    "Сначала примените сжатие-растяжение (Ctrl+T), ": (
        "Apply time-stretch first (Ctrl+T), "
    ),
    "Сохранено: %1 и %2": "Saved: %1 and %2",
    "Сохранить изменения?": "Save changes?",
    "Сохранить метки и аудио перед продолжением?": (
        "Save markers and audio before continuing?"
    ),
    "Экспорт WAV…": "Export WAV…",
    "анализ BPM…": "analyzing BPM…",
    "анализ завершён: %1, найдено нот: %2": "analysis done: %1, notes found: %2",
    "аудио %1 сэмплов @ %2 Гц · BPM %3": "audio %1 samples @ %2 Hz · BPM %3",
    "аудио: %1 сэмплов, %2 Гц": "audio: %1 samples, %2 Hz",
    "аудио: %1 сэмплов, %2 Гц — нажмите «Анализ BPM»": (
        "audio: %1 samples, %2 Hz — click “BPM analysis”"
    ),
    "выравнивание долей…": "aligning beats…",
    "доли выровнены (BPM %1). Перетащите метки stretch при необходимости.": (
        "beats aligned (BPM %1). Drag stretch markers if needed."
    ),
    "загрузите аудио или воспроизведите трек в DAW для захвата сигнала.": (
        "load audio or play a track in the DAW to capture the signal."
    ),
    "коррекция не удалась": "correction failed",
    "коррекция применена — обработанное аудио в сессии плагина": (
        "correction applied — processed audio is in the plugin session"
    ),
    "не удалось определить BPM": "could not detect BPM",
    "не удалось применить stretch": "failed to apply stretch",
    "нет аудиоданных для анализа": "no audio data for analysis",
    "нет аудиоданных для анализа BPM": "no audio data for BPM analysis",
    "нет изменённых нот для коррекции": "no modified notes for correction",
    "нет исходного аудио для коррекции": "no source audio for correction",
    "ошибка выравнивания долей": "beat alignment error",
    "ошибка импорта: %1": "import error: %1",
    "ошибка экспорта: %1": "export error: %1",
    "применение коррекции высоты…": "applying pitch correction…",
    "сетка ▶": "grid ▶",
    "экспортировано: %1": "exported: %1",
    "◀ сетка\n": "◀ grid\n",
    "◀ сетка": "◀ grid",
}


def norm(s: str) -> str:
    return (
        s.replace("\u2014", "-")
        .replace("\u2013", "-")
        .replace("—", "-")
        .replace("–", "-")
        .replace("\u2026", "...")
        .replace("…", "...")
        .replace("\u00b7", "|")
        .replace("·", "|")
    )


def unescape_cpp(body: str) -> str:
    return (
        body.replace("\\\\", "\0")
        .replace("\\n", "\n")
        .replace("\\t", "\t")
        .replace('\\"', '"')
        .replace("\\'", "'")
        .replace("\0", "\\")
    )


def escape_cpp(s: str) -> str:
    return (
        s.replace("\\", "\\\\")
        .replace("\n", "\\n")
        .replace("\t", "\\t")
        .replace('"', '\\"')
    )


def looks_stylesheet(s: str) -> bool:
    if "QProgressBar" in s or "QPushButton" in s or "background-color" in s:
        return True
    if "border:" in s and "color:" in s and "{" in s:
        return True
    return False


def build_maps() -> tuple[dict[str, str], dict[str, str]]:
    exact = dict(json.loads(MAP_PATH.read_text(encoding="utf-8"))["ru_to_en"])
    exact.update(EXTRA_RU_TO_EN)
    # Merge live en_US.ts (RU source -> EN translation)
    live = ROOT / "translations" / "en_US.ts"
    if live.exists():
        for ctx in ET.parse(live).getroot().findall("context"):
            for msg in ctx.findall("message"):
                s = msg.findtext("source") or ""
                t = msg.findtext("translation") or s
                if s and not looks_stylesheet(s):
                    exact.setdefault(s, t)
    fuzzy = {norm(k): v for k, v in exact.items()}
    return exact, fuzzy


def lookup(ru: str, exact: dict[str, str], fuzzy: dict[str, str]) -> str | None:
    if ru in exact:
        return exact[ru]
    n = norm(ru)
    if n in fuzzy:
        return fuzzy[n]
    # Try without trailing whitespace/newlines differences
    if ru.rstrip("\n") in exact:
        en = exact[ru.rstrip("\n")]
        return en + ("\n" if ru.endswith("\n") and not en.endswith("\n") else "")
    if norm(ru.rstrip("\n")) in fuzzy:
        en = fuzzy[norm(ru.rstrip("\n"))]
        return en + ("\n" if ru.endswith("\n") and not en.endswith("\n") else "")
    return None


def migrate_cpp(text: str, exact: dict[str, str], fuzzy: dict[str, str], stats: dict) -> str:
    pat = re.compile(r'\btr\(\s*"(?P<body>(?:\\.|[^"\\])*)"')

    def repl(m: re.Match) -> str:
        body = m.group("body")
        raw = unescape_cpp(body)
        if looks_stylesheet(raw) or not CYR.search(raw):
            return m.group(0)
        en = lookup(raw, exact, fuzzy)
        if en is None:
            stats["unmapped"].append(raw)
            return m.group(0)
        stats["replaced"] += 1
        return 'tr("' + escape_cpp(en) + '"'

    return pat.sub(repl, text)


def migrate_ui(text: str, exact: dict[str, str], fuzzy: dict[str, str], stats: dict) -> str:
    # Only match <string>...</string> whose body has no nested tags.
    pat = re.compile(r"<string(?P<attrs>[^>]*)>(?P<body>[^<]*)</string>")

    def repl(m: re.Match) -> str:
        attrs = m.group("attrs") or ""
        if "notr=" in attrs:
            return m.group(0)
        body = m.group("body")
        if looks_stylesheet(body) or not CYR.search(body):
            return m.group(0)
        en = lookup(body, exact, fuzzy)
        if en is None:
            stats["unmapped"].append(body)
            return m.group(0)
        stats["replaced"] += 1
        return f"<string{attrs}>{en}</string>"

    return pat.sub(repl, text)


def iter_files() -> list[Path]:
    out: list[Path] = []
    for folder in ("src", "include", "ui", "plugins"):
        base = ROOT / folder
        if not base.exists():
            continue
        for p in base.rglob("*"):
            if "thirdparty" in p.parts:
                continue
            if p.suffix.lower() in {".cpp", ".h", ".hpp", ".ui"}:
                out.append(p)
    return out


def main() -> int:
    exact, fuzzy = build_maps()
    stats = {"replaced": 0, "unmapped": [], "files": 0}
    for path in iter_files():
        original = path.read_text(encoding="utf-8")
        if path.suffix.lower() == ".ui":
            text = migrate_ui(original, exact, fuzzy, stats)
        else:
            text = migrate_cpp(original, exact, fuzzy, stats)
        if text != original:
            path.write_text(text, encoding="utf-8", newline="\n")
            stats["files"] += 1
            print(f"updated {path.relative_to(ROOT)}")

    unmapped = sorted(set(stats["unmapped"]))
    report = ROOT / "translations" / "from_en" / "migration_report.txt"
    report.write_text(
        f"replaced={stats['replaced']}\nfiles={stats['files']}\nunmapped={len(unmapped)}\n\n"
        + "\n---\n".join(unmapped)
        + "\n",
        encoding="utf-8",
    )
    print(
        f"Done: {stats['replaced']} replacements in {stats['files']} files; "
        f"unmapped={len(unmapped)} -> {report.relative_to(ROOT)}"
    )
    return 0 if len(unmapped) == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
