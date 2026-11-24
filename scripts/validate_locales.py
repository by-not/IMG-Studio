#!/usr/bin/env python3
"""Validate IMG Studio locale JSON files for completeness, encoding, duplicates, and placeholders."""

import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOCALES_DIR = os.path.join(ROOT, "assets", "locales")
SRC_DIR = os.path.join(ROOT, "src")
PLACEHOLDER_RE = re.compile(r"\{([a-zA-Z0-9_]+)\}")
CODE_TR_RE = re.compile(r'\b(?:tr(?:f|_c)?|modal_label)\s*\((?:[^;()]*?\?\s*)?"([a-z0-9_.]+)"(?:\s*:\s*"([a-z0-9_.]+)")?')


def load_json_strict(path: str):
    with open(path, "rb") as f:
        raw = f.read()
    if raw.startswith(b"\xef\xbb\xbf"):
        raise ValueError("UTF-8 BOM is forbidden; save as UTF-8 without BOM")
    text = raw.decode("utf-8", errors="strict")

    duplicates = []

    def check_dups(pairs):
        obj = {}
        for k, v in pairs:
            if k in obj:
                duplicates.append(k)
            obj[k] = v
        return obj

    data = json.loads(text, object_pairs_hook=check_dups)
    if duplicates:
        raise ValueError(f"Duplicate JSON keys found: {', '.join(duplicates)}")
    return data


def collect_code_keys() -> set[str]:
    keys: set[str] = set()
    for dirpath, _, filenames in os.walk(SRC_DIR):
        for fn in filenames:
            if not (fn.endswith(".cpp") or fn.endswith(".hpp")):
                continue
            p = os.path.join(dirpath, fn)
            with open(p, "r", encoding="utf-8") as f:
                content = f.read()
            for m in CODE_TR_RE.finditer(content):
                keys.add(m.group(1))
                if m.group(2):
                    keys.add(m.group(2))
            for m in re.finditer(r'\{\s*"([a-z0-9_.]+)"\s*,', content):
                if "." in m.group(1):
                    keys.add(m.group(1))
    return keys


def main() -> int:
    en_path = os.path.join(LOCALES_DIR, "en.json")
    if not os.path.exists(en_path):
        print(f"ERROR: Missing base locale {en_path}")
        return 1

    try:
        en_data = load_json_strict(en_path)
    except Exception as exc:
        print(f"ERROR in en.json: {exc}")
        return 1

    en_strings = en_data.get("strings", {})
    if not isinstance(en_strings, dict) or not en_strings:
        print("ERROR: en.json missing non-empty 'strings' object")
        return 1

    en_keys = set(en_strings.keys())
    errors = 0

    for fn in sorted(os.listdir(LOCALES_DIR)):
        if not fn.endswith(".json"):
            continue
        path = os.path.join(LOCALES_DIR, fn)
        try:
            data = load_json_strict(path)
        except Exception as exc:
            print(f"ERROR in {fn}: {exc}")
            errors += 1
            continue

        meta = data.get("_meta")
        strings = data.get("strings")
        if not isinstance(meta, dict) or not isinstance(strings, dict):
            print(f"ERROR in {fn}: missing '_meta' or 'strings' object")
            errors += 1
            continue

        for req_meta in ("code", "native_name", "english_name"):
            if not isinstance(meta.get(req_meta), str) or not meta[req_meta].strip():
                print(f"ERROR in {fn}: missing '_meta.{req_meta}'")
                errors += 1

        loc_keys = set(strings.keys())
        missing = sorted(en_keys - loc_keys)
        extra = sorted(loc_keys - en_keys)
        if missing:
            print(f"ERROR in {fn}: missing {len(missing)} key(s): {missing[:8]}")
            errors += 1
        if extra:
            print(f"ERROR in {fn}: unknown extra {len(extra)} key(s): {extra[:8]}")
            errors += 1

        if fn == "template.json":
            continue

        for k in sorted(en_keys & loc_keys):
            val = strings[k]
            if not isinstance(val, str) or not val.strip():
                print(f"ERROR in {fn}: empty translation for '{k}'")
                errors += 1
                continue
            en_ph = set(PLACEHOLDER_RE.findall(en_strings[k]))
            loc_ph = set(PLACEHOLDER_RE.findall(val))
            if en_ph != loc_ph:
                print(f"ERROR in {fn}: placeholder mismatch for '{k}': expected {sorted(en_ph)}, got {sorted(loc_ph)}")
                errors += 1

    # Check source code keys against en.json if any tr() calls exist
    code_keys = collect_code_keys()
    if code_keys:
        missing_in_en = sorted(code_keys - en_keys)
        if missing_in_en:
            print(f"ERROR: Keys used in src/ but missing in en.json ({len(missing_in_en)}): {missing_in_en[:10]}")
            errors += 1
        unused_in_en = sorted(en_keys - code_keys)
        if unused_in_en:
            print(f"WARNING: Keys in en.json not directly matched in src/ ({len(unused_in_en)}): {unused_in_en[:10]}")

    if errors > 0:
        print(f"Validation FAILED with {errors} error(s).")
        return 1

    print(f"Validation OK: {len(en_keys)} keys verified across locales in {LOCALES_DIR}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
