#!/bin/bash
# lane PERFFIX3 (item 8): the FO4CS bloom's per-pixel combine, exact against the base per-sample tent on the real driver.
# Green must PASS; each red (half: an RGBA16F combine target; nolod: the combine ignores the red wide's lod) must FAIL.
# usage: bash tests/features/post/bloomcomb/run.sh [--time]
cd "$(dirname "$0")/../../../.." || exit 2
PY="${PY:-python}"
rc=0
"$PY" -I tests/features/post/bloomcomb/bloomcomb_check.py "$@" | tail -25
[ "${PIPESTATUS[0]}" = 0 ] || { echo "GREEN FAILED"; rc=1; }
for r in half nolod; do
	out=$("$PY" -I tests/features/post/bloomcomb/bloomcomb_check.py --red $r | tail -1)
	echo "red $r: $out"
	case "$out" in *FAIL*) ;; *) echo "RED $r DID NOT FAIL"; rc=1 ;; esac
done
echo "BLOOMCOMB run rc=$rc"
exit $rc
