#!/usr/bin/env python3
"""The cell census walk's independent checker (lane PRTP5, tests/spells/cell_census.sh).

It reads the PLUGIN and nothing else of ours: its own walk of the group tree, its own
subrecord reader. From that it derives

  * the cell list: every interior CELL of the top CELL group, and every exterior CELL of the
    named worldspace that carries a grid position (XCLC);
  * per cell, the reference count (REFR records in the cell's own child groups; for an exterior
    cell also the worldspace's persistent REFRs whose position falls in that cell's 4096-unit
    square) and the placed-light count (of those, the ones not flagged deleted whose base
    (NAME) is a LIGH record);

and compares them with the walk's plan file and census rows.

  cell_census_check.py sample <plugin> [--interiors N] [--center X,Y] [--world W] [--small]
        prints the sample's keys: N interiors spread evenly over the plugin in file order, plus
        Vault111Cryo and CabotHouse01, plus the 3x3 exterior block around the centre cell
  cell_census_check.py plan <plugin> <plan file> [--world W]
        the plan against the plugin's own cell list
  cell_census_check.py rows <plugin> <census file> <keys file> [--world W] [--block N]
        every sampled key's row against the plugin (a row opened as a smaller block than N must
        name the limit, and the plugin's own count of the bigger block must be over it)
  cell_census_check.py check <plugin> <census file> <plan file> <keys file> [--world W] [--block N]
        both

Each check prints `PASS  ...` or `FAIL  ...`; the last line is `cell_census_check: N checks,
M failures  PASS|FAIL`. Exit code 0 only when nothing failed.
"""

import math
import mmap
import re
import struct
import sys
import zlib

CELL_UNITS = 4096.0
SAMPLE_NAMED = ("Vault111Cryo", "CabotHouse01")


class Plugin:
    def __init__(self, path):
        self.f = open(path, "rb")
        self.mm = mmap.mmap(self.f.fileno(), 0, access=mmap.ACCESS_READ)
        self.size = len(self.mm)
        self.tops = {}
        off = 0
        while off + 24 <= self.size:
            typ = self.mm[off:off + 4]
            size = struct.unpack_from("<I", self.mm, off + 4)[0]
            if typ == b"GRUP":
                gtype = struct.unpack_from("<i", self.mm, off + 12)[0]
                if gtype == 0:
                    self.tops[bytes(self.mm[off + 8:off + 12])] = (off + 24, off + size)
                off += size
            else:
                off += 24 + size

    def items(self, start, end):
        """Yield ('G', off, end, label, gtype) for a group and ('R', off, type, flags, form, size) for a record."""
        mm = self.mm
        off = start
        while off + 24 <= end:
            typ = mm[off:off + 4]
            size = struct.unpack_from("<I", mm, off + 4)[0]
            if typ == b"GRUP":
                label, gtype = struct.unpack_from("<Ii", mm, off + 8)
                yield ("G", off, off + size, label, gtype)
                off += size
            else:
                flags, form = struct.unpack_from("<II", mm, off + 8)
                yield ("R", off, bytes(typ), flags, form, size)
                off += 24 + size

    def data(self, off, flags, size):
        raw = self.mm[off + 24:off + 24 + size]
        if flags & 0x00040000:
            return zlib.decompress(raw[4:])
        return raw

    @staticmethod
    def fields(data):
        off, big, n = 0, None, len(data)
        while off + 6 <= n:
            typ = data[off:off + 4]
            size = struct.unpack_from("<H", data, off + 4)[0]
            off += 6
            if typ == b"XXXX":
                big = struct.unpack_from("<I", data, off)[0]
                off += size
                continue
            if big is not None:
                size, big = big, None
            yield typ, data[off:off + size]
            off += size


def zstr(b):
    return bytes(b).split(b"\0", 1)[0].decode("cp1252", "replace")


def light_forms(p):
    out = set()
    if b"LIGH" in p.tops:
        for it in p.items(*p.tops[b"LIGH"]):
            if it[0] == "R" and it[2] == b"LIGH":
                out.add(it[4])
    return out


