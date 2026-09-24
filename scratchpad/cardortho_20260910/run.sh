#!/bin/bash
#
# LANE CARDORTHO -- everything that needs a built exe, in the order it has to run.
#
# The code, the gates, the documents and the measurement scripts are on disk and
# compile-checked; this is the part that could not run because lanes WATER2 and
# CLAMP build first (the brief's build rule). Run it AFTER the build, with the
# game down. Every step prints what it wrote, and the chain does not stop on a
# red gate -- a refused gate is a deliverable (CONSTITUTION 9).
#
#   bash scratchpad/cardortho_20260910/run.sh
#
# One NifSkope instance at a time: every step below is sequential and each
# launch takes its own --port. Nothing here may run while another lane's harness
# is alive in the tree.

set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
OUT=$REPO/scratchpad/cardortho_20260910
CARDS=$OUT/cards
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
mkdir -p "$OUT"

echo "=== 0. the game, and any other instance"
tasklist | grep -i -E "Fallout4|NifSkope"; echo "rc=$?"
echo "  (rc=1 above means neither is up; anything else and this script must not run)"
ls -l "$REPO/release/NifSkope.exe"

echo
echo "=== 1. the gates the change reaches"
# lodgen_octahedral: the bake block itself, the sidecar's projection line, the
#   orthofit read-back, the card .lodm's projection key, and bake 4 -- the cube
#   proof with its perspective control.
# lodgen_card_arrays: the per-layer projection key, written on one fixture and
#   absent on the other, in ONE array.
# lodgen_impostor_cards / lodgen_identity: floors -- the crossed card and the
#   byte-identity path must not have moved.
for h in lodgen_octahedral lodgen_card_arrays lodgen_impostor_cards lodgen_identity; do
	echo "--- $h"
	bash "$REPO/tests/spells/$h.sh" > "$OUT/$h.log" 2>&1
	echo "  rc=$? $(grep -c '^  ok ' "$OUT/$h.log") ok, $(grep -c '^  FAIL' "$OUT/$h.log") FAIL, $(grep -E '^RESULT' "$OUT/$h.log" | tail -1)"
	grep '^  FAIL' "$OUT/$h.log" | sed 's/^/    /'
done

echo
echo "=== 2. the cube proof, quoted from the octahedral log against the pre-registration"
sed -n '/bake 4/,$p' "$OUT/lodgen_octahedral.log" | grep -E "cube|predicted|asymmetry|near-edge|ok |FAIL" | sed 's/^/  /'
echo "  pre-registered in $OUT/prereg_cube.md"

echo
echo "=== 3. re-bake the 19 Sanctuary trees on the orthographic camera"
rm -rf "$CARDS"
CANDIDATES=trees OCT=8 TILE=128 bash "$REPO/tools/bake_impostor_cards.sh" \
	"$ESM" -20 24 -17 27 "$CARDS" 2>&1 | tee "$OUT/bake19.log" | tail -25
echo "  baked: $(grep -c ' baked ' "$OUT/bake19.log"), failed: $(grep -c ' FAILED ' "$OUT/bake19.log")"
echo "  sidecars saying ortho: $(grep -lx 'projection ortho' "$CARDS"/*.txt 2>/dev/null | wc -l) of $(ls "$CARDS"/*.txt 2>/dev/null | wc -l)"
grep -h '^orthofit ' "$CARDS"/*.txt | awk '{ if ($4 != 0) p++ } END { printf "  orthofit lines read through a perspective camera: %d (must be 0)\n", p+0 }'

echo
echo "=== 4. the transition measurement, in a scene with ONE reference"
"$PY" "$OUT/transition.py" "$CARDS" "$OUT/bake19.log" "$OUT" 2>&1 | tee "$OUT/transition.log" | tail -40

echo
echo "=== 5. the FO4CS sample set, on the new library"
bash "$REPO/scratchpad/handoff_fo4cs/samples/make_samples.sh" "$CARDS" \
	> "$REPO/scratchpad/handoff_fo4cs/samples/make_samples.log" 2>&1
echo "  rc=$? -- tail:"
tail -6 "$REPO/scratchpad/handoff_fo4cs/samples/make_samples.log" | sed 's/^/    /'
( cd "$REPO/scratchpad/handoff_fo4cs/samples" && "$PY" make_manifest.py "$CARDS" "lane CARDORTHO, 2026-09-10" )
echo "  MANIFEST.md rows: $(grep -c '^| ' "$REPO/scratchpad/handoff_fo4cs/samples/MANIFEST.md")"
"$PY" - "$REPO/scratchpad/handoff_fo4cs/samples" <<'PYEOF'
import glob, json, os, struct, sys
d = sys.argv[1]
seen = {}
for f in glob.glob(os.path.join(d, '**', '*.lodm'), recursive=True):
    b = open(f, 'rb').read()
    try:
        lm = json.loads(b[12:])
    except Exception:
        continue
    if lm.get('kind') == 'card':
        seen.setdefault(lm.get('card', {}).get('projection', '(absent)'), []).append(os.path.basename(f))
    elif lm.get('kind') == 'cardArray':
        for L in lm.get('array', {}).get('layers', []):
            seen.setdefault(L.get('projection', '(absent)'), []).append(os.path.basename(f) + '#' + str(L.get('id')))
for k in sorted(seen):
    print('  sample set, projection %-10s : %d card entries' % (k, len(seen[k])))
PYEOF

echo
echo "=== done. Pictures:"
ls -l "$OUT"/cardortho_transition_*.png 2>/dev/null
