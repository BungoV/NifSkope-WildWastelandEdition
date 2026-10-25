#!/usr/bin/env python3
"""The whole-game cell walk cut into MAP TILES that run one at a time (lane PRTP5-RUNNER, 2026-10-25).

The cell walk (src/cellcensustest.cpp, gate tests/spells/cell_census.sh) already loads the game a UNIT at a
time (an interior, or a BxB exterior tile). Its --whole pass was one run of 4.5-6.2 days that has never run.
This cuts that pass into MAP TILES: a run of whole units in plan order, sized by the projected seconds, each
with its own cell list, census file and check. tests/spells/cell_census_tiles.sh runs them one at a time.

  plan   <plugin> <root> [--world W] [--block 5] [--margin 0] [--refs-max 6500] [--target-s 1800]
         [--rss-max 4000] [--steps census|census,bake] [--model high|low] [--no-skip-water]
         cuts the game into map tiles: <root>/tiles.tsv (one line a tile, with its projected seconds and
         memory), <root>/tiles/<id>/cells.keys, <root>/all.keys, <root>/knobs.txt; prints the projection.
         With a bake step, OPEN WATER is skipped (lane PRTP6, bungo 10-25: water is dynamic, so the open sea
         gets no PRTP): an exterior unit none of whose cells has LAND above its water height and whose own
         block places nothing, measured from the plugin (cell_census_check.py Model.open_water). Those units
         get no tile; <root>/skipped.tsv names each with its reason. --no-skip-water bakes every unit (the red).
  pick   <plugin> <root> [same options]
         a PROOF plan of three small tiles (a few small interiors; one exterior unit with something placed;
         one exterior unit with nothing placed) for the next hold
  status <root> [--exe EXE]
         one line a tile: done / stale (its census or the exe changed since it was checked) / partial (rows
         but no pass: a killed or unfinished tile, it reruns) / failed (gave up after --tries) / todo
  next   <root> [--exe EXE]   the id of the next tile to run (empty = nothing left); `stale` counts as left
  mark   <root> <id> <check output> [--exe EXE] [--tries 3]
         after a tile's run: if its check output ends PASS, writes <tile>/DONE (exe sha1, knobs, census sha1,
         rows, the check line); otherwise counts an attempt, and after --tries attempts with no new rows FAILED
  merge  <root> <out.tsv> [--partial]
         joins the DONE tiles' census files in plan order into one census (and <out>.keys, the cells they
         cover). It joins and never tidies; the checker (cell_census_check.py rows) proves one row per cell.
         Without --partial it refuses while any tile is not done.

Everything here is offline (no NifSkope window). Projected seconds use measured fits; see COST below.
"""

import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cell_census_check as cc  # noqa: E402  the checker's own plugin walk: the tiles see what the checker sees

# ---- COST: measured fits (seconds), two exes. Both are inferred for a load this file has not seen.
# high = exe of 10-02 (lane PRTP5, census only, first lock slot): interior 3.5 + 19.0 ms a reference (17
#        interiors); exterior load 1.51 + 23.3 ms a reference (9 loads incl. Sanctuary 5x5); lone cell 4.8 s
#        at least; a load with nothing placed 0.003 s (no scene is built).
# low  = exe of 10-13 (lane BAKEALL1, census + bake, 3x3 + ring 1): exterior 15.7 + 8.71 ms a reference
#        loaded; one-cell load 17.5 + 4.70 ms; interior 2.4 s under 300 references, else 11.9 + 3.93 ms.
#        Loading was 70-80% of it, so census alone is taken as 0.75 x that (inferred, not measured).
# A window costs 37 s to open and close (10-02 hold 3: two windows with nothing to load in 75 s).
WINDOW_S = 37.0
# Memory, 10-13 private bytes of a fresh window after one load: 3.4 GB at 1.8k references, 9.4-10.1 GB at
# 5.0-6.1k: about 0.6 GB + 1.56 MB a reference loaded. 10-02: a window does not hand memory back between
# loads, so a window's figure grows by each load (taken here as the worst case: the sum).
MEM_BASE_MB, MEM_PER_REF_MB = 600.0, 1.56
GUARD_MB = 12000.0   # withlock's memory guard: kills the run past 12 GB private


