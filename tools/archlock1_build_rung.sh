#!/bin/bash
#
# BUILD THE REFUTER EXE for tests/spells/gamemanager_archlock.sh (lane ARCHLOCK1).
#
# release/NifSkope.archlock1_rung.exe is THIS working tree with the archive-lock
# fix taken back out and nothing else changed: the two
# `archiveReadLock.unlock();` lines that `GameResources::find_file` and
# `::get_file` call before they recurse to `parent`. With them gone the read
# lock is still held when the parent's lazy `init_archives()` asks for the write
# lock, and `QReadWriteLock::Recursive` -- which grants read-after-read and
# write-after-write to one thread -- will not upgrade a read to a write. The
# thread then waits for its own reader to leave. Forever.
#
# WHY THE RUNG CANNOT BE AN OLDER EXE. The harness (src/archlocktest.cpp), the
# seam it drives (NifSkope::wwFilesTabOpenConfiguredRow) and the batch verb
# (`-no-gui archlock-probe`) are all new in this lane. No exe built before it
# can run either leg, so "the old exe did not hang" would say only that it never
# reached the lock. One variable, one difference.
#
# WHAT IT DOES, in order:
#   1. refuses if Fallout4.exe is up (CONSTITUTION rule 6) -- ww_build.sh checks
#      this too, but this script edits a source file, so it checks first
#   2. removes the two lines, records the file's hash before and after
#   3. builds, and COPIES release/NifSkope.exe to release/NifSkope.archlock1_rung.exe
#   4. PUTS THE TWO LINES BACK -- always, including on a failed build (trap) --
#      and verifies the restored file is byte-identical to the original
#   5. builds again, so release/NifSkope.exe is the fixed exe once more
#
# The tree is left exactly as it was found. Run it from anywhere:
#   bash tools/archlock1_build_rung.sh
#
# Lane ARCHLOCK1, 2026-09-17.

set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 2

SRC="src/gamemanager.cpp"
RUNG="release/NifSkope.archlock1_rung.exe"
BAK="$(mktemp)"

if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then
	echo "GAME UP: Fallout4.exe is running; no build"; exit 3
fi

n="$(grep -c 'archiveReadLock\.unlock();' "$SRC")"
if [ "$n" -lt 2 ]; then
	echo "REFUSED: $SRC has $n unlock site(s), expected at least 2 -- the fix is not in this tree"
	exit 2
fi
cp "$SRC" "$BAK"
before="$(sha1sum "$SRC" | cut -d' ' -f1)"
echo "$SRC before: $before ($n unlock site(s))"

restore() {
	cp "$BAK" "$SRC"
	after="$(sha1sum "$SRC" | cut -d' ' -f1)"
	if [ "$after" = "$before" ]; then
		echo "$SRC restored: $after (byte-identical)"
	else
		echo "RESTORE FAILED: $SRC is $after, was $before -- the backup is at $BAK"
	fi
}
trap restore EXIT

# THE ONE DIFFERENCE. Only the two sites that precede a recursion to `parent`;
# the third unlock (the loose-file retry in get_file) predates this lane and is
# left alone, which is why the pattern is matched with its comment marker.
python - "$SRC" <<'PYEOF'
import re, sys
p = sys.argv[1]
b = open(p, "rb").read()
pat = re.compile(rb"\n[\t ]*archiveReadLock\.unlock\(\);[^\n]*\n(?=[\t ]*return parent->)")
b2, n = pat.subn(b"\n", b)
print("removed %d unlock-before-recursion site(s)" % n)
if n != 2:
    print("REFUSED: expected 2")
    sys.exit(1)
open(p, "wb").write(b2)
PYEOF
[ $? -eq 0 ] || exit 2
echo "$SRC broken:  $(sha1sum "$SRC" | cut -d' ' -f1)"

bash tools/ww_build.sh "$SRC" || { echo "the RUNG build failed"; exit 2; }
cp release/NifSkope.exe "$RUNG" || exit 2
ls -l --time-style=+%H:%M:%S "$RUNG"

restore
trap - EXIT
bash tools/ww_build.sh "$SRC" || { echo "the RESTORE build failed -- release/NifSkope.exe is the RUNG, do not ship it"; exit 2; }
echo "release/NifSkope.exe is the fixed exe again; the rung is $RUNG"
