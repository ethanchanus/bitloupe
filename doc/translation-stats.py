#!/usr/bin/env python3
from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List


@dataclass
class EntryStats:
    total: int = 0
    translated: int = 0
    fuzzy: int = 0
    untranslated: int = 0

    def add(self, other: "EntryStats") -> None:
        self.total += other.total
        self.translated += other.translated
        self.fuzzy += other.fuzzy
        self.untranslated += other.untranslated

    def completion(self) -> float:
        if self.total == 0:
            return 0.0
        return (self.translated / self.total) * 100.0


@dataclass
class PoEntry:
    msgid_parts: List[str]
    msgstr_parts: Dict[int, List[str]]
    fuzzy: bool


def _parse_po_quoted(line: str) -> str:
    line = line.strip()
    if not (line.startswith('"') and line.endswith('"')):
        return ""
    content = line[1:-1]
    return bytes(content, "utf-8").decode("unicode_escape")


def _finalize_entry(entry: PoEntry, stats: EntryStats) -> None:
    msgid = "".join(entry.msgid_parts)
    if msgid == "":
        return

    stats.total += 1
    translated = any("".join(parts) != "" for parts in entry.msgstr_parts.values())
    if translated:
        stats.translated += 1
    else:
        stats.untranslated += 1
    if entry.fuzzy:
        stats.fuzzy += 1


def parse_po_file(path: Path) -> EntryStats:
    stats = EntryStats()
    current: PoEntry | None = None
    active_field: str | None = None
    active_index = 0

    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for raw_line in handle:
            line = raw_line.rstrip("\n")

            if line.startswith("#,") and "fuzzy" in line:
                if current is None:
                    current = PoEntry(msgid_parts=[], msgstr_parts={0: []}, fuzzy=True)
                else:
                    current.fuzzy = True
                continue

            if line.startswith("msgid "):
                if current is not None:
                    _finalize_entry(current, stats)
                current = PoEntry(msgid_parts=[], msgstr_parts={0: []}, fuzzy=False)
                active_field = "msgid"
                active_index = 0
                current.msgid_parts.append(_parse_po_quoted(line[len("msgid "):]))
                continue

            if current is None:
                continue

            if line.startswith("msgstr["):
                idx_text = line[len("msgstr["):].split("]", 1)[0]
                active_index = int(idx_text)
                active_field = "msgstr"
                if active_index not in current.msgstr_parts:
                    current.msgstr_parts[active_index] = []
                start = line.find('"')
                current.msgstr_parts[active_index].append(_parse_po_quoted(line[start:]))
                continue

            if line.startswith("msgstr "):
                active_field = "msgstr"
                active_index = 0
                current.msgstr_parts[0] = [_parse_po_quoted(line[len("msgstr "):])]
                continue

            if line.startswith('"'):
                value = _parse_po_quoted(line)
                if active_field == "msgid":
                    current.msgid_parts.append(value)
                elif active_field == "msgstr":
                    if active_index not in current.msgstr_parts:
                        current.msgstr_parts[active_index] = []
                    current.msgstr_parts[active_index].append(value)
                continue

            active_field = None

    if current is not None:
        _finalize_entry(current, stats)

    return stats


def find_language_dirs(locale_root: Path) -> List[Path]:
    dirs: List[Path] = []
    for child in sorted(locale_root.iterdir()):
        if not child.is_dir():
            continue
        lc_messages = child / "LC_MESSAGES"
        if lc_messages.is_dir():
            dirs.append(child)
    return dirs


def print_report(locale_root: Path) -> None:
    language_dirs = find_language_dirs(locale_root)
    if not language_dirs:
        print(f"No language directories found under: {locale_root}")
        return

    print("Translation Statistics")
    print(f"Locale root: {locale_root}")
    print()

    for lang_dir in language_dirs:
        lang = lang_dir.name
        po_dir = lang_dir / "LC_MESSAGES"
        po_files = sorted(po_dir.glob("*.po"))
        if not po_files:
            continue

        global_stats = EntryStats()
        file_stats: List[tuple[str, EntryStats]] = []
        for po_file in po_files:
            stats = parse_po_file(po_file)
            global_stats.add(stats)
            file_stats.append((po_file.name, stats))

        print(f"Language: {lang}")
        print(
            "  Global: "
            f"files={len(file_stats)} "
            f"total={global_stats.total} "
            f"translated={global_stats.translated} "
            f"fuzzy={global_stats.fuzzy} "
            f"untranslated={global_stats.untranslated} "
            f"completion={global_stats.completion():.1f}%"
        )
        print("  Files:")
        print("    {:<24} {:>8} {:>11} {:>7} {:>13} {:>11}".format(
            "name", "total", "translated", "fuzzy", "untranslated", "completion"
        ))
        for name, stats in file_stats:
            print("    {:<24} {:>8} {:>11} {:>7} {:>13} {:>10.1f}%".format(
                name,
                stats.total,
                stats.translated,
                stats.fuzzy,
                stats.untranslated,
                stats.completion(),
            ))
        print()


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    locale_root = script_dir / "src" / "locale"
    if not locale_root.is_dir():
        print(f"Locale directory not found: {locale_root}")
        return 1

    print_report(locale_root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
