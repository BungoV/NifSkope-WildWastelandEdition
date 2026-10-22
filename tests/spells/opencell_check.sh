#!/usr/bin/env bash
# OPENCELL1: build tests/opencell/opencelltest, run it green, then with the broken writer (must FAIL).
# LEFTOVERS1: and with the string tables red / switched off (localized names blank: must FAIL),
# and with light plugins numbered as full ones (WW_ESM_LIGHT_RED=full: must FAIL).
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
out="${1:-${TMPDIR:-/tmp}/opencelltest}"
qmake="${QMAKE:-qmake6}"
mkdir -p "$out" && cd "$out" || exit 2
"$qmake" "$root/tests/opencell/opencelltest.pro" >/dev/null && make -j4 >/dev/null || { echo "BUILD FAILED"; exit 2; }
exe="$out/opencelltest"; [ -x "$exe" ] || exe="$out/release/opencelltest.exe"
"$exe"; green=$?
WW_OPENCELL_RED=writer "$exe" >/dev/null 2>&1; red=$?
WW_OPENCELL_RED=strings "$exe" >/dev/null 2>&1; reds=$?
WW_OPENCELL_STRINGS=0 "$exe" >/dev/null 2>&1; off=$?
WW_ESM_LIGHT_RED=full "$exe" >/dev/null 2>&1; redl=$?
echo "green exit $green (want 0), red writer exit $red, red strings exit $reds, strings off exit $off, red light exit $redl (each want 1)"
[ "$green" -eq 0 ] && [ "$red" -ne 0 ] && [ "$reds" -ne 0 ] && [ "$off" -ne 0 ] && [ "$redl" -ne 0 ] && echo "OPENCELL CHECK PASS" && exit 0
echo "OPENCELL CHECK FAIL"; exit 1
