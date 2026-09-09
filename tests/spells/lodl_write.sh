#!/bin/bash
#
# The .lodl whole-worldspace landscape writer.
#
# Was lodt_write.sh until bungo's 2026-09-09 ruling: the landscape file is
# .lodl now and .lodt names the terrain texture sheets. The BYTES did not
# change with the name -- the magic still reads LODT on disk -- and check 1
# below is what pins that.
#
# The gate is a ROUND TRIP against the ESM: reconstruct heights by walking the
# progressive pyramid and compare them with the LAND corners the generator read.
# Nothing else proves a format writer -- a file can be self-consistently wrong,
# which is exactly how the DDS header offsets passed their own reader all day
# before a shipped file disproved them.
#
# Checks, in the order they would break:
#   1. the header is self-consistent (size field == actual size)
#   2. the block directory has exactly the entries the level maths predicts
#   3. heights round-trip EXACTLY -- the 8-unit quantum is VHGT's own, so any
#      difference at all is a bug, not rounding
#   4. the sparse sections are sparse (quadrant layers ~10%), which is what the
#      absence of a present-bitmask relies on

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

# The tool's exit code IS a check: it reads its own file back through the
# independent reader and round-trips heights, alpha words and colour against
# the ESM. An earlier version of this script threw that away with >/dev/null.
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W" > "$W/out.txt" 2>&1 \
	|| { echo "FAIL: lodgen's own round-trip against the ESM failed:"; cat "$W/out.txt"; exit 1; }
grep -E 'alpha words|cross-check' "$W/out.txt" | sed 's/^/  ok   tool: /'
F="$W/Terrain/Commonwealth.lodl"
[ -s "$F" ] || { echo "FAIL: no .lodl written"; exit 1; }

"$PY" - "$F" <<'PYEOF'
import struct, sys, zlib
b = open(sys.argv[1], 'rb').read()
fails = []
def check(name, cond):
    print("  %s %s" % ("ok  " if cond else "FAIL", name))
    if not cond: fails.append(name)

# The extension moved on 2026-09-09; the MAGIC deliberately did not, because
# the gate on the rename is that a .lodl is byte-identical to the .lodt the same
# worldspace wrote the day before. What tells the two formats apart is the
# TEXTURE file's own magic, LDTX -- see the refusal section at the end.
check("magic is still LODT after the .lodl rename", b[0:4] == b'LODT')
minX, minY, maxX, maxY = struct.unpack_from('<iiii', b, 8)
spc, be, levels = struct.unpack_from('<III', b, 0x18)
quant = struct.unpack_from('<f', b, 0x2C)[0]
ltexN, watrN, gcvrN, aoS, ovS, flags = struct.unpack_from('<IIIIII', b, 0x30)
o = struct.unpack_from('<QQQQQQQQQQ', b, 0x48)
oLtex, oWatr, oGcvr, oQuad, oCell, oOver, oAo, oDir, oData, oSize = o
check("header size field matches the file", oSize == len(b))
check("offsets ascend and stay in the file",
      all(o[i] <= o[i+1] for i in range(9)) and oSize == len(b))
check("LTEX and WATR tables are populated", ltexN > 0 and watrN > 0)
check("height quantum is VHGT's own 8", abs(quant - 8.0) < 1e-6)

cellsX, cellsY = maxX-minX+1, maxY-minY+1
coarsest = levels - 1
def bxy(L): return ((cellsX+(1<<L)-1)>>L, (cellsY+(1<<L)-1)>>L)
first, run = {}, 0
for L in range(coarsest, -1, -1):
    first[L] = run
    bx, by = bxy(L); run += bx*by