def cells_in(p, start, end, out, depth=0):
    """Every CELL record under [start, end), with the span of its own child group."""
    last = None
    for it in p.items(start, end):
        if it[0] == "R":
            if it[2] != b"CELL":
                last = None
                continue
            edid, grid = "", None
            for typ, d in p.fields(p.data(it[1], it[3], it[5])):
                if typ == b"EDID":
                    edid = zstr(d)
                elif typ == b"XCLC" and len(d) >= 8:
                    grid = struct.unpack_from("<ii", d, 0)
            last = {"form": it[4], "edid": edid, "grid": grid, "children": None, "nested": depth > 0}
            out.append(last)
        else:
            _, off, gend, label, gtype = it
            if gtype == 6:
                if last is not None and label == last["form"]:
                    last["children"] = (off + 24, gend)
            else:
                cells_in(p, off + 24, gend, out, depth + 1)
                last = None


def refs_of(p, span, lights):
    """[(form, deleted, is_light, x, y)] for every REFR under a cell's child group."""
    out = []
    if not span:
        return out
    stack = [span]
    while stack:
        s, e = stack.pop()
        for it in p.items(s, e):
            if it[0] == "G":
                stack.append((it[1] + 24, it[2]))
                continue
            if it[2] != b"REFR":
                continue
            base, x, y = 0, 0.0, 0.0
            for typ, d in p.fields(p.data(it[1], it[3], it[5])):
                if typ == b"NAME" and len(d) >= 4:
                    base = struct.unpack_from("<I", d, 0)[0]
                elif typ == b"DATA" and len(d) >= 12:
                    x, y = struct.unpack_from("<ff", d, 0)
            deleted = bool(it[3] & 0x20)
            out.append((it[4], deleted, (not deleted) and base in lights, x, y))
    return out


class Model:
    def __init__(self, path, world):
        self.p = Plugin(path)
        self.world = world
        self.lights = light_forms(self.p)
        self.interiors = []
        if b"CELL" in self.p.tops:
            cells_in(self.p, *self.p.tops[b"CELL"], self.interiors)
        self.exteriors = {}        # (x, y) -> cell
        self.persistent = None     # the worldspace's persistent cell
        self.duplicates = 0
        self._pers = None
        self._cache = {}
        if world and b"WRLD" in self.p.tops:
            want = None
            for it in self.p.items(*self.p.tops[b"WRLD"]):
                if it[0] == "R" and it[2] == b"WRLD":
                    want = None
                    for typ, d in self.p.fields(self.p.data(it[1], it[3], it[5])):
                        if typ == b"EDID" and zstr(d).lower() == world.lower():
                            want = it[4]
                elif it[0] == "G" and it[4] == 1 and want is not None and it[3] == want:
                    found = []
                    cells_in(self.p, it[1] + 24, it[2], found)
                    for c in found:
                        if c["grid"] is None or not c["nested"]:
                            if not c["nested"]:
                                self.persistent = c
                            continue
                        if c["grid"] in self.exteriors:
                            self.duplicates += 1
                        self.exteriors[c["grid"]] = c
                    want = None

    def keys(self):
        """{key: (form, edid)} for the whole walk."""
        out = {}
        for c in self.interiors:
            out["I:%08X" % c["form"]] = (c["form"], c["edid"])
        for (x, y), c in self.exteriors.items():
            out["E:%s:%d,%d" % (self.world, x, y)] = (c["form"], c["edid"])
        return out

    def _persistent_by_grid(self):
        if self._pers is None:
            self._pers = {}
            if self.persistent:
                for r in refs_of(self.p, self.persistent["children"], self.lights):
                    g = (math.floor(r[3] / CELL_UNITS), math.floor(r[4] / CELL_UNITS))
                    self._pers.setdefault(g, []).append(r)
        return self._pers

    def exterior_counts(self, x, y):
        """(references, placed lights) of one exterior cell, or None if the plugin has no such cell."""
        if (x, y) in self._cache:
            return self._cache[(x, y)]
        c = self.exteriors.get((x, y))
        if c is None:
            self._cache[(x, y)] = None
            return None
        seen, refs, lights = set(), 0, 0
        for r in refs_of(self.p, c["children"], self.lights) + self._persistent_by_grid().get((x, y), []):
            if r[0] in seen:
                continue
            seen.add(r[0])
            refs += 1
            lights += 1 if r[2] else 0
        self._cache[(x, y)] = (refs, lights)
        return self._cache[(x, y)]

    def interior_counts(self, form):
        for c in self.interiors:
            if c["form"] == form:
                rs = refs_of(self.p, c["children"], self.lights)
                return (len(rs), sum(1 for r in rs if r[2]))
        return None


