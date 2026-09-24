"""Lane CELLVIEW4B, fix 03 -- tests/spells/cell_pick.sh rows 6-7 read the
BLENDED census as well as the mosaic one, plus two new rows for the budget and
the refusal.

WHY THE ROWS FAILED, AND WHY THAT IS NOT A REGRESSION.  Rows 6-7 scrape the
ground census line.  CELLVIEW4 replaced the mosaic's legend with the splat's,
which says different words for the same facts, so every `sed` in the block
captured nothing, every variable defaulted, and four rows went red against a
viewer that was working.  Quoted from the run of 20:44:

    ground: 0 textures, 0 textured quads, 0 from a layer
    ground bare: ? = ? unpainted + ? LTEX with no texture

while the notes file said

    ground: 1 LAND cells (1 with VCLR), 1024 land quads drawn as 2234 passes
    over 11 landscape textures -- 1018 opaque base, 1216 blended layer, 6 bare

The patterns below read BOTH wordings, deliberately: the mosaic is still the
fallback arm and a gate that can only read the fast arm cannot see the fallback
break.  The row MEANINGS are unchanged -- more than one texture, the bare quads
add up, the bare count is within the independent count, the ATXT layers are
actually read -- and each keeps a shape that fails on a broken reader.
"""
import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
REL = 'tests/spells/cell_pick.sh'
CHECK = '--check' in sys.argv

OLD = '''tex=$(sed -n 's/.*quads over \\([0-9,]*\\) landscape textures.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
textured=$(sed -n 's/.*landscape textures -- \\([0-9,]*\\) textured.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
bare=$(sed -n 's/.*textured, \\([0-9,]*\\) bare.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
layered=$(sed -n 's/.*; \\([0-9,]*\\) quads took an ATXT layer.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
unpainted=$(sed -n 's/.*bare quads \\([0-9,]*\\) are unpainted.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
notex=$(sed -n 's/.*and \\([0-9,]*\\) chose an LTEX that named no texture.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
say "  ground: ${tex:-0} textures, ${textured:-0} textured quads, ${layered:-0} from a layer"
say "  ground bare: ${bare:-?} = ${unpainted:-?} unpainted + ${notex:-?} LTEX with no texture"'''

NEW = '''# TWO WORDINGS, ONE SET OF FACTS (lane CELLVIEW4B). The blend (src/cellsplat.cpp)
# and the mosaic fallback (src/cellground.cpp) both write this line and they say
# it differently. Reading only one of them is how four rows went red on 2026-09-19
# against a viewer that was working -- and reading only the BLEND would leave the
# fallback arm untested, which is worse. Every pattern below accepts either.
#
#   blend   ... drawn as 2234 passes over 11 landscape textures -- 1018 opaque
#               base, 1216 blended layer, 6 bare; ...
#   mosaic  ... quads over 11 landscape textures -- 1000 textured, 24 bare; ...
#               24 quads took an ATXT layer over the quadrant's BTXT
tex=$(sed -n 's/.*over \\([0-9,]*\\) landscape textures.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
quadstotal=$(sed -n 's/.*(\\?[0-9,]* with VCLR)\\?, \\([0-9,]*\\) land quads.*/\\2/p' "$g" | head -1 | tr -dc '0-9')
bare=$(sed -n 's/.*[,-] \\([0-9,]*\\) bare[;,].*/\\1/p' "$g" | head -1 | tr -dc '0-9')
# "textured" is the row's real subject: quads that resolved SOMETHING. The mosaic
# states it; the blend states the complement, so it is derived, and the derivation
# is printed so a reader can see which arm answered.
textured=$(sed -n 's/.*landscape textures -- \\([0-9,]*\\) textured.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
if [ -z "${textured:-}" ] && [ -n "${quadstotal:-}" ] && [ -n "${bare:-}" ]; then
	textured=$(( quadstotal - bare ))
	arm="blend"
else
	arm="mosaic"
fi
# the ATXT layers being READ, not just the quadrant base textures
layered=$(sed -n 's/.*; \\([0-9,]*\\) quads took an ATXT layer.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
if [ -z "${layered:-}" ]; then
	layered=$(sed -n 's/.*opaque base, \\([0-9,]*\\) blended layer.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
fi
# these two the blend deliberately words exactly as the mosaic does, so one
# pattern serves both and the split cannot be lost in a rewording again
unpainted=$(sed -n 's/.*bare quads \\([0-9,]*\\) are unpainted.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
notex=$(sed -n 's/.*and \\([0-9,]*\\) chose an LTEX that named no texture.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
say "  ground [$arm]: ${tex:-0} textures, ${textured:-0} textured quads of ${quadstotal:-0}, ${layered:-0} from a layer"
say "  ground bare: ${bare:-?} = ${unpainted:-?} unpainted + ${notex:-?} LTEX with no texture"'''