def cost(model, kind, refs, steps):
    bake = "bake" in steps
    if model == "high":
        if refs == 0:
            # a bake still builds the far soup and bakes the sea: 11.6 s measured (BAKEALL1, tile -80,-80)
            return 11.6 if bake and kind != "I" else 0.003
        if kind == "I":
            s = 3.5 + 0.0190 * refs
        elif kind == "C":
            s = max(4.8, 1.51 + 0.0233 * refs)
        else:
            s = 1.51 + 0.0233 * refs
        return s + (9.0 if bake and kind != "I" else 0.0)   # bake 7-11 s a tile (10-13)
    if refs == 0 and not bake:
        return 0.003
    if kind == "I":
        s = 2.4 if refs < 300 else 11.9 + 0.00393 * refs
    elif kind == "C":
        s = 17.5 + 0.00470 * refs
    else:
        s = 15.7 + 0.00871 * refs
    return s if bake else 0.75 * s


class Opts:
    def __init__(self, argv):
        self.world, self.block, self.margin, self.refs_max = "Commonwealth", 5, 0, 6500
        self.target_s, self.rss_max, self.steps, self.model = 1800.0, 4000.0, "census", "high"
        self.exe, self.tries, self.partial = "", 3, False
        self.skip_water = None   # None = on when the steps bake; --no-skip-water = off (the red)
        self.skip_red = ""       # --skip-red land: also skip the first unit that is NOT open water (checker red)
        self.rest = []
        i = 0
        while i < len(argv):
            a = argv[i]
            nx = argv[i + 1] if i + 1 < len(argv) else ""
            if a == "--world": self.world = nx; i += 1
            elif a == "--block": self.block = int(nx); i += 1
            elif a == "--margin": self.margin = int(nx); i += 1
            elif a == "--refs-max": self.refs_max = int(nx); i += 1
            elif a == "--target-s": self.target_s = float(nx); i += 1
            elif a == "--rss-max": self.rss_max = float(nx); i += 1
            elif a == "--steps": self.steps = nx; i += 1
            elif a == "--model": self.model = nx; i += 1
            elif a == "--exe": self.exe = nx; i += 1
            elif a == "--tries": self.tries = int(nx); i += 1
            elif a == "--partial": self.partial = True
            elif a == "--no-skip-water": self.skip_water = False
            elif a == "--skip-water": self.skip_water = True
            elif a == "--skip-red": self.skip_red = nx; i += 1
            else: self.rest.append(a)
            i += 1

    def exe_sha(self):
        if not hasattr(self, "_exe_sha"):
            self._exe_sha = sha1(self.exe) if self.exe and os.path.isfile(self.exe) else "-"
        return self._exe_sha

    def skipping(self):
        return self.skip_water if self.skip_water is not None else "bake" in self.steps

    def knobs(self):
        return "block=%d margin=%d refs_max=%d steps=%s world=%s skip_water=%d" % (
            self.block, self.margin, self.refs_max, self.steps, self.world, 1 if self.skipping() else 0)


def units(m, o):
    """[(unit id, kind I|E, [keys], [(load kind, refs)])] in the walk's plan order (interiors first)."""
    order, cells, _ = cc.units_of(m.order(), o.block, None, 1)
    ring = 2 * o.margin
    out = []
    for u in order:
        keys = cells[u]
        if u.startswith("I:"):
            out.append((u, "I", keys, [("I", m.interior_counts(int(u[2:], 16))[0])]))
            continue
        tx, ty = [int(v) for v in u.rsplit(":", 1)[1].split(",")]
        whole = m.block_counts(tx, ty, o.block + ring)[0]
        if whole > o.refs_max:
            # the walk's split rule: over the limit, every cell of the tile is opened alone
            loads = [("C", m.block_counts(*cc.grid_of(k), 1 + ring)[0]) for k in keys]
        else:
            loads = [("E", whole)]
        out.append((u, "E", keys, loads))
    return out


def project(loads, o, model):
    """(seconds, windows, peak MB, loads past the guard) of a run of loads in one chain of windows."""
    s, windows, mem, peak, over = 0.0, 1, MEM_BASE_MB, MEM_BASE_MB, 0
    for kind, refs in loads:
        s += cost(model, kind, refs, o.steps)
        if refs == 0 and "bake" not in o.steps:
            continue
        mem += MEM_PER_REF_MB * refs
        peak = max(peak, mem)
        over += 1 if mem > GUARD_MB else 0
        if mem > o.rss_max:   # the window stops starting loads past WW_CELL_CENSUS_RSS_MAX; the next one opens
            windows += 1
            mem = MEM_BASE_MB
    return s + WINDOW_S * windows, windows, peak, over


