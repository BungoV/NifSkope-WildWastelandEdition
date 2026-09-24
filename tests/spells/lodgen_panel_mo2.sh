#!/bin/bash
#
# THE LOD PANEL'S "MOD ORGANIZER 2 PROFILE" SOURCE, DRIVEN (lane LOADORDER1, 2026-09-24).
#
# The panel reads a Mod Organizer 2 profile off disk (modlist.txt + plugins.txt,
# no Mod Organizer running) through the same reader as `lodgen --mo2-profile`.
# The self-test leg WW_LODGEN_MO2DISK=1 (src/nifskope_ui.cpp) sets the Source
# row, reads the resolved plugin list and mod order off the widgets, shows a bad
# profile refused by name, then presses Generate for one FO4CS chunk (Sanctuary,
# -20,24, dim 4) into a STAND-IN mod folder named FO4CSLOD inside $OUT -- never
# into his mods folder -- and walks what landed.
#
# The profile is a COPY of his (modlist.txt verbatim; plugins.txt with every
# plugin the ESM reader refuses unticked -- TestWorldspace.esp on 2026-09-24,
# see lodgen_loadorder.sh G5), read against his real mods folder.
#
# Checks (lines carrying MO2DISK in release/ww_lodgen_test.log, floor 14):
#   the Source row offers the choice; a bad profile is refused by name and
#   Generate refuses; plugin 0 is Fallout4.esm by full path; every plugin an
#   existing full path; the plugin count and the mod-order row count equal the
#   checker's independent re-derivation; Resources hidden, the list read only,
#   the profile rows shown; Commonwealth offered; the status line; the GUI bake
#   writes FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.lodo; no .BTO/.BTR
#   anywhere in the mod folder and no scratch folder left (bungo's R2).
# Plus, here: the panel's plugin list equals `lodgen --mo2-profile --print-source`
# on the same profile, path for path.
# RED: the rung exe, same environment -- it has no such source.
#
# USAGE
#   RUNG=<pre-lane exe> OUT=<dir> bash tests/spells/lodgen_panel_mo2.sh

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
RUNG="${RUNG:-}"
PROFILE="${PROFILE:-E:/Projects/Fallout 4 Mods/profiles/Default}"
MODS="${MODS:-E:/Projects/Fallout 4 Mods/mods}"
DATA="${DATA:-X:/Programs/Steam/steamapps/common/Fallout 4/Data}"
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
PORT="${PORT:-42377}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
CHK="$ROOT/tests/spells/lodgen_loadorder_check.py"
LOG="$ROOT/release/ww_lodgen_test.log"
if [ -n "${OUT:-}" ]; then mkdir -p "$OUT"; W="$OUT"; else W="$(mktemp -d)"; fi
WA="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no source NIF at $SRC"; exit 2; }
echo "exe   : $NS ($(stat -c%s "$NS") bytes, $(stat -c%y "$NS" | cut -c1-19))"
checks=0; fails=0
ok()  { checks=$((checks+1)); echo "  ok   $*"; }
bad() { checks=$((checks+1)); fails=$((fails+1)); echo "  FAIL $*"; }

# the profile copy (read only on his side: two files copied, nothing written there)
mkdir -p "$W/prof_panel"
"$NS" -no-gui lodgen --mo2-profile "$PROFILE" --print-source 2>&1 | tr -d '\r' > "$W/panel_src_live.txt"
cp "$PROFILE/modlist.txt" "$W/prof_panel/"
"$PY" "$CHK" untick "$W/panel_src_live.txt" "$PROFILE/plugins.txt" "$W/prof_panel/plugins.txt"
read NP NS_ROWS <<< "$("$PY" "$CHK" counts "$WA/prof_panel" "$MODS" "$DATA")"
echo "  expected: $NP plugins, $NS_ROWS mod-order rows"

