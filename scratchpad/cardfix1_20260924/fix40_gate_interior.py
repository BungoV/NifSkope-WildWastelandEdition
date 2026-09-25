# CARDFIX1 step 7, DIRECTOR DECISION (2026-09-25, not a ruling by bungo) after run 4: on BOTH arms and every
# material row, judge only fully covered texels with no 4-neighbour of another material -- COVER=full,interior.
# A boundary texel that mixes two materials is correct behaviour and not what the rows test. The identity
# floors are measured on the same population. LF-only files.
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
    ("    return 255 if os.environ.get(\"COVER\", \"\") == \"full\" else 128\n\n\n"
     "def classes(alb, third):\n    cov = alb[..., 3] >= covmin()\n"
     "    return cov, {0: cov & (third[..., 3] < 128), 1: cov & (third[..., 3] >= 128)}\n",
     "    return 255 if \"full\" in os.environ.get(\"COVER\", \"\").split(\",\") else 128\n\n\n"
     "def interior():\n"
     "    \"\"\"COVER=...,interior (DIRECTOR DECISION 2026-09-25, both arms): a material class keeps only texels with no\n"
     "    4-neighbour of the OTHER class (any coverage). A texel where trunk and leaf meet mixes the two materials:\n"
     "    correct behaviour, and not what a per-material row tests (run 4: 98 % of the non-aa misses).\"\"\"\n"
     "    return \"interior\" in os.environ.get(\"COVER\", \"\").split(\",\")\n\n\n"
     "def near(mask):\n"
     "    p = np.pad(mask, 1)\n"
     "    return p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:]\n\n\n"
     "def classes(alb, third):\n    cov = alb[..., 3] >= covmin()\n"
     "    c1 = third[..., 3] >= 128\n"
     "    cl = {0: cov & ~c1, 1: cov & c1}\n"
     "    if interior():\n"
     "        ink = alb[..., 3] > 0\n"
     "        cl = {0: cl[0] & ~near(ink & c1), 1: cl[1] & ~near(ink & ~c1)}\n"
     "    return cov, cl\n"),
    ("    both = (a[..., 3] >= covmin()) & (r[..., 3] >= covmin())\n",
     "    both = (a[..., 3] >= covmin()) & (r[..., 3] >= covmin())\n"
     "    if interior():   # the same population the colour rows judge: the identity card's own material classes\n"
     "        _, cl = classes(a, load(idcards, ident, \"rmaos\"))\n"
     "        both &= cl[0] | cl[1]\n"),
])

patch(SH, [
    ("#       edge law every channel shares (run 3: 14/15 FAIL, all partial texels). The same reds run on it.\n",
     "#       edge law every channel shares (run 3: 14/15 FAIL, all partial texels). The same reds run on it.\n"
     "#   RULE COVER=full,interior -- DIRECTOR DECISION 2026-09-25 after run 4, BOTH arms, every material row and\n"
     "#       both identity floors: fully covered texels (alpha 255) with no 4-neighbour of the other material.\n"
     "#       A texel where two materials meet mixes them, correctly; run 4's non-aa misses were 98 % such texels.\n"),
    ("\t# env: FL (the floor for this arm), CV (COVER: empty = alpha >= 128, full = coverage == 1)\n",
     "\t# env: FL (the floor for this arm), CV (COVER: empty = alpha >= 128, full = coverage == 1, interior = away\n"
     "\t# from the other material; the gate runs full,interior on both arms)\n"),
    ("v=$( \"$PY\" \"$here/impostor_pbrm.py\" floor \"$WORK/ident/bake\" \"$WORK/legacy/bake\" \"$ID\" 2>&1 | tail -1 ); say \"$v\"\n",
     "CVR=full,interior\n"
     "v=$( COVER=\"$CVR\" \"$PY\" \"$here/impostor_pbrm.py\" floor \"$WORK/ident/bake\" \"$WORK/legacy/bake\" \"$ID\" 2>&1 | tail -1 ); say \"aa, COVER=$CVR: $v\"\n"),
    ("v=$( COVER=full \"$PY\" \"$here/impostor_pbrm.py\" floor \"$WORK/identna/bake\" \"$WORK/legacyna/bake\" \"$ID\" 2>&1 | tail -1 ); say \"non-aa, coverage == 1: $v\"\n",
     "v=$( COVER=\"$CVR\" \"$PY\" \"$here/impostor_pbrm.py\" floor \"$WORK/identna/bake\" \"$WORK/legacyna/bake\" \"$ID\" 2>&1 | tail -1 ); say \"non-aa, COVER=$CVR: $v\"\n"),
    ("\tif [ \"$3\" = non-aa ]; then FL=\"$FLOORNA\" CV=full; else FL=\"$FLOOR\" CV=; fi\n",
     "\tif [ \"$3\" = non-aa ]; then FL=\"$FLOORNA\"; else FL=\"$FLOOR\"; fi; CV=\"$CVR\"\n"),
    ("\tif [ \"$4\" = non-aa ]; then FL=\"$FLOORNA\" CV=full; else FL=\"$FLOOR\" CV=; fi\n",
     "\tif [ \"$4\" = non-aa ]; then FL=\"$FLOORNA\"; else FL=\"$FLOOR\"; fi; CV=\"$CVR\"\n"),
])
