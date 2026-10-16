#!/usr/bin/env bash
# OPENCELL1: build tests/opencell/opencelltest, run it green, then with the broken writer (must FAIL).
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
out="${1:-${TMPDIR:-/tmp}/opencelltest}"
qmake="${QMAKE:-qmake6}"
mkdir -p "$out" && cd "$out" || exit 2
"$qmake" "$root/tests/opencell/opencelltest.pro" >/dev/null && make -j4 >/dev/null || { echo "BUILD FAILED"; exit 2; }
exe="$out/opencelltest"; [ -x "$exe" ] || exe="$out/release/opencelltest.exe"
"$exe"; green=$?
WW_OPENCELL_RED=writer "$exe" >/dev/null 2>&1; red=$?
echo "green exit $green (want 0), red exit $red (want 1)"
[ "$green" -eq 0 ] && [ "$red" -ne 0 ] && echo "OPENCELL CHECK PASS" && exit 0
echo "OPENCELL CHECK FAIL"; exit 1
