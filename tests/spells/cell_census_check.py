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
  * per block of cells, the same two counts for everything a load of that block reads;
  * the TILES: the exterior grid cut into squares of B cells that do not overlap (a tile holds
    x in [k*B, k*B + B - 1]; its centre is k*B + B//2), the UNITS of the walk (an interior, a
    tile) in the order the plan first names them, and the SLICE each unit belongs to (its place
    among the units walked, mod N, plus 1);

and compares them with the walk's plan file and census rows. Census v2: one row per cell, a tile
loaded once; the load's figures are on the load's first row and `^` on its other rows.

  cell_census_check.py sample <plugin> [--interiors N] [--center X,Y] [--world W] [--block B] [--small]
        prints the sample's keys: N interiors spread evenly over the plugin in file order, plus
        Vault111Cryo and CabotHouse01, plus the 3x3 exterior cells around the centre cell, plus
        one cell of the first tile in which nothing is placed (its rows are count only)
  cell_census_check.py splitkeys <plugin> [--center X,Y] [--world W] [--block B]
        the cells of the tile the centre cell is in (tag `over`) and of the nearest tile that
        holds 1 to 300 references (tag `died`): the split rule's two cases
  cell_census_check.py bakecell <plugin> [--interiors N]
        the sampled interior with the fewest references, 40 at least: the bake step's proof cell
  cell_census_check.py share <plugin> [per-cell table to write] [--world W]
        no census at all: of everything the plugin places, per cell and over the game, how much
        is a pick-up item, how much an actor, how much the rest (what a bake that leaves items
        and actors out does not have to load)
  cell_census_check.py plan <plugin> <plan file> [--world W] [--block B] [--slices N] [--only KEYS]
        the plan against the plugin's own cell list, tiles and slices
  cell_census_check.py rows <plugin> <census file> <keys file> [--plan PLAN] [--world W]
                            [--block B] [--margin M] [--slices N]
        every cell of every unit the keys are in: exactly one row, and the row against the plugin
  cell_census_check.py check <plugin> <census file> <plan file> <keys file> [same options]
        both
  cell_census_check.py bake <plugin> <census file> <key> <bake folder> [--tbk 4|3]
        the row says the bake step ran in the same visit, and its files are on disk

Each check prints `PASS  ...` or `FAIL  ...`; the last line is `cell_census_check: N checks,
M failures  PASS|FAIL`. Exit code 0 only when nothing failed.
"""

import glob
import math
import mmap
import os
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
    """[(form, deleted, is_light, x, y, base)] for every REFR under a cell's child group."""
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
            out.append((it[4], deleted, (not deleted) and base in lights, x, y, base))
    return out


