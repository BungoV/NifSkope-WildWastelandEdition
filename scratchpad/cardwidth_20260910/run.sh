#!/bin/bash
#
# LANE CARDWIDTH -- everything that needs a built exe, in the order it has to run.
#
# The code, the fixture, the documents and the measurement scripts are on disk and
# syntax-checked; this is the part that could not run because Fallout4.exe was up
# (PID 12500) at the lane's single build check. Run it AFTER the build, with the
# game down. Every step prints what it wrote, and the chain does not stop on a red
# gate -- a refused gate is a deliverable.
#
#   bash scratchpad/cardwidth_20260910/run.sh
#
# One NifSkope instance at a time: every step below is sequential and each launch
# takes its own --port. Nothing here may run while another lane's harness is alive.

set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
OUT=$REPO/scratchpad/cardwidth_20260910
CARDS=$OUT/cards
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
mkdir -p "$OUT"

echo "=== 0. the game, and any other instance"
tasklist | grep -i -E "Fallout4|NifSkope"; echo "rc=$?"
echo "  (rc=1 above means neither is up; anything else and this script must not run)"
ls -l "$REPO/release/NifSkope.exe"
echo "  the exe must be NEWER than all of these:"
ls -l "$REPO/src/nifskope_ui.cpp" "$REPO/src/lodgen.cpp"

echo
echo "=== 1. the gates the change reaches"
# lodgen_octahedral: the bake block itself, the sidecar's `coverage` line, the
#   card .lodm's coverage object, and bake 4 -- the cube at the READER's threshold
#   (F1) with its perspective control (F2) and the encoding invariants (F3/F4/F5).
# lodgen_card_arrays: the per-layer coverage key travelling with its layer.
# lodgen_impostor_cards / lodgen_identity: floors -- the crossed card and the
#   byte-identity path must not have moved by an alpha change on the oct sheets.
for h in lodgen_octahedral lodgen_card_arrays lodgen_impostor_cards lodgen_identity; do
	echo "--- $h"
	bash "$REPO/tests/spells/$h.sh" > "$OUT/$h.log" 2>&1
	echo "  rc=$? $(grep -c '^  ok ' "$OUT/$h.log") ok, $(grep -c '^  FAIL' "$OUT/$h.log") FAIL, $(grep -E '^RESULT' "$OUT/$h.log" | tail -1)"
	grep '^  FAIL' "$OUT/$h.log" | sed 's/^/    /'
done

echo
echo "=== 2. the cube fixture, quoted against the pre-registration"
grep -E "F1:|F2 |F3:|F4 |F5:|coverage contract|READER threshold" "$OUT/lodgen_octahedral.log" | sed 's/^/  /'
echo "  pre-registered in $REPO/scratchpad/lane_cardwidth_report.md section 8"

echo
echo "=== 3. re-bake the 19 Sanctuary trees under the coverage contract"
rm -rf "$CARDS"
CANDIDATES=trees OCT=8 TILE=128 bash "$REPO/tools/bake_impostor_cards.sh" \
	"$ESM" -20 24 -17 27 "$CARDS" 2>&1 | tee "$OUT/bake19.log" | tail -25
echo "  baked: $(grep -c ' baked ' "$OUT/bake19.log"), failed: $(grep -c ' FAILED ' "$OUT/bake19.log")"
echo "  sidecars stating the contract: $(grep -lx 'coverage 16 128 160' "$CARDS"/*.txt 2>/dev/null | wc -l) of $(ls "$CARDS"/*.txt 2>/dev/null | grep -v library.txt | wc -l)"
echo "  (library.txt is the run manifest and correctly has no camera and no contract)"

echo
echo "=== 4. the sheets themselves: no texel between 1 and 159, on every tree"
"$PY" - "$CARDS" <<'PYEOF'
import glob, os, sys
from PIL import Image
bad = 0
for f in sorted(glob.glob(os.path.join(sys.argv[1], '*_oct_albedo.png'))):
    a = list(Image.open(f).convert('RGBA').split()[3].getdata())
    between = sum(1 for v in a if 0 < v < 160)
    covered = sum(1 for v in a if v >= 128)
    print('  %-28s %8d covered, %6d between 1 and 159' % (os.path.basename(f), covered, between))
    if between:
        bad += 1
print('  sheets carrying a texel the contract forbids: %d (must be 0)' % bad)
PYEOF

echo
# ORDER (lane CARDWIDTH, first run): the card `.lodm` files do not exist until
# `lodgen card` has run, and the thing that runs it is the sample-set step -- so
# that step comes BEFORE the two checks that read a .lodm. On the first run the
# transition step died with FileNotFoundError for exactly this reason.
echo "=== 5. the FO4CS sample set, on the new library"
bash "$REPO/scratchpad/handoff_fo4cs/samples/make_samples.sh" "$CARDS" \
	> "$REPO/scratchpad/handoff_fo4cs/samples/make_samples.log" 2>&1
echo "  rc=$? -- tail:"
tail -6 "$REPO/scratchpad/handoff_fo4cs/samples/make_samples.log" | sed 's/^/    /'
( cd "$REPO/scratchpad/handoff_fo4cs/samples" && "$PY" make_manifest.py "$CARDS" "lane CARDWIDTH, 2026-09-10" )
echo "  MANIFEST.md rows: $(grep -c '^| ' "$REPO/scratchpad/handoff_fo4cs/samples/MANIFEST.md")"
"$PY" - "$REPO/scratchpad/handoff_fo4cs/samples" <<'PYEOF'
import glob, json, os, sys
seen = {}
for f in glob.glob(os.path.join(sys.argv[1], '**', '*.lodm'), recursive=True):
    try:
        lm = json.loads(open(f, 'rb').read()[12:])
    except Exception:
        continue
    if lm.get('kind') == 'card':
        seen.setdefault(json.dumps(lm.get('card', {}).get('coverage'), sort_keys=True), []).append(1)
    elif lm.get('kind') == 'cardArray':
        for L in lm.get('array', {}).get('layers', []):
            seen.setdefault(json.dumps(L.get('coverage'), sort_keys=True), []).append(1)
for k in sorted(seen):
    print('  sample set, coverage %-40s : %d card entries' % (k, len(seen[k])))
PYEOF

echo
echo "=== 6. the .lodm carries the contract, on every card"
"$PY" - "$CARDS" <<'PYEOF'
import glob, json, os, sys
seen = {}
for f in glob.glob(os.path.join(sys.argv[1], '*.lodm')):
    lm = json.loads(open(f, 'rb').read()[12:])
    if lm.get('kind') == 'card':
        c = lm.get('card', {}).get('coverage')
        seen.setdefault(json.dumps(c, sort_keys=True), []).append(os.path.basename(f))
for k in sorted(seen):
    print('  coverage %-42s : %d cards' % (k, len(seen[k])))
PYEOF

echo
echo "=== 7. the transition, on the fixed instrument"
"$PY" "$OUT/transition2.py" "$CARDS" "$OUT/bake19.log" "$OUT" 2>&1 | tee "$OUT/transition2.log" | tail -50

echo
echo "=== done. Pictures (open every one before reporting):"
ls -l "$OUT"/cardwidth_transition_*.png 2>/dev/null
