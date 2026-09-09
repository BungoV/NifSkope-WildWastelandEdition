#!/bin/bash
#
# The terrain virtual texture, under docs/LODGEN_TERRAIN_VT.md: a pyramid of
# 256-texel tiles with an 8-texel border, one .lodt container per level under
# Data\Terrain\, indexed by a terrainVT .lodm, FOUR sheets a tile -- colour,
# model-space normal, data (AO, wetness, shore, cover) and HEIGHT (R16, the
# shadow heightmap's own encoding, on the same grid with the same border, so a
# consumer that wants nested grids has the geometry side too).
#
# FIXTURE: the cells (-24,24)..(-17,31), 8x8. West -24 and south 24 both divide
# 8 and not 16, so the ladder is dim 2, 4 and 8 -- 16 finest tiles, 4, then 1.
# (The spec's own -20 24 -13 31 is NOT dim-8 aligned: -20 mod 8 = 4, which would
# cut the ladder to two levels and leave nothing to filter twice.)
#
# WHAT IS BEING GUARDED:
#   * the file, exactly: every header field, the exact size, 4,096-aligned
#     payloads in table-index order with zero pad, per-tile CRCs and the index
#     CRC that per-tile ones cannot replace (offset aliasing);
#   * the filter law, EXACTLY where it can be exact -- the height sheet is
#     uncompressed R16, so (a+b+c+d+2)>>2 is checked with zero violations on the
#     content AND on each of the four border strips separately, and a nearest
#     downsample is required to DISAGREE;
#   * the border really is the neighbour's content and not a clamp of the
#     tile's own edge;
#   * north-up, and two tiles not being one tile;
#   * the validator: a mutation battery that must be REFUSED BY NAME, and an
#     unmutated file that must not be;
#   * determinism: two runs, byte for byte;
#   * and that a terrain-only change moved nothing on the object path.
#
# USAGE
#   bash tests/spells/lodgen_terrain_vt.sh
#
# Exit 2 = a missing input or an unpinned corpus; 1 = a failed check.

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

X0=-24; Y0=24; X1=-17; Y1=31

checks=0
fails=0
ok() { checks=$((checks + 1)); echo "  ok   $1"; }
bad() { checks=$((checks + 1)); fails=$((fails + 1)); echo "  FAIL $1"; }
say() { echo "       $1"; }

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }
[ -d "$DATA" ] || { echo "no unpacked Data at $DATA"; exit 2; }

echo "== preflight =="
STALE=0
for s in src/lodgen.cpp src/lodgen.h src/esmdata.cpp src/esmdata.h src/nifcli.cpp \
		src/io/lodvfile.cpp src/io/lodmfile.cpp src/lodgenmanager.cpp; do
	if [ -f "$ROOT/$s" ] && [ ! "$NS" -nt "$ROOT/$s" ]; then
		say "exe is NOT newer than $s"
		STALE=1
	fi
done
say "exe: $(ls -l --time-style=+%Y-%m-%d\ %H:%M:%S "$NS" | awk '{print $6, $7}')"
say "ESM: $(stat -c %s "$ESM") bytes"
[ $STALE -eq 0 ] && ok "the exe is newer than every source this answer depends on" \
	|| bad "the exe is newer than every source this answer depends on"

"$NS" -no-gui lodgen "$ESM" --worldspace 3C --corpus-hash > "$W/corpus.txt" 2>&1
grep -a '^corpus ' "$W/corpus.txt" | sed 's/^/       /'
VHGT="$(grep -a '^corpus vhgtCorpusHash ' "$W/corpus.txt" | awk '{print $3}')"
PAINT="$(grep -a '^corpus paintCorpusHash ' "$W/corpus.txt" | awk '{print $3}')"
if [ "$VHGT" != "0xD8337D022F637F22" ]; then
	echo "  corpus hash is $VHGT, not the Commonwealth's; every floor below is a"
	echo "  property of ONE plugin set. Refusing."
	exit 2
fi
ok "the corpus is the shipped Commonwealth"

echo "== the estimate, before any work =="
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --vt-estimate > "$W/est.txt" 2>&1
grep -a '^vt ' "$W/est.txt" | sed 's/^/       /'
grep -qa '^vt estimate: levels ' "$W/est.txt" \
	&& ok "--vt-estimate prints a cost and bakes nothing" \
	|| bad "--vt-estimate prints a cost and bakes nothing"
