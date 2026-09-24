# CARDFIX1 step 6c: DIRECTOR DECISION on G4 (2026-09-25, option (a); not a ruling by bungo). G4's bar becomes
# the codec floor of the SAME sheet -- the BC7 error of its other channels (normal R/G), measured in the same
# run -- times the margin the skill ww-preregister-bar-from-the-subject prescribes (x 1.25 on the mean, x 1.25
# rounded up to a whole level on the p95). BC7 weights unchanged: option (b) would move every card's normals for
# a sway error of about 1.4 % of full scale, which nobody can see.
# Two red controls must fail the SAME bar: the next frame's picture (a wrong weight), and a CORRUPTED sway
# channel -- the decoded alpha cut to 4 bits (16 levels), a codec about 3x coarser than BC7's own.
# Both files LF-only.
R = 'E:/Projects/NifskopeWWE-cardfix1/tests/spells/'


def patch(name, pairs):
    p = R + name
    b = open(p, 'rb').read(); assert b.count(b'\r') == 0
    s = b.decode('utf-8')
    for o, n in pairs:
        assert s.count(o) == 1, (name, o[:60], s.count(o))
        s = s.replace(o, n)
    out = s.encode('utf-8'); assert out.count(b'\r') == 0
    open(p, 'wb').write(out); print('patched', name)


patch('impostor_wind.py', [
    ("    erg = np.concatenate([np.abs(Rd - nrm[..., 0])[cov], np.abs(Gd - nrm[..., 1])[cov]])\n",
     "    erg = np.concatenate([np.abs(Rd - nrm[..., 0])[cov], np.abs(Gd - nrm[..., 1])[cov]])\n"
     "    Aq = np.floor(Ad / 16.0) * 16.0 + 8.0              # RED: the sway channel corrupted to 4 bits\n"
     "    eq = np.abs(Aq - A)[cov]\n"),
    ("          + ' | normal R/G error mean %.3f p95 %.1f' % (erg.mean(), np.percentile(erg, 95)))\n",
     "          + ' | normal R/G error mean %.3f p95 %.1f' % (erg.mean(), np.percentile(erg, 95))\n"
     "          + ' | 4-bit sway: mean %.3f p95 %.1f' % (eq.mean(), np.percentile(eq, 95)))\n"),
])

patch('impostor_wind.sh', [
    ("#   G4  BC7 on the real weight: the compressed _n alpha against the bake's own PNG, covered texels, elm:\n"
     "#       mean <= 3.0 levels, p95 <= 12 (the synthetic law measured 1.34 / 4; real W has hard 0/255 steps).\n"
     "#       RED: the same metric against the NEXT frame's PNG must exceed it.\n",
     "#   G4  BC7 on the real weight: the compressed _n alpha against the bake's own PNG, covered texels, elm.\n"
     "#       BAR = CODEC FLOOR OF THE SHEET'S OTHER CHANNELS, MEASURED: the same sheet's normal R/G error in the\n"
     "#       same run, x 1.25 (mean) and x 1.25 rounded up (p95) -- the margin of the skill\n"
     "#       ww-preregister-bar-from-the-subject. DIRECTOR DECISION 2026-09-25 (option a): the first bar, 3.0 / 12,\n"
     "#       was copied from the synthetic law's 1.34 / 4 and failed correct code at 3.573 / 13 against an R/G\n"
     "#       floor of 3.266 / 12 (gates/impostor_wind.run3.out). The BC7 weights were NOT raised: that would move\n"
     "#       every card's normals for a sway error of about 1.4 % of full scale.\n"
     "#       RED (both must fail the same bar): the NEXT frame's picture; the sway channel corrupted to 4 bits.\n"),
    ("m=$( num \"$v\" \"sway error mean\" ); p=$( num \"${v#*sway error mean}\" \"p95\" ); ms=$( num \"${v#*next frame:}\" \"mean\" )\n"
     "if le \"${m:-99}\" 3.0 && le \"${p:-99}\" 12; then ok \"G4 BC7 on the real weight: mean $m <= 3.0, p95 $p <= 12\"\n"
     "else bad \"G4 BC7 on the real weight: mean ${m:-?}, p95 ${p:-?} (bar 3.0 / 12)\"; fi\n"
     "if ! le \"${ms:-0}\" 3.0; then ok \"G4 red control: against the next frame's picture the same metric reads $ms > 3.0\"\n"
     "else bad \"G4 red control did not bite: the next frame reads ${ms:-?}\"; fi\n",
     "m=$( num \"$v\" \"sway error mean\" ); p=$( num \"${v#*sway error mean}\" \"p95\" ); ms=$( num \"${v#*next frame:}\" \"mean\" )\n"
     "fm=$( num \"${v#*normal R/G error}\" \"mean\" ); fp=$( num \"${v#*normal R/G error}\" \"p95\" )\n"
     "qm=$( num \"${v#*4-bit sway:}\" \"mean\" ); qp=$( num \"${v#*4-bit sway:}\" \"p95\" )\n"
     "bm=$( \"$PY\" -c \"print('%.3f' % (1.25 * float('${fm:-0}')))\" ); bp=$( \"$PY\" -c \"import math; print(math.ceil(1.25 * float('${fp:-0}')))\" )\n"
     "say \"G4 bar = codec floor of the sheet's other channels, measured: normal R/G mean ${fm:-?} p95 ${fp:-?}, x 1.25 -> $bm / $bp\"\n"
     "if [ -z \"$fm\" ] || ! ge \"$fm\" 0.5; then bad \"G4 the codec floor was not measured (normal R/G mean '${fm:-}'): no bar\"\n"
     "elif le \"${m:-99}\" \"$bm\" && le \"${p:-99}\" \"$bp\"; then ok \"G4 BC7 on the real weight: mean $m <= $bm, p95 $p <= $bp (bar = codec floor of the sheet's other channels, measured)\"\n"
     "else bad \"G4 BC7 on the real weight: mean ${m:-?}, p95 ${p:-?} (bar = codec floor of the sheet's other channels, measured: $bm / $bp)\"; fi\n"
     "if ! le \"${ms:-0}\" \"$bm\"; then ok \"G4 red control: against the next frame's picture the same metric reads $ms > $bm\"\n"
     "else bad \"G4 red control did not bite: the next frame reads ${ms:-?}\"; fi\n"
     "if ! { le \"${qm:-0}\" \"$bm\" && le \"${qp:-0}\" \"$bp\"; }; then ok \"G4 red control: the sway channel corrupted to 4 bits fails the same bar (mean $qm, p95 $qp)\"\n"
     "else bad \"G4 red control did not bite: the 4-bit sway reads mean ${qm:-?}, p95 ${qp:-?} within $bm / $bp\"; fi\n"),
])
