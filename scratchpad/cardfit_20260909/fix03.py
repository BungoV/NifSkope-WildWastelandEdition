#!/usr/bin/env python3
"""CARDFIT3 fix 03 -- the half-aux toggle, named where a bake is driven.

The 2026-09-06 ruling (bungo: "should our impostors use half the res for normal
and other map, and full resolution for diffuse? Maybe make it a toggle.") made
`--card-half-aux` a GENERATION option, off by default, because the base colour
carries the coverage and is the silhouette.

Confirmed for tonight's FO4CS sample set: `make_samples.sh` runs
`--arrays --impostors "$CARDS" --cover` and NEVER `--card-half-aux`, so the
sample set is at full resolution on all four sheets -- which is the ruling's
default and therefore correct, but nothing said so.

Both drivers now carry the toggle by name, off by default, and say what it costs.
"""
BAKE = 'tools/bake_impostor_cards.sh'
SAMP = 'scratchpad/handoff_fo4cs/samples/make_samples.sh'

b = open(BAKE, 'rb').read()
assert b.count(b'\r') == 0, 'bake_impostor_cards.sh is LF-only'
s = b.decode('utf-8')

old = """# The bake writes PNG sheets at full size either way; HALF-RESOLUTION normal, mask
# and emissive sheets are a GENERATION option (`lodgen --card-half-aux`), applied
# when the PNGs are converted, so a library does not need re-photographing to try
# it. The base colour never divides: its alpha is the coverage.
"""
new = """# The bake writes PNG sheets at full size either way; HALF-RESOLUTION normal, mask
# and emissive sheets are a GENERATION option (`lodgen --card-half-aux`), applied
# when the PNGs are converted, so a library does not need re-photographing to try
# it. The base colour never divides: its alpha is the coverage.
#
#   HALF_AUX=1 bash tools/bake_impostor_cards.sh ...
#
# records that choice in the library, as `<outdir>/library.txt`, so whoever
# converts it later does not have to remember: the flag belongs to `lodgen`, not
# to this script, and this script is where the decision is actually made. OFF by
# default, which is the 2026-09-06 ruling (bungo: "Maybe make it a toggle").
# Measured across a two-layer array set the payload falls 3,584 -> 1,664 bytes,
# 46.4%. Since 2026-09-09 the padding is rounded UP TO EVEN precisely so a
# halving lands the aux gutter on a whole texel, and the aux mip count comes down
# with it: auxMips = 1 + log2(min(padX,padY)/auxDiv).
"""
assert s.count(old) == 1
s = s.replace(old, new)

old = """n=0
while read -r formid extent model; do"""
new = """# the half-aux choice, recorded beside the library it applies to
{
	echo "oct ${OCT}"
	echo "tile ${TILE}"
	echo "ref ${REF}"
	echo "half_aux ${HALF_AUX:-0}"
	echo "candidates ${CANDIDATES:-missing}"
} > "$OUT/library.txt"
if [ "${HALF_AUX:-0}" = "1" ]; then
	echo "half-aux ON: convert this library with 'lodgen --card-half-aux' (normal, mask and emissive at half of each side)"
else
	echo "half-aux off (the 2026-09-06 default): all four sheets convert at full size"
fi

n=0
while read -r formid extent model; do"""
assert s.count(old) == 1
s = s.replace(old, new)

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(BAKE, 'wb').write(out)
print('bake_impostor_cards.sh: HALF_AUX recorded')

b = open(SAMP, 'rb').read()
assert b.count(b'\r') == 0
s = b.decode('utf-8')
old = """    --arrays --impostors "$CARDS" --cover "$@" 2>&1 | tail -12"""
new = """    --arrays --impostors "$CARDS" --cover ${HALFAUX:+--card-half-aux} "$@" 2>&1 | tail -12"""
assert s.count(old) == 1
s = s.replace(old, new)

old = """CARDS=${1:?cards dir}
"""
new = """CARDS=${1:?cards dir}
# HALF_AUX=1 writes the normal, mask and emissive sheets at half of each side
# (`lodgen --card-half-aux`). OFF by default, which is the 2026-09-06 ruling: the
# base colour carries the coverage and is the silhouette, so it never divides.
# The sample set shipped 2026-09-09 was made WITHOUT it -- all four sheets full
# size -- and the MANIFEST says so.
HALFAUX=${HALF_AUX:+1}
"""
assert s.count(old) == 1
s = s.replace(old, new)

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(SAMP, 'wb').write(out)
print('make_samples.sh: --card-half-aux behind HALF_AUX')