grep -qa 'minutes unmeasured' "$W/est.txt" \
	&& ok "and it says the bake time is unmeasured rather than inventing one" \
	|| bad "and it says the bake time is unmeasured rather than inventing one"

echo "== the bakes =="
vt() {   # vt <name> <extra...>
	local name="$1"; shift
	mkdir -p "$W/$name/mod" "$W/$name/obj" "$W/$name/tex"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region $X0 $Y0 $X1 $Y1 --dim 4 \
		--out-dir "$W/$name/obj" --data-root "$DATA" "$@" > "$W/$name.log" 2>&1
	return $?
}
# --vt-height: the height sheet is OFF by default (+133% on a tile, and for a Fallout 4
# source it is interpolation, not measurement), but it is the ONE sheet that is not
# block-compressed, which is where V8 proves the filter law exactly. So the gate asks for it.
vt run1 --vt "$W/run1/mod" --tex-dir "$W/run1/tex" --cover --vt-height \
	|| { echo "the --vt bake failed"; tail -8 "$W/run1.log"; exit 1; }
grep -a '^vt:' "$W/run1.log" | sed 's/^/       /'
vt run2 --vt "$W/run2/mod" --tex-dir "$W/run2/tex" --cover --vt-height \
	|| { echo "the second --vt bake failed"; exit 1; }
vt novt --tex-dir "$W/novt/tex" --cover || { echo "the --no-vt control failed"; exit 1; }
# V9a needs the pair again with the ground-cover tint OFF. The tint is
# weighted by a slope gate that reads the terrain normal, and the ringed
# tile bake and the clamped chunk bake legitimately disagree on that normal
# within one heightfield sample of a chunk boundary (V9b exempts it there).
# With the tint off nothing else in the colour composite can differ, so this
# pair is where byte identity is the honest bar -- and it is the dominantBase
# gate the spec asks for, exactly.
vt run1nc --vt "$W/run1nc/mod" --tex-dir "$W/run1nc/tex" \
	|| { echo "the cover-free --vt bake failed"; tail -8 "$W/run1nc.log"; exit 1; }
vt novtnc --tex-dir "$W/novtnc/tex" || { echo "the cover-free control failed"; exit 1; }

DIR="$W/run1/mod/Terrain"
ls -l "$DIR" | sed 's/^/       /'
CONT="$(ls "$DIR"/Commonwealth.VT.*.lodt 2>/dev/null)"
[ -n "$CONT" ] && ok "the containers are written under Terrain/, one per level" \
	|| { bad "the containers are written under Terrain/, one per level"; echo "RESULT FAIL"; exit 1; }
NLEV="$(echo "$CONT" | wc -l)"
say "levels written: $NLEV"
[ "$NLEV" = "3" ] && ok "the fixture's ladder is dim 2, 4 and 8" \
	|| bad "the fixture's ladder is dim 2, 4 and 8 (got $NLEV levels)"

echo "== V1/V3/V4/V7/V18 the header, exactly =="
"$PY" "$ROOT/tests/spells/lodgen_vt_check.py" header $CONT
RC=$?; checks=$((checks + 1))
[ $RC -eq 0 ] && echo "  ok   V1/V3/V4/V7/V18 (the block above)" \
	|| { fails=$((fails + 1)); echo "  FAIL V1/V3/V4/V7/V18 (the block above)"; }

echo "== V5/V6/V7 the table and the payloads =="
"$PY" "$ROOT/tests/spells/lodgen_vt_check.py" tiles "$DIR/Commonwealth.VT.2.lodt"
RC=$?; checks=$((checks + 1))
[ $RC -eq 0 ] && echo "  ok   V5/V6/V7 (the block above)" \
	|| { fails=$((fails + 1)); echo "  FAIL V5/V6/V7 (the block above)"; }

echo "== V8/V10 the filter law =="
"$PY" "$ROOT/tests/spells/lodgen_vt_check.py" filter \
	"$DIR/Commonwealth.VT.2.lodt" "$DIR/Commonwealth.VT.4.lodt"
RC=$?; checks=$((checks + 1))
[ $RC -eq 0 ] && echo "  ok   V8/V10 (the block above)" \
	|| { fails=$((fails + 1)); echo "  FAIL V8/V10 (the block above)"; }

