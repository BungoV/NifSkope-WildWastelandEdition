#!/usr/bin/env python
"""TERRAINFIX step 4: lodgen_terrain.sh gains rung 4 -- the pyramid path's own
_msn, which is a SECOND writer of the same file name and had both of the
2026-09-07 defects until today."""

P = 'tests/spells/lodgen_terrain.sh'
b = open(P, 'rb').read().decode('utf-8')
cr0 = b.count('\r')

anchor = '''echo "$checks checks, $fails failures"'''
assert b.count(anchor) == 1

rung = r'''# --- rung 4: the SECOND _msn writer -- the virtual-texture pyramid ----
#
# With --vt on, the .btr chunk sheets are ASSEMBLED from the pyramid tiles
# (docs/LODGEN_TERRAIN_VT.md 2.4), so `lodgenBakeVtTile` writes the very file
# rung 3 checks, under the same name, through different code. It was a COPY of
# the per-chunk normal block and it kept BOTH defects that path lost on
# 2026-09-07 -- nearest sampling and up-in-blue -- until 2026-09-09. Nothing
# pinned it, which is exactly why it survived.
#
# Two facts, the same two:
#   * the up channel is GREEN (same argument as rung 3: up is the only
#     component recoverable as +sqrt(1 - a^2 - b^2) and the only one that never
#     encodes a negative, so whichever channel wins both IS up);
#   * the sheet is not BLOCK-FLAT. Nearest sampling made all sixteen texels of
#     a 4x4 block read one height sample and get one normal, so the fraction of
#     texels differing from their LEFT NEIGHBOUR, by x mod 4, was
#     83.3 / 0.0 / 0.0 / 0.0 against vanilla's 99.8 / 64.4 / 64.7 / 64.3
#     (measured 2026-09-07). Classes 1, 2 and 3 are the intra-block ones and
#     they are ZERO under the defect. That is the discriminator, and vanilla's
#     own shipped sheet is read beside it as the known-answer control: a metric
#     that cannot separate them is not measuring what it claims.
cat > "$W/msnstat.py" <<'STATEOF'
import struct, sys
b = open(sys.argv[1], 'rb').read()
h, w = struct.unpack_from('<II', b, 12)
fcc = b[84:88]
# block size from the HEADER: ours is BC1 and vanilla's is BC3, and assuming
# 16 read past the end of our own file once (2026-09-07)
bs = 8 if fcc == b'DXT1' else 16
col = 0 if bs == 8 else 8          # BC3 puts its alpha block first
off = 148 if fcc == b'DX10' else 128
bw, bh = (w + 3) // 4, (h + 3) // 4


def block(o):
    c0, c1 = struct.unpack_from('<HH', b, o)
    bits = struct.unpack_from('<I', b, o + 4)[0]

    def rgb(c):
        return (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63,
                (c & 31) * 255 // 31)
    e0, e1 = rgb(c0), rgb(c1)
    if c0 > c1 or bs == 16:
        t = [e0, e1, tuple((2 * e0[k] + e1[k]) // 3 for k in range(3)),
             tuple((e0[k] + 2 * e1[k]) // 3 for k in range(3))]
    else:
        t = [e0, e1, tuple((e0[k] + e1[k]) // 2 for k in range(3)), (0, 0, 0)]
    return [t[(bits >> (2 * k)) & 3] for k in range(16)]


px = [[(0, 0, 0)] * w for _ in range(h)]
for by in range(bh):
    for bx in range(bw):
        t = block(off + (by * bw + bx) * bs + col)
        for k in range(16):
            y, x = by * 4 + k // 4, bx * 4 + k % 4
            if y < h and x < w:
                px[y][x] = t[k]
res = [0.0, 0.0, 0.0]
neg = [0, 0, 0]
diff = [0, 0, 0, 0]
tot = [0, 0, 0, 0]
for y in range(h):
    row = px[y]
    for x in range(w):
        c = row[x]
        if x:
            tot[x % 4] += 1
            if c != row[x - 1]:
                diff[x % 4] += 1
    if y % 8:                      # the channel verdict needs a sample, not all
        continue
    for c in row:
        v = [c[k] / 255.0 * 2 - 1 for k in range(3)]
        for k in range(3):
            a, bb = v[(k + 1) % 3], v[(k + 2) % 3]
            s = 1.0 - a * a - bb * bb
            res[k] += abs((s ** 0.5 if s > 0 else 0.0) - abs(v[k]))
            if c[k] < 128:
                neg[k] += 1
up = min(range(3), key=lambda k: (neg[k], res[k]))
print('UP=%s %s' % ('RGB'[up], ' '.join(
    'D%d=%d' % (i, 100 * diff[i] // max(1, tot[i])) for i in range(4))))
STATEOF

mkdir -p "$W/vt/obj" "$W/vt/tex" "$W/vt/mod"
"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
	--terrain-region -20 24 -19 25 --dim 4 \
	--out-dir "$W/vt/obj" --tex-dir "$W/vt/tex" --vt "$W/vt/mod" > "$W/vt.log" 2>&1
VMSN="$W/vt/tex/Commonwealth.4.-20.24_msn.DDS"
check "the --vt run assembles the chunk _msn from the pyramid" \
	"$([ -s "$VMSN" ] && echo 1 || echo 0)"
if [ -s "$VMSN" ]; then
	VSTAT=$("$PY" "$W/msnstat.py" "$VMSN")
	DSTAT="n/a"; [ -f "$MSN" ] && DSTAT=$("$PY" "$W/msnstat.py" "$MSN")
	VANMSN="$(dirname "$VAN")/../../../textures/terrain/Commonwealth/Commonwealth.4.-20.24_msn.DDS"
	CSTAT="n/a"; [ -f "$VANMSN" ] && CSTAT=$("$PY" "$W/msnstat.py" "$VANMSN")
	echo "  pyramid-assembled msn: $VSTAT"
	echo "  direct-bake msn:       $DSTAT"
	echo "  vanilla's own sheet:   $CSTAT   (the control)"
	vup=${VSTAT%% *}
	check "the pyramid's msn writes UP in GREEN too" \
		"$([ "$vup" = "UP=G" ] && echo 1 || echo 0)"
	flat=0
	for k in 1 2 3; do
		d=$(echo "$VSTAT" | tr ' ' '\n' | sed -n "s/^D$k=//p")
		[ "${d:-0}" -gt 20 ] || flat=1
	done
	check "the pyramid's msn is not block-flat (D1..D3 > 20%, nearest gave 0)" \
		"$([ "$flat" = "0" ] && echo 1 || echo 0)"
	if [ "$CSTAT" != "n/a" ]; then
		c1=$(echo "$CSTAT" | tr ' ' '\n' | sed -n 's/^D1=//p')
		check "CONTROL: the same statistic reads high on vanilla's own sheet" \
			"$([ "${c1:-0}" -gt 20 ] && echo 1 || echo 0)"
	fi
fi

'''

b = b.replace(anchor, rung + anchor)
assert b.count('\r') == cr0
open(P, 'wb').write(b.encode('utf-8'))
print('%s patched, CR %d (unchanged)' % (P, cr0))
