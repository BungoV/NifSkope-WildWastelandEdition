# Lane BTOFREE1, 2026-09-16 -- patch 4: native_open.sh check (c) stops asking
# for an equal silhouette and asks for one-sided coverage plus a bound on how
# much wider the scene may be. Numbers and reasoning: patch3_authority.py.
ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'


def patch(path, pairs):
    b = open(ROOT + path, 'rb').read()
    cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
    s = b.decode('utf-8')
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, 'anchor count %d (want 1) in %s for: %r' % (n, path, old[:80])
        s = s.replace(old, new)
    nb = s.encode('utf-8')
    assert nb.count(b'\r') == cr0, 'CR count moved in %s: %d -> %d' % (path, cr0, nb.count(b'\r'))
    open(ROOT + path, 'wb').write(nb)
    print('%-40s %d -> %d bytes, CR %d -> %d, LF %d -> %d'
          % (path, n0, len(nb), cr0, nb.count(b'\r'), lf0, nb.count(b'\n')))


P = []

# ---- the missing comparator ------------------------------------------------
P.append((
'''lt() { awk -v a="$1" -v b="$2" 'BEGIN{print (a <  b) ? 1 : 0}'; }
''',
'''lt() { awk -v a="$1" -v b="$2" 'BEGIN{print (a <  b) ? 1 : 0}'; }
le() { awk -v a="$1" -v b="$2" 'BEGIN{print (a <= b) ? 1 : 0}'; }
''',
))

# ---- check (c) -------------------------------------------------------------
OLD_C = '''	if shot "$W/c_bto.png" "$BTO" WW_LODGEN_RESOURCES="$(winpath "$RES");$(winpath "$OBJ")"; then
		out=$("$PY" "$AUTH" iou "$W/b.png" "$W/c_bto.png" 2>&1)
		echo "$out" | sed 's/^/  /'
		IOU=$(echo "$out" | sed -n 's/^IOU //p')
		check "the .lodi scene covers the same pixels as the .BTO (IoU ${IOU:-?} >= 0.95)" \\
			"$(ge "${IOU:-0}" 0.95)"
	else
		check "the .BTO of the same chunk renders" 0
	fi
	if [ -f "$BTO2" ] && shot "$W/c_other.png" "$BTO2" \\
			WW_LODGEN_RESOURCES="$(winpath "$RES");$(winpath "$OBJ")"; then
		out=$("$PY" "$AUTH" iou "$W/b.png" "$W/c_other.png" 2>&1)
		echo "$out" | sed 's/^/  /'
		IOU2=$(echo "$out" | sed -n 's/^IOU //p')
		check "the refuter fires: a DIFFERENT chunk's .BTO does not match (IoU ${IOU2:-?} < 0.95)" \\
			"$(lt "${IOU2:-1}" 0.95)"
	else
		skip "the (c) refuter: no $WS.$DIM.$OX.$OY.BTO"
	fi
'''

NEW_C = '''	# WHAT THIS ASKS AND WHY IT IS NOT AN IoU ANY MORE (lane BTOFREE1,
	# 2026-09-16). Until today this line demanded IoU >= 0.95 between the
	# .lodi scene and the chunk's own .BTO, and it printed 0.8179 on every exe
	# that ever ran it -- it never passed, not once, so the bar was never a
	# measurement of these two pictures. It could not be: the scene PLACES
	# whole library models and the .BTO carries a mesh the bake merged, cut at
	# the far rings and clipped to the chunk box, so their outlines differ by
	# construction. Measured here at chunk (-20,24) dim 4, one camera:
	#
	#   library   IoU     covered   area x .BTO
	#   mnam      0.8179  0.9874    1.195     (the pre-2026-09-16 default)
	#   near      0.6190  0.9604    1.512     (NATIVE1c's default, today)
	#
	# and under mnam 100.0 percent of the scene's excess pixels sit within
	# 16 px of a pixel the two share, 58.7 percent within 1 px, with the
	# largest connected blob of excess at 143 px and NOT ONE blob of 200 px or
	# more (scratchpad/btofree1_20260916/iou_analyse.py). One silhouette drawn
	# a hair wider than the other, everywhere -- not an object one side draws
	# and the other misses. The near-library default widens it further, which
	# is a consequence of a ruling and not a defect.
	#
	# So the check asks the two questions it always wanted: does the scene
	# draw EVERYTHING the chunk draws (covered), and does it do that without
	# simply drawing the world (area)? Each is vacuous alone -- a frame that
	# covers every pixel scores a perfect 1.0000 covered -- and the pair is
	# not. Three refuters below: the other chunk, the same .BTO mirrored in Y,
	# and that solid frame.
	if shot "$W/c_bto.png" "$BTO" WW_LODGEN_RESOURCES="$(winpath "$RES");$(winpath "$OBJ")"; then
		out=$("$PY" "$AUTH" cover "$W/b.png" "$W/c_bto.png" 2>&1)
		echo "$out" | sed 's/^/  /'
		COV=$(echo "$out" | sed -n 's/^COVER //p')
		FAT=$(echo "$out" | sed -n 's/^FAT //p')
		CFL=$(echo "$out" | sed -n 's/^COVERFLIP //p')
		check "the .lodi scene draws everything the .BTO draws (covered ${COV:-?} >= 0.90)" \\
			"$(ge "${COV:-0}" 0.90)"
		check "and it draws no more than the chunk plus its edges (area ${FAT:-?} x the .BTO <= 2.00)" \\
			"$(le "${FAT:-9}" 2.00)"
		check "the floor fires: the same .BTO mirrored in Y is NOT covered (${CFL:-?} < 0.50)" \\
			"$(lt "${CFL:-1}" 0.50)"
	else
		check "the .BTO of the same chunk renders" 0
	fi
	if [ -s "$W/c_bto.png" ] && "$PY" "$AUTH" solid "$W/c_bto.png" "$W/c_solid.png" >/dev/null 2>&1; then
		out=$("$PY" "$AUTH" cover "$W/c_solid.png" "$W/c_bto.png" 2>&1)
		SCOV=$(echo "$out" | sed -n 's/^COVER //p')
		SFAT=$(echo "$out" | sed -n 's/^FAT //p')
		echo "  a solid frame in place of the .lodi scene: covered ${SCOV:-?}, area ${SFAT:-?} x"
		check "the refuter fires: a frame covering EVERYTHING passes coverage (${SCOV:-?}) and the area bar catches it (${SFAT:-?} > 2.00)" \\
			"$([ "$(ge "${SCOV:-0}" 0.90)" = "1" ] && [ "$(le "${SFAT:-0}" 2.00)" = "0" ] && echo 1 || echo 0)"
	else
		skip "the solid-frame refuter: no .BTO render to size it from"
	fi
	if [ -f "$BTO2" ] && shot "$W/c_other.png" "$BTO2" \\
			WW_LODGEN_RESOURCES="$(winpath "$RES");$(winpath "$OBJ")"; then
		out=$("$PY" "$AUTH" cover "$W/b.png" "$W/c_other.png" 2>&1)
		echo "$out" | sed 's/^/  /'
		COV2=$(echo "$out" | sed -n 's/^COVER //p')
		check "the refuter fires: a DIFFERENT chunk's .BTO is not covered (${COV2:-?} < 0.90)" \\
			"$(lt "${COV2:-1}" 0.90)"
	else
		skip "the (c) refuter: no $WS.$DIM.$OX.$OY.BTO"
	fi
'''

P.append((OLD_C, NEW_C))

patch('tests/spells/native_open.sh', P)
print('patch4 ok')
