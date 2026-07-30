# -*- coding: utf-8 -*-
"""Fill unfinished ru_RU translations (EN source → RU) from source_map + EXTRA."""
from __future__ import annotations

import json
import xml.etree.ElementTree as ET
from pathlib import Path

from migrate_tr_to_english import EXTRA_RU_TO_EN

ROOT = Path(__file__).resolve().parents[1]
MAP_PATH = ROOT / "translations" / "from_en" / "source_map.json"
RU_PATH = ROOT / "translations" / "ru_RU.ts"
EN_PATH = ROOT / "translations" / "en_US.ts"

# Policy invariants: translation equals English source in all locales.
IDENTITY = {
    "Language",
    "English",
    "Language: English",
    "Русский",
}

# Extra EN→RU for strings fixed after the automated migration.
EXTRA_EN_TO_RU: dict[str, str] = {
    "Shift the bar grid one beat backward (Shift — with markers)\n"
    "Shift + LMB drag on waveform — fine grid adjustment": (
        "Сдвинуть тактовую сетку на один удар назад (Shift — вместе с метками)\n"
        "Shift + перетаскивание ЛКМ на волне — тонкая подстройка сетки"
    ),
    "Shift the bar grid one beat forward (Shift — with markers)\n"
    "Shift + LMB drag on waveform — fine grid adjustment": (
        "Сдвинуть тактовую сетку на один удар вперёд (Shift — вместе с метками)\n"
        "Shift + перетаскивание ЛКМ на волне — тонкая подстройка сетки"
    ),
    "The audio file has unsaved changes.\nDo you want to save the changes?": (
        "В аудиофайле есть несохраненные изменения.\nХотите сохранить изменения?"
    ),
    "Apply time-stretch first (Ctrl+T), then note pitch correction.": (
        "Сначала примените сжатие-растяжение (Ctrl+T), затем коррекцию высоты нот."
    ),
    "At least 2 markers are required to apply stretch.\nPress M to add markers.": (
        "Для применения растяжения необходимо минимум 2 метки.\n"
        "Используйте клавишу M для добавления меток."
    ),
}


def build_en_to_ru() -> dict[str, str]:
    data = json.loads(MAP_PATH.read_text(encoding="utf-8"))
    en_to_ru = dict(data.get("en_to_ru", {}))
    for ru, en in EXTRA_RU_TO_EN.items():
        if en != ru:
            en_to_ru.setdefault(en, ru)
        else:
            IDENTITY.add(en)
    en_to_ru.update(EXTRA_EN_TO_RU)
    # Prefer Russian status text for Language: Russian
    en_to_ru.setdefault("Language: Russian", "Язык: Русский")
    return en_to_ru


# Backward-compatible name used by finalize_translations.py (was RU→EN).
EN: dict[str, str] = {}


def _load() -> dict[str, str]:
    global EN
    if not EN:
        EN = build_en_to_ru()
    return EN


def apply_en_translations() -> int:
    """Identity-fill en_US.ts (source EN → translation EN)."""
    tree = ET.parse(EN_PATH)
    root = tree.getroot()
    n = 0
    for msg in root.iter("message"):
        tr = msg.find("translation")
        src_el = msg.find("source")
        if tr is None or src_el is None or tr.get("type") == "vanished":
            continue
        src = "".join(src_el.itertext())
        body = "".join(tr.itertext())
        if tr.get("type") == "unfinished" or not body.strip() or body != src:
            tr.text = src
            tr.attrib.pop("type", None)
            n += 1
    tree.write(EN_PATH, encoding="utf-8", xml_declaration=True)
    return n


def apply_ru_translations() -> int:
    """Fill unfinished/empty ru_RU from EN→RU map; identity for policy keys."""
    en_to_ru = _load()
    tree = ET.parse(RU_PATH)
    root = tree.getroot()
    n = 0
    for msg in root.iter("message"):
        tr = msg.find("translation")
        src_el = msg.find("source")
        if tr is None or src_el is None or tr.get("type") == "vanished":
            continue
        src = "".join(src_el.itertext())
        body = "".join(tr.itertext())
        unfinished = tr.get("type") == "unfinished" or not body.strip()
        if src in IDENTITY:
            if body != src or unfinished:
                tr.text = src
                tr.attrib.pop("type", None)
                n += 1
            continue
        if unfinished or (body == src and src in en_to_ru):
            if src in en_to_ru:
                tr.text = en_to_ru[src]
                tr.attrib.pop("type", None)
                n += 1
    tree.write(RU_PATH, encoding="utf-8", xml_declaration=True)
    return n


if __name__ == "__main__":
    print("en_US identity:", apply_en_translations())
    print("ru_RU filled:", apply_ru_translations())