TAIL_ANCHOR = '''check "the ground is painted with more than one landscape texture" \\
	"$([ "${tex:-0}" -ge 2 ] && echo 1 || echo 0)"'''

TAIL_NEW = '''check "the ground is painted with more than one landscape texture" \\
	"$([ "${tex:-0}" -ge 2 ] && echo 1 || echo 0)"

# ---------------------------------------------------------------------------
# ROWS ADDED BY LANE CELLVIEW4B: the vertex budget, and the refusal.
#
# The blend costs one quad per contributing layer, so a heavily painted block is
# a MULTIPLE of the mosaic's fixed 4096 verts per cell. cellSplatCountVerts()
# runs before a single vertex is allocated and the blend refuses past the cap.
# Both halves are rows here, because a refusal path nobody runs is a refusal
# path nobody knows works.
# ---------------------------------------------------------------------------
counted=$(sed -n 's/.*; \\([0-9,]*\\) land vertices counted before allocating.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
capsaid=$(sed -n 's/.*counted before allocating, against the \\([0-9,]*\\) cap.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
mult=$(sed -n 's/.*; \\([0-9.]*\\) passes per land quad.*/\\1/p' "$g" | head -1)
say "  budget: ${counted:-?} land vertices counted, cap ${capsaid:-?}, ${mult:-?} passes per land quad"
check "the blend counts its vertices before allocating and names the cap" \\
	"$([ -n "${counted:-}" ] && [ "${capsaid:-0}" = "12000000" ] && echo 1 || echo 0)"
check "the passes-per-quad multiplier is stated, so the cost is measured not guessed" \\
	"$([ -n "${mult:-}" ] && echo 1 || echo 0)"

# THE REFUSAL, forced with the harness hook so it costs one cell instead of a
# 38x38 block. WW_CELL_SPLAT_CAP is set to 1, which every painted cell exceeds.
run_cell ground_cap WW_CELL_OPEN="$ESM|$WORLD|$CELLX,$CELLY|1" WW_CELL_SPLAT_CAP=1
gc="$OUT/ww_pick_ground_cap.notes"
grep -E "^ *ground: " "$gc" >> "$LOG" 2>&1
capbare=$(sed -n 's/.*[,-] \\([0-9,]*\\) bare[;,].*/\\1/p' "$gc" | head -1 | tr -dc '0-9')
say "  refusal: $(grep -c "the BLEND REFUSED" "$gc") refusal line(s), mosaic bare ${capbare:-?}"
check "past the cap the blend REFUSES BY NAME instead of stalling or dying" \\
	"$(grep -q "the BLEND REFUSED" "$gc" && echo 1 || echo 0)"
check "the refusal survives the fallback: the mosaic legend is printed TOO, so the downgrade is not silent" \\
	"$(grep -q "the BLEND REFUSED" "$gc" && grep -q "quads took an ATXT layer\\|quads over" "$gc" && echo 1 || echo 0)"
check "RED: the UNCAPPED run of the same cell carries no refusal line at all" \\
	"$(grep -q "the BLEND REFUSED" "$g" && echo 0 || echo 1)"'''


def edit(anchor, new):
    path = os.path.join(ROOT, REL)
    with io.open(path, 'rb') as fh:
        raw = fh.read()
    cr_before = raw.count(b'\r')
    txt = raw.decode('utf-8')
    n = txt.count(anchor)
    print('%-22s %d  %r' % (REL, n, anchor[:56]))
    assert n == 1, 'anchor matched %d times, not once' % n
    out = txt.replace(anchor, new)
    if CHECK:
        return
    data = out.encode('utf-8')
    assert data.count(b'\r') == cr_before, 'CR count moved'
    with io.open(path, 'wb') as fh:
        fh.write(data)


edit(OLD, NEW)
edit(TAIL_ANCHOR, TAIL_NEW)
print()
print('CHECK ONLY, nothing written' if CHECK else 'APPLIED')