class Tally:
    def __init__(self):
        self.checks = 0
        self.failures = 0

    def check(self, ok, what, detail=""):
        self.checks += 1
        if not ok:
            self.failures += 1
        print("%s  %s%s" % ("PASS" if ok else "FAIL", what, ("   [%s]" % detail) if detail else ""))

    def end(self):
        print("cell_census_check: %d checks, %d failures  %s"
              % (self.checks, self.failures, "PASS" if self.failures == 0 else "FAIL"))
        return 0 if self.failures == 0 else 1


def read_keys(path):
    out = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if not line or line.startswith("#") or line.startswith("key\t"):
                continue
            out.append(line.split("\t")[0].strip())
    return out


def read_rows(path):
    rows, cols = [], None
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if parts[0] == "key":
                cols = parts
                continue
            if cols:
                rows.append(dict(zip(cols, parts)))
    return rows


def do_sample(m, n, center, small):
    named = [c for c in m.interiors if c["edid"] in SAMPLE_NAMED]
    if small:
        picks = list(named)
        step = max(1, len(m.interiors) // 2)
        for c in m.interiors[step // 2::step][:2]:
            if c not in picks:
                picks.append(c)
    else:
        step = max(1, len(m.interiors) // n)
        picks = m.interiors[step // 2::step][:n]
        for c in named:
            if c not in picks:
                picks.append(c)
    order = {id(c): i for i, c in enumerate(m.interiors)}
    picks.sort(key=lambda c: order[id(c)])
    print("# the cell census sample (tests/spells/cell_census_check.py): %d interiors%s"
          % (len(picks), "" if small else ", and the 3x3 exterior block around %d,%d" % center))
    for c in picks:
        print("I:%08X\t%s" % (c["form"], c["edid"] or "-"))
    if not small:
        for y in range(center[1] - 1, center[1] + 2):
            for x in range(center[0] - 1, center[0] + 2):
                if (x, y) in m.exteriors:
                    print("E:%s:%d,%d\t%s" % (m.world, x, y, m.exteriors[(x, y)]["edid"] or "-"))
    return 0


def do_plan(m, t, plan_path):
    own = m.keys()
    plan = {}
    with open(plan_path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            plan[parts[0]] = (int(parts[1], 16) if len(parts) > 1 else 0, parts[2] if len(parts) > 2 else "-")
    ni = sum(1 for k in own if k.startswith("I:"))
    missing = sorted(set(own) - set(plan))
    extra = sorted(set(plan) - set(own))
    t.check(len(own) > 0 and ni > 0, "the plugin has cells to walk",
            "%d interiors, %d exterior cells of %s (%d grid positions named twice)"
            % (ni, len(own) - ni, m.world, m.duplicates))
    t.check(not missing, "the plan leaves out no cell of the plugin",
            "%d missing%s" % (len(missing), (": " + ", ".join(missing[:4])) if missing else ""))
    t.check(not extra, "the plan names no cell the plugin does not have",
            "%d extra%s" % (len(extra), (": " + ", ".join(extra[:4])) if extra else ""))
    both = [k for k in plan if k in own]
    bad_form = [k for k in both if plan[k][0] != own[k][0]]
    bad_edid = [k for k in both if plan[k][1] != (own[k][1] or "-")]
    t.check(not bad_form, "every planned cell carries the plugin's form id",
            "%d differ%s" % (len(bad_form), (": " + ", ".join(bad_form[:4])) if bad_form else ""))
    t.check(not bad_edid, "every planned cell carries the plugin's editor id",
            "%d differ%s" % (len(bad_edid), (": " + ", ".join(bad_edid[:4])) if bad_edid else ""))


def num(row, col):
    try:
        return int(row.get(col, "-"))
    except ValueError:
        return None


def do_rows(m, t, census_path, keys_path, block):
    own = m.keys()
    keys = read_keys(keys_path)
    rows = read_rows(census_path)
    by_key = {}
    for r in rows:
        by_key.setdefault(r["key"], []).append(r)
    t.check(len(keys) > 0, "the sample names cells", "%d keys" % len(keys))
    ok_rows = 0
    for k in keys:
        got = by_key.get(k, [])
        if len(got) != 1:
            t.check(False, "%s: exactly one census row" % k, "%d rows" % len(got))
            continue
        r = got[0]
        if k not in own:
            t.check(False, "%s: the plugin has this cell" % k)
            continue
        form, edid = own[k]
        t.check(r.get("form", "") == "%08X" % form and r.get("edid", "") == (edid or "-"),
                "%s: the row names the plugin's cell" % k, "%s %s" % (r.get("form"), r.get("edid")))
        if r.get("status") != "ok":
            t.check(r.get("status") in ("refused", "crashed") and r.get("note", "-") != "-",
                    "%s: a cell that was not drawn says why" % k,
                    "%s: %s" % (r.get("status"), r.get("note", "")[:160]))
            continue
        ok_rows += 1
        if k.startswith("I:"):
            want = m.interior_counts(form)
            wb = want
        else:
            x, y = [int(v) for v in k.rsplit(":", 1)[1].split(",")]
            want = m.exterior_counts(x, y)

            def block_counts(n):
                h, out = n // 2, [0, 0]
                for yy in range(y - h, y + h + 1):
                    for xx in range(x - h, x + h + 1):
                        c = m.exterior_counts(xx, yy)
                        if c:
                            out[0] += c[0]
                            out[1] += c[1]
                return out

            # the walk opens a block that holds too many references as a smaller one, and must say so
            rb = num(r, "block")
            if rb is not None and 1 <= rb < block and rb % 2 == 1:
                said = re.search(r"opened as (\d+)x\d+: the (\d+)x\d+ block holds about (\d+) references, "
                                 r"over the walk's limit of (\d+)", r.get("note", ""))
                full = block_counts(block)[0]
                bigger = block_counts(rb + 2)[0]
                t.check(bool(said) and int(said.group(1)) == rb and int(said.group(2)) == block
                        and abs(int(said.group(3)) - full) <= max(5, full // 50)
                        and bigger > int(said.group(4))
                        and (rb == 1 or block_counts(rb)[0] <= int(said.group(4))),
                        "%s: opened smaller than %dx%d only because the plugin's own count is over the limit it names"
                        % (k, block, block),
                        "block %d; plugin: %dx%d %d, %dx%d %d; note: %s"
                        % (rb, block, block, full, rb + 2, rb + 2, bigger, r.get("note", "")[-120:]))
            else:
                t.check(rb == block, "%s: drawn as the %dx%d block" % (k, block, block), r.get("block", ""))
                rb = block
            wb = block_counts(rb)
        t.check(num(r, "refs") == want[0], "%s: references" % k, "census %s, plugin %d" % (r.get("refs"), want[0]))
        t.check(num(r, "lights_cell") == want[1], "%s: placed lights" % k,
                "census %s, plugin %d" % (r.get("lights_cell"), want[1]))
        t.check(num(r, "refs_block") == wb[0], "%s: references in the block" % k,
                "census %s, plugin %d" % (r.get("refs_block"), wb[0]))
        t.check(num(r, "lights_block") == wb[1], "%s: placed lights in the block" % k,
                "census %s, plugin %d" % (r.get("lights_block"), wb[1]))
        lit, off, norad, black, amb = (num(r, c) for c in ("lit", "skip_off", "skip_noradius", "skip_black", "ambient_only"))
        t.check(None not in (lit, off, norad, black, amb) and lit + off + norad + black + amb == wb[1],
                "%s: lit + skipped (off, no radius, black, ambient only) = placed" % k,
                "%s + %s + %s + %s + %s vs %d" % (lit, off, norad, black, amb, wb[1]))
    t.check(ok_rows > 0, "at least one sampled cell was drawn", "%d of %d" % (ok_rows, len(keys)))


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    mode, plugin = argv[1], argv[2]
    rest, opts = [], {"--world": "Commonwealth", "--interiors": "18", "--center": "-20,7", "--block": "5"}
    small = False
    i = 3
    while i < len(argv):
        if argv[i] == "--small":
            small = True
        elif argv[i] in opts and i + 1 < len(argv):
            opts[argv[i]] = argv[i + 1]
            i += 1
        else:
            rest.append(argv[i])
        i += 1
    m = Model(plugin, opts["--world"])
    if mode == "sample":
        cx, cy = [int(v) for v in opts["--center"].split(",")]
        return do_sample(m, int(opts["--interiors"]), (cx, cy), small)
    t = Tally()
    if mode == "plan" and len(rest) >= 1:
        do_plan(m, t, rest[0])
    elif mode == "rows" and len(rest) >= 2:
        do_rows(m, t, rest[0], rest[1], int(opts["--block"]))
    elif mode == "check" and len(rest) >= 3:
        do_plan(m, t, rest[1])
        do_rows(m, t, rest[0], rest[2], int(opts["--block"]))
    else:
        print(__doc__)
        return 2
    return t.end()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
