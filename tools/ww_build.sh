#!/bin/bash
#
# The gated build, as one in-tree script (nifskope-ww-build-verify):
#   1. a running release/NifSkope.exe is renamed aside, never killed
#   2. make -j2 under MSYS2 UCRT64 with git on the PATH, gated on make's OWN exit code
#   3. the exe must be newer than every source given (or src/ when none)
#   4. the link-time copies must be in step (style.qss, the shaders)
# Exit 0 only when every gate holds. Runs from Git Bash; the MSYS2 bash it
# calls is a child, so a session allowed to execute only under this repo can
# still build through it.
#
# USAGE
#   bash tools/ww_build.sh [source-to-be-older-than-exe ...]

set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 2

if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then
	echo "GAME UP: Fallout4.exe is running; no build"; exit 3
fi
if tasklist //FI "IMAGENAME eq make.exe" 2>/dev/null | grep -q make.exe; then
	echo "another make is running on this machine; wait"; exit 3
fi

# the exe held by a window: rename aside (Windows allows it), the link writes a fresh one
if tasklist //FI "IMAGENAME eq NifSkope.exe" 2>/dev/null | grep -q NifSkope.exe; then
	pid="$(tasklist //FI "IMAGENAME eq NifSkope.exe" 2>/dev/null | grep NifSkope.exe | head -1 | awk '{print $2}')"
	if [ -f release/NifSkope.exe ]; then
		mv release/NifSkope.exe "release/NifSkope_inuse_${pid}.exe" && echo "running copy (pid $pid) renamed aside"
	fi
else
	echo "exe not held by a window"
fi

MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && make -j2 > /tmp/ww_build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" /tmp/ww_build.log | head -20; echo BUILD-RC=$rc; exit $rc'
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