echo "== V11 the border =="
"$PY" "$ROOT/tests/spells/lodgen_vt_check.py" border "$DIR/Commonwealth.VT.2.lodt"
RC=$?; checks=$((checks + 1))
[ $RC -eq 0 ] && echo "  ok   V11 (the block above)" \
	|| { fails=$((fails + 1)); echo "  FAIL V11 (the block above)"; }

echo "== V20 georeferencing, and the clipmap ladder =="
"$PY" "$ROOT/tests/spells/lodgen_vt_check.py" georef "$DIR/Commonwealth.VT.2.lodt"
RC=$?; checks=$((checks + 1))
[ $RC -eq 0 ] && echo "  ok   V20 (the block above)" \
	|| { fails=$((fails + 1)); echo "  FAIL V20 (the block above)"; }
"$PY" "$ROOT/tests/spells/lodgen_vt_check.py" ladder $CONT
RC=$?; checks=$((checks + 1))
[ $RC -eq 0 ] && echo "  ok   one aligned grid (the block above)" \
	|| { fails=$((fails + 1)); echo "  FAIL one aligned grid (the block above)"; }

echo "== V2 the validator refuses, by name =="
ROOTC="$DIR/Commonwealth.VT.8.lodt"

# The landscape route must NAME this file rather than misparse it: `.lodt` was
# the landscape file's own extension until 2026-09-09. This is the real-file
# half of the pair; tests/spells/lodl_write.sh does the other direction and a
# synthetic header.
if "$NS" -no-gui lodl "$ROOTC" --info > "$W/land_on_tex.txt" 2>&1; then
	bad "the landscape route refuses a real terrain texture container"
else
	if grep -qai "TEXTURE file" "$W/land_on_tex.txt"; then
		ok "the landscape route names the .lodt texture container it was handed"
	else
		bad "the landscape route's refusal names the texture container"
		sed 's/^/       /' "$W/land_on_tex.txt"
	fi
fi
"$NS" -no-gui lodgen --lodt-check "$ROOTC" > "$W/lodt.txt" 2>&1
if grep -qa '^lodt ok 1' "$W/lodt.txt"; then
	ok "V2 the unmutated root passes the shipped validator"
	grep -a '^lodt ' "$W/lodt.txt" | head -12 | sed 's/^/       /'
else
	bad "V2 the unmutated root passes the shipped validator"
	sed 's/^/       /' "$W/lodt.txt"
fi
# one mutated copy per rule, each expected to name the field it broke
"$PY" - "$ROOTC" "$W/mut" <<'PYEOF'
import os, struct, sys
src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
b = bytearray(open(src, 'rb').read())
def w32(bb, o, v): struct.pack_into('<I', bb, o, v)
def w16(bb, o, v): struct.pack_into('<H', bb, o, v)
def w64(bb, o, v): struct.pack_into('<Q', bb, o, v)
muts = [
    ('magic',        lambda x: x.__setitem__(slice(0, 4), b'DDS ')),
    ('version',      lambda x: w32(x, 4, 2)),
    ('headerBytes',  lambda x: w32(x, 8, 512)),
    ('fileBytes',    lambda x: w64(x, 0x10, 12345)),
    ('worldspaceEdid', lambda x: x.__setitem__(slice(0x38, 0x58), b'A' * 32)),
    ('rectangle',    lambda x: w16(x, 0x58 + 4, 0x8000)),
    ('levelDim',     lambda x: w16(x, 0x68, 7)),
    ('tilesX',       lambda x: w16(x, 0x6E, 99)),
    # current + 1: setting it to a constant wrote the value already there on a
    # single-tile container, so rule 10 was never reached and the file was valid
    ('tileCount',    lambda x: w32(x, 0x7C, int.from_bytes(x[0x7C:0x80], 'little') + 1)),
    ('contentTexels', lambda x: w16(x, 0x72, 300)),
    ('borderTexels', lambda x: w16(x, 0x74, 6)),
    ('storedTexels', lambda x: w16(x, 0x76, 271)),
    ('mipCount',     lambda x: x.__setitem__(0x78, 0)),
    ('sheetCount',   lambda x: x.__setitem__(0x79, 5)),
    ('sheetFormat',  lambda x: w16(x, 0xA0, 999)),
    ('sheetRole',    lambda x: x.__setitem__(0xA4, 0)),
    ('compression',  lambda x: x.__setitem__(0x7B, 2)),
    ('tileTableOffset', lambda x: w64(x, 0x18, 7)),
    ('payloadOffset', lambda x: w64(x, 0x20, 1)),
    ('rowOrder',     lambda x: w32(x, 0x0C, 0)),
    ('anisoSupported', lambda x: x.__setitem__(0x7A, 64)),
    ('levelDims',    lambda x: w16(x, 0x88, 3)),
    ('indexCrc32',   lambda x: w32(x, 0x98, 0xDEADBEEF)),
]
for name, fn in muts:
    c = bytearray(b)
    fn(c)
    open(os.path.join(out, name + '.lodt'), 'wb').write(bytes(c))
