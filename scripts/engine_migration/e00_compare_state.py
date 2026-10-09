#!/usr/bin/env python3
"""Compare canonical gameplay and tick-owned effect state by simulation tick.

Records are keyed by the tick number. Within a record, the `tick` line and each
entity are parsed into key/value fields. By default, shared fields/ticks allow
older dump formats to be compared. --strict requires identical tick, entity and
field inventories and should be used for same-format regression checks.

Presentation-only keys are ignored: the draw counter (`frame`), the digest
(`hash`), legacy draw scratch fields (`life`, `life2`) and camera state.
The explicit `carphase` indicator/siren records and `effectsstate` pool digest
are compared. A recorded presentation mutation always fails validation.

Exit code 0 when every shared tick's authoritative fields match, 1 otherwise.

Usage:
    python scripts/engine_migration/e00_compare_state.py a.txt b.txt
"""

import argparse
import re
import sys
from collections import OrderedDict

_IGNORED_KEYS = {
    "frame", "hash",
    "life", "life2",
    "camx", "camy", "camz", "camang", "camdist",
    "head", "headtarget", "viewchange", "camvx", "camvy", "camvz",
}

_PAIR_RE = re.compile(r"\b([A-Za-z_][A-Za-z_0-9]*) (-?\d+)")


def parse_fields(line):
    return {key: int(value) for key, value in _PAIR_RE.findall(line)}


def parse(path):
    records = OrderedDict()
    current = None
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            line = raw.rstrip("\n")
            if line.startswith("tick "):
                current = {"tick": parse_fields(line)}
                records[int(line.split()[1])] = current
            elif line.startswith("player ") and current is not None:
                current["player"] = parse_fields(line)
            elif line.startswith("car ") and current is not None:
                slot = int(line.split()[1])
                current[f"car:{slot}"] = parse_fields(line)
            elif line.startswith("missionstate ") and current is not None:
                current["missionstate"] = parse_fields(line)
            elif line.startswith("effectsstate ") and current is not None:
                current["effectsstate"] = parse_fields(line)
            elif line.startswith("carphase ") and current is not None:
                current[f"carphase:{int(line.split()[1])}"] = parse_fields(line)
            elif line.startswith("replay ") and current is not None:
                stream = int(line.split()[1])
                current[f"replay:{stream}"] = parse_fields(line)
            elif line.startswith("presentation-mutation "):
                raise ValueError(f"Presentation mutated tick-owned state: {path}: {line}")
            elif line.startswith(("reset ", "# ")):
                current = None
    return records


def compare_record(a, b, strict=False):
    """Return the list of differing (entity, key, a_value, b_value)."""
    diffs = []
    for entity in sorted(set(a) | set(b)):
        if entity not in a or entity not in b:
            if strict:
                diffs.append((entity, "presence", int(entity in a), int(entity in b)))
            continue
        fields_a, fields_b = a[entity], b[entity]
        for key in sorted(set(fields_a) | set(fields_b)):
            if key in _IGNORED_KEYS:
                continue
            if key not in fields_a or key not in fields_b:
                if strict:
                    diffs.append((entity, key, fields_a.get(key), fields_b.get(key)))
                continue
            value_a = fields_a[key]
            value_b = fields_b[key]
            if value_a != value_b:
                diffs.append((entity, key, value_a, value_b))
    return diffs


def main():
    arguments = argparse.ArgumentParser(description=__doc__)
    arguments.add_argument("a")
    arguments.add_argument("b")
    arguments.add_argument("--strict", action="store_true",
                           help="Also require identical tick/entity/field inventories.")
    options = arguments.parse_args()
    a = parse(options.a)
    b = parse(options.b)
    shared = [tick for tick in a if tick in b]

    mismatches = []
    for tick in shared:
        if compare_record(a[tick], b[tick], options.strict):
            mismatches.append(tick)

    print(f"ticks: A={len(a)} B={len(b)} shared={len(shared)} "
          f"A-only={len(a) - len(shared)} B-only={len(b) - len(shared)} "
          f"mismatches={len(mismatches)}")

    if not shared:
        print("no shared ticks; comparison cannot establish determinism")
        return 1
    if options.strict and set(a) != set(b):
        print("tick inventories differ")
        return 1
    if mismatches:
        first = mismatches[0]
        print(f"first mismatch at tick {first}")
        for entity, key, value_a, value_b in compare_record(a[first], b[first], options.strict):
            print(f"  {entity}.{key}: A={value_a} B={value_b}")
        return 1

    print("all shared ticks match (authoritative fields)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
