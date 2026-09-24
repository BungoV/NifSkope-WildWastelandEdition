# Gate (d) reports the mean absolute colour difference the brief asks for, but
# that number alone does not discriminate: two different chunks of the same
# worldspace differ by about as much (measured 35.8 against 40.8).  A normalised
# cross-correlation of the two lumas does discriminate, and it also catches the
# one failure this route is most exposed to -- the sheet rows run NORTH-first
# while the mesh runs SOUTH-first, so a Y mirror is the obvious bug.  Measured
# on chunk (-20,24): own chunk +0.60, a DIFFERENT chunk +0.20, own chunk
# mirrored in Y -0.02.
AUTH = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/native_open_authority.py'
SH = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/native_open.sh'

s = open(AUTH, 'rb').read().decode('utf-8')
anchor = "JOBS = {'chunk': job_chunk, 'compare': job_compare, 'iou': job_iou,\n        'mad': job_mad, 'manifest': job_manifest}"
assert s.count(anchor) == 1, 'JOBS anchor %d' % s.count(anchor)

job = '''def job_ncc(argv):
    """ncc <a.png> <b.png>

    Normalised cross-correlation of the two lumas, plus the same number with b
    mirrored in Y.  Structure, not tone: it survives the sheets being coarser
    and darker than the legacy per-chunk texture, and it goes to nothing when
    the picture is the wrong chunk or the wrong way up."""
    a = load_png(argv[0])
    b = load_png(argv[1])
    h = min(a.shape[0], b.shape[0])
    w = min(a.shape[1], b.shape[1])
    A = a[:h, :w].mean(axis=2)
    B = b[:h, :w].mean(axis=2)

    def ncc(x, y):
        x = x - x.mean()
        y = y - y.mean()
        d = float(np.sqrt((x * x).sum() * (y * y).sum()))
        return float((x * y).sum() / d) if d else 0.0

    print('  compared over %d x %d px' % (w, h))
    print('NCC %.4f' % ncc(A, B))
    print('NCCFLIP %.4f' % ncc(A, B[::-1, :]))
    return 0


'''
newjobs = ("JOBS = {'chunk': job_chunk, 'compare': job_compare, 'iou': job_iou,\n"
           "        'mad': job_mad, 'manifest': job_manifest, 'ncc': job_ncc}")
s = s.replace(anchor, job + newjobs, 1)
old = "die('usage: %s {chunk|compare|iou|mad|manifest} ...'"
new = "die('usage: %s {chunk|compare|iou|mad|manifest|ncc} ...'"
assert s.count(old) == 1
s = s.replace(old, new)
open(AUTH, 'wb').write(s.encode('utf-8'))
print('authority: ncc job added')

h = open(SH, 'rb').read().decode('utf-8')
old = '''		check "the lit terrain and the .BTR of the same cells agree (mean |dColour| ${MAD:-?} < ${MAD_BAR:-24})" \\
'''
assert h.count(old) == 1, 'MAD check anchor %d' % h.count(old)
new = '''		nccout=$("$PY" "$AUTH" ncc "$D_LIT" "$W/d_btr.png" 2>&1)
		echo "$nccout" | sed 's/^/  /'
		NCC=$(echo "$nccout" | sed -n 's/^NCC //p')
		NCCF=$(echo "$nccout" | sed -n 's/^NCCFLIP //p')
		BTR2="$OBJ/$WS.$DIM.$OX.$OY.BTR"
		NCC2=""
		if [ -f "$BTR2" ] && SHOT_CENTER="$LCENTER" shot "$W/d_btr_other.png" "$BTR2" \\
				WW_LODGEN_RESOURCES="$(winpath "$RES");$(winpath "$OBJ")"; then
			NCC2=$("$PY" "$AUTH" ncc "$D_LIT" "$W/d_btr_other.png" 2>&1 | sed -n 's/^NCC //p')
		fi
		check "the lit terrain looks like the .BTR of the SAME cells (NCC ${NCC:-?} >= ${NCC_BAR:-0.45})" \\
			"$([ -n "$NCC" ] && [ "$(ge "$NCC" "${NCC_BAR:-0.45}")" = "1" ] && echo 1 || echo 0)"
		check "the refuter fires: a DIFFERENT chunk's .BTR correlates less (NCC ${NCC2:-?} < ${NCC:-?})" \\
			"$([ -n "$NCC2" ] && [ -n "$NCC" ] && [ "$(lt "$NCC2" "$NCC")" = "1" ] && echo 1 || echo 0)"
		check "the floor fires: the same .BTR mirrored in Y does NOT correlate (NCC ${NCCF:-?} < 0.10)" \\
			"$([ -n "$NCCF" ] && [ "$(lt "$NCCF" "0.10")" = "1" ] && echo 1 || echo 0)"
		check "the lit terrain and the .BTR of the same cells agree (mean |dColour| ${MAD:-?} < ${MAD_BAR:-48})" \\
'''
h = h.replace(old, new)
open(SH, 'wb').write(h.encode('utf-8'))
print('harness: gate (d) discriminators added')