print('       %d mutations written' % len(muts))
PYEOF
refused=0
total=0
for m in "$W/mut"/*.lodt; do
	total=$((total + 1))
	name="$(basename "$m" .lodt)"
	if "$NS" -no-gui lodgen --lodt-check "$m" > "$W/m.txt" 2>&1; then
		say "$name: ACCEPTED (it should have been refused)"
	else
		msg="$(grep -a '^lodt refused' "$W/m.txt" | head -1)"
		say "$name: ${msg:-refused with no message}"
		refused=$((refused + 1))
	fi
done
say "refusals: $refused of $total"
[ "$refused" = "$total" ] && ok "V2 every mutated container is refused" \
	|| bad "V2 every mutated container is refused ($refused of $total)"

echo "== V15/V16 truncation and a flipped payload byte =="
head -c $(( $(stat -c %s "$ROOTC") - 1 )) "$ROOTC" > "$W/trunc.lodt"
if "$NS" -no-gui lodgen --lodt-check "$W/trunc.lodt" > "$W/t.txt" 2>&1; then
	bad "V15 a container short by one byte is refused"
else
	grep -qa 'fileBytes' "$W/t.txt" && ok "V15 a container short by one byte is refused, naming fileBytes" \
		|| bad "V15 the refusal names fileBytes"
	grep -a '^lodt refused' "$W/t.txt" | sed 's/^/       /'
fi
"$PY" - "$ROOTC" "$W/flip.lodt" <<'PYEOF'
import struct, sys
b = bytearray(open(sys.argv[1], 'rb').read())
off = struct.unpack_from('<Q', b, 0x20)[0]
b[off + 64] ^= 0xFF
open(sys.argv[2], 'wb').write(bytes(b))
PYEOF
if "$NS" -no-gui lodgen --lodt-check "$W/flip.lodt" > "$W/f.txt" 2>&1; then
	bad "V16 a flipped payload byte fails that tile's CRC"
else
	grep -qa 'crc32' "$W/f.txt" && ok "V16 a flipped payload byte fails that tile's CRC, by index" \
		|| bad "V16 the refusal names the tile's crc32"
	grep -a '^lodt refused' "$W/f.txt" | sed 's/^/       /'
fi

echo "== V12/V13/V19 the index =="
IDX="$DIR/Commonwealth.VT.lodm"
"$NS" -no-gui lodgen --lodm-check "$IDX" > "$W/lodm.txt" 2>&1
sed 's/^/       /' "$W/lodm.txt" | head -30
grep -qa '^lodm ok 1' "$W/lodm.txt" && ok "V12 the index parses through this tree's OWN parser" \
	|| bad "V12 the index parses through this tree's OWN parser"
grep -qa '^lodm kind terrainVT' "$W/lodm.txt" && ok "V12 and it says kind terrainVT" \
	|| bad "V12 and it says kind terrainVT"
grep -qa '^lodm family legacy' "$W/lodm.txt" && ok "V12 with family legacy, which is what parses today" \
	|| bad "V12 with family legacy"
LV="$(grep -ca '^level ' "$W/lodm.txt")"
[ "$LV" = "$NLEV" ] && ok "V13 the index names one container per level ($LV)" \
	|| bad "V13 the index names one container per level (index $LV, files $NLEV)"
grep -qa "terrain vhgtCorpusHash $VHGT" "$W/lodm.txt" \
	&& ok "V19 the index's VHGT hash is the plugin's own" \
	|| bad "V19 the index's VHGT hash is the plugin's own"
grep -qa "terrain paintCorpusHash $PAINT" "$W/lodm.txt" \
	&& ok "V19 and its PAINT hash is too (the one that catches a grass mod)" \
	|| bad "V19 and its PAINT hash is too"
grep -qa 'terrain rowOrder northUp' "$W/lodm.txt" && ok "V13 the index states the row order" \
	|| bad "V13 the index states the row order"
grep -qa 'terrain coarseLevelsAreDownsamples true' "$W/lodm.txt" \
	&& ok "V13 and says in the file that coarse levels are downsamples, not measurements" \
	|| bad "V13 and says coarse levels are downsamples"
grep -qa 'terrain alignedToWorldOrigin true' "$W/lodm.txt" \
	&& ok "V13 and that every level is on one aligned grid" \
	|| bad "V13 and that every level is on one aligned grid"
grep -a '^level ' "$W/lodm.txt" | grep -qa 'unitsPerTexel' \
	&& ok "V13 each level states its world span and its texel count, not implies them" \
	|| bad "V13 each level states its world span and its texel count"
say "the index is at Terrain\\, which lodmSourceCandidate() cannot produce: it"
say "prepends materials\\ for a diffuse and strips only a leading data\\ for a"
say "material, so no source lookup can reach Terrain\\*.VT.lodm."

echo "== V22 two runs, byte for byte =="
same=1
for f in "$DIR"/*.lodt "$DIR"/*.lodm; do
	cmp -s "$f" "$W/run2/mod/Terrain/$(basename "$f")" || { same=0; say "differs: $(basename "$f")"; }
done
[ $same -eq 1 ] && ok "V22 two --vt runs are byte-identical, containers and index" \
	|| bad "V22 two --vt runs are byte-identical, containers and index"

echo "== V17 a terrain-only change moved nothing on the object path =="
objsame=1
for f in "$W/run1/obj"/*.BTO "$W/run1/obj"/*.manifest.txt; do
	[ -e "$f" ] || continue
	cmp -s "$f" "$W/novt/obj/$(basename "$f")" || { objsame=0; say "differs: $(basename "$f")"; }
done
[ $objsame -eq 1 ] && ok "V17 the .bto files and their manifests are identical with and without --vt" \
	|| bad "V17 the .bto files and their manifests are identical with and without --vt"

echo "== V9 the chunk sheets assembled from the pyramid =="
STEM="Commonwealth.4.-24.24"
CHUNKS="Commonwealth.4.-24.24 Commonwealth.4.-20.24 Commonwealth.4.-24.28 Commonwealth.4.-20.28"
if [ -f "$W/run1/tex/$STEM.DDS" ] && [ -f "$W/novt/tex/$STEM.DDS" ]; then
	say "assembled: $(stat -c %s "$W/run1/tex/$STEM.DDS") bytes, direct: $(stat -c %s "$W/novt/tex/$STEM.DDS")"
	# V9a-1: the dominantBase gate, exact. dominantBase reaches the colour
	# composite unconditionally, so a tile-scoped dominantBase moves texels with
	# the tint off as well; the tint cannot.
	ncsame=1
	for c in $CHUNKS; do
		cmp -s "$W/run1nc/tex/$c.DDS" "$W/novtnc/tex/$c.DDS" || { ncsame=0; say "differs: $c"; }
	done
	[ $ncsame -eq 1 ] \
		&& ok "V9a with the ground-cover tint OFF the assembled colour sheet is byte-identical to a direct bake, on all four dim-4 chunks" \
		|| bad "V9a with the ground-cover tint OFF the assembled colour sheet is byte-identical to a direct bake"
	# V9a-2: with the tint ON, the difference must be the boundary normal's and
	# nothing else -- bounded in size and confined to one heightfield sample
	# (4 texels at 32 units) of the CHUNK'S OUTER boundary.
	"$PY" - "$W/run1/tex" "$W/novt/tex" $CHUNKS <<'PYEOF'
import struct, sys

def c565(c):
    return (((c >> 11) & 31) * 255 + 15) // 31, (((c >> 5) & 63) * 255 + 31) // 63, ((c & 31) * 255 + 15) // 31

def bc1(path):
    """Decode mip 0 of a DXT1 DDS. Re-typed from the format on purpose: a check
    that decoded through the writer's own code could not fail on the writer."""
    b = open(path, 'rb').read()
    assert b[:4] == b'DDS ', path
    h, w = struct.unpack_from('<II', b, 12)
    off = 148 if b[84:88] == b'DX10' else 128
    px = [(0, 0, 0)] * (w * h)
    p = off
    for by in range((h + 3) // 4):
        for bx in range((w + 3) // 4):
            c0, c1 = struct.unpack_from('<HH', b, p)
            idx = struct.unpack_from('<I', b, p + 4)[0]
            p += 8
            p0, p1 = c565(c0), c565(c1)
            if c0 > c1:
                pal = [p0, p1, tuple((2 * p0[k] + p1[k]) // 3 for k in range(3)),
                       tuple((p0[k] + 2 * p1[k]) // 3 for k in range(3))]
            else:
                pal = [p0, p1, tuple((p0[k] + p1[k]) // 2 for k in range(3)), (0, 0, 0)]
            for j in range(4):
                for i in range(4):
                    x, y = bx * 4 + i, by * 4 + j
                    if x < w and y < h:
                        px[y * w + x] = pal[(idx >> (2 * (j * 4 + i))) & 3]
    return px, w, h

BAND = 4          # one heightfield sample at 32 units per texel
MAXD = 24         # measured 17 on the worst of the four chunks, 2026-09-09
FRAC = 0.0005     # measured 0.016% and 0.032%; 0 on the other two chunks
SEAM = 8          # a dominantBase rescope would sit ON the interior tile seams

da, db, stems = sys.argv[1], sys.argv[2], sys.argv[3:]
fails, total = 0, 0
for stem in stems:
    A, w, h = bc1(da + '/' + stem + '.DDS')
    B, w2, h2 = bc1(db + '/' + stem + '.DDS')
    if (w, h) != (w2, h2):
        print('       %s: SIZE %dx%d vs %dx%d' % (stem, w, h, w2, h2)); fails += 1; continue
    n = far = mx = 0
    seam = 10 ** 9
    for y in range(h):
        for x in range(w):
            d = max(abs(A[y * w + x][k] - B[y * w + x][k]) for k in range(3))
            if not d:
                continue
            n += 1
            if d > mx:
                mx = d
            if min(x, w - 1 - x, y, h - 1 - y) >= BAND:
                far += 1
            seam = min(seam, min(abs(x - w // 2), abs(y - h // 2)))
    total += n
    print('       %-24s differing %5d/%d (%.4f%%) max %3d  outside the %d-texel boundary band %d  nearest interior tile seam %s'
          % (stem, n, w * h, 100.0 * n / (w * h), mx, BAND, far, seam if n else '-'))
    if far:
        print('       FAIL %s: %d texels differ AWAY from the chunk boundary' % (stem, far)); fails += 1
    if mx > MAXD:
        print('       FAIL %s: max channel difference %d > %d' % (stem, mx, MAXD)); fails += 1
    if n > FRAC * w * h:
        print('       FAIL %s: %d differing texels > %.4f%% of the sheet' % (stem, n, 100.0 * FRAC)); fails += 1
    if n and seam < SEAM:
        print('       FAIL %s: a differing texel sits %d texels from an interior tile seam' % (stem, seam)); fails += 1
# the floor: a check that cannot fail is not a check. Cover is ON, the fixture
# is painted, and the boundary normals DO differ, so at least one chunk must
# show the difference -- otherwise the tint, the cover pass or this decode is
# silently inert and every bar above passed on nothing.
if total == 0:
    print('       FAIL the difference the bars bound is absent entirely; nothing was tested'); fails += 1
print('       %d differing texels over %d chunks, %d bar(s) failed' % (total, len(stems), fails))
sys.exit(1 if fails else 0)
PYEOF
	RC=$?; checks=$((checks + 1))
	[ $RC -eq 0 ] \
		&& echo "  ok   V9a with the tint ON the difference is the boundary normal's only (the block above)" \
		|| { fails=$((fails + 1)); echo "  FAIL V9a with the tint ON the difference is the boundary normal's only (the block above)"; }
	# the msn and the data sheet are EXPECTED to differ, and only near the
	# chunk's outer boundary: the pyramid bakes a one-cell ring, so its AO
	# march and its outer-ring normals have data the per-chunk clamp did not
	if cmp -s "$W/run1/tex/$STEM""_msn.DDS" "$W/novt/tex/$STEM""_msn.DDS"; then
		say "the msn sheets are identical (no clamp difference at this chunk's edges)"
	else
		say "the msn sheets differ, which is the ringed bake fixing the per-chunk edge clamp"
	fi
	ok "V9 the assembled and direct sheets were both produced and compared"
else
	bad "V9 both an assembled and a direct chunk sheet exist to compare"
fi

echo
echo "$checks checks, $fails failures"
[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
