# VTFIX patch 1 -- tests/spells/lodgen_terrain_vt.sh
#
#   (a) two more bakes, --vt and direct, both WITHOUT --cover;
#   (b) V9a split in two, because the measurement says the old bar could not
#       hold: with the ground-cover tint OFF the two colour sheets are
#       byte-identical (that is the dominantBase gate, and it is exact), and
#       with it ON they differ only inside one heightfield sample of the
#       CHUNK'S OUTER boundary, because the tint is weighted by a slope gate
#       that reads the same normal V9b already exempts there.
#
# Numbers pinned below were measured on the BUILD2 exe (18:43:13) over all four
# dim-4 chunks of the fixture -- scratchpad/lane_vtfix_report.md sections 1-2.
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")
P = "tests/spells/lodgen_terrain_vt.sh"
b = open(P, "rb").read()
assert b.count(b"\r") == 0, b.count(b"\r")
s = b.decode("utf-8")


def sub(old, new, n=1):
    global s
    c = s.count(old)
    assert c == n, "anchor count %d != %d for %r" % (c, n, old[:70])
    s = s.replace(old, new)


# --- (a) the two cover-free bakes -------------------------------------------
sub(
    'vt novt --tex-dir "$W/novt/tex" --cover || { echo "the --no-vt control failed"; exit 1; }\n',
    'vt novt --tex-dir "$W/novt/tex" --cover || { echo "the --no-vt control failed"; exit 1; }\n'
    '# V9a needs the pair again with the ground-cover tint OFF. The tint is\n'
    '# weighted by a slope gate that reads the terrain normal, and the ringed\n'
    '# tile bake and the clamped chunk bake legitimately disagree on that normal\n'
    '# within one heightfield sample of a chunk boundary (V9b exempts it there).\n'
    '# With the tint off nothing else in the colour composite can differ, so this\n'
    '# pair is where byte identity is the honest bar -- and it is the dominantBase\n'
    '# gate the spec asks for, exactly.\n'
    'vt run1nc --vt "$W/run1nc/mod" --tex-dir "$W/run1nc/tex" \\\n'
    '\t|| { echo "the cover-free --vt bake failed"; tail -8 "$W/run1nc.log"; exit 1; }\n'
    'vt novtnc --tex-dir "$W/novtnc/tex" || { echo "the cover-free control failed"; exit 1; }\n')

# --- (b) the V9 section ------------------------------------------------------
OLD = '''echo "== V9 the chunk sheets assembled from the pyramid =="
STEM="Commonwealth.4.-24.24"
if [ -f "$W/run1/tex/$STEM.DDS" ] && [ -f "$W/novt/tex/$STEM.DDS" ]; then
	say "assembled: $(stat -c %s "$W/run1/tex/$STEM.DDS") bytes, direct: $(stat -c %s "$W/novt/tex/$STEM.DDS")"
	if cmp -s "$W/run1/tex/$STEM.DDS" "$W/novt/tex/$STEM.DDS"; then
		ok "V9a the assembled colour sheet is byte-identical to a direct bake"
	else
		bad "V9a the assembled colour sheet is byte-identical to a direct bake"
		say "expected where dominantBase is chunk-scoped; a difference here means it was rescoped"
	fi
'''
NEW = '''echo "== V9 the chunk sheets assembled from the pyramid =="
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
	[ $ncsame -eq 1 ] \\
		&& ok "V9a with the ground-cover tint OFF the assembled colour sheet is byte-identical to a direct bake, on all four dim-4 chunks" \\
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
	[ $RC -eq 0 ] \\
		&& echo "  ok   V9a with the tint ON the difference is the boundary normal's only (the block above)" \\
		|| { fails=$((fails + 1)); echo "  FAIL V9a with the tint ON the difference is the boundary normal's only (the block above)"; }
'''
sub(OLD, NEW)

out = s.encode("utf-8")
assert out.count(b"\r") == 0
open(P, "wb").write(out)
print("OK", P, len(out), "bytes, CR", out.count(b"\r"), "LF", out.count(b"\n"))
