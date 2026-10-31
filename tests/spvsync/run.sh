#!/bin/bash
# Lane RESTIRSPV1: committed SPIR-V headers (src/**/*.spv.h) == their sources rebuilt today (tests/spvsync/check_spv.py).
#   bash tests/spvsync/run.sh <out dir>        (from the tree root; GLSLANG=<glslangValidator.exe>, PY=<python>)
# green: the tree's headers, all identical -> SPVSYNC PASS, rc 0.
# reds (each must FAIL, rc 1): stale = restir.spv.h as committed at 563dcf97 (made from an older restir.comp, found by
#   SKYBELOW1; skipped in a clone without that commit); flip = today's restir.spv.h with one SPIR-V word changed.
# No glslangValidator, or one of another version than the headers' stamp: SKIP, rc 77 (never a pass).
set -u
OUT=${1:?out dir}
mkdir -p "$OUT"
PY="${PY:-$(command -v python)}"
p() { if command -v cygpath > /dev/null; then cygpath -am "$1"; else echo "$1"; fi; }
T=$(p "$(pwd)")
O=$(p "$OUT")
"$PY" -I tests/spvsync/check_spv.py "$T" "$O/green" > "$OUT/green.txt" 2>&1; rc=$?
cat "$OUT/green.txt"
if [ $rc -eq 77 ]; then echo "SPVSYNC SKIP (see the SKIP lines above)"; exit 77; fi
bad=0
if [ $rc -ne 0 ] || ! grep -q '^SPVSYNC PASS ' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
red() {	# red <name> <header file standing in for restir.spv.h>
	"$PY" -I tests/spvsync/check_spv.py "$T" "$O/red_$1" --committed "restir.spv.h=$2" > "$OUT/red_$1.txt" 2>&1
	local r=$?
	if [ $r -eq 1 ] && grep -q 'restir.spv.h: STALE' "$OUT/red_$1.txt"; then echo "red $1 FAILS as it must (rc $r)"
	else echo "RED $1 DID NOT FAIL (rc $r)"; tail -n 4 "$OUT/red_$1.txt"; bad=1; fi
}
"$PY" -I tests/spvsync/flip_word.py src/rtddgi/restir.spv.h "$OUT/flip_restir.spv.h" && red flip "$O/flip_restir.spv.h"
if git show 563dcf97:src/rtddgi/restir.spv.h > "$OUT/stale_restir.spv.h" 2> /dev/null; then red stale "$O/stale_restir.spv.h"
else echo "red stale: commit 563dcf97 not in this clone, not run"; fi
[ $bad -eq 0 ] && echo "SPVSYNC TEST OK" || echo "SPVSYNC TEST FAILED"
exit $bad