def write_plan(root, o, tiles, m):
    os.makedirs(os.path.join(root, "tiles"), exist_ok=True)
    with open(os.path.join(root, "knobs.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write(o.knobs() + "\n")
    allkeys = []
    hi_tot = lo_tot = 0.0
    with open(os.path.join(root, "tiles.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# map tiles of the cell walk (tests/spells/cell_census_tiles.py); %s; rss_max=%d MB; seconds: "
                "high = exe 10-02 fits, low = exe 10-13 fits (inferred for census alone)\n" % (o.knobs(), o.rss_max))
        f.write("id\tkind\tunits\tcells\tloads\tsplit_units\trefs_read\tbiggest_load\tproj_s_high\tproj_s_low"
                "\twindows\tpeak_mb\tloads_past_guard\tfirst_unit\tlast_unit\n")
        for tid, kind, us in tiles:
            loads = [l for u in us for l in u[3]]
            hi = project(loads, o, "high")
            lo = project(loads, o, "low")
            hi_tot += hi[0]
            lo_tot += lo[0]
            keys = [k for u in us for k in u[2]]
            allkeys += keys
            d = os.path.join(root, "tiles", tid)
            os.makedirs(d, exist_ok=True)
            with open(os.path.join(d, "cells.keys"), "w", encoding="utf-8", newline="\n") as kf:
                kf.write("# map tile %s: %d units, %d cells (every cell of every unit)\n" % (tid, len(us), len(keys)))
                for k in keys:
                    kf.write(k + "\n")
            f.write("%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%.0f\t%.0f\t%d\t%.0f\t%d\t%s\t%s\n" % (
                tid, kind, len(us), len(keys), len(loads), sum(1 for u in us if u[3] and u[3][0][0] == "C"),
                sum(r for _, r in loads), max([r for _, r in loads] or [0]), hi[0], lo[0], hi[1], hi[2], hi[3],
                us[0][0], us[-1][0]))
    with open(os.path.join(root, "all.keys"), "w", encoding="utf-8", newline="\n") as f:
        for k in allkeys:
            f.write(k + "\n")
    return hi_tot, lo_tot


def open_water_units(m, o, us):
    """[(unit, keys, reason)] of the exterior units the plan skips as open water, in plan order."""
    if not o.skipping():
        return []
    out, red = [], o.skip_red == "land"
    for u, kind, keys, _ in us:
        if kind != "E":
            continue
        ok, why = m.open_water(keys, u.rsplit(":", 1)[1], o.block)
        if ok:
            out.append((u, keys, why))
        elif red and keys:
            out.append((u, keys, "RED (not open water: %s)" % why))
            red = False
    return out


def write_skipped(root, skipped):
    with open(os.path.join(root, "skipped.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# units the plan gives no tile (lane PRTP6): open water gets no PRTP; measured from the plugin\n")
        f.write("unit\tcells\treason\n")
        for u, keys, why in skipped:
            f.write("%s\t%d\t%s\n" % (u, len(keys), why))


def read_skipped(root):
    out = []
    path = os.path.join(root, "skipped.tsv")
    if os.path.isfile(path):
        for line in open(path, encoding="utf-8"):
            p = line.rstrip("\r\n").split("\t")
            if len(p) >= 3 and p[0][:2] == "T:":
                out.append((p[0], int(p[1]), p[2]))
    return out


def do_plan(plugin, root, o, pick=False):
    m = cc.Model(plugin, o.world)
    us = units(m, o)
    skipped = open_water_units(m, o, us)
    gone = set(u for u, _, _ in skipped)
    all_s = {"high": 0.0, "low": 0.0}
    for u in us:
        for model in all_s:
            all_s[model] += sum(cost(model, k, r, o.steps) for k, r in u[3]) if u[0] in gone else 0.0
    us = [u for u in us if u[0] not in gone]
    os.makedirs(root, exist_ok=True)
    write_skipped(root, skipped)
    tiles = []
    if pick:
        ints = [u for u in us if u[1] == "I" and 40 <= u[3][0][1] <= 300][:3]
        ext = [u for u in us if u[1] == "E" and len(u[3]) == 1 and 1 <= u[3][0][1] <= 400 and len(u[2]) == o.block ** 2][:1]
        empty = [u for u in us if u[1] == "E" and u[3] and u[3][0][1] == 0 and len(u[2]) == o.block ** 2][:1]
        tiles = [("P1", "I", ints), ("P2", "E", ext), ("P3", "E", empty)]
    else:
        for kind in ("I", "E"):
            cur, cur_s, n = [], 0.0, 0
            for u in (x for x in us if x[1] == kind):
                s = sum(cost(o.model, k, r, o.steps) for k, r in u[3])
                if cur and cur_s + s > o.target_s:
                    n += 1
                    tiles.append(("%s%03d" % (kind, n), kind, cur))
                    cur, cur_s = [], 0.0
                cur.append(u)
                cur_s += s
            if cur:
                n += 1
                tiles.append(("%s%03d" % (kind, n), kind, cur))
    hi, lo = write_plan(root, o, tiles, m)
    # lane PRTP6: only a whole-game plan asks the merge to prove every unit walked or skipped (a pick is a sample)
    whole = os.path.join(root, "plan.whole")
    if pick:
        if os.path.exists(whole):
            os.remove(whole)
    else:
        with open(whole, "w", encoding="utf-8", newline="\n") as f:
            f.write("every unit of %s is in tiles.tsv or skipped.tsv\n" % o.world)
    nint = sum(1 for t in tiles if t[1] == "I")
    print("plan: %d map tiles (%d of interiors, %d of exterior units), %d units, %d cells; %s"
          % (len(tiles), nint, len(tiles) - nint, sum(len(t[2]) for t in tiles),
             sum(len(u[2]) for t in tiles for u in t[2]), o.knobs()))
    print("projected, one window at a time: high %.1f h (%.2f days), low %.1f h (%.2f days)"
          % (hi / 3600, hi / 86400, lo / 3600, lo / 86400))
    print("open water, skipped: %d units, %d cells (skipped.tsv; their loads would have cost %.1f h high, %.1f h low)%s"
          % (len(skipped), sum(len(k) for _, k, _ in skipped), all_s["high"] / 3600, all_s["low"] / 3600,
             "" if o.skipping() else "; skipping is OFF (--no-skip-water or no bake step)"))
    return 0


def read_tiles(root):
    out = []
    with open(os.path.join(root, "tiles.tsv"), encoding="utf-8") as f:
        cols = None
        for line in f:
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            p = line.split("\t")
            if p[0] == "id":
                cols = p
                continue
            out.append(dict(zip(cols, p)))
    return out


def sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def rows_of(path):
    if not os.path.isfile(path):
        return 0
    with open(path, encoding="utf-8", errors="replace") as f:
        return sum(1 for line in f if line[:2] in ("I:", "E:"))


def read_kv(path):
    out = {}
    if os.path.isfile(path):
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                k, _, v = line.rstrip("\r\n").partition(" ")
                out[k] = v
    return out


def state(root, t, o):
    d = os.path.join(root, "tiles", t["id"])
    census = os.path.join(d, "census.tsv")
    done = read_kv(os.path.join(d, "DONE"))
    knobs = open(os.path.join(root, "knobs.txt"), encoding="utf-8").read().strip()
    if done:
        why = []
        if not os.path.isfile(census) or sha1(census) != done.get("census_sha1"):
            why.append("its census changed after the check")
        if done.get("knobs") != knobs:
            why.append("the knobs changed")
        if o.exe_sha() != "-" and done.get("exe_sha1") != o.exe_sha():
            why.append("the exe changed")
        return ("stale", "; ".join(why)) if why else ("done", done.get("check", ""))
    if os.path.isfile(os.path.join(d, "FAILED")):
        return "failed", open(os.path.join(d, "FAILED"), encoding="utf-8").read().strip()
    n = rows_of(census)
    if n:
        return "partial", "%d of %s rows; reruns (the window resumes, a load it died on opens cell by cell)" % (n, t["cells"])
    return "todo", ""


def do_status(root, o):
    tiles = read_tiles(root)
    tally, left_hi, left_lo = {}, 0.0, 0.0
    for t in tiles:
        st, why = state(root, t, o)
        tally[st] = tally.get(st, 0) + 1
        if st != "done":
            left_hi += float(t["proj_s_high"])
            left_lo += float(t["proj_s_low"])
        print("%s\t%s\t%s cells\tproj %s s (low %s)\t%s" % (t["id"], st, t["cells"], t["proj_s_high"], t["proj_s_low"], why))
    print("tiles: %s; left: %.1f h high, %.1f h low" % (", ".join("%s %d" % kv for kv in sorted(tally.items())),
                                                       left_hi / 3600, left_lo / 3600))
    sk = read_skipped(root)
    print("open water, skipped: %d units, %d cells (no tile, by plan)" % (len(sk), sum(n for _, n, _ in sk)))
    return 0


def do_next(root, o):
    for t in read_tiles(root):
        if state(root, t, o)[0] in ("todo", "partial", "stale"):
            print(t["id"])
            return 0
    return 0


def do_mark(root, tid, check_out, o):
    d = os.path.join(root, "tiles", tid)
    census = os.path.join(d, "census.tsv")
    lines = [l.rstrip("\r\n") for l in open(check_out, encoding="utf-8", errors="replace")] if os.path.isfile(check_out) else []
    last = lines[-1] if lines else "(no check output)"
    n = rows_of(census)
    if last.startswith("cell_census_check:") and last.endswith("PASS") and n > 0:
        knobs = open(os.path.join(root, "knobs.txt"), encoding="utf-8").read().strip()
        with open(os.path.join(d, "DONE"), "w", encoding="utf-8", newline="\n") as f:
            f.write("census_sha1 %s\nrows %d\nknobs %s\nexe_sha1 %s\ncheck %s\n"
                    % (sha1(census), n, knobs, o.exe_sha(), last))
        print("mark %s: DONE (%d rows; %s)" % (tid, n, last))
        return 0
    att = os.path.join(d, "attempts")
    hist = [l.split() for l in open(att, encoding="utf-8")] if os.path.isfile(att) else []
    with open(att, "a", encoding="utf-8", newline="\n") as f:
        f.write("%d %s\n" % (n, last.replace(" ", "_")[:200]))
    tail = [int(h[0]) for h in hist[-(o.tries - 1):]] + [n] if o.tries > 1 else [n]
    if len(hist) + 1 >= o.tries and len(set(tail)) == 1:
        with open(os.path.join(d, "FAILED"), "w", encoding="utf-8", newline="\n") as f:
            f.write("%d attempts made no new rows (%d rows); last check: %s\n" % (len(hist) + 1, n, last))
        print("mark %s: FAILED after %d attempts with no new rows (%s)" % (tid, len(hist) + 1, last))
        return 1
    print("mark %s: not done (attempt %d, %d rows; %s); it reruns" % (tid, len(hist) + 1, n, last))
    return 1


def do_merge(root, out, o):
    tiles = read_tiles(root)
    comment, columns, rows, keys, used, missing = None, None, [], [], [], []
    for t in tiles:
        d = os.path.join(root, "tiles", t["id"])
        if state(root, t, o)[0] != "done":
            missing.append(t["id"])
            continue
        with open(os.path.join(d, "census.tsv"), encoding="utf-8", errors="replace") as f:
            for line in f:
                line = line.rstrip("\r\n")
                if not line:
                    continue
                if line.startswith("#"):
                    comment = comment or line
                elif line.startswith("key\t"):
                    if columns is None:
                        columns = line
                    elif line != columns:
                        print("REFUSED: tile %s does not have the columns of the first tile" % t["id"])
                        return 2
                else:
                    rows.append(line)
        keys += [l.strip() for l in open(os.path.join(d, "cells.keys"), encoding="utf-8") if l[:2] in ("I:", "E:")]
        used.append(t["id"])
    if missing and not o.partial:
        print("REFUSED: %d of %d tiles are not done (%s%s)" % (len(missing), len(tiles), ", ".join(missing[:6]),
                                                               " ..." if len(missing) > 6 else ""))
        return 2
    if columns is None:
        print("REFUSED: no tile is done")
        return 2
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write((comment or "# WW cell census v2") + "   [joined from %d map tiles]\n" % len(used))
        f.write(columns + "\n")
        for line in rows:
            f.write(line + "\n")
    with open(out + ".keys", "w", encoding="utf-8", newline="\n") as f:
        for k in keys:
            f.write(k + "\n")
    print("joined %d map tiles into %s: %d rows%s" % (len(used), out, len(rows),
                                                     ("; %d tiles left out (--partial)" % len(missing)) if missing else ""))
    return 0


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    mode = argv[1]
    o = Opts(argv[2:])
    r = o.rest
    if mode in ("plan", "pick") and len(r) >= 2:
        return do_plan(r[0], r[1], o, pick=(mode == "pick"))
    if mode == "status" and r:
        return do_status(r[0], o)
    if mode == "next" and r:
        return do_next(r[0], o)
    if mode == "mark" and len(r) >= 3:
        return do_mark(r[0], r[1], r[2], o)
    if mode == "merge" and len(r) >= 2:
        return do_merge(r[0], r[1], o)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
