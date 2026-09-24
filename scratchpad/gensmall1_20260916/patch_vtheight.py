#!/usr/bin/env python
"""Item 3: pin --vt-height's default OFF inside lodgen_terrain_vt.sh.

One extra bake (the run1 profile with the flag taken away) and ONE check. The
check is a number with a stated floor, not a "the flag seems to do something":
the contract page says a tile goes 138,720 -> 323,680 bytes when the height
layer is carried, a factor of 2.333, so the pyramid the flag produces must be
at least twice the one the default produces, and the default one must not
carry the layer at all.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_terrain_vt.sh'

BAKE_ANCHOR = b'vt novtnc --tex-dir "$W/novtnc/tex" || { echo "the cover-free control failed"; exit 1; }\n'

BAKE_NEW = BAKE_ANCHOR + b'''# ITEM 3 (lane GENSMALL1, 2026-09-16): --vt-height is OFF BY DEFAULT and this is
# the pin on that. run1 is the same profile WITH the flag, so the only
# difference between the two bakes is the flag itself -- the way back from the
# height layer is "do not pass it", and a way back that nothing measures is a
# claim. The CLI table in docs/LODGEN_TERRAIN_VT.md 5 gained the row this lane
# is pinning; the panel side (LodgenVtHeightCheck -> vtHeight, unticked) is
# already inside lod_generation.sh's 57-row group, which reports missing 0,
# not round-tripping through QSettings 0, without a tooltip 0.
vt run1noh --vt "$W/run1noh/mod" --tex-dir "$W/run1noh/tex" --cover \\
\t|| { echo "the no-height --vt bake failed"; tail -8 "$W/run1noh.log"; exit 1; }
'''

CHECK_ANCHOR = b'''else
\tbad "V9 both an assembled and a direct chunk sheet exist to compare"
fi
'''

CHECK_NEW = CHECK_ANCHOR + b'''
# V10 (lane GENSMALL1, 2026-09-16), item 3: the height layer is opt-in.
#
# docs/LODGEN_TERRAIN_VT.md 2.2 and 3.6: the height sheet is the one layer that
# is NOT block-compressed, so at content 256 / border 8 / 2 mips it is 184,960
# bytes against a BC1 sheet's 46,240, and a tile goes 138,720 -> 323,680 bytes,
# a factor of 2.333. The bar below is 1.80, comfortably under that factor and
# far above 1.0, so it discriminates: a build that carried the layer by default
# would read about 1.00 and go red, and so would one that ignored the flag.
H_ON="$(grep -a '^vt:' "$W/run1.log" | sed -n 's/.* bytes \\([0-9][0-9]*\\) .*/\\1/p' | head -1)"
H_OFF="$(grep -a '^vt:' "$W/run1noh.log" | sed -n 's/.* bytes \\([0-9][0-9]*\\) .*/\\1/p' | head -1)"
say "pyramid bytes: default $H_OFF, with --vt-height $H_ON"
if [ -n "$H_ON" ] && [ -n "$H_OFF" ] && [ "$H_OFF" -gt 0 ] \\
	&& [ "$((H_ON * 100 / H_OFF))" -ge 180 ]; then
\tsay "ratio $((H_ON * 100 / H_OFF))/100, bar 180/100"
\tok "V10 --vt-height is OFF by default and the flag is what turns the height layer on"
else
\tsay "ratio $([ -n "$H_OFF" ] && [ "$H_OFF" -gt 0 ] && echo "$((H_ON * 100 / H_OFF))" || echo n/a)/100, bar 180/100"
\tbad "V10 --vt-height is OFF by default and the flag is what turns the height layer on"
fi
'''

d = open(P, 'rb').read()
if d.count(BAKE_ANCHOR) != 1:
    sys.exit('bake anchor appears %d times' % d.count(BAKE_ANCHOR))
if d.count(CHECK_ANCHOR) != 1:
    sys.exit('check anchor appears %d times' % d.count(CHECK_ANCHOR))
out = d.replace(BAKE_ANCHOR, BAKE_NEW).replace(CHECK_ANCHOR, CHECK_NEW)
open(P, 'wb').write(out)
print('lodgen_terrain_vt.sh  %d -> %d bytes  CR %d -> %d  LF %d -> %d'
      % (len(d), len(out), d.count(b'\r'), out.count(b'\r'),
         d.count(b'\n'), out.count(b'\n')))
