#!/usr/bin/env python3
"""Fill unfinished translation entries after lupdate (English source)."""
import re
from pathlib import Path

from apply_remaining_en import IDENTITY, build_en_to_ru

ROOT = Path(__file__).resolve().parents[1] / "translations"

KEEP_AS_IS = {"▶", "■", "00:00.0", "❚❚", "BPM:", "&File", "&Edit"}


def last_source(message: str) -> str:
    sources = re.findall(r"<source>(.*?)</source>", message, re.DOTALL)
    return sources[-1] if sources else ""


def fix_message(message: str, lang: str, en_to_ru: dict[str, str]) -> tuple[str, bool]:
    match = re.search(
        r"<translation(?P<attrs>[^>]*)>(?P<body>.*?)</translation>",
        message,
        re.DOTALL,
    )
    if not match or 'type="unfinished"' not in match.group("attrs"):
        return message, False

    source = last_source(message)
    body = match.group("body")

    if lang == "en":
        translation = body if body.strip() else source
    elif source in IDENTITY or source in KEEP_AS_IS:
        translation = source
    elif source in en_to_ru:
        translation = en_to_ru[source]
    elif body.strip():
        translation = body
    else:
        translation = source

    new_tag = f"<translation>{translation}</translation>"
    new_message = message[: match.start()] + new_tag + message[match.end() :]
    return new_message, True


def fix_file(path: Path, lang: str, en_to_ru: dict[str, str]) -> int:
    text = path.read_text(encoding="utf-8")
    parts = re.split(r"(<message>.*?</message>)", text, flags=re.DOTALL)
    fixed = 0
    for i, part in enumerate(parts):
        if part.startswith("<message>"):
            parts[i], changed = fix_message(part, lang, en_to_ru)
            if changed:
                fixed += 1
    path.write_text("".join(parts), encoding="utf-8")
    return fixed


if __name__ == "__main__":
    en_to_ru = build_en_to_ru()
    for name, lang in (("en_US.ts", "en"), ("ru_RU.ts", "ru")):
        count = fix_file(ROOT / name, lang, en_to_ru)
        print(f"{name}: fixed {count} unfinished entries")