check("block directory has exactly the predicted entries",
      (oData - oDir)//16 == run)

cache = {}
def block(i):
    if i in cache: return cache[i]
    off, csz, usz = struct.unpack_from('<QII', b, oDir + i*16)
    raw = zlib.decompress(b[off:off+csz])
    if len(raw) != usz: raise AssertionError("block %d size %d != %d" % (i, len(raw), usz))
    cache[i] = raw; return raw

def height(gx, gy):
    L = 0
    while L < coarsest and gx % (1 << (L+1)) == 0 and gy % (1 << (L+1)) == 0: L += 1
    lx, ly = gx >> L, gy >> L
    bi, bj = lx // be, ly // be
    wx, wy = lx % be, ly % be
    bx, _ = bxy(L)
    raw = block(first[L] + bj*bx + bi)
    if L == coarsest: k = wy*be + wx
    else:
        px, py = wx//2, wy//2
        off = 0 if (wx & 1 and not wy & 1) else (1 if (not wx & 1 and wy & 1) else 2)
        k = (py*(be//2) + px)*3 + off
    return (struct.unpack_from('<H', raw, k*2)[0] - 32767) * quant

# LAND corners read from the ESM by an independent path (nifskope-cli --cell)
truth = {(-20, 24): (8208, 7336, 8704, 8160), (0, 0): (512, 560, 608, 552)}
exact = True
for (cx, cy), (sw, se, nw, ne) in truth.items():
    x0, y0 = (cx-minX)*spc, (cy-minY)*spc
    got = (height(x0, y0), height(x0+spc, y0), height(x0, y0+spc), height(x0+spc, y0+spc))
    if got != (sw, se, nw, ne):
        exact = False
        print("     cell (%d,%d) got %s want %s" % (cx, cy, got, (sw, se, nw, ne)))
check("heights round-trip EXACTLY against the ESM's LAND corners", exact)

qs = struct.unpack_from('<%dH' % (cellsX*2*cellsY*2*6), b, oQuad)
layered = sum(1 for k in range(0, len(qs), 6) if any(v != 0xFFFF for v in qs[k:k+5]))
pct = 100.0*layered/(cellsX*2*cellsY*2)
check("quadrant layers stay sparse (%.1f%%, ESM scan said 10.5%%)" % pct, pct < 15.0)

aos = b[oAo:oAo+cellsX*aoS*cellsY*aoS]
check("AO plane varies (not a constant)", len(set(aos)) > 32)
print("RESULT %s" % ("PASS" if not fails else "FAIL"))
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
WW_LODL_VERSION=1 "$NS" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/v1" \
	> "$W/v1.txt" 2>&1 || { echo "FAIL: the version 1 fallback write failed"; rc=1; }
F1="$W/v1/Terrain/Commonwealth.lodl"

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
    # The block directory stores each payload's offset ABSOLUTE, so it moves
    # by the same eight bytes and cannot be compared byte for byte. Its
    # entries are compared as NUMBERS instead, and the two spans on either
    # side of it byte for byte -- which is the stronger statement: a
    # directory rewritten wholesale would pass a raw memcmp of the tail
    # only by accident, and this fails on any entry that moved by anything
    # other than 8 or whose sizes changed.
    d2s, d2e, d1s, d1e = off[7], off[8], o1[7], o1[8]
    nblk = (d2e - d2s) // 16
    badoff = badsz = 0
    for k in range(nblk):
        a2, c2, u2 = struct.unpack_from("<QII", b, d2s + k * 16)
        a1, c1, u1 = struct.unpack_from("<QII", v1, d1s + k * 16)
        if a2 != a1 + 8:
            badoff += 1
        if (c2, u2) != (c1, u1):
            badsz += 1
    print("  %d directory entries, %d offsets not +8, %d sizes changed"
          % (nblk, badoff, badsz))
    check("the directory has entries at all (so this can fail)", nblk > 0)
    check("every block payload offset moved by exactly 8, and no size moved",
          badoff == 0 and badsz == 0)
    check("and NOTHING ELSE moved: identical either side of the directory",
          b[0xA0:d2s] == v1[0x98:d1s] and b[d2e:] == v1[d1e:])
else:
    check("the version 1 fallback file exists", False)
print("RESULT %s" % ("PASS" if not fails else "FAIL"))
sys.exit(1 if fails else 0)
PYEOF2
[ $? = 0 ] || rc=1

# --- the landless cell, against the shadow heightmap ------------------
#
# The .lodl and the HeightMap.dds are read by FO4CS as ONE surface -- terrain
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
	"$NS" -no-gui lodgen "$NWESM" --worldspace 52931 --lodl "$W/nwa" \
		> "$W/nwa.txt" 2>&1 || { bad2 "the NukaWorldAmphitheater .lodl writes"; }
	"$NS" -no-gui lodgen "$NWESM" --worldspace 52931 --heightmap "$W/hm" \
		> "$W/hm.txt" 2>&1 || { bad2 "its shadow heightmap bakes"; }
	L2="$W/nwa/Terrain/NukaWorldAmphitheater.lodl"
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


check("the .lodl grid and the heightmap are the same size (%dx%d)" % (w, h),
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
    print("  first difference: cell (%d,%d) col %d row %d  lodl %d  heightmap %d"
          % first)
check("the worldspace HAS landless cells, so this can fail", landless > 0)
check("every texel agrees with the shadow heightmap (%d differ)" % n, n == 0)
print("RESULT %s" % ("PASS" if not fails else "FAIL"))
sys.exit(1 if fails else 0)
PYEOF3
		[ $? = 0 ] || rc=1
	else
		bad2 "both the .lodl and the heightmap were produced"
	fi
fi

# --- the two readers refuse each other's file, BY NAME -----------------
#
# `.lodt` named THIS format until 2026-09-09 and names the terrain texture
# sheets now, so the one mistake a user or a script will really make is handing
# one route the other's file. Neither may say only "bad magic", and neither may
# parse it: the landscape reader must NAME the texture file and the texture
# validator must NAME the landscape file.
#
# The controls are on both sides of each check. A file that IS the right format
# must still open (or the refusal proves nothing but that the route is broken),
# and the refusal text must contain the OTHER format's name (or "refused"
# alone would pass on any error at all).
echo "== each reader refuses the other's file by name =="

# a real landscape file, under its new name and unchanged in content
cp "$F" "$W/Commonwealth_probe.lodl"
# the smallest thing that is unmistakably a terrain TEXTURE file: 0x98 bytes
# whose first four are the LDTX magic. The land reader reads the header before
# anything else, so this reaches exactly the branch under test.
"$PY" -c "import sys; open(sys.argv[1],'wb').write(b'LDTX'+bytes(0x94))" \
	"$W/fake_texture.lodt"

# CONTROL: the landscape route opens a real .lodl
if "$NS" -no-gui lodl "$W/Commonwealth_probe.lodl" --info > "$W/ctl_land.txt" 2>&1; then
	ok2 "control: the landscape route opens a .lodl"
else
	bad2 "control: the landscape route opens a .lodl"; cat "$W/ctl_land.txt"
fi

# the landscape route, handed a terrain texture file
if "$NS" -no-gui lodl "$W/fake_texture.lodt" --info > "$W/ref_land.txt" 2>&1; then
	bad2 "the landscape route REFUSES a .lodt terrain texture file"
else
	if grep -qi "TEXTURE file" "$W/ref_land.txt"; then
		ok2 "the landscape route names the .lodt texture file it was handed"
		say "$(head -1 "$W/ref_land.txt")"
	else
		bad2 "the landscape route's refusal NAMES the texture file"
		cat "$W/ref_land.txt"
	fi
fi

# the texture validator, handed the landscape file (the old .lodt bytes)
cp "$F" "$W/old_meaning.lodt"
if "$NS" -no-gui lodgen --lodt-check "$W/old_meaning.lodt" > "$W/ref_tex.txt" 2>&1; then
	bad2 "the texture route REFUSES an old-meaning .lodt (the landscape file)"
else
	if grep -qi "lodl" "$W/ref_tex.txt"; then
		ok2 "the texture route names the landscape file it was handed"
		say "$(grep -i refused "$W/ref_tex.txt" | head -1)"
	else
		bad2 "the texture route's refusal NAMES the landscape file"
		cat "$W/ref_tex.txt"
	fi
fi

# the retired spellings fail LOUDLY and name their replacement
if "$NS" -no-gui lodgen "$ESM" --worldspace 3C --lodt "$W/retired" \
	> "$W/retired.txt" 2>&1; then
	bad2 "--lodt is refused"
else
	grep -qi -- "--lodl" "$W/retired.txt" \
		&& ok2 "--lodt is refused and names --lodl" \
		|| { bad2 "--lodt's refusal names --lodl"; cat "$W/retired.txt"; }
fi
if "$NS" -no-gui lodt "$W/Commonwealth_probe.lodl" --info > "$W/retired2.txt" 2>&1; then
	bad2 "the 'lodt' command is refused"
else
	grep -qi "lodl" "$W/retired2.txt" \
		&& ok2 "the 'lodt' command is refused and names 'lodl'" \
		|| { bad2 "the 'lodt' command's refusal names 'lodl'"; cat "$W/retired2.txt"; }
fi
if WW_LODT_VERSION=1 "$NS" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/oldenv" \
	> "$W/oldenv.txt" 2>&1; then
	bad2 "WW_LODT_VERSION is refused"
else
	grep -qi "WW_LODL_VERSION" "$W/oldenv.txt" \
		&& ok2 "WW_LODT_VERSION is refused and names WW_LODL_VERSION" \
		|| { bad2 "WW_LODT_VERSION's refusal names WW_LODL_VERSION"; cat "$W/oldenv.txt"; }
fi

# and the renamed file still VERIFIES against its own source: the rename moved
# the name, not a byte.
mkdir -p "$W/verify/Terrain"
cp "$F" "$W/verify/Terrain/Commonwealth.lodl"
if "$NS" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/verify" --verify-only \
	> "$W/verify.txt" 2>&1; then
	MM="$(grep -o '[0-9]* mismatched' "$W/verify.txt" | head -1)"
	say "verify-only under the new name: ${MM:-no mismatch line}"
	echo "$MM" | grep -q '^0 mismatched' \
		&& ok2 "the file verifies under its new name with 0 mismatches" \
		|| ok2 "the file verifies under its new name (rc 0)"
else
	bad2 "the file verifies under its new name"; tail -5 "$W/verify.txt"
fi

[ "$rc" = 0 ] && echo PASS || echo FAIL
exit $rc
