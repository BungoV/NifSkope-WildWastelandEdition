#!/usr/bin/env python3
"""A stand-in for the cell walk's window, for proving the map-tile runner WITHOUT a NifSkope window
(lane PRTP5-RUNNER, 2026-10-25; tests/spells/cell_census_tiles.sh WALK=...).

Called the way the runner calls tests/spells/cell_census.sh (`--cells KEYS`, environment CENSUS, OUT, ESM,
WORLD, BLOCK, MARGIN, WW_CELL_CENSUS_REFS_MAX), it writes what a window would: the plan file
(<OUT>/plan_1.tsv) and one census row per cell of every unit the keys are in, the counts taken from the
plugin by the checker's own walk. It resumes like the window: units whose cells already have rows are skipped.

  SYNTH_UNITS=K   write at most K units this call, then exit 3 (a window out of time: run again)
  SYNTH_KILL=1    after those K units, exit 137 with a .pending left (a window the memory guard killed)
  SYNTH_RED=drop  leave one cell of each call without a row (the checker must FAIL the tile)

Only the bookkeeping is real here: the rows are right by construction, so this proves the runner (skip,
rerun, check, merge), never the walk.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cell_census_check as cc  # noqa: E402

COLS = ("key kind world x y form edid block tile slice status refs refs_drawn refs_block placements shapes verts tris "
        "lights_cell lights_block lit omni spot skip_off skip_noradius skip_black ambient_only light_types "
        "lights_approx models_loaded models_failed tex_asked tex_missing mats_unreadable far cover_px fb steps "
        "bake_probes bake_files build_ms bake_ms render_ms total_ms rss_mb note").split()
LOAD_COLS = COLS[COLS.index("refs_block"):COLS.index("note")]


def row(m, key, block, tile, lead, counts, load, note):
    r = {c: "-" for c in COLS}
    form, edid = m.keys()[key]
    r.update(key=key, kind=key[0], form="%08X" % form, edid=edid or "-", block=str(block), tile=tile, slice="1/1",
             status="ok", refs=str(counts[0]), lights_cell=str(counts[1]), note=note)
    if key.startswith("E:"):
        r["world"] = key.split(":")[1]
        r["x"], r["y"] = [str(v) for v in cc.grid_of(key)]
    if lead:
        r.update(refs_block=str(load[0]), lights_block=str(load[1]), lit=str(load[1]), skip_off="0",
                 skip_noradius="0", skip_black="0", ambient_only="0", total_ms="1")
    else:
        for c in LOAD_COLS:
            r[c] = "^"
        r.update(refs=str(counts[0]), lights_cell=str(counts[1]))   # the cell's own, never ^
    return "\t".join(r[c] for c in COLS)


def main(argv):
    keys_path = argv[argv.index("--cells") + 1]
    census, out = os.environ["CENSUS"], os.environ["OUT"]
    world = os.environ.get("WORLD", "Commonwealth")
    block, margin = int(os.environ.get("BLOCK", "5")), int(os.environ.get("MARGIN", "0"))
    refs_max = int(os.environ.get("WW_CELL_CENSUS_REFS_MAX", "12000"))
    budget = int(os.environ.get("SYNTH_UNITS", "1000000"))
    ring = 2 * margin
    m = cc.Model(os.environ["ESM"], world)
    named = set(cc.read_keys(keys_path))
    order, cells, place = cc.units_of(m.order(), block, named, 1)
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "plan_1.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# cell census plan (synth stand-in)\n")
        own = m.keys()
        for u in order:
            for k in cells[u]:
                p = place.get(u)
                f.write("%s\t%08X\t%s\t%s\t%s\t%s\n" % (k, own[k][0], own[k][1] or "-", u,
                                                       p[0] if p else "-", p[1] if p else "-"))
    have = set(cc.read_keys(census)) if os.path.isfile(census) else set()
    lines = []
    if not os.path.isfile(census):
        lines += ["# WW cell census v2 (synth stand-in, lane PRTP5-RUNNER); exterior tile=%dx%d" % (block, block),
                  "\t".join(COLS)]
    done = 0
    dropped = os.environ.get("SYNTH_RED") != "drop"
    for u in [u for u in order if u in place]:
        if all(k in have for k in cells[u]):
            continue
        if done >= budget:
            break
        done += 1
        if u.startswith("I:"):
            c = m.interior_counts(int(u[2:], 16))
            lines.append(row(m, u, block, "-", True, c, c, "-"))
            continue
        tile = u.rsplit(":", 1)[1]
        tx, ty = [int(v) for v in tile.split(",")]
        whole = m.block_counts(tx, ty, block + ring)
        for i, k in enumerate(cells[u]):
            if k in have:
                continue
            if not dropped:
                dropped = True
                continue
            x, y = cc.grid_of(k)
            c = m.exterior_counts(x, y)
            if whole[0] > refs_max:
                lines.append(row(m, k, 1 + ring, tile, True, c, m.block_counts(x, y, 1 + ring),
                                 "opened alone: the %dx%d tile holds %d references, over the walk's limit of %d"
                                 % (block, block, whole[0], refs_max)))
            elif whole[0] == 0:
                lines.append(row(m, k, block + ring, tile, i == 0, c, whole,
                                 "count only: nothing is placed in the tile, so the scene was not built" if i == 0 else "^"))
            else:
                lines.append(row(m, k, block + ring, tile, i == 0, c, whole, "-" if i == 0 else "^"))
    with open(census, "a", encoding="utf-8", newline="\n") as f:
        for line in lines:
            f.write(line + "\n")
    left = sum(1 for u in order if u in place and not all(k in have or k in {l.split("\t")[0] for l in lines}
                                                          for k in cells[u]))
    print("synth: %d units written, %d left" % (done, left))
    if os.environ.get("SYNTH_KILL") == "1":
        with open(census + ".pending", "w", encoding="utf-8", newline="\n") as f:
            f.write("block %d\n" % block)
        return 137
    return 3 if left else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