def tile_centre(v, block):
    """The centre cell, along one axis, of the tile this cell is in (Python's // floors)."""
    return (v // block) * block + block // 2


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
        self._own = {}
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

    def _own_refs(self, x, y):
        """The REFRs of one exterior cell's own groups, or None if the plugin has no such cell."""
        if (x, y) not in self._own:
            c = self.exteriors.get((x, y))
            self._own[(x, y)] = None if c is None else refs_of(self.p, c["children"], self.lights)
        return self._own[(x, y)]

    def exterior_counts(self, x, y):
        """(references, placed lights) of one exterior cell, or None if the plugin has no such cell."""
        own = self._own_refs(x, y)
        if own is None:
            return None
        seen, refs, lights = set(), 0, 0
        for r in own + self._persistent_by_grid().get((x, y), []):
            if r[0] in seen:
                continue
            seen.add(r[0])
            refs += 1
            lights += 1 if r[2] else 0
        return (refs, lights)

    def block_counts(self, cx, cy, n):
        """(references, placed lights) of everything a load of the n x n block around cx,cy reads:
        the own references of every cell the plugin has there, then the worldspace's persistent
        references standing anywhere in the block that were not among those."""
        h = n // 2
        seen, refs, lights = set(), 0, 0
        squares = [(x, y) for y in range(cy - h, cy + h + 1) for x in range(cx - h, cx + h + 1)]
        for g in squares:
            for r in self._own_refs(*g) or []:
                seen.add(r[0])
                refs += 1
                lights += 1 if r[2] else 0
        pers = self._persistent_by_grid()
        for g in squares:
            for r in pers.get(g, []):
                if r[0] in seen:
                    continue
                seen.add(r[0])
                refs += 1
                lights += 1 if r[2] else 0
        return (refs, lights)

    def interior_counts(self, form):
        for c in self.interiors:
            if c["form"] == form:
                rs = refs_of(self.p, c["children"], self.lights)
                return (len(rs), sum(1 for r in rs if r[2]))
        return None

    def order(self):
        """The plugin's own order of the walk's keys: interiors in file order, then the exterior
        cells row by row from the south-west (used only where no plan file is given)."""
        out = ["I:%08X" % c["form"] for c in self.interiors]
        for (x, y) in sorted(self.exteriors, key=lambda g: (g[1], g[0])):
            out.append("E:%s:%d,%d" % (self.world, x, y))
        return out


def grid_of(key):
    x, y = key.rsplit(":", 1)[1].split(",")
    return int(x), int(y)


def unit_of(key, block):
    """The unit a key is in: the interior itself, or its exterior tile."""
    if key.startswith("I:"):
        return key
    x, y = grid_of(key)
    world = key.split(":")[1]
    return "T:%s:%d,%d" % (world, tile_centre(x, block), tile_centre(y, block))


def units_of(ordered_keys, block, only, slices):
    """(units in order, {unit: [keys]}, {unit: (place among the units walked, slice)})."""
    order, cells = [], {}
    for k in ordered_keys:
        u = unit_of(k, block)
        if u not in cells:
            cells[u] = []
            order.append(u)
        cells[u].append(k)
    place, n = {}, 0
    for u in order:
        if only is not None and not any(k in only for k in cells[u]):
            continue
        place[u] = (n, n % slices + 1)
        n += 1
    return order, cells, place


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


def read_plan(path):
    """[(key, form, edid, unit, place, slice)] in file order; the last three may be missing."""
    out = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t") + ["-"] * 6
            out.append((parts[0], int(parts[1], 16) if parts[1] != "-" else 0, parts[2], parts[3], parts[4], parts[5]))
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


def sample_interiors(m, n, small):
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
    return picks


def tiles_in_order(m, block):
    """Every tile that holds a cell, in the order the plan first names one of its cells."""
    seen, out = set(), []
    for (x, y) in sorted(m.exteriors, key=lambda g: (g[1], g[0])):
        t = (tile_centre(x, block), tile_centre(y, block))
        if t not in seen:
            seen.add(t)
            out.append(t)
    return out


def tile_cells(m, t, block):
    h = block // 2
    return [(x, y) for y in range(t[1] - h, t[1] + h + 1) for x in range(t[0] - h, t[0] + h + 1) if (x, y) in m.exteriors]


def do_sample(m, n, center, small, block):
    picks = sample_interiors(m, n, small)
    print("# the cell census sample (tests/spells/cell_census_check.py): %d interiors%s"
          % (len(picks), "" if small else ", the 3x3 exterior cells around %d,%d (the walk does the whole %dx%d tiles "
             "they are in), and one cell of a tile in which nothing is placed" % (center + (block, block))))
    for c in picks:
        print("I:%08X\t%s" % (c["form"], c["edid"] or "-"))
    if not small:
        for y in range(center[1] - 1, center[1] + 2):
            for x in range(center[0] - 1, center[0] + 2):
                if (x, y) in m.exteriors:
                    print("E:%s:%d,%d\t%s" % (m.world, x, y, m.exteriors[(x, y)]["edid"] or "-"))
        for t in tiles_in_order(m, block):
            if len(tile_cells(m, t, block)) == block * block and m.block_counts(t[0], t[1], block)[0] == 0:
                x, y = tile_cells(m, t, block)[0]
                print("E:%s:%d,%d\t%s\tnothing placed in its tile" % (m.world, x, y, m.exteriors[(x, y)]["edid"] or "-"))
                break
    return 0


def do_splitkeys(m, center, block):
    over = (tile_centre(center[0], block), tile_centre(center[1], block))
    print("# the split rule's two tiles (%dx%d): `over` holds %d references; `died` is a small one the gate says "
          "the walk died on" % (block, block, m.block_counts(over[0], over[1], block)[0]))
    for (x, y) in tile_cells(m, over, block):
        print("E:%s:%d,%d\t%s\tover" % (m.world, x, y, m.exteriors[(x, y)]["edid"] or "-"))
    for ring in range(1, 40):
        for dy in range(-ring, ring + 1):
            for dx in range(-ring, ring + 1):
                if max(abs(dx), abs(dy)) != ring:
                    continue
                t = (over[0] + dx * block, over[1] + dy * block)
                cells = tile_cells(m, t, block)
                if len(cells) == block * block and 1 <= m.block_counts(t[0], t[1], block)[0] <= 300:
                    for (x, y) in cells:
                        print("E:%s:%d,%d\t%s\tdied" % (m.world, x, y, m.exteriors[(x, y)]["edid"] or "-"))
                    return 0
    return 1


def do_bakecell(m, n):
    best = None
    for c in sample_interiors(m, n, False):
        refs = m.interior_counts(c["form"])[0]
        if refs >= 40 and (best is None or refs < best[0]):
            best = (refs, c)
    if best is None:
        return 1
    print("I:%08X\t%s\t%d references" % (best[1]["form"], best[1]["edid"] or "-", best[0]))
    return 0


def do_plan(m, t, plan_path, block, slices, only):
    own = m.keys()
    rows = read_plan(plan_path)
    plan = {r[0]: r for r in rows}
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
    t.check(len(plan) == len(rows), "the plan names no cell twice", "%d lines, %d keys" % (len(rows), len(plan)))
    both = [k for k in plan if k in own]
    bad_form = [k for k in both if plan[k][1] != own[k][0]]
    bad_edid = [k for k in both if plan[k][2] != (own[k][1] or "-")]
    t.check(not bad_form, "every planned cell carries the plugin's form id",
            "%d differ%s" % (len(bad_form), (": " + ", ".join(bad_form[:4])) if bad_form else ""))
    t.check(not bad_edid, "every planned cell carries the plugin's editor id",
            "%d differ%s" % (len(bad_edid), (": " + ", ".join(bad_edid[:4])) if bad_edid else ""))
    # the units and the slices, derived here from the plan's order and nothing else of the walk's
    order, cells, place = units_of([r[0] for r in rows], block, only, slices)
    bad_unit = [r[0] for r in rows if r[3] != unit_of(r[0], block)]
    t.check(not bad_unit, "every planned cell is in the unit its position gives (an interior, or its %dx%d tile)"
            % (block, block), "%d differ%s" % (len(bad_unit), (": " + ", ".join(bad_unit[:4])) if bad_unit else ""))
    bad_slice, no_slice = [], []
    for r in rows:
        u = unit_of(r[0], block)
        want = place.get(u)
        got = (r[4], r[5])
        if want is None:
            if got != ("-", "-"):
                bad_slice.append(r[0])
            continue
        if not (got[1].isdigit() and 1 <= int(got[1]) <= slices):
            no_slice.append(r[0])
        if got != (str(want[0]), str(want[1])):
            bad_slice.append(r[0])
    t.check(not no_slice, "every unit to walk belongs to one of the %d slices" % slices,
            "%d cells are in a slice nobody walks%s" % (len(no_slice), (": " + ", ".join(no_slice[:4])) if no_slice else ""))
    t.check(not bad_slice, "every unit to walk has the place and the slice the plan's order gives it",
            "%d cells differ%s; %d units to walk" % (len(bad_slice), (": " + ", ".join(bad_slice[:4])) if bad_slice else "",
                                                     len(place)))


def num(row, col):
    try:
        return int(row.get(col, "-"))
    except ValueError:
        return None


RE_OVER = re.compile(r"opened alone: the (\d+)x\d+ tile holds (\d+) references, over the walk's limit of (\d+)")
RE_DIED = re.compile(r"opened alone: the walk died on this tile as (\d+)x\d+")


def do_rows(m, t, census_path, keys_path, plan_path, block, margin, slices):
    own = m.keys()
    named = read_keys(keys_path)
    rows = read_rows(census_path)
    ordered = [r[0] for r in read_plan(plan_path)] if plan_path else m.order()
    # a cell the plan left out is still a cell of its unit: the plugin's own list closes the gap
    planned = set(ordered)
    ordered += [k for k in m.order() if k not in planned]
    order, cells, place = units_of(ordered, block, set(named), slices)
    walked = [u for u in order if u in place]
    keys = [k for u in walked for k in cells[u]]
    by_key = {}
    for r in rows:
        by_key.setdefault(r["key"], []).append(r)
    t.check(len(named) > 0 and len(keys) >= len(named), "the sample names cells, and the units they are in are known",
            "%d keys named, %d units, %d cells" % (len(named), len(walked), len(keys)))
    twice = sorted(k for k, v in by_key.items() if len(v) > 1)
    t.check(not twice, "no cell has two rows in the census",
            "%d cells%s" % (len(twice), (": " + ", ".join(twice[:4])) if twice else ""))
    outside = sorted(set(by_key) - set(keys))
    t.check(not outside, "the census holds no row for a cell outside the units walked",
            "%d rows%s" % (len(outside), (": " + ", ".join(outside[:4])) if outside else ""))
    ring = 2 * margin
    ok_rows, count_only = 0, 0
    leads = {}      # tile -> rows of a whole-tile load that carry the load's figures
    members = {}    # tile -> rows of a whole-tile load
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
        u = unit_of(k, block)
        want_tile = "-" if k.startswith("I:") else u.rsplit(":", 1)[1]
        want_slice = "%d/%d" % (place[u][1], slices)
        t.check(r.get("tile") == want_tile and r.get("slice") == want_slice,
                "%s: the row is in its tile, written by the slice that owns it" % k,
                "tile %s (plugin %s), slice %s (plan order %s)" % (r.get("tile"), want_tile, r.get("slice"), want_slice))
        if r.get("status") != "ok":
            t.check(r.get("status") in ("refused", "crashed") and r.get("note", "-") not in ("-", "^"),
                    "%s: a cell that was not drawn says why" % k,
                    "%s: %s" % (r.get("status"), r.get("note", "")[:160]))
            continue
        ok_rows += 1
        lead = r.get("refs_block") != "^"
        if k.startswith("I:"):
            want = m.interior_counts(form)
            wb = want
        else:
            x, y = grid_of(k)
            tx, ty = [int(v) for v in want_tile.split(",")]
            want = m.exterior_counts(x, y)
            whole = m.block_counts(tx, ty, block + ring)
            rb = num(r, "block")
            note = r.get("note", "")
            if note.startswith("count only"):
                # rows without a load: the plugin must place nothing in what the load would have read
                count_only += 1
                t.check(whole[0] == 0 and rb == block + ring,
                        "%s: count only, and the plugin places nothing in its tile" % k,
                        "plugin %d references in the %dx%d block; block column %s" % (whole[0], block + ring, block + ring, rb))
                members.setdefault(want_tile, []).append(k)
                leads.setdefault(want_tile, []).append(k)
                wb = (0, 0)
            elif block > 1 and rb == 1 + ring:
                # a cell of a split tile: opened alone, and the row says why
                over, died = RE_OVER.search(note), RE_DIED.search(note)
                t.check(lead and ((over and int(over.group(1)) == block and int(over.group(2)) == whole[0]
                                   and whole[0] > int(over.group(3)))
                                  or (died and int(died.group(1)) == block)),
                        "%s: opened alone only because its %dx%d tile is over the limit the row names (by the "
                        "plugin's own count) or the walk died on it" % (k, block, block),
                        "plugin: %d references in the tile's load; note: %s" % (whole[0], note[-130:]))
                wb = m.block_counts(x, y, 1 + ring)
            else:
                t.check(rb == block + ring, "%s: loaded as its %dx%d tile%s" % (k, block, block,
                        (" with a ring of %d" % margin) if margin else ""), "block column %s" % r.get("block"))
                members.setdefault(want_tile, []).append(k)
                if lead:
                    leads.setdefault(want_tile, []).append(k)
                wb = whole
        t.check(num(r, "refs") == want[0], "%s: references" % k, "census %s, plugin %d" % (r.get("refs"), want[0]))
        t.check(num(r, "lights_cell") == want[1], "%s: placed lights" % k,
                "census %s, plugin %d" % (r.get("lights_cell"), want[1]))
        if not lead:
            continue
        t.check(num(r, "refs_block") == wb[0], "%s: references in the load" % k,
                "census %s, plugin %d" % (r.get("refs_block"), wb[0]))
        t.check(num(r, "lights_block") == wb[1], "%s: placed lights in the load" % k,
                "census %s, plugin %d" % (r.get("lights_block"), wb[1]))
        if r.get("note", "").startswith("count only"):
            continue
        lit, off, norad, black, amb = (num(r, c) for c in ("lit", "skip_off", "skip_noradius", "skip_black", "ambient_only"))
        t.check(None not in (lit, off, norad, black, amb) and lit + off + norad + black + amb == wb[1],
                "%s: lit + skipped (off, no radius, black, ambient only) = placed" % k,
                "%s + %s + %s + %s + %s vs %d" % (lit, off, norad, black, amb, wb[1]))
    for tile in sorted(members):
        t.check(len(leads.get(tile, [])) == 1,
                "tile %s: one load, so exactly one of its %d rows carries the load's figures" % (tile, len(members[tile])),
                "%d rows carry them" % len(leads.get(tile, [])))
    t.check(ok_rows > 0, "at least one sampled cell was drawn", "%d of %d (%d count only)" % (ok_rows, len(keys), count_only))


def tbk_version(path):
    """The version a .tbk sector file says it is: 4 bytes `TBK1`, then a little-endian 32-bit number."""
    with open(path, "rb") as f:
        head = f.read(8)
    if len(head) < 8 or head[:4] != b"TBK1":
        return None
    return struct.unpack("<I", head[4:8])[0]


def do_bake(t, census_path, key, folder, tbk=4):
    rows = [r for r in read_rows(census_path) if r["key"] == key]
    t.check(len(rows) == 1, "%s: exactly one census row" % key, "%d rows" % len(rows))
    if len(rows) != 1:
        return
    r = rows[0]
    steps = r.get("steps", "").split("+")
    t.check(r.get("status") == "ok" and "census" in steps and "bake" in steps,
            "%s: one visit did the check-up and the bake" % key, "status %s, steps %s" % (r.get("status"), r.get("steps")))
    files = [f for f in glob.glob(os.path.join(folder, "**", "sector_*.tbk"), recursive=True) if os.path.getsize(f) > 0]
    probes = glob.glob(os.path.join(folder, "**", "probes_*.tsv"), recursive=True)
    t.check(len(files) > 0 and num(r, "bake_files") == len(files),
            "%s: the bake's files are on disk, as many as the row says" % key,
            "on disk %d (%d bytes), row %s" % (len(files), sum(os.path.getsize(f) for f in files), r.get("bake_files")))
    versions = sorted(set(str(tbk_version(f)) for f in files))
    t.check(len(files) > 0 and versions == [str(tbk)],
            "%s: every bake file is the version the run asked for (.tbk v%d)" % (key, tbk),
            "the files say v%s" % ", v".join(versions))
    t.check((num(r, "bake_probes") or 0) > 0 and len(probes) == 1 and os.path.getsize(probes[0]) > 0,
            "%s: probes were placed and listed" % key, "row %s probes, %d probe lists" % (r.get("bake_probes"), len(probes)))
    bake, build = num(r, "bake_ms"), num(r, "build_ms")
    t.check(bake is not None and build is not None and 0 < bake <= build,
            "%s: the row carries the bake's time, inside the load's" % key, "bake %s ms of build %s ms" % (bake, build))


# ---- what is placed, by kind of base (no window, the plugin alone)
ITEM_TYPES = (b"ALCH", b"AMMO", b"ARMO", b"BOOK", b"INGR", b"KEYM", b"MISC", b"NOTE", b"WEAP", b"LVLI", b"CMPO", b"OMOD")
ACTOR_TYPES = (b"NPC_", b"LVLN")


def base_types(p):
    """{form: record type} for every record outside the cell and worldspace groups."""
    out = {}
    for top, span in p.tops.items():
        if top in (b"CELL", b"WRLD"):
            continue
        stack = [span]
        while stack:
            s, e = stack.pop()
            for it in p.items(s, e):
                if it[0] == "G":
                    stack.append((it[1] + 24, it[2]))
                else:
                    out[it[4]] = it[2]
    return out


def placed_of(p, span, types):
    """[(kind, x, y)] for every placed record under a cell's child group that is not deleted:
    kind is `item` (a REFR whose base is something picked up), `actor` (an ACHR, or a REFR of an
    actor base) or `rest`; `other` for the placed records that are neither REFR nor ACHR."""
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
            if it[2] in (b"NAVM", b"LAND", b"PGRD") or (it[3] & 0x20):
                continue
            base, x, y = 0, 0.0, 0.0
            for typ, d in p.fields(p.data(it[1], it[3], it[5])):
                if typ == b"NAME" and len(d) >= 4:
                    base = struct.unpack_from("<I", d, 0)[0]
                elif typ == b"DATA" and len(d) >= 12:
                    x, y = struct.unpack_from("<ff", d, 0)
            bt = types.get(base)
            if it[2] == b"ACHR" or bt in ACTOR_TYPES:
                kind = "actor"
            elif it[2] != b"REFR":
                kind = "other"
            elif bt in ITEM_TYPES:
                kind = "item"
            else:
                kind = "rest"
            out.append((kind, x, y))
    return out


def do_share(m, out_path):
    """The share of what is placed, per cell and over the game, by kind of base."""
    types = base_types(m.p)
    kinds = ("item", "actor", "rest", "other")
    per = {}    # key -> {kind: n}

    def add(key, kind):
        per.setdefault(key, dict.fromkeys(kinds, 0))[kind] += 1

    for c in m.interiors:
        key = "I:%08X" % c["form"]
        per.setdefault(key, dict.fromkeys(kinds, 0))
        for kind, _, _ in placed_of(m.p, c["children"], types):
            add(key, kind)
    for (x, y), c in m.exteriors.items():
        key = "E:%s:%d,%d" % (m.world, x, y)
        per.setdefault(key, dict.fromkeys(kinds, 0))
        for kind, _, _ in placed_of(m.p, c["children"], types):
            add(key, kind)
    outside = dict.fromkeys(kinds, 0)
    if m.persistent:
        for kind, px, py in placed_of(m.p, m.persistent["children"], types):
            g = (math.floor(px / CELL_UNITS), math.floor(py / CELL_UNITS))
            if g in m.exteriors:
                add("E:%s:%d,%d" % (m.world, g[0], g[1]), kind)
            else:
                outside[kind] += 1
    if out_path:
        with open(out_path, "w", encoding="utf-8", newline="\n") as f:
            f.write("key\titems\tactors\trest\tother\titem_share\tactor_share\n")
            for key in sorted(per):
                d = per[key]
                n = sum(d.values())
                f.write("%s\t%d\t%d\t%d\t%d\t%.4f\t%.4f\n" % (key, d["item"], d["actor"], d["rest"], d["other"],
                                                             d["item"] / n if n else 0.0, d["actor"] / n if n else 0.0))
    for name, pick in (("interiors", lambda k: k.startswith("I:")), ("exterior cells of " + m.world, lambda k: k.startswith("E:")),
                       ("the whole plan", lambda k: True)):
        cells = [per[k] for k in per if pick(k)]
        tot = {k: sum(d[k] for d in cells) for k in kinds}
        n = sum(tot.values())
        placed = [d for d in cells if sum(d.values()) > 0]
        shares = sorted((d["item"] + d["actor"]) / sum(d.values()) for d in placed)
        q = lambda f: shares[min(len(shares) - 1, int(f * len(shares)))] if shares else 0.0
        print("%s: %d cells (%d with something placed), %d placed: pick-up items %d (%.1f%%), actors %d (%.1f%%), "
              "the rest %d (%.1f%%), other placed records %d (%.1f%%)"
              % (name, len(cells), len(placed), n, tot["item"], 100.0 * tot["item"] / max(n, 1), tot["actor"],
                 100.0 * tot["actor"] / max(n, 1), tot["rest"], 100.0 * tot["rest"] / max(n, 1), tot["other"],
                 100.0 * tot["other"] / max(n, 1)))
        print("   per cell with something placed, items + actors as a share of the cell: median %.1f%%, "
              "nine in ten cells under %.1f%%, highest %.1f%%" % (100 * q(0.5), 100 * q(0.9), 100 * (shares[-1] if shares else 0)))
    print("persistent records standing in no cell of the plan: %s" % ", ".join("%s %d" % (k, outside[k]) for k in kinds))
    return 0


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    mode, plugin = argv[1], argv[2]
    rest, opts = [], {"--world": "Commonwealth", "--interiors": "18", "--center": "-20,7", "--block": "5",
                      "--margin": "0", "--slices": "1", "--only": "", "--plan": "", "--tbk": "4"}
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
    block, margin, slices = int(opts["--block"]), int(opts["--margin"]), int(opts["--slices"])
    if mode == "bake" and len(rest) >= 3:
        t = Tally()
        do_bake(t, rest[0], rest[1], rest[2], int(opts["--tbk"]))
        return t.end()
    m = Model(plugin, opts["--world"])
    cx, cy = [int(v) for v in opts["--center"].split(",")]
    if mode == "sample":
        return do_sample(m, int(opts["--interiors"]), (cx, cy), small, block)
    if mode == "splitkeys":
        return do_splitkeys(m, (cx, cy), block)
    if mode == "bakecell":
        return do_bakecell(m, int(opts["--interiors"]))
    if mode == "share":
        return do_share(m, rest[0] if rest else "")
    t = Tally()
    if mode == "plan" and len(rest) >= 1:
        do_plan(m, t, rest[0], block, slices, set(read_keys(opts["--only"])) if opts["--only"] else None)
    elif mode == "rows" and len(rest) >= 2:
        do_rows(m, t, rest[0], rest[1], opts["--plan"], block, margin, slices)
    elif mode == "check" and len(rest) >= 3:
        do_plan(m, t, rest[1], block, slices, set(read_keys(rest[2])))
        do_rows(m, t, rest[0], rest[2], rest[1], block, margin, slices)
    else:
        print(__doc__)
        return 2
    return t.end()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
