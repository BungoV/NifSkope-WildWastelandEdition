# Patch 1: teach the authority a `manifest` job that splits a .BTO manifest's
# placement rows into the ones inside the chunk footprint and the ones outside.
# Patch 2: make gate (b)'s manifest leg compare the INSIDE count against the
# identity-on bake's own .lodi chunk table and against the viewer's census of
# that same file.

AUTH = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/native_open_authority.py'
SH = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/native_open.sh'

s = open(AUTH, 'rb').read().decode('utf-8')
anchor = "JOBS = {'chunk': job_chunk, 'compare': job_compare, 'iou': job_iou, 'mad': job_mad}"
assert s.count(anchor) == 1, 'auth JOBS anchor %d' % s.count(anchor)

job = '''def job_manifest(argv):
    """manifest <manifest.txt> <cellX> <cellY> <dim>

    A .BTO manifest carries every placement the chunk DRAWS, and that includes a
    few whose origin sits just outside the chunk's own cells (measured on
    (-20,24) dim 4: two trees, 46 and 117 units out).  The .lodi chunk table is
    keyed by the cell the origin falls in, so the two counts can only be
    compared after those rows are separated out.  Both counts are printed."""
    path, cx, cy, dim = argv[0], int(argv[1]), int(argv[2]), int(argv[3])
    x0 = cx * 4096.0
    x1 = (cx + dim) * 4096.0
    y0 = cy * 4096.0
    y1 = (cy + dim) * 4096.0
    inside = 0
    outside = []
    for line in open(path, encoding='utf-8', errors='replace'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        t = line.split()
        if len(t) < 11 or not t[0].isdigit():
            continue          # the I / A / M rows of the same file
        try:
            x, y = float(t[3]), float(t[4])
        except ValueError:
            continue
        if x0 <= x <= x1 and y0 <= y <= y1:
            inside += 1
        else:
            outside.append((t[9], t[10], x, y))
    print('manifest.rows %d' % (inside + len(outside)))
    print('manifest.inside %d' % inside)
    print('manifest.outside %d' % len(outside))
    for ref, part, x, y in outside[:20]:
        dx = 0.0 if x0 <= x <= x1 else (x0 - x if x < x0 else x - x1)
        dy = 0.0 if y0 <= y <= y1 else (y0 - y if y < y0 else y - y1)
        print('  outside %s %s by %.1f,%.1f units' % (ref, part, dx, dy))
    return 0


'''
newjobs = ("JOBS = {'chunk': job_chunk, 'compare': job_compare, 'iou': job_iou,\n"
           "        'mad': job_mad, 'manifest': job_manifest}")
s = s.replace(anchor, job + newjobs, 1)
old = "die('usage: %s {chunk|compare|iou|mad} ...'"
new = "die('usage: %s {chunk|compare|iou|mad|manifest} ...'"
assert s.count(old) == 1
s = s.replace(old, new)
open(AUTH, 'wb').write(s.encode('utf-8'))
print('authority patched')

# ---------------------------------------------------------------- the harness
h = open(SH, 'rb').read().decode('utf-8')
old = '''if [ -n "$MAN" ] && [ -f "$MAN" ]; then
	M_N=$(grep -cE '^[0-9]+[[:space:]]' "$MAN")
	echo "  $(basename "$MAN") carries $M_N placement rows"
	check "the manifest of the SAME bake carries the same number of rows ($M_N == ${A_N:-?})" \\
		"$([ "$M_N" = "${A_N:-x}" ] && echo 1 || echo 0)"
	GC="$GNATIVE/$WS.lodo"; GI="$GNATIVE/$WS.lodi"
	if [ -f "$GC" ] && [ -f "$GI" ]; then
		"$PY" "$ROOT/tests/spells/lodgen_native_decode.py" "$GC" "$GI" --manifest "$MAN" 2>&1 \\
			| sed 's/^/    /' | tail -12
	fi
'''
new = '''if [ -n "$MAN" ] && [ -f "$MAN" ]; then
	MANOUT="$W/manifest.txt"
	"$PY" "$AUTH" manifest "$MAN" $CX $CY $DIM > "$MANOUT" 2>&1
	sed 's/^/  /' "$MANOUT"
	M_ALL=$(sed -n 's/^manifest.rows //p' "$MANOUT")
	M_IN=$(sed -n 's/^manifest.inside //p' "$MANOUT")
	# the manifest and the .lodi belong to the SAME (identity-on) bake, so the
	# count it must agree with is that bake's own .lodi, not the look arm's
	GC="$GNATIVE/$WS.lodo"; GI="$GNATIVE/$WS.lodi"
	G_A=""; G_C=""
	if [ -f "$GC" ] && [ -f "$GI" ]; then
		G_AUTH="$W/auth_gate.txt"
		"$PY" "$AUTH" chunk "$GC" "$GI" $CX $CY $DIM > "$G_AUTH" 2>&1
		G_A=$(sed -n '1s/^chunk.instances //p' "$G_AUTH")
		G_CEN="$W/census_gate.txt"
		if shot "$W/b_gate.png" "$GI" \\
				WW_LODI_REGION="$CX,$CY,$((CX+DIM-1)),$((CY+DIM-1))" \\
				WW_LODI_DUMP="$(winpath "$G_CEN")" \\
				WW_LODGEN_RESOURCES="$(winpath "$RES");$(winpath "$GBAKE")"; then
			G_C=$(grep -cv '^#' "$G_CEN" 2>/dev/null || echo 0)
		fi
		echo "  the identity-on bake: .lodi says ${G_A:-?}, the viewer placed ${G_C:-?}, the manifest has $M_ALL rows of which $M_IN are inside the chunk"
		check "the three counts of the SAME bake agree (manifest inside $M_IN == .lodi ${G_A:-?} == viewer ${G_C:-?})" \\
			"$([ -n "$G_A" ] && [ "$M_IN" = "$G_A" ] && [ "$G_C" = "$G_A" ] && echo 1 || echo 0)"
		if [ -s "$G_CEN" ]; then
			cmpout=$("$PY" "$AUTH" compare "$G_CEN" "$G_AUTH" 1.0 2>&1); cmprc=$?
			echo "$cmpout" | sed 's/^/  /'
			check "every identity-on placement is within 1 unit of its own file" \\
				"$([ "$cmprc" = "0" ] && echo 1 || echo 0)"
		fi
		"$PY" "$ROOT/tests/spells/lodgen_native_decode.py" "$GC" "$GI" --manifest "$MAN" 2>&1 \\
			| sed 's/^/    /' | tail -12
	else
		check "the identity-on bake's own .lodo/.lodi sit under GNATIVE" 0
	fi
'''
assert h.count(old) == 1, 'harness (b) anchor %d' % h.count(old)
h = h.replace(old, new)
open(SH, 'wb').write(h.encode('utf-8'))
print('harness patched')
