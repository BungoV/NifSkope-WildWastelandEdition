#!/usr/bin/env python3
"""Reader for the NifSkope WW telemetry file (WW_TELEMETRY=<file>, lane MODULES1).

One JSON object per line: t, frame, row, gen, module, event, fields. Events: start, row, switch set/unset/reset, cost.

  python tools/telemetry.py <file>                  per row: frame-time and per-module medians (GPU/CPU ms)
  python tools/telemetry.py <file> --zones          per row: per-zone medians too
  python tools/telemetry.py <file> --switches       launch switches and every live set with its generation
  python tools/telemetry.py <file> --json           the per-row medians as JSON (for harnesses and judges)
  --last N   only the last N cost rows of each row (default all); --after-gen  only cost rows drawn after the row's
  last live set took effect (gen >= that set's gen).
"""
import json
import statistics
import sys


def read(path):
    recs = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for n, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            try:
                recs.append(json.loads(line))
            except ValueError:
                print(f"skip line {n}: not JSON", file=sys.stderr)
    return recs


def med(v):
    return round(statistics.median(v), 3) if v else None


def summarise(recs, last=0, after_gen=False):
    rows = {}
    order = []
    set_gen = {}
    for r in recs:
        row = r.get("row", "")
        if r.get("event") in ("set", "unset", "reset") and r.get("module") == "switch":
            set_gen[row] = r.get("gen", 0)
        if r.get("event") != "cost":
            continue
        if after_gen and r.get("gen", 0) < set_gen.get(row, 0):
            continue
        if row not in rows:
            rows[row] = []
            order.append(row)
        rows[row].append(r)
    out = {}
    for row in order:
        cs = rows[row][-last:] if last else rows[row]
        mods, zones = {}, {}
        for c in cs:
            for m, (cpu, gpu) in c.get("modules", {}).items():
                mods.setdefault(m, ([], []))
                mods[m][0].append(cpu)
                mods[m][1].append(gpu)
            for z, v in c.get("zones", {}).items():
                zones.setdefault(z, ([], []))
                zones[z][0].append(v[0])
                zones[z][1].append(v[1])
        out[row] = {
            "frames": len(cs),
            "gpu": med([c["gpu"] for c in cs]),
            "cpu": med([c["cpu"] for c in cs]),
            "modules": {m: {"cpu": med(a), "gpu": med(b)} for m, (a, b) in mods.items()},
            "zones": {z: {"cpu": med(a), "gpu": med(b)} for z, (a, b) in zones.items()},
        }
    return out


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    path = argv[1]
    last = 0
    if "--last" in argv:
        last = int(argv[argv.index("--last") + 1])
    recs = read(path)
    if "--switches" in argv:
        for r in recs:
            if r.get("event") == "start":
                for k, v in sorted(r.get("switches", {}).items()):
                    print(f"launch {k}={v}")
            elif r.get("module") == "switch":
                print(f"frame {r.get('frame')} row {r.get('row')!r} gen {r.get('gen')}: {r.get('event')} "
                      f"{r.get('name', r.get('what', ''))}={r.get('value', '')} stage {r.get('stage', '')} -> {r.get('result')}")
        return 0
    s = summarise(recs, last, "--after-gen" in argv)
    if "--json" in argv:
        print(json.dumps(s, indent=1))
        return 0
    for row, d in s.items():
        print(f"row {row!r}: {d['frames']} frames, frame GPU {d['gpu']} ms CPU {d['cpu']} ms")
        for m, v in sorted(d["modules"].items(), key=lambda kv: -(kv[1]["gpu"] or 0)):
            print(f"  {m:<12} gpu {v['gpu']:>8} cpu {v['cpu']:>8}")
        if "--zones" in argv:
            for z, v in sorted(d["zones"].items(), key=lambda kv: -(kv[1]["gpu"] or 0)):
                print(f"    zone {z:<14} gpu {v['gpu']:>8} cpu {v['cpu']:>8}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
