#!/bin/bash
#
# THE ARCHIVE INDEX'S LOCK, AND THE THREAD THAT WAITED ON ITSELF.
# Lane ARCHLOCK1, 2026-09-17.
#
# WHY THIS EXISTS
#
# bungo, 2026-09-17 00:4x, verbatim: "When I click on anything from 'files', it
# freezes nifskope". No CPU, nine threads, none of them working: a wait.
#
# GameManager::archiveLock() is ONE static QReadWriteLock in Recursive mode.
# Recursive grants read-after-read and write-after-write to the SAME thread, and
# never a read -> write UPGRADE: lockForWrite waits for the reader count to
# reach zero, and the reader it waits for is the calling thread itself.
# GameResources::get_file and ::find_file took the READ lock, missed, and then
# recursed `return parent->...` WHILE STILL HOLDING IT; the parent's lazy
# init_archives() opens with a QWriteLocker on that same lock. The GUI thread
# slept on itself and nothing could ever wake it.
#
# WHAT MAKES THIS GATE HARD TO WRITE, AND WHY THE FIRST PROBE WAS GREEN
#
# The bug needs a document whose OWN resource set has an EMPTY data path, so
# that every lookup falls through to the shared index, PLUS a shared index that
# nothing has built yet. Opening a loose .nif by path gives the document a data
# path (derived from the file name) and it builds the shared index from its own
# init_archives(), with no lock held -- so the obvious probe opens fine on the
# broken build and proves nothing. The route that does reproduce it is the Files
# tab's CONFIGURED-RESOURCE row: the bytes come out of the archive through a
# QBuffer, NifModel::load never sees a file name, and the data path is empty.
# That is bungo's click. Both halves of the condition are FORCED and then read
# back in src/archlocktest.cpp; neither is assumed.
#
# THE LEGS
#   (a) THE REFUTER, in a real window. The GUI harness is run against the RUNG
#       and must HANG: the watchdog
#       kills it and "hung, killed after N s" is the PASSING row. The same
#       command against the exe under test must load the file, paint one frame
#       and exit 0 inside the limit. A gate that only ran the new exe would pass
#       on a build where the harness never reached the lock at all.
#   (b) THE SAME THING WITH NO WINDOW: `archlock-probe`, a -no-gui verb that
#       loads the .nif through a QBuffer (empty data path) and asks for its
#       first material. Rung hangs and is killed; the new exe answers.
#   (c) find_file, the twin of get_file, covered by a TEXTURE lookup in both
#       runs. This is the leg that catches a "fix" which merely makes every
#       lookup miss: a miss is a FAILURE here, not a pass.
#   (d) NIFPARSE1's reason for the lock still stands. The lock is kept, so the
#       sixteen chunk workers are still serialised against a teardown: the same
#       fixture region baked by the rung and by the new exe must come out BYTE
#       IDENTICAL. (tests/spells/lodgen_native.sh and lodgen_defaults.sh are run
#       separately and their counts go in the lane report.)
#   (e) the `--bake-record` refusal NAMES THE DIRECTORY IT LOOKED IN. Batch mode
#       resolves a relative path against the NifSkope folder on purpose
#       (src/nifcli.cpp:179, how nif.xml is found), so a relative --bake-record
#       is refused while the file plainly exists. BAKEREC1 measured it; the
#       wording now says where it looked. The FLOOR is the other half: an
#       ABSOLUTE path to a file that really is missing must still refuse, and
#       name that file's own directory.
#
# USAGE
#   bash tests/spells/gamemanager_archlock.sh
#   LEGS=abc bash tests/spells/gamemanager_archlock.sh
#   EXE=... RUNG=... LIMIT=90 OUT=<dir> bash tests/spells/gamemanager_archlock.sh
#
# THE RUNG IS BUILT FROM THIS TREE, not taken off the shelf.
#   release/NifSkope.archlock1_rung.exe is this working tree with the two
#   `archiveReadLock.unlock()` lines in src/gamemanager.cpp REMOVED and nothing
#   else changed. It has to be: the harness and the `archlock-probe` verb are
#   new in this lane, so an older exe -- 01:00, 01:17 or any other -- cannot run
#   either leg at all, and "the old exe did not hang" would mean only that it
#   never reached the lock. One variable, one difference, which is the whole
#   point of a refuter. Build it with tools/archlock1_build_rung.sh, or by hand:
#   drop the two lines, build, copy release/NifSkope.exe to that name, put the
#   two lines back and build again.
#
#   Leg (d) is the exception: it compares BYTES ACROSS BUILDS, so it uses $PREV
#   (release/NifSkope.before_archlock1.exe, the real previous build). Comparing
#   the new exe against a rung that differs from it in one file would prove much
#   less.
#
# Leg (d) bakes twice and takes minutes; LEGS=abce skips it.

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
RUNG="${RUNG:-$ROOT/release/NifSkope.archlock1_rung.exe}"
PREV="${PREV:-$ROOT/release/NifSkope.before_archlock1.exe}"
FO4="${FO4:-/x/Programs/Steam/steamapps/common/Fallout 4/Data}"
ESM="${ESM:-$FO4/Fallout4.esm}"
# leg (d) only needs a region small enough to bake twice in a gate: four
# cells from the same corner lodgen_native.sh starts at.
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
REGION="${REGION:--20 24 -17 27}"
LIMIT="${LIMIT:-90}"
PORT="${PORT:-45931}"
LEGS="${LEGS:-abcde}"
SHOT="${SHOT:-}"
WSHOT="${WSHOT:-}"
# ABSOLUTE, always. The exe writes these itself and its working directory is
# its own folder, so a relative SHOT lands nowhere: the first run of this gate
# reported `REFUSED, 867x730` for a grab that had rendered perfectly.
case "$SHOT" in "" | /* | ?:* ) ;; *) SHOT="$PWD/$SHOT" ;; esac
case "$WSHOT" in "" | /* | ?:* ) ;; *) WSHOT="$PWD/$WSHOT" ;; esac
GUILOG="$ROOT/release/ww_archlock_test.log"

# The fixture: one of the meshes bungo's own Files tab was filtered to when it
# froze ("armor"). Its material and its texture live in two OTHER archives, so
# the lookup cannot be served from wherever the mesh came from.
NIFV="${NIFV:-meshes/armor/combatarmor/m_arm_heavy_l.nif}"
MATV="Materials/Armor/Combat/CombatArmor_Arm.BGSM"
TEXV="${TEXV:-Armor/CombatArmor/CombatArmor_Arm_d.dds}"
NIFDISK="${NIFDISK:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Armor/CombatArmor/M_Arm_Heavy_L.nif}"
A_MESH="$FO4/Fallout4 - Meshes.ba2"
A_MAT="$FO4/Fallout4 - Materials.ba2"
A_TEX="$FO4/Fallout4 - Textures3.ba2"

[ -x "$EXE" ]  || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -x "$RUNG" ] || { echo "no rung exe at $RUNG"; exit 2; }
[ -x "$PREV" ] || PREV="$RUNG"
[ -s "$A_MESH" ] || { echo "no meshes archive at $A_MESH"; exit 2; }
[ -s "$A_MAT" ]  || { echo "no materials archive at $A_MAT"; exit 2; }
[ -s "$A_TEX" ]  || { echo "no textures archive at $A_TEX"; exit 2; }
[ -s "$NIFDISK" ] || { echo "no disk copy of the fixture nif at $NIFDISK"; exit 2; }
# CONSTITUTION 6: one instance, and never while the game is up.
if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "REFUSED: Fallout4.exe is running"; exit 2
fi
if tasklist 2>/dev/null | grep -qi "NifSkope"; then
	echo "REFUSED: a NifSkope is already running -- one instance ever"; exit 2
fi

CHECKS=0
FAILS=0
ok()   { CHECKS=$((CHECKS+1)); echo "  ok   $1"; }
bad()  { CHECKS=$((CHECKS+1)); FAILS=$((FAILS+1)); echo "  FAIL $1"; }
note() { echo "    $1"; }
check(){ if [ "$1" = "0" ]; then ok "$2"; else bad "$2"; fi; }

KEEP="${OUT:-}"
if [ -n "$KEEP" ]; then W="$KEEP"; mkdir -p "$W"
else W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT; fi
WA="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })"

ROOTS="$(winpath "$A_MESH");$(winpath "$A_MAT");$(winpath "$A_TEX")"

# ---------------------------------------------------------------------------
# The watchdog. Returns 0 when the command finished by itself, 124 when it had
# to be killed. SIGKILL after SIGTERM because a thread parked in
# QReadWriteLock::lockForWrite never runs an event loop again and cannot answer
# a polite close.
# ---------------------------------------------------------------------------
run_with_watchdog() {	# $1 = label, rest = command
	local label="$1"; shift
	# `timeout`, NOT a hand-rolled background job.
	#
	# The first version backgrounded the exe, polled `kill -0` once a second and
	# then asked `wait` for the status. That is a RACE: when the process is gone
	# bash may already have reaped it and forgotten the status, and `wait` then
	# answers 127 -- "no such job" -- for a run that exited 0. Measured twice on
	# the same passing exe, seconds apart: `exits 0 in 7s` the first time and
	# `did not finish: rc=127 after 6s` the second, with a complete PASS log on
	# disk both times. A refuter whose verdict flips on a reaping race refutes
	# nothing. `timeout` runs the child in the foreground and answers its real
	# exit code, or 124 when it had to kill it -- which is the same 124 this
	# gate already treats as the hang.
	local start end
	start="$(date +%s)"
	timeout -k 5 "$LIMIT" "$@" >"$W/$label.out" 2>&1
	RC=$?
	if [ "$RC" = "127" ]; then
		# 127 here is not the program's answer: Windows still had the image of
		# the process that just ended open, CreateProcess failed and NOTHING
		# RAN. Seen 2 times in about 15 launches, always straight after the
		# previous exe exited, and once inside this gate -- where it was
		# printed as `the exe under test did not finish`, which is exactly the
		# row a refuter must never invent. A launch that never happened is
		# retried once, after a pause, and only the second answer counts.
		sleep 3
		start="$(date +%s)"
		timeout -k 5 "$LIMIT" "$@" >"$W/$label.out" 2>&1
		RC=$?
	fi
	end="$(date +%s)"
	SECS=$((end-start))
	return 0
}

gui_run() {	# $1 = exe, $2 = label
	rm -f "$GUILOG"
	WW_ARCHLOCK_TEST=1 \
	WW_ARCHLOCK_RESOURCES="$ROOTS" \
	WW_ARCHLOCK_NIF="$NIFV" \
	WW_ARCHLOCK_TEXTURE="$TEXV" \
	WW_ARCHLOCK_SHOT="${SHOT:+$(winpath "$SHOT")}" \
	WW_ARCHLOCK_WINDOW_SHOT="${WSHOT:+$(winpath "$WSHOT")}" \
	WW_WINDOW_AT="$WW_WINDOW_AT" \
		run_with_watchdog "$2" "$1" --port "$PORT"
	cp -f "$GUILOG" "$W/$2.log" 2>/dev/null || true
}

cli_run() {	# $1 = exe, $2 = label
	run_with_watchdog "$2" "$1" -no-gui archlock-probe "$(winpath "$NIFDISK")" \
		--data-root "$ROOTS" --probe "$TEXV"
}

echo "== gamemanager_archlock: the read->write upgrade that never comes =="
echo "exe : $EXE"
echo "rung: $RUNG"
echo "prev: $PREV"
echo "roots: $ROOTS"
echo "limit: ${LIMIT}s"

# ---------------------------------------------------------------------------
case "$LEGS" in *a*)
echo
echo "== (a) the refuter: a real window, the configured-resource route =="

gui_run "$RUNG" "rung_gui"
if [ "$RC" = "124" ]; then
	ok "(a) THE RUNG HANGS -- killed after ${SECS}s (this is the passing refuter row)"
else
	bad "(a) the rung was supposed to hang; it exited rc=$RC after ${SECS}s"
fi
note "the rung's log stopped at:"
tail -2 "$W/rung_gui.log" 2>/dev/null | sed 's/^/      /'
if grep -q '^opening ' "$W/rung_gui.log" 2>/dev/null && ! grep -q '^done$' "$W/rung_gui.log" 2>/dev/null; then
	ok "(a) ...and it stopped AT THE OPEN, not somewhere else"
else
	bad "(a) the rung did not stop at the open -- the hang is not the one under test"
fi
# and the floors the harness itself measured, before it died
if grep -q 'the shared Fallout 4 index is NOT built yet' "$W/rung_gui.log" 2>/dev/null; then
	ok "(a floor) the rung reached the open with the shared index unbuilt"
else
	bad "(a floor) the rung never recorded the precondition"
fi

gui_run "$EXE" "new_gui"
if [ "$RC" = "0" ]; then
	ok "(a) the exe under test opens it and exits 0 in ${SECS}s"
else
	bad "(a) the exe under test did not finish: rc=$RC after ${SECS}s"
fi
if [ -f "$W/new_gui.log" ]; then
	sed 's/^/    /' "$W/new_gui.log"
	if grep -q '^PASS$' "$W/new_gui.log"; then
		ok "(a) ...and every check inside the harness passed"
	else
		bad "(a) the harness ran but did not pass"
	fi
else
	bad "(a) the exe under test wrote no harness log"
fi
;; esac

# ---------------------------------------------------------------------------
case "$LEGS" in *b*)
echo
echo "== (b) the same with no window: archlock-probe =="

cli_run "$RUNG" "rung_cli"
if [ "$RC" = "124" ]; then
	ok "(b) THE RUNG HANGS with no window either -- killed after ${SECS}s"
else
	bad "(b) the rung's CLI probe was supposed to hang; rc=$RC after ${SECS}s"
fi
note "the rung's probe printed:"
sed 's/^/      /' "$W/rung_cli.out" 2>/dev/null | tail -6
if grep -q 'archlock-probe getFile' "$W/rung_cli.out" 2>/dev/null; then
	bad "(b) the rung answered the lookup -- it never reached the lock"
else
	ok "(b) ...and it never answered the lookup (it died inside get_file)"
fi

cli_run "$EXE" "new_cli"
if [ "$RC" = "0" ]; then
	ok "(b) the exe under test answers and exits 0 in ${SECS}s"
else
	bad "(b) the exe under test's probe: rc=$RC after ${SECS}s"
fi
sed 's/^/    /' "$W/new_cli.out" 2>/dev/null
grep -q 'archlock-probe sharedIndexBuilt 0' "$W/new_cli.out" 2>/dev/null \
	&& ok "(b floor) the shared index really was unbuilt when the probe started" \
	|| bad "(b floor) the shared index was already built -- the probe measured nothing"
grep -q 'archlock-probe documentDataPaths 0' "$W/new_cli.out" 2>/dev/null \
	&& ok "(b floor) the document really had an EMPTY data path" \
	|| bad "(b floor) the document had a data path -- wrong condition"
grep -q 'archlock-probe fallsThroughToShared 1' "$W/new_cli.out" 2>/dev/null \
	&& ok "(b floor) ...and it falls through to the shared index" \
	|| bad "(b floor) the document does not fall through to the shared index"
grep -q 'archlock-probe getFile found' "$W/new_cli.out" 2>/dev/null \
	&& ok "(b) get_file answers through the parent, with bytes" \
	|| bad "(b) get_file did not find the material"
;; esac

# ---------------------------------------------------------------------------
case "$LEGS" in *c*)
echo
echo "== (c) find_file, the twin, on a TEXTURE =="
grep -q 'archlock-probe findFile found' "$W/new_cli.out" 2>/dev/null \
	&& ok "(c) find_file resolves the texture through the parent (CLI)" \
	|| bad "(c) find_file did not resolve the texture (CLI)"
grep -q 'find_file answers through the parent, and FINDS it' "$W/new_gui.log" 2>/dev/null \
	&& ok "(c) and the same through the open document in the window (GUI)" \
	|| bad "(c) the GUI harness's find_file row is missing or red"
;; esac

# ---------------------------------------------------------------------------
case "$LEGS" in *d*)
echo
echo "== (d) the lock is KEPT: the chunk workers still bake the same bytes =="
if [ ! -s "$ESM" ]; then
	note "SKIP (d): no plugin at $ESM"
else
	for who in rung new; do
		exe="$PREV"; [ "$who" = "new" ] && exe="$EXE"
		rm -rf "$W/bake_$who"; mkdir -p "$W/bake_$who"
		# --worldspace and --terrain-region X0 Y0 X1 Y1, the spelling
		# tests/spells/lodgen_native.sh uses. The first draft said
		# `--region`, which is not a flag: both bakes exited 2 with the
		# worldspace list on stdout and wrote no files, and the leg's own
		# floor is what caught it (0 files compared).
		"$exe" -no-gui lodgen "$(winpath "$ESM")" --worldspace 3C \
			--terrain-region $REGION --dim 4 --data-root "$(winpath "$DATA")" \
			--out-dir "$WA/bake_$who" >"$W/bake_$who.out" 2>&1
		note "$who bake rc=$? files=$(find "$W/bake_$who" -type f | wc -l)"
	done
	# the bake record names the exe that wrote it and the minute it ran, so it
	# is excluded by name rather than by hoping it matches
	same=0; diffn=0
	while IFS= read -r f; do
		rel="${f#$W/bake_new/}"
		case "$rel" in *.lodb) continue ;; esac
		if [ -f "$W/bake_rung/$rel" ] && cmp -s "$f" "$W/bake_rung/$rel"; then
			same=$((same+1))
		else
			diffn=$((diffn+1)); note "differs: $rel"
		fi
	done < <(find "$W/bake_new" -type f)
	note "$same identical, $diffn differ, vs $PREV (the .lodb bake record is excluded: it stamps the exe)"
	[ "$same" -gt 0 ] && ok "(d floor) the comparison actually compared files ($same)" \
		|| bad "(d floor) nothing was compared"
	[ "$diffn" = "0" ] && ok "(d) the fixture region is byte-identical to the rung's" \
		|| bad "(d) $diffn file(s) differ from the rung's bake"
fi
;; esac

# ---------------------------------------------------------------------------
case "$LEGS" in *e*)
echo
echo "== (e) the --bake-record refusal names the directory it looked in =="
REL="scratchpad/archlock1_20260917/no_such_record.lodb"
out="$("$EXE" -no-gui lodgen --bake-record "$REL" 2>&1 | head -2)"
note "$out"
case "$out" in
	*"bake-record REFUSED"*) ok "(e) a relative path is still refused (behaviour unchanged)" ;;
	*) bad "(e) the relative path was not refused" ;;
esac
case "$out" in
	*"looked in"*) ok "(e) ...and the refusal says where it looked" ;;
	*) bad "(e) the refusal does not say where it looked" ;;
esac
# The drive letter is spelled by whoever printed the path: winpath() keeps the
# shell's (e:/...), Qt answers E:/... . Same directory; compare without case.
EXEDIR="$(winpath "$(cd "$(dirname "$EXE")" && pwd)")"
if printf '%s' "$out" | grep -qi -- "$EXEDIR"; then
	ok "(e) ...and the directory it names is the NIFSKOPE folder, which is the surprise"
else
	bad "(e) the named directory is not the exe's folder: expected $EXEDIR"
fi
# THE FLOOR: an absolute path that really is missing must still refuse, and must
# name ITS OWN directory, not the exe's -- otherwise the clause is a constant.
ABS="$WA/definitely_absent.lodb"
out2="$("$EXE" -no-gui lodgen --bake-record "$ABS" 2>&1 | head -2)"
note "$out2"
if printf '%s' "$out2" | grep -qi -- "looked in $WA"; then
	ok "(e floor) an absolute missing path names ITS OWN directory"
else
	bad "(e floor) the clause does not follow the path it was given"
fi
;; esac

echo
echo "== gamemanager_archlock: $CHECKS check(s), $FAILS failure(s) =="
[ "$FAILS" = "0" ] && { echo "RESULT PASS"; exit 0; }
echo "RESULT FAIL"
exit 1
