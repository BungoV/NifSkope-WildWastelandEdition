#!/usr/bin/env python
"""TERRAINFIX step 5: lodt_write.sh gains

  * an exit code (the python block's RESULT line was printed and thrown away,
    so the harness passed whatever it measured);
  * the header version 2 fields, read back from the FILE and compared with what
    the writer SAID it wrote;
  * the version 1 fallback, proved exact: same bytes, shifted by the 8 the two
    new fields occupy;
  * the water plane's two cases, inheriting and explicit;
  * the landless-cell rule, against a freshly baked shadow heightmap of a
    worldspace that HAS landless cells -- which the Commonwealth has not, which
    is why nothing caught it.
"""

P = 'tests/spells/lodt_write.sh'
b = open(P, 'rb').read().decode('utf-8')
cr0 = b.count('\r')

old_tail = '''print("RESULT %s" % ("PASS" if not fails else "FAIL"))
PYEOF
'''
assert b.count(old_tail) == 1

new_tail = r'''print("RESULT %s" % ("PASS" if not fails else "FAIL"))
sys.exit(1 if fails else 0)
PYEOF
rc=$?
# The block above printed its verdict and threw the exit code away until
# 2026-09-09: `"$PY" - <<EOF` was the last command of the script, python never
# called sys.exit, and the harness reported PASS on any measurement at all.

say() { echo "  $*"; }
ok2() { echo "  ok   $1"; }
bad2() { echo "  FAIL $1"; rc=1; }

# --- the header's version, and the worldspace water it carries --------
#
# A cell whose water type reads 0xFFFF is inheriting the WORLDSPACE's water,
# and until version 2 the file said which cells inherit without ever saying
# WHAT they inherit: the default WATR form is deliberately not interned in the
# WATR table (that is what keeps "inherited" distinguishable from "explicitly
# this type"), so it appeared nowhere. Version 2 appends the default height and
# form AFTER the ten section offsets, so every offset a version 1 reader uses
# is still at the byte it was.
echo "== header version 2 and the worldspace default water =="
CENSUS="$(grep -o 'worldspace default height [^)]*' "$W/out.txt" | head -1)"
say "the writer says: $CENSUS"
WH="$(echo "$CENSUS" | awk '{print $4}')"
WT="$(echo "$CENSUS" | awk '{print $6}')"

# the same worldspace at version 1: the fallback must be EXACT, not similar
WW_LODT_VERSION=1 "$NS" -no-gui lodgen "$ESM" --worldspace 3C --lodt "$W/v1" \
	> "$W/v1.txt" 2>&1 || { echo "FAIL: the version 1 fallback write failed"; rc=1; }
F1="$W/v1/Terrain/Commonwealth.lodt"

"$PY" - "$F" "$F1" "${WH:-nan}" "${WT:-none}" <<'PYEOF2'
import struct, sys
b = open(sys.argv[1], 'rb').read()
fails = []


def check(name, cond):
    print("  %s %s" % ("ok  " if cond else "FAIL", name))
    if not cond:
        fails.append(name)


check("the written header says version 2", struct.unpack_from('<I', b, 4)[0] == 2)
off = struct.unpack_from('<QQQQQQQQQQ', b, 0x48)
check("the first section starts at 0xA0, so the header really grew",
      off[0] == 0xA0)
dh = struct.unpack_from('<f', b, 0x98)[0]
dt = struct.unpack_from('<I', b, 0x9C)[0]
print("  file says default water height %g type %08x" % (dh, dt))
# TELEMETRY ECHOES TRUTH: the census line is compared against the BYTES, not
# trusted as a statement about them.
sh, st = sys.argv[3], sys.argv[4]
check("the file's default water height is what the writer reported (%s)" % sh,
      abs(dh - float(sh)) < 1e-3)
check("the file's default water form is what the writer reported (%s)" % st,
      ("%08x" % dt) == st.lower())
check("the Commonwealth has a default water type (WRLD NAM2)", dt != 0)

# the water plane's two cases must BOTH occur, or 0xFFFF is untested
minX, minY, maxX, maxY = struct.unpack_from('<iiii', b, 8)
watrN = struct.unpack_from('<I', b, 0x34)[0]
cellsX, cellsY = maxX - minX + 1, maxY - minY + 1
oCell = off[4]
inherit = explicit = nowater = 0
bad = 0
for i in range(cellsX * cellsY):
    wh, wt, fl = struct.unpack_from('<fHH', b, oCell + i * 16 + 8)
    if not (fl & 1):
        nowater += 1
        continue
    if wt == 0xFFFF:
        inherit += 1
    else:
        explicit += 1
        if wt >= watrN:
            bad += 1
print("  water cells: %d inherit the worldspace type, %d name their own, "
      "%d have no water" % (inherit, explicit, nowater))
check("cells that INHERIT the worldspace water exist", inherit > 0)
check("cells that name their OWN water type exist", explicit > 0)
check("every explicit water type indexes the WATR table (%d entries)" % watrN,
      bad == 0)

# the version 1 fallback, exact at its off value
try:
    v1 = open(sys.argv[2], 'rb').read()
except IOError:
    v1 = b''
if v1:
    o1 = struct.unpack_from('<QQQQQQQQQQ', v1, 0x48)
    check("version 1 still writes a 152-byte header (first section at 0x98)",
          struct.unpack_from('<I', v1, 4)[0] == 1 and o1[0] == 0x98)
    check("every section offset moved by exactly the 8 bytes appended",
          all(off[k] == o1[k] + 8 for k in range(10)))
    check("and NOTHING ELSE moved: the two files are identical past the header",
          b[0xA0:] == v1[0x98:])
else:
    check("the version 1 fallback file exists", False)
print("RESULT %s" % ("PASS" if not fails else "FAIL"))
sys.exit(1 if fails else 0)
PYEOF2
[ $? = 0 ] || rc=1

# --- the landless cell, against the shadow heightmap ------------------
#
# The .lodt and the HeightMap.dds are read by FO4CS as ONE surface -- terrain
# from the first, far shadows from the second -- so a sample they disagree on
# is a ridge that casts a shadow without being drawn (2026-09-05c). The
# heightmap reproduces Bethesda's own Commonwealth_fine byte for byte, so where
# they differ it is this writer that is wrong.
#
# The Commonwealth cannot test it: all 36,864 of its cells carry LAND. Every
# worldspace with a HOLE in its landscape was wrong until 2026-09-09 --
# DiamondCity by 167,936 texels of 172,032, NukaWorldAmphitheater by 97,
# DLC03FarHarbor by 62 -- because a landless cell was written as flat height
# ZERO and lost the row and column its neighbours share with it.
NWESM="${NWESM:-$(dirname "$ESM")/DLCNukaWorld.esm}"
echo "== a worldspace with landless cells (NukaWorldAmphitheater) =="
if [ ! -f "$NWESM" ]; then
	say "SKIPPED: no DLCNukaWorld.esm at $NWESM"
else
	"$NS" -no-gui lodgen "$NWESM" --worldspace 52931 --lodt "$W/nwa" \
		> "$W/nwa.txt" 2>&1 || { bad2 "the NukaWorldAmphitheater .lodt writes"; }
	"$NS" -no-gui lodgen "$NWESM" --worldspace 52931 --heightmap "$W/hm" \
		> "$W/hm.txt" 2>&1 || { bad2 "its shadow heightmap bakes"; }
	L2="$W/nwa/Terrain/NukaWorldAmphitheater.lodt"
	H2="$(ls "$W/hm"/Textures/Terrain/NukaWorldAmphitheater/*.dds 2>/dev/null | head -1)"
	if [ -s "$L2" ] && [ -n "$H2" ]; then
		"$PY" - "$L2" "$H2" <<'PYEOF3'
import struct, sys, zlib
f = open(sys.argv[1], 'rb').read()
minX, minY, maxX, maxY = struct.unpack_from('<iiii', f, 8)
spc, be, levels = struct.unpack_from('<III', f, 0x18)
quant = struct.unpack_from('<f', f, 0x2C)[0]
off = struct.unpack_from('<QQQQQQQQQQ', f, 0x48)
oCell, oDir, oData = off[4], off[7], off[8]
cellsX, cellsY = maxX - minX + 1, maxY - minY + 1
gw, gh = cellsX * spc, cellsY * spc
H = [[0] * gw for _ in range(gh)]
idx = 0
for L in range(levels - 1, -1, -1):
    s = 1 << L
    bx, by = (cellsX + s - 1) // s, (cellsY + s - 1) // s
    for j in range(by):
        for i in range(bx):
            o, csz, usz = struct.unpack_from('<QII', f, oDir + idx * 16)
            idx += 1
            raw = zlib.decompress(f[o:o + csz])
            n = be * be if L == levels - 1 else be * be * 3 // 4
            v = struct.unpack_from('<%dH' % n, raw, 0)
            ox, oy = i * be, j * be
            k = 0
            if L == levels - 1:
                for y in range(be):
                    for x in range(be):
                        gy, gx = (oy + y) * s, (ox + x) * s
                        if gy < gh and gx < gw:
                            H[gy][gx] = v[k]
                        k += 1
            else:
                for y in range(0, be, 2):
                    for x in range(0, be, 2):
                        for (dy, dx) in ((0, 1), (1, 0), (1, 1)):
                            gy, gx = (oy + y + dy) * s, (ox + x + dx) * s
                            if gy < gh and gx < gw:
                                H[gy][gx] = v[k]
                            k += 1
d = open(sys.argv[2], 'rb').read()
h, w = struct.unpack_from('<II', d, 12)
po = 148 if d[84:88] == b'DX10' else 128
D = struct.unpack_from('<%dH' % (w * h), d, po)
fails = []


def check(name, cond):
    print("  %s %s" % ("ok  " if cond else "FAIL", name))
    if not cond:
        fails.append(name)


check("the .lodt grid and the heightmap are the same size (%dx%d)" % (w, h),
      (w, h) == (gw, gh))
n = 0
first = None
if (w, h) == (gw, gh):
    for y in range(gh):
        row = D[(gh - 1 - y) * gw:(gh - 1 - y) * gw + gw]   # the DDS is north-up
        for x in range(gw):
            if row[x] != H[y][x]:
                n += 1
                if first is None:
                    first = (minX + x // spc, minY + y // spc, x % spc, y % spc,
                             H[y][x], row[x])
landless = sum(1 for i in range(cellsX * cellsY)
               if not (struct.unpack_from('<H', f, oCell + i * 16 + 14)[0] & 2))
print("  %d of %d cells carry no LAND record" % (landless, cellsX * cellsY))
if first:
    print("  first difference: cell (%d,%d) col %d row %d  lodt %d  heightmap %d"
          % first)
check("the worldspace HAS landless cells, so this can fail", landless > 0)
check("every texel agrees with the shadow heightmap (%d differ)" % n, n == 0)
print("RESULT %s" % ("PASS" if not fails else "FAIL"))
sys.exit(1 if fails else 0)
PYEOF3
		[ $? = 0 ] || rc=1
	else
		bad2 "both the .lodt and the heightmap were produced"
	fi
fi

[ "$rc" = 0 ] && echo PASS || echo FAIL
exit $rc
'''

b = b.replace(old_tail, new_tail)
assert b.count('\r') == cr0
open(P, 'wb').write(b.encode('utf-8'))
print('%s patched, CR %d (unchanged)' % (P, cr0))
