#!/bin/bash
# Lane HKXEDIT2: g++ -fsyntax-only with the real Makefile.Release flags (sx_tmp.sh verbatim), one file per argument.
# Run from MSYS2 UCRT64: MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'cd /e/Projects/NifskopeWildWastelandEdition && bash sx_HKXEDIT2.sh src/x.cpp ...'
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
rc=0
for f in "$@"; do
  echo "== $f"
  bash sx_tmp.sh $EXTRA_DEFS "$f" 2>&1 | grep -v "sfinae-incomplete\|qchar.h" | grep -E "error|warning" | head -30
  r=${PIPESTATUS[0]}
  echo "RC=$r"
  [ $r -ne 0 ] && rc=$r
done
echo "ALL-RC=$rc"
exit $rc
