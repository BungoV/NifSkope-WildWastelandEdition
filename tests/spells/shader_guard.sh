#!/bin/bash
#
# NO SILENT GREY PICTURES (lane VOLAIR1, 2026-10-15; src/gl/glcontext.cpp printCompileError,
# tests/spells/_harness.sh ww_notes_shader_refusal).
#
# A shader that failed to compile or link used to show only in a message box that a headless run
# never saw; every draw that wanted the program fell back and the picture came out grey or
# untextured while looking like a measurement (the VOLAIR1 Memory Den shots, 10-15). Now a harness
# run stops at the first such error: exit 87, no picture, "SHADER REFUSED: <text>" in the notes.
#
# Two runs of one small cell shot (Vault111Cryo, lit):
#   green  the shipped shaders: a picture, exit 0, ww_notes_shader_refusal finds nothing
#   red    WW_SHADER_RED=<shader> breaks that one shader on purpose: exit 87, NO picture, and
#          ww_notes_shader_refusal must name the error (default fo4_default.frag; any shader works)
# PASS needs both. A guard that lets the red through is the bug this spell exists for.
#
# USAGE  bash tests/spells/shader_guard.sh [shader-file-to-break]

set -u
. "$(dirname "$0")/_harness.sh"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE=shader_guard
REGKEY="HKCU\Software\NifTools\NifSkope 2.0 $SCOPE"
BREAK="${1:-fo4_default.frag}"
OUT="$REPO/scratchpad/shader_guard"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
mkdir -p "$OUT"
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ "$REPO/src/gl/glcontext.cpp" -nt "$EXE" ] && { echo "FAIL  src/gl/glcontext.cpp is NEWER than the exe"; exit 1; }

shot() {   # shot <tag> <env...>; sets RC
	local tag="$1"; shift
	reg delete "$REGKEY" //f > /dev/null 2>&1
	reg add "$REGKEY\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1
	local gm; gm="$(mktemp)"
	python "$(dirname "$0")/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm" "$OUT/$tag.png"
	env "$@" WW_RENDER_CENTER=-4600,-280,0 WW_RENDER_DIST=250 WW_RENDER_FOV=70 WW_RENDER_VIEW=1 \
		WW_CELL_OPEN="$ESM|interior|Vault111Cryo" WW_CELL_DATAROOT="$DATA" WW_CELL_LIT=1 \
		WW_RENDER_SHOT="$(winpath "$OUT/$tag.png")" WW_RENDER_SIZE=960x600 WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE=$SCOPE timeout 900 "$EXE" --port 14746 "$(winpath "$REPO/tests/fixtures/empty.wwcell")" \
		> "$OUT/$tag.notes" 2>&1
	RC=$?
	reg delete "$REGKEY" //f > /dev/null 2>&1
}

FAIL=0
shot green; GRC=$RC
if g="$(ww_notes_shader_refusal "$OUT/green.notes")"; then gref=none; else gref=found; fi
gpic=NO; [ -s "$OUT/green.png" ] && gpic=yes
if [ "$GRC" = 0 ] && [ "$gpic" = yes ] && [ "$gref" = none ]; then
	echo "PASS  green: exit 0, picture, no shader refusal"
else
	echo "FAIL  green: exit $GRC, picture $gpic, refusal $gref"; [ -n "$g" ] && printf '%s\n' "$g"; FAIL=1
fi

shot red WW_SHADER_RED="$BREAK"; RRC=$RC
if r="$(ww_notes_shader_refusal "$OUT/red.notes")"; then rref=none; else rref=found; fi
rpic=no; [ -s "$OUT/red.png" ] && rpic=YES
if [ "$RRC" = 87 ] && [ "$rpic" = no ] && [ "$rref" = found ] && printf '%s' "$r" | grep -q 'WW_SHADER_RED'; then
	echo "PASS  red ($BREAK broken): exit 87, no picture, the notes name it:"
	printf '%s\n' "$r" | head -3
else
	echo "FAIL  red ($BREAK broken): exit $RRC (want 87), picture $rpic (want no), refusal $rref -- the guard let a broken shader through"
	FAIL=1
fi
echo "SHADER GUARD VERDICT $( [ $FAIL = 0 ] && echo PASS || echo FAIL )"
exit $FAIL
