#!/bin/bash
#
# Does loading a nif destroy a browsed archive tree?
#
# THE BUG, as reported: "Loading any nif from the nif browser resets the NIF
# folder tree."
#
# WHY A SECOND HARNESS
#
# WW_BROWSER_TEST already covers the CONFIGURED "Available NIFs" tree, and it
# passes: an unchanged (game, resource paths) signature takes a fast path that
# reuses the tree entirely, so an ordinary same-game load leaves it alone. The
# reported symptom lives in the OTHER mode, which that harness never enters, and
# adding an assertion to it would not have reached this.
#
# Browse mode (File > Browse Archive / Browse Game Folder) sets
# currentArchivePath, and `configuredIndexLive` is
# `currentArchive && currentArchivePath.isEmpty()` -- so in browse mode the fast
# path can NEVER be taken, and every load fell through to the full teardown and
# rebuilt from the configured resource paths instead.
#
# The third check is the one that matters most, and it is invisible from the UI:
# the teardown also deletes currentArchive and clears currentArchiveNames, and
# openArchiveFileString() returns immediately when that list is empty. So the
# first load out of a browsed archive did not merely reset the tree -- it made
# every other file in that archive unopenable.
#
# Verified to fail on the code this replaces. With the guard removed:
#
#   after load: archive path set: 0, ... archive names 0, anchor still valid 0
#   FAIL the browsed tree survives a load
#   FAIL the browser is still in archive mode
#   FAIL the archive can still be opened from
#
# A run that never enters browse mode is a FAILURE, not a skip -- the first
# version of the harness reported "0 checks, 0 failures / PASS" from a test that
# had not run.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# PHASE 2 (2026-08-31): the same hook against Fallout 76's GeneratedMeshes
# archive, with WW_ARCHIVEBROWSE_EXPECT=bto — the tree the user SEES (through
# the proxy) must contain .bto file rows. Added for the report "I cannot browse
# those .bto meshes in the nif browser": the index, the tree build and the
# proxy all accept .bto in code, so the check is against the assembled view,
# where any of them failing would land. Skipped with a note if the FO76
# install is absent.
#
# USAGE
#   bash tests/browser/archive_browse_survives_load.sh
#   TARGET=/path/to/Data bash tests/browser/archive_browse_survives_load.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
# a Data folder (loose meshes are fine) or a single .ba2
TARGET="${TARGET:-E:/Tools/Fallout 4/DataUnpacked/Data}"
TARGET76="${TARGET76:-X:/Programs/Steam/steamapps/common/Fallout76/Data/SeventySix - GeneratedMeshes01.ba2}"
PORT="${PORT:-46221}"
LOG="$ROOT/release/ww_archivebrowse_test.log"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }

# The hook writes its verdict and then quits; give the app a whole minute and
# then stop waiting for it — a launch that lingers after 'done' is not a
# finding of this suite (the log already holds the verdict).
run_case() {	# $1 target, $2 expected extensions ('' = none)
	rm -f "$LOG"
	WW_ARCHIVEBROWSE_EXPECT="$2" WW_ARCHIVEBROWSE_TEST="$1" \
		"$EXE" --port "$PORT" > /dev/null 2>&1 &
	local pid=$!
	for _ in $(seq 1 60); do
		[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
		sleep 1
	done
	kill "$pid" 2>/dev/null
	wait "$pid" 2>/dev/null
	[ -f "$LOG" ] || { echo "harness produced no log"; return 1; }
	tr -d '\r' < "$LOG"
	grep -q '^PASS' "$LOG"
}

rc=0
run_case "$TARGET" "" || rc=1

if [ -f "$TARGET76" ]; then
	echo "--- FO76 GeneratedMeshes: .bto must be visible ---"
	run_case "$TARGET76" "bto" || rc=1
else
	echo "SKIP: no FO76 archive at $TARGET76"
fi

exit "$rc"
