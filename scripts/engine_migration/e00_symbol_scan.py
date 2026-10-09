#!/usr/bin/env python3
"""E00 scan: inventory PsyCross/Psy-Q API consumers in REDRIVER2 game sources.

Scans the Psy-Q compatibility headers and PsyX public headers shipped by the
PsyCross submodule, extracts the declared functions/macros per header category,
then counts word-boundary usages in game-owned sources (Game/, utils/,
platform/, redriver2_psxpc.cpp) while excluding the PsyCross submodule itself.

The output is the mechanical appendix for
knowledge/engine-migration/e00/dependency-inventory.md. It is a scan of the
current source, not a removal claim: a symbol being unused here only means this
branch's game sources do not reference it.

Usage:
    python scripts/engine_migration/e00_symbol_scan.py <repo-root> <output-dir>
"""

import os
import re
import sys
from collections import defaultdict

# Header -> service category used in the generated report.
HEADER_NAMES = {
    "abs.h": "libc-abs",
    "asm.h": "asm",
    "gtemac.h": "gte-macros",
    "gtereg.h": "gte-registers",
    "inline_c.h": "gte-inline-c",
    "kernel.h": "kernel",
    "libapi.h": "libapi",
    "libcd.h": "libcd",
    "libetc.h": "libetc",
    "libgpu.h": "libgpu",
    "libgte.h": "libgte",
    "libmath.h": "libmath",
    "libmcrd.h": "libmcrd",
    "libpad.h": "libpad",
    "libpress.h": "libpress",
    "libsn.h": "libsn",
    "libspu.h": "libspu",
    "r3000.h": "r3000",
    "rand.h": "rand",
    "strings.h": "strings",
    "types.h": "types",
}

# Conservative prototype recognizer for the Psy-Q headers.
PROTO_RE = re.compile(
    r"^\s*(?:extern\s+)?(?:const\s+)?(?:unsigned\s+|signed\s+)?"
    r"(?:void|int|short|long|char|float|double|u_char|u_short|u_int|u_long|"
    r"VECTOR|SVECTOR|CVECTOR|DVECTOR|MATRIX|RECT|RECT16|OT_TAG|"
    r"struct\s+\w+|unsigned\s+\w+)\s*\*?\s+"
    r"([A-Za-z_]\w*)\s*\("
)
DEFINE_RE = re.compile(r"^\s*#define\s+([A-Za-z_]\w*)(\(|\s)")
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
TOKEN_RE = re.compile(r"[A-Za-z_]\w*")


def extract_symbols(path):
    """Return (functions, macros) declared in one header."""
    funcs, macros = set(), set()
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            lines = handle.readlines()
    except OSError:
        return funcs, macros
    for line in lines:
        match = DEFINE_RE.match(line)
        if match:
            macros.add(match.group(1))
            continue
        match = PROTO_RE.match(line)
        if match:
            funcs.add(match.group(1))
    return funcs, macros


def iter_sources(root):
    """Yield game-owned source files, never anything under PsyCross."""
    roots = [
        os.path.join(root, "src_rebuild", "Game"),
        os.path.join(root, "src_rebuild", "utils"),
        os.path.join(root, "src_rebuild", "platform"),
    ]
    exts = {".c", ".cpp", ".h", ".cc"}
    for base in roots:
        for dirpath, _, files in os.walk(base):
            for name in files:
                if os.path.splitext(name)[1].lower() in exts:
                    yield os.path.join(dirpath, name)
    yield os.path.join(root, "src_rebuild", "redriver2_psxpc.cpp")


def main():
    root = sys.argv[1]
    outdir = sys.argv[2]
    psx_inc = os.path.join(root, "src_rebuild", "PsyCross", "include", "psx")
    psyx_inc = os.path.join(root, "src_rebuild", "PsyCross", "include", "PsyX")

    category = {}
    for fname in sorted(os.listdir(psx_inc)):
        if not fname.endswith(".h"):
            continue
        cat = HEADER_NAMES.get(fname, fname)
        funcs, macros = extract_symbols(os.path.join(psx_inc, fname))
        for symbol in funcs:
            category.setdefault(symbol, cat)
        for symbol in macros:
            category.setdefault(symbol, cat + "-macro")

    for dirpath, _, files in os.walk(psyx_inc):
        for fname in files:
            if not fname.endswith(".h"):
                continue
            funcs, macros = extract_symbols(os.path.join(dirpath, fname))
            for symbol in funcs:
                category.setdefault(symbol, "psyx-public")
            for symbol in macros:
                category.setdefault(symbol, "psyx-public-macro")

    usage = defaultdict(lambda: defaultdict(int))
    includes = defaultdict(set)

    for path in iter_sources(root):
        rel = os.path.relpath(path, root).replace("\\", "/")
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        for match in INCLUDE_RE.finditer(text):
            header = match.group(1)
            if header.startswith("psx/") or header.startswith("PsyX/"):
                includes[rel].add(header)
        for token in TOKEN_RE.findall(text):
            if token in category:
                usage[token][rel] += 1

    os.makedirs(outdir, exist_ok=True)
    bycat = defaultdict(list)
    for symbol, files in usage.items():
        bycat[category[symbol]].append((symbol, sum(files.values()), files))

    with open(os.path.join(outdir, "symbol-usage.md"), "w", encoding="utf-8") as out:
        out.write("# PsyCross / Psy-Q symbol usage in game sources\n\n")
        out.write("Generated by `scripts/engine_migration/e00_symbol_scan.py`.\n\n")
        for cat in sorted(bycat):
            out.write(f"## {cat}\n\n| symbol | total | top consumers |\n|---|---|---|\n")
            for symbol, total, files in sorted(bycat[cat], key=lambda item: -item[1]):
                top = ", ".join(
                    f"{name}({count})"
                    for name, count in sorted(files.items(), key=lambda item: -item[1])[:4]
                )
                out.write(f"| `{symbol}` | {total} | {top} |\n")
            out.write("\n")
        unused = [symbol for symbol in category if symbol not in usage]
        out.write(f"## Declared but unused ({len(unused)})\n\n")
        out.write(", ".join(f"`{symbol}`" for symbol in sorted(unused)) + "\n\n")

    with open(os.path.join(outdir, "includes.md"), "w", encoding="utf-8") as out:
        out.write("# PsyX/Psy-Q header includes per game source\n\n")
        for rel in sorted(includes):
            out.write(f"- `{rel}`: {', '.join(sorted(includes[rel]))}\n")

    print(
        f"declared symbols: {len(category)}; used in game sources: {len(usage)}; "
        f"unused: {len(category) - len(usage)}"
    )
    for cat in sorted(bycat):
        references = sum(item[1] for item in bycat[cat])
        print(f"  {cat}: {len(bycat[cat])} used symbols, {references} references")


if __name__ == "__main__":
    main()
