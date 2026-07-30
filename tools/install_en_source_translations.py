#!/usr/bin/env python3
"""Copy translations/from_en/*.ts → translations/ and merge EXTRA_RU_TO_EN pairs."""
from __future__ import annotations

import json
import shutil
import xml.etree.ElementTree as ET
from pathlib import Path

from migrate_tr_to_english import EXTRA_RU_TO_EN

ROOT = Path(__file__).resolve().parents[1]
FROM = ROOT / "translations" / "from_en"
LIVE = ROOT / "translations"


def text_of(el: ET.Element | None) -> str:
    if el is None:
        return ""
    return "".join(el.itertext())


def ensure_message(ctx: ET.Element, source: str, translation: str) -> bool:
    for msg in ctx.findall("message"):
        src = msg.find("source")
        if src is not None and text_of(src) == source:
            tr = msg.find("translation")
            if tr is None:
                tr = ET.SubElement(msg, "translation")
            tr.text = translation
            tr.attrib.pop("type", None)
            return False
    msg = ET.SubElement(ctx, "message")
    ET.SubElement(msg, "source").text = source
    ET.SubElement(msg, "translation").text = translation
    return True


def merge_extra(en_path: Path, ru_path: Path) -> int:
    en_tree = ET.parse(en_path)
    ru_tree = ET.parse(ru_path)
    en_root = en_tree.getroot()
    ru_root = ru_tree.getroot()

    def first_ctx(root: ET.Element) -> ET.Element:
        ctx = root.find("context")
        if ctx is None:
            ctx = ET.SubElement(root, "context")
            ET.SubElement(ctx, "name").text = "DONTFLOAT"
        return ctx

    en_ctx = first_ctx(en_root)
    ru_ctx = first_ctx(ru_root)
    added = 0
    for ru, en in EXTRA_RU_TO_EN.items():
        if not en or en == ru:
            # Autonym / identity — still ensure EN catalog has the source.
            ensure_message(en_ctx, en or ru, en or ru)
            ensure_message(ru_ctx, en or ru, en or ru)
            continue
        if ensure_message(en_ctx, en, en):
            added += 1
        if ensure_message(ru_ctx, en, ru):
            added += 1
    en_tree.write(en_path, encoding="utf-8", xml_declaration=True)
    ru_tree.write(ru_path, encoding="utf-8", xml_declaration=True)
    return added


def refresh_source_map() -> None:
    map_path = FROM / "source_map.json"
    data = json.loads(map_path.read_text(encoding="utf-8"))
    ru_to_en = dict(data.get("ru_to_en", {}))
    en_to_ru = dict(data.get("en_to_ru", {}))
    for ru, en in EXTRA_RU_TO_EN.items():
        ru_to_en[ru] = en
        if en != ru:
            en_to_ru.setdefault(en, ru)
    data["ru_to_en"] = ru_to_en
    data["en_to_ru"] = en_to_ru
    map_path.write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def main() -> None:
    refresh_source_map()
    en_src = FROM / "en_US.ts"
    ru_src = FROM / "ru_RU.ts"
    added = merge_extra(en_src, ru_src)
    shutil.copy2(en_src, LIVE / "en_US.ts")
    shutil.copy2(ru_src, LIVE / "ru_RU.ts")
    print(f"Copied from_en -> translations/; EXTRA merge touch/add ops~={added}")


if __name__ == "__main__":
    main()
