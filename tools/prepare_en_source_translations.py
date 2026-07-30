#!/usr/bin/env python3
"""
HISTORICAL helper: invert RU-source live en_US.ts into translations/from_en/.

After the EN-source migration, live translations/*.ts already use English <source>.
Do NOT re-run this against current live catalogs (it would double-invert).

Kept for regenerating maps from an old RU-source checkout if needed.
See: MARKDOWN/PLAN_I18N_ENGLISH_SOURCE.md, tools/install_en_source_translations.py
"""
from __future__ import annotations

import json
import re
import xml.etree.ElementTree as ET
from copy import deepcopy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIVE_EN = ROOT / "translations" / "en_US.ts"
OUT_DIR = ROOT / "translations" / "from_en"


def local_tag(el: ET.Element) -> str:
    return el.tag.rsplit("}", 1)[-1]


def find_child(parent: ET.Element, name: str) -> ET.Element | None:
    for c in parent:
        if local_tag(c) == name:
            return c
    return None


def text_of(el: ET.Element | None) -> str:
    if el is None:
        return ""
    return "".join(el.itertext())


def set_text(el: ET.Element, value: str) -> None:
    el.clear()
    el.text = value


def looks_like_non_ui(source: str) -> bool:
    s = source.strip()
    if not s:
        return True
    if "QProgressBar" in s or "QPushButton" in s or "background-color" in s:
        return True
    if s.startswith("font-") and "color:" in s:
        return True
    return False


def invert_catalogs() -> dict:
    tree = ET.parse(LIVE_EN)
    root = tree.getroot()

    en_root = ET.Element("TS", version="2.1", language="en_US")
    ru_root = ET.Element("TS", version="2.1", language="ru_RU")

    # Preserve header comments via README; TS itself starts clean.
    ru_to_en: dict[str, str] = {}
    en_to_ru: dict[str, str] = {}
    skipped_css = 0
    message_count = 0

    for ctx in root:
        if local_tag(ctx) != "context":
            continue
        name_el = find_child(ctx, "name")
        ctx_name = text_of(name_el) or "Unknown"

        en_ctx = ET.SubElement(en_root, "context")
        ET.SubElement(en_ctx, "name").text = ctx_name
        ru_ctx = ET.SubElement(ru_root, "context")
        ET.SubElement(ru_ctx, "name").text = ctx_name

        for msg in ctx:
            if local_tag(msg) != "message":
                continue
            src_el = find_child(msg, "source")
            tr_el = find_child(msg, "translation")
            ru_src = text_of(src_el)
            en_tr = text_of(tr_el)
            if not en_tr.strip():
                en_tr = ru_src

            # Non-UI / stylesheet: keep source==translation as-is in both catalogs.
            if looks_like_non_ui(ru_src):
                skipped_css += 1
                en_src = ru_src
                ru_dst = ru_src
            else:
                en_src = en_tr
                ru_dst = ru_src
                if ru_src and en_src:
                    ru_to_en[ru_src] = en_src
                    # Prefer first RU if EN already mapped (should be rare; we checked collisions).
                    en_to_ru.setdefault(en_src, ru_dst)

            def copy_locations(dst_msg: ET.Element) -> None:
                for loc in msg:
                    if local_tag(loc) == "location":
                        new_loc = ET.SubElement(dst_msg, "location")
                        for k, v in loc.attrib.items():
                            new_loc.set(k, v)

            en_msg = ET.SubElement(en_ctx, "message")
            copy_locations(en_msg)
            ET.SubElement(en_msg, "source").text = en_src
            ET.SubElement(en_msg, "translation").text = en_src

            ru_msg = ET.SubElement(ru_ctx, "message")
            copy_locations(ru_msg)
            ET.SubElement(ru_msg, "source").text = en_src
            ET.SubElement(ru_msg, "translation").text = ru_dst

            # Carry translator comments if present.
            comment = find_child(msg, "translatorcomment")
            if comment is not None and text_of(comment).strip():
                ET.SubElement(en_msg, "translatorcomment").text = text_of(comment)
                ET.SubElement(ru_msg, "translatorcomment").text = text_of(comment)

            message_count += 1

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    def write_ts(path: Path, ts_root: ET.Element) -> None:
        # Pretty-ish XML for Linguist.
        rough = ET.tostring(ts_root, encoding="utf-8")
        # ElementTree doesn't include DOCTYPE; add manually.
        text = (
            '<?xml version="1.0" encoding="utf-8"?>\n'
            "<!DOCTYPE TS>\n"
            + rough.decode("utf-8")
        )
        # Indent with a simple pass via reparse if available
        try:
            import xml.dom.minidom as md

            dom = md.parseString(text.encode("utf-8"))
            text = dom.toprettyxml(indent="    ", encoding="utf-8").decode("utf-8")
            # minidom adds extra XML declaration quirks — normalize
            if text.startswith("<?xml"):
                lines = text.splitlines()
                # drop empty lines after declaration
                out_lines = []
                for i, line in enumerate(lines):
                    if i == 0:
                        out_lines.append('<?xml version="1.0" encoding="utf-8"?>')
                        continue
                    if line.strip() == "":
                        continue
                    if line.strip() == "<?xml version='1.0' encoding='utf-8'?>":
                        continue
                    out_lines.append(line)
                # Ensure DOCTYPE once
                body = "\n".join(out_lines)
                if "<!DOCTYPE TS>" not in body:
                    body = body.replace(
                        '<?xml version="1.0" encoding="utf-8"?>',
                        '<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>',
                        1,
                    )
                text = body + "\n"
        except Exception:
            text = text if text.endswith("\n") else text + "\n"
        path.write_text(text, encoding="utf-8")

    write_ts(OUT_DIR / "en_US.ts", en_root)
    write_ts(OUT_DIR / "ru_RU.ts", ru_root)

    map_payload = {
        "description": (
            "Prepared from live translations/en_US.ts. "
            "After migrating tr()/UI strings to English, replace translations/*.ts "
            "with translations/from_en/*.ts and re-run lupdate."
        ),
        "message_count": message_count,
        "stylesheet_passthrough": skipped_css,
        "ru_to_en": ru_to_en,
        "en_to_ru": en_to_ru,
    }
    (OUT_DIR / "source_map.json").write_text(
        json.dumps(map_payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    return {
        "messages": message_count,
        "map_pairs": len(ru_to_en),
        "css": skipped_css,
        "out": str(OUT_DIR),
    }


if __name__ == "__main__":
    info = invert_catalogs()
    print(
        f"Prepared {info['messages']} messages "
        f"({info['map_pairs']} RU<->EN pairs, {info['css']} stylesheet passthrough) -> {info['out']}"
    )