# one GUI run: exe, tag
run_panel() {
	rm -f "$LOG"
	WW_LODGEN_TEST=1 WW_LODGEN_MO2DISK=1 \
		WW_LODGEN_MO2_PROFILE="$WA/prof_panel" WW_LODGEN_MO2_MODS="$MODS" \
		WW_LODGEN_MO2_NPLUGINS="$NP" WW_LODGEN_MO2_NSTACK="$NS_ROWS" \
		WW_LODGEN_MO2_OUT="$WA/$2/FO4CSLOD" WW_LODGEN_MO2_SHOT="$WA/$2_panel_mo2.png" \
		"$1" --port "$PORT" "$SRC" >/dev/null 2>&1
	if [ -f "$LOG" ]; then tr -d '\r' < "$LOG" > "$W/$2_test.log"; else : > "$W/$2_test.log"; fi
}

echo; echo "== the panel, this exe =="
t0=$(date +%s)
run_panel "$NS" panel
echo "  GUI run $(( $(date +%s) - t0 )) s; $(grep -c '' "$W/panel_test.log") log lines; whole self-test: $(grep -a ' checks, ' "$W/panel_test.log" | tail -1)"
grep -a "MO2DISK status\|MO2DISK plugins \|MO2DISK run finished\|MO2DISK [0-9]* files\|MO2DISK shot" "$W/panel_test.log" | cut -c1-220 | sed 's/^/  |/'
N_OK=$(grep -ac "^  ok   MO2DISK" "$W/panel_test.log"); N_FAIL=$(grep -ac "^  FAIL MO2DISK" "$W/panel_test.log")
grep -a "^  FAIL MO2DISK" "$W/panel_test.log" | sed 's/^/  |/'
if [ "$N_FAIL" = "0" ] && [ "$N_OK" -ge 14 ]; then ok "the panel's MO2DISK leg: $N_OK checks, 0 failures (floor 14)"
else bad "the panel's MO2DISK leg: $N_OK ok, $N_FAIL failed (floor 14)"; fi
WHOLE="$(grep -a ' checks, ' "$W/panel_test.log" | tail -1)"
case "$WHOLE" in
	*" 0 failures"*) ok "and the whole WW_LODGEN_TEST suite beside it stays green ($WHOLE)" ;;
	*) bad "the whole WW_LODGEN_TEST suite: '${WHOLE:-no count}'"; grep -a "^  FAIL" "$W/panel_test.log" | grep -v MO2DISK | sed 's/^/  |/' ;;
esac

# the panel resolves exactly what the command line resolves
"$NS" -no-gui lodgen --mo2-profile "$WA/prof_panel" --mo2-mods "$MODS" --data-root "$DATA" --print-source 2>&1 | tr -d '\r' \
	| grep -E "^plugin [0-9]+: " | sed 's/^plugin \([0-9]*\): /\1 /' > "$W/panel_cli_plugins.txt"
grep -a "^  MO2DISK plugin [0-9]*: " "$W/panel_test.log" | sed 's/^  MO2DISK plugin \([0-9]*\): /\1 /' > "$W/panel_gui_plugins.txt"
if [ -s "$W/panel_cli_plugins.txt" ] && cmp -s "$W/panel_cli_plugins.txt" "$W/panel_gui_plugins.txt"; then
	ok "the panel's plugin list equals lodgen --mo2-profile --print-source, path for path ($(grep -c '' "$W/panel_cli_plugins.txt"))"
else bad "the panel's plugin list differs from the command line's ($(grep -c '' "$W/panel_gui_plugins.txt") vs $(grep -c '' "$W/panel_cli_plugins.txt"))"; fi

if [ -n "$RUNG" ]; then
	echo; echo "== RED: the rung, same environment =="
	run_panel "$RUNG" rung
	R_OK=$(grep -ac "^  ok   MO2DISK" "$W/rung_test.log"); R_FAIL=$(grep -ac "^  FAIL MO2DISK" "$W/rung_test.log")
	echo "  rung: $R_OK MO2DISK ok, $R_FAIL failed; whole self-test: $(grep -a ' checks, ' "$W/rung_test.log" | tail -1)"
	if [ "$R_FAIL" != "0" ] || [ "$R_OK" -lt 14 ]; then ok "RED: the rung's panel has no MO2 profile source ($R_OK of floor 14)"
	else bad "RED: the rung passed the MO2DISK leg"; fi
	[ -d "$W/rung/FO4CSLOD" ] && echo "  rung wrote into its stand-in: $(find "$W/rung/FO4CSLOD" -type f | wc -l) files" || echo "  rung wrote nothing"
fi

echo
echo "$checks checks, $fails failures"
[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
