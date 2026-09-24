#!/bin/bash
#
# The gated build, as one in-tree script (nifskope-ww-build-verify):
#   1. THIS tree's release/NifSkope.exe, when a running process's image is it,
#      is renamed aside, never killed; a NifSkope running from any other tree
#      (bungo's window, a sibling lane's harness) is not this exe and is left be
#   2. make -j2 under MSYS2 UCRT64 with git on the PATH, in THIS tree, gated on
#      make's OWN exit code
#   3. the exe must be newer than every source given (or src/ when none)
#   4. the link-time copies must be in step (style.qss, the shaders)
# Exit 0 only when every gate holds. Runs from Git Bash; the MSYS2 bash it
# calls is a child, so a session allowed to execute only under this repo can
# still build through it. Works in the main tree and in any lane worktree
# (E:\Projects\NifskopeWWE-<lane>): every path is derived from the script's own.
#
# USAGE
#   bash tools/ww_build.sh [source-to-be-older-than-exe ...]
#   WW_BUILD_LOCK_ONLY=1 bash tools/ww_build.sh   # step 1 only: report/rename, no make

set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 2

if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then
	echo "GAME UP: Fallout4.exe is running; no build"; exit 3
fi
# one build per TREE (several lane worktrees build at once by design, -j2 each)
LOCK="$ROOT/release/.ww_build.lock"
mkdir -p "$ROOT/release"
if ! mkdir "$LOCK" 2>/dev/null; then
	echo "another ww_build.sh is running in this tree ($LOCK); wait"; exit 3
fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT

# 1. Is THIS tree's exe the image of a running process? Compare the full image
# path of every running NifSkope.exe with this tree's, case- and slash-folded.
# A path match alone is not trusted (an image path can outlive a rename): the
# exe must also refuse an exclusive open, i.e. really be held.
low() { tr 'A-Z\\' 'a-z/' | sed 's#//*#/#g'; }
MYEXE_W="$(cd "$ROOT/release" && { pwd -W 2>/dev/null || pwd; })/NifSkope.exe"
MYEXE_N="$(printf '%s' "$MYEXE_W" | low)"
procs="$(powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | ForEach-Object { '{0}|{1}' -f \$_.ProcessId,\$_.ExecutablePath }" 2>/dev/null | tr -d '\r')"
mine=""; others=0
while IFS='|' read -r pid path; do
	[ -n "$pid" ] || continue
	if [ -z "$path" ]; then echo "  NifSkope pid $pid: image path unreadable, not counted as this tree's"; others=$((others + 1)); continue; fi
	if [ "$(printf '%s' "$path" | low)" = "$MYEXE_N" ]; then mine="$mine $pid"; else others=$((others + 1)); fi
done <<< "$procs"
held=no
if [ -n "$mine" ] && [ -f release/NifSkope.exe ]; then
	held="$(powershell -NoProfile -Command "try { \$f=[IO.File]::Open('$MYEXE_W','Open','ReadWrite','None'); \$f.Close(); 'no' } catch { 'yes' }" 2>/dev/null | tr -d '\r')"
fi
if [ -n "$mine" ] && [ "$held" = "yes" ]; then
	pid="${mine# }"; pid="${pid%% *}"
	mv release/NifSkope.exe "release/NifSkope_inuse_${pid}.exe" && echo "this tree's exe is held by pid(s)$mine: renamed aside to release/NifSkope_inuse_${pid}.exe"
elif [ -n "$mine" ]; then
	echo "pid(s)$mine report this tree's exe path, but the exe is not held (renamed earlier?): not renamed"
else
	echo "this tree's exe is not held by a window ($others NifSkope running from elsewhere, left be)"
fi
[ "${WW_BUILD_LOCK_ONLY:-0}" = "1" ] && exit 0

MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd "$1" || exit 2; make -j2 > release/ww_build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" release/ww_build.log | head -20; echo BUILD-RC=$rc; exit $rc' _ "$ROOT"
rc=$?
ls -l --time-style=+%H:%M:%S release/NifSkope.exe
[ $rc -eq 0 ] || { echo "BUILD FAILED"; exit $rc; }

if [ $# -gt 0 ]; then
	for s in "$@"; do
		[ release/NifSkope.exe -nt "$s" ] || { echo "exe is NOT newer than $s"; exit 4; }
	done
else
	newer="$(find src -newer release/NifSkope.exe -type f | head -3)"
	[ -z "$newer" ] || { echo "exe is NOT newer than: $newer"; exit 4; }
fi
echo "exe newer than the sources"
cmp res/style.qss release/style.qss || { echo "style.qss copy out of step"; exit 5; }
for f in res/shaders/*; do
	[ -f "release/shaders/$(basename "$f")" ] && ! cmp -s "$f" "release/shaders/$(basename "$f")" && { echo "shader copy out of step: $f"; exit 5; }
done
echo "copies in step"
exit 0
