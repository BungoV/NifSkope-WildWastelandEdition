#!/bin/bash
# lane BUGS1102C (SHOTCAPS-COLD): shadercache/ keeps one binary a define set (gl/progcachekeep.h), offline, no NifSkope.
#   bash tests/progcachekeep/run.sh <scratch dir>     (from the tree root; MSYS2 UCRT64 g++ + Qt6Core)
# Green (keep 6) exits 0; red WW_PROGCACHE_KEEP=1 (the old one-file rule) must fail the alternate case; the wiring red
# (glcontext.cpp at the base, before the lane) must fail too.
set -u
S=${1:?out dir}
BASE=2c0ec12a
mkdir -p "$S"
export PATH="/c/msys64/ucrt64/bin:$PATH"
fail=0
Q=/c/msys64/ucrt64/include/qt6
g++ -O1 -std=c++17 -w -I src -I $Q -I $Q/QtCore tests/progcachekeep/progcachekeep_test.cpp -o "$S/pck.exe" -lQt6Core \
	> "$S/build.log" 2>&1 || { echo "BUILD FAIL"; cat "$S/build.log"; exit 1; }
"$S/pck.exe" "$S/green" 6 > "$S/green.log" 2>&1; rc=$?
echo "green keep 6 rc=$rc (want 0): $(tail -1 "$S/green.log")"; [ $rc -eq 0 ] || fail=1
"$S/pck.exe" "$S/red" 1 > "$S/red.log" 2>&1; rc=$?
echo "red keep 1 rc=$rc (want 1): $(grep 'case alternate' "$S/red.log")"; [ $rc -eq 1 ] || fail=1
# BUGS1102F (SHOTCAPS-MTIME): the hit stamp red (no stamp, the newest WRITTEN stay) must fail the lru case
"$S/pck.exe" "$S/redtouch" 6 notouch > "$S/redtouch.log" 2>&1; rc=$?
echo "red notouch rc=$rc (want 1): $(grep 'case lru' "$S/redtouch.log")"; [ $rc -eq 1 ] || fail=1
grep -q 'case lru FAIL' "$S/redtouch.log" || { echo "red notouch did not fail on lru"; fail=1; }
t() { grep -c 'wwProgCacheTouchOn( qgetenv( "WW_PROGCACHE_TOUCH_RED" ) ) )' "$1"; }
n=$(t src/gl/glcontext.cpp); echo "wiring: hit stamp $n (want 1)"; [ "$n" = 1 ] || fail=1
git show 08053859:src/gl/glcontext.cpp > "$S/base_touch_glcontext.cpp" 2>/dev/null
n=$(t "$S/base_touch_glcontext.cpp"); echo "wiring red (base 08053859): hit stamp $n (want 0)"; [ "$n" = 0 ] || fail=1
grep -q '"WW_PROGCACHE_TOUCH_RED", 0,' src/wwswitchstages.inc; rc=$?; echo "stage table WW_PROGCACHE_TOUCH_RED live rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
w() { grep -c 'wwProgCachePrune( fi.absolutePath(), QString::fromLatin1( name.data(), qsizetype( name.size() ) ), fi.fileName(),' "$1"; }
n=$(w src/gl/glcontext.cpp); echo "wiring: prune call $n (want 1)"; [ "$n" = 1 ] || fail=1
git show $BASE:src/gl/glcontext.cpp > "$S/base_glcontext.cpp" 2>/dev/null
n=$(w "$S/base_glcontext.cpp"); echo "wiring red (base $BASE): prune call $n (want 0)"; [ "$n" = 0 ] || fail=1
grep -q '"WW_PROGCACHE_KEEP", 0,' src/wwswitchstages.inc; rc=$?; echo "stage table WW_PROGCACHE_KEEP live (read at every link) rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
[ $fail -eq 0 ] && echo "PROGCACHEKEEP-RUN PASS" || echo "PROGCACHEKEEP-RUN FAIL"
exit $fail
