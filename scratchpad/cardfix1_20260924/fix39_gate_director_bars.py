# CARDFIX1 step 7, DIRECTOR DECISIONS (2026-09-25, not rulings by bungo) on impostor_pbrm's two reds:
# (1) the colour rows' bar = max(1.25 x identity floor, 0.5 level): one 8-bit rounding plus margin;
# (2) the non-aa arm is judged on FULLY COVERED texels only (coverage == 1, alpha 255), with its own
#     identity floor on that population and the same breakages run on it. LF-only files.
PY = 'E:/Projects/NifskopeWWE-cardfix1/tests/spells/impostor_pbrm.py'
SH = 'E:/Projects/NifskopeWWE-cardfix1/tests/spells/impostor_pbrm.sh'


def patch(P, pairs):
    b = open(P, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for o, n in pairs:
        assert s.count(o) == 1, (P, o[:70], s.count(o))
        s = s.replace(o, n)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr
    open(P, 'wb').write(out)
    print('patched %s: %+d bytes, CR %d' % (P.split('/')[-1], len(out) - len(b), cr))


patch(PY, [
    ("def classes(alb, third):\n    cov = alb[..., 3] >= 128\n",
     "def covmin():\n"
     "    \"\"\"The covered-texel threshold. COVER=full (DIRECTOR DECISION 2026-09-25, the non-aa arm): coverage == 1\n"
     "    only, alpha 255 -- that arm un-premultiplies partially covered texels by the matte, an edge law of its own\n"
     "    that every channel (old and new) shares. Default: alpha >= 128.\"\"\"\n"
     "    return 255 if os.environ.get(\"COVER\", \"\") == \"full\" else 128\n\n\n"
     "def classes(alb, third):\n    cov = alb[..., 3] >= covmin()\n"),
    ("    the colour row: median |pbrm - law(legacy)| <= the identity floor x 1.25 (passed in env FLOOR).\"\"\"\n",
     "    the colour row: median |pbrm - law(legacy)| <= max(the identity floor x 1.25, 0.5) (FLOOR in env; the 0.5\n"
     "    minimum is a DIRECTOR DECISION 2026-09-25: one 8-bit rounding plus margin, since the floor can reach 0).\n"
     "    COVER=full judges fully covered texels only (see covmin).\"\"\"\n"),
    ("        both = m & (ref[..., 3] >= 128)\n        lin_ref",
     "        both = m & (ref[..., 3] >= covmin())\n        lin_ref"),
    ("        bar = 1.25 * floor\n        med = float(np.median(e))\n        good = np.isfinite(bar) and med <= bar\n"
     "        print(\"row %s colour %s: median |card - law(legacy card)| %.2f levels, p90 %.1f, over %d texels \"\n"
     "              \"(bar = identity floor %.2f x 1.25 = %.2f); gain %s\"\n"
     "              % (name, \"ok\" if good else \"FAIL\", med, float(np.percentile(e, 90)), bright.sum(), floor, bar,\n",
     "        bar = max(1.25 * floor, 0.5) if np.isfinite(floor) else float(\"nan\")\n        med = float(np.median(e))\n"
     "        good = np.isfinite(bar) and med <= bar\n"
     "        print(\"row %s colour %s: median |card - law(legacy card)| %.2f levels, p90 %.1f, over %d texels \"\n"
     "              \"(bar = max(identity floor %.2f x 1.25, 0.5) = %.2f; margin %+.2f); gain %s\"\n"
     "              % (name, \"ok\" if good else \"FAIL\", med, float(np.percentile(e, 90)), bright.sum(), floor, bar,\n"
     "                 bar - med,\n"),
    ("    both = (a[..., 3] >= 128) & (r[..., 3] >= 128)\n",
     "    both = (a[..., 3] >= covmin()) & (r[..., 3] >= covmin())\n"),
])

patch(SH, [
    ("#       no tint, the same maps -- baked the same way, against the legacy card).\n"
     "#   R2  the same rows on the NON-aa arm (WW_IMPOSTOR_AA=0) against a non-aa legacy card.\n",
     "#       no tint, the same maps -- baked the same way, against the legacy card).\n"
     "#       DIRECTOR DECISION 2026-09-25 (not a ruling by bungo): colour bar = max(1.25 x floor, 0.5 level), one\n"
     "#       8-bit rounding plus margin -- the floor reached 0 once the colour mips were right (run 3).\n"
     "#   R2  the same rows on the NON-aa arm (WW_IMPOSTOR_AA=0) against a non-aa legacy card, JUDGED ON FULLY\n"
     "#       COVERED TEXELS ONLY (coverage == 1, alpha 255; COVER=full), with that arm's own identity floor on that\n"
     "#       population. DIRECTOR DECISION 2026-09-25: the arm un-premultiplies partial texels by the matte, an\n"
     "#       edge law every channel shares (run 3: 14/15 FAIL, all partial texels). The same reds run on it.\n"),
    ("bake ident    \"$EXE\"  \"$IDF\"\n",
     "bake ident    \"$EXE\"  \"$IDF\"\nbake identna  \"$EXE\"  \"$IDF\"   WW_IMPOSTOR_AA=0\n"),
    ("bake prev     \"$PREV\" \"$FIX\"\n",
     "bake prev     \"$PREV\" \"$FIX\"\nbake prevna   \"$PREV\" \"$FIX\"   WW_IMPOSTOR_AA=0\n"),
    ("if [ -z \"$FLOOR\" ] || [ \"$fam\" != pbr ]; then bad \"floor not measured (identity card family '${fam:-?}', median '${FLOOR:-}')\"; FLOOR=nan; fi\n",
     "if [ -z \"$FLOOR\" ] || [ \"$fam\" != pbr ]; then bad \"floor not measured (identity card family '${fam:-?}', median '${FLOOR:-}')\"; FLOOR=nan; fi\n"
     "v=$( COVER=full \"$PY\" \"$here/impostor_pbrm.py\" floor \"$WORK/identna/bake\" \"$WORK/legacyna/bake\" \"$ID\" 2>&1 | tail -1 ); say \"non-aa, coverage == 1: $v\"\n"
     "FLOORNA=$( num \"$v\" \"median\" )\n"
     "fam=$( echo \"$v\" | grep -oE \"family [a-z]+\" | awk '{print $2}' )\n"
     "if [ -z \"$FLOORNA\" ] || [ \"$fam\" != pbr ]; then bad \"non-aa floor not measured (identity card family '${fam:-?}', median '${FLOORNA:-}')\"; FLOORNA=nan; fi\n"),
    ("rows() {   # $1 label  $2 cards  $3 refcards  rest = --red X ; prints the rows, counts FAILs into $nf\n"
     "\tlocal rl=\"$1\" rc=\"$2\" rr=\"$3\"; shift 3\n"
     "\tout=$( FLOOR=\"$FLOOR\" \"$PY\"",
     "rows() {   # $1 label  $2 cards  $3 refcards  rest = --red X ; prints the rows, counts FAILs into $nf\n"
     "\t# env: FL (the floor for this arm), CV (COVER: empty = alpha >= 128, full = coverage == 1)\n"
     "\tlocal rl=\"$1\" rc=\"$2\" rr=\"$3\"; shift 3\n"
     "\tout=$( FLOOR=\"${FL:-$FLOOR}\" COVER=\"${CV:-}\" \"$PY\""),
    ("for arm in \"pbrm legacy aa\" \"pbrmna legacyna non-aa\"; do\n\tset -- $arm\n\trows \"$3\" \"$WORK/$1/bake\" \"$WORK/$2/bake\"\n",
     "for arm in \"pbrm legacy aa\" \"pbrmna legacyna non-aa\"; do\n\tset -- $arm\n"
     "\tif [ \"$3\" = non-aa ]; then FL=\"$FLOORNA\" CV=full; else FL=\"$FLOOR\" CV=; fi\n"
     "\trows \"$3\" \"$WORK/$1/bake\" \"$WORK/$2/bake\"\n"),
    ("for red in add ior decode; do\n\trows \"red-$red\" \"$WORK/pbrm/bake\" \"$WORK/legacy/bake\" --red \"$red\"\n"
     "\tif [ \"$nf\" -ge 1 ]; then ok \"red control --red $red: $nf row(s) FAIL ($( echo \"$out\" | grep -m1 FAIL | cut -c1-90 ))\"\n"
     "\telse bad \"red control --red $red did not bite: all $nr rows ok\"; fi\ndone\n"
     "rows \"prev\" \"$WORK/prev/bake\" \"$WORK/legacy/bake\"\n"
     "if [ \"$nf\" -ge 1 ]; then ok \"red control, the pre-step-7 exe: $nf row(s) FAIL ($( echo \"$out\" | grep -m1 FAIL | cut -c1-90 ))\"\n"
     "else bad \"red control, the pre-step-7 exe, did not bite: all $nr rows ok\"; fi\n",
     "for arm in \"pbrm legacy prev aa\" \"pbrmna legacyna prevna non-aa\"; do\n\tset -- $arm\n"
     "\tif [ \"$4\" = non-aa ]; then FL=\"$FLOORNA\" CV=full; else FL=\"$FLOOR\" CV=; fi\n"
     "\tfor red in add ior decode; do\n"
     "\t\trows \"red-$red-$4\" \"$WORK/$1/bake\" \"$WORK/$2/bake\" --red \"$red\"\n"
     "\t\tif [ \"$nf\" -ge 1 ]; then ok \"red control --red $red ($4): $nf row(s) FAIL\"; echo \"$out\" | grep FAIL | sed 's/^/        /' | tee -a \"$log\"\n"
     "\t\telse bad \"red control --red $red ($4) did not bite: all $nr rows ok\"; fi\n"
     "\tdone\n"
     "\trows \"prev-$4\" \"$WORK/$3/bake\" \"$WORK/$2/bake\"\n"
     "\tif [ \"$nf\" -ge 1 ]; then ok \"red control, the pre-step-7 exe ($4): $nf row(s) FAIL\"; echo \"$out\" | grep FAIL | sed 's/^/        /' | tee -a \"$log\"\n"
     "\telse bad \"red control, the pre-step-7 exe ($4), did not bite: all $nr rows ok\"; fi\n"
     "done\nFL=; CV=\n"),
])
