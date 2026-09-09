#!/bin/bash
#
# Adaptive water subdivision: dense at the shoreline, welded, and free of
# T-junctions.
#
# Vanilla LOD water is ONE quad per wet cell -- four vertices 4096 units apart
# -- so no per-vertex channel can describe a coastline however much room the
# format has. This guards the replacement on the four properties that matter,
# in the order they would break:
#
#   1. --water-subdiv 0 still reproduces vanilla EXACTLY (the fallback),
#   2. refinement is ADAPTIVE, not uniform (several quad sizes coexist),
#   3. vertices are WELDED -- no two share a position, so a per-vertex channel
#      has one value per point instead of two that can disagree,
#   4. NO T-JUNCTIONS -- no vertex lies in the interior of another triangle's
#      edge. This is the one that actually costs something: water is flat, so a
#      hanging node cannot crack the geometry, but the moment a channel rides on
#      these vertices the coarse side interpolates linearly across an edge while
#      the fine side passes through a midpoint, and the two disagree wherever
#      the field is non-linear -- which is exactly at the shoreline.
#
# (4) regressed twice while this was being written: first from probing only an
# edge's midpoint (63 survivors), then from fanning a stitched leaf off a corner
# instead of its centre, which made a zero-area triangle and put the full-length
# edge back (6 survivors). Both were silent in the geometry.

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }

checks=0; fails=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then echo "  ok   $1"; else echo "  FAIL $1"; fails=$((fails+1)); fi; }

# chunk 0,0 is the harbour: 12 wet cells with a real coastline through them
for lv in 0 3 4; do
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain 0 0 --dim 4 \
		--water-subdiv $lv -o "$W/s$lv.btr" >/dev/null 2>&1
	[ -f "$W/s$lv.btr" ] || { echo "FAIL: subdiv $lv did not generate"; exit 1; }
done

v0=$("$NS" -no-gui dump "$W/s0.btr" -b 5 2>/dev/null | awk '/Num Vertices/{print $NF; exit}')
t0=$("$NS" -no-gui dump "$W/s0.btr" -b 5 2>/dev/null | awk '/Num Triangles/{print $NF; exit}')
check "subdiv 0 keeps vanilla's one quad per wet cell (48 verts, 24 tris)" \
	"$([ "$v0" = "48" ] && [ "$t0" = "24" ] && echo 1 || echo 0)"

"$PY" - "$NS" "$W/s3.btr" "$W/s4.btr" <<'PYEOF' > "$W/report.txt" 2>&1
import re, subprocess, sys
NS = sys.argv[1]
for f in sys.argv[2:]:
    listing = subprocess.run([NS, '-no-gui', 'list', f], capture_output=True, text=True).stdout
    blk = re.search(r"\[(\d+)\] BSSubIndexTriShape", listing).group(1)
    d = subprocess.run([NS, '-no-gui', 'dump', f, '-b', blk, '-d', '4', '-n', '400000'],
                       capture_output=True, text=True).stdout
    verts = [(round(float(x), 3), round(float(y), 3)) for x, y, z in re.findall(
        r"Vertex\s+<\w+>\s*=\s*X\s*(-?[\d.eE+]+)\s+Y\s*(-?[\d.eE+]+)\s+Z\s*(-?[\d.eE+]+)", d)]
    tris = [(int(a), int(b), int(c)) for a, b, c in
            re.findall(r"<Triangle>\s*=\s*(\d+)\s+(\d+)\s+(\d+)", d)]
    pos = set(verts)
    edges = set()
    for a, b, c in tris:
        for u, v in ((a, b), (b, c), (c, a)):
            edges.add((min(u, v), max(u, v)))
    tj = 0
    for u, v in edges:
        ax, ay = verts[u]; bx, by = verts[v]
        for p in pos:
            if p == (ax, ay) or p == (bx, by):
                continue
            px, py = p
            if (ax == bx and px == ax and min(ay, by) < py < max(ay, by)) or \
               (ay == by and py == ay and min(ax, bx) < px < max(ax, bx)):
                tj += 1
                break
    # quad sizes still visible via the axis-aligned edge lengths present
    lens = sorted({round(abs(verts[u][0] - verts[v][0]) + abs(verts[u][1] - verts[v][1]), 1)
                   for u, v in edges
                   if verts[u][0] == verts[v][0] or verts[u][1] == verts[v][1]})
    print("FILE %s verts %d distinct %d dup %d tj %d sizes %d"
          % (f.rsplit('/', 1)[-1], len(verts), len(pos), len(verts) - len(pos), tj, len(lens)))
PYEOF
cat "$W/report.txt"

for f in s3 s4; do
	line=$(grep "FILE $f.btr" "$W/report.txt")
	# FILE <name> verts N distinct N dup N tj N sizes N -> values, not labels
	dup=$(echo "$line" | awk '{print $8}')
	tj=$(echo "$line" | awk '{print $10}')
	sizes=$(echo "$line" | awk '{print $12}')
	check "$f: vertices are WELDED (no duplicated positions)" \
		"$([ "${dup:-1}" = "0" ] && echo 1 || echo 0)"
	check "$f: NO T-junctions (a channel can cross every edge without a seam)" \
		"$([ "${tj:-1}" = "0" ] && echo 1 || echo 0)"
	check "$f: refinement is ADAPTIVE (several edge lengths coexist)" \
		"$([ "${sizes:-0}" -ge 2 ] && echo 1 || echo 0)"
done

echo "$checks checks, $fails failures"
[ "$fails" = "0" ] && echo PASS || echo FAIL
[ "$fails" = "0" ]
