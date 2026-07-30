#!/usr/bin/env python3
"""Fail if UI source strings still contain Cyrillic (except whitelist autonyms).

Scans tr("…"), QObject::tr("…"), QT_TR_NOOP("…"), and
QCoreApplication::translate("Context", "…") in src/, include/, ui/, plugins/.
Stylesheets / CSS-looking literals are ignored.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CYR = re.compile(r"[А-Яа-яЁё]")

# Intentional non-English UI sources (autonyms / policy).
WHITELIST = {
    "Русский",
}

FOLDERS = ("src", "include", "ui", "plugins")
SUFFIXES = {".cpp", ".h", ".hpp", ".ui"}

TR_PAT = re.compile(
    r"""(?:\b(?:QObject::)?tr|QT_TR_NOOP)\(\s*"(?P<body>(?:\\.|[^"\\])*)"""
)
TRANSLATE_PAT = re.compile(
    r"""QCoreApplication::translate\(\s*"(?:\\.|[^"\\])*"\s*,\s*"(?P<body>(?:\\.|[^"\\])*)"""
)
UI_STRING_PAT = re.compile(r"<string(?P<a>[^>]*)>(?P<body>[^<]*)</string>")


def unescape_cpp(body: str) -> str:
    return (
        body.replace("\\\\", "\0")
        .replace("\\n", "\n")
        .replace("\\t", "\t")
        .replace('\\"', '"')
        .replace("\\'", "'")
        .replace("\0", "\\")
    )


def looks_stylesheet(s: str) -> bool:
    if "QProgressBar" in s or "QPushButton" in s or "background-color" in s:
        return True
    if "border:" in s and "color:" in s and "{" in s:
        return True
    return False


def check_cpp(path: Path, violations: list[tuple[str, int, str]]) -> None:
    text = path.read_text(encoding="utf-8")
    for pat in (TR_PAT, TRANSLATE_PAT):
        for m in pat.finditer(text):
            raw = unescape_cpp(m.group("body"))
            if looks_stylesheet(raw) or not CYR.search(raw):
                continue
            if raw in WHITELIST:
                continue
            line = text.count("\n", 0, m.start()) + 1
            violations.append((str(path.relative_to(ROOT)), line, raw))


def check_ui(path: Path, violations: list[tuple[str, int, str]]) -> None:
    text = path.read_text(encoding="utf-8")
    for m in UI_STRING_PAT.finditer(text):
        if "notr=" in (m.group("a") or ""):
            continue
        body = m.group("body")
        if looks_stylesheet(body) or not CYR.search(body):
            continue
        if body in WHITELIST:
            continue
        line = text.count("\n", 0, m.start()) + 1
        violations.append((str(path.relative_to(ROOT)), line, body))


def iter_files() -> list[Path]:
    out: list[Path] = []
    for folder in FOLDERS:
        base = ROOT / folder
        if not base.exists():
            continue
        for p in base.rglob("*"):
            if "thirdparty" in p.parts:
                continue
            if p.suffix.lower() in SUFFIXES:
                out.append(p)
    return out


def main() -> int:
    violations: list[tuple[str, int, str]] = []
    for path in iter_files():
        if path.suffix.lower() == ".ui":
            check_ui(path, violations)
        else:
            check_cpp(path, violations)

    if not violations:
        print("OK: no unexpected Cyrillic in UI source strings")
        return 0

    print(f"FAIL: {len(violations)} Cyrillic UI source string(s) (English msgid required):")
    for rel, line, raw in violations:
        preview = raw.replace("\n", "\\n")
        if len(preview) > 80:
            preview = preview[:77] + "..."
        print(f"  {rel}:{line}: {preview!r}")
    print("Whitelist autonyms only:", ", ".join(sorted(WHITELIST)))
    return 1


if __name__ == "__main__":
    sys.exit(main())
