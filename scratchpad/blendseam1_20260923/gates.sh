#!/bin/bash
# BLENDSEAM1 gate chain, sequential, one NifSkope at a time.
ROOT=/e/Projects/NifskopeWildWastelandEdition
H=$ROOT/scratchpad/blendseam1_20260923
cd "$ROOT"
if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then echo "REFUSED: game up"; exit 2; fi
echo "exe $(stat -c '%s %y' release/NifSkope.exe) sha1 $(sha1sum release/NifSkope.exe | cut -c1-8)"
S=$(date +%s); bash tests/spells/lodgen_terrain_vt.sh > "$H/terrain_vt_new.log" 2>&1
echo "terrain_vt rc=$? $(( $(date +%s) - S ))s: $(grep -E '^[0-9]+ checks' "$H/terrain_vt_new.log")"
S=$(date +%s); bash "$H/terrain_gate.sh" > "$H/gate_terrain.log" 2>&1
echo "terrain_gate rc=$? $(( $(date +%s) - S ))s: $(grep RESULT "$H/gate_terrain.log")"
S=$(date +%s); bash "$H/card_gate.sh" > "$H/gate_card.log" 2>&1
echo "card_gate rc=$? $(( $(date +%s) - S ))s: $(grep RESULT "$H/gate_card.log")"
S=$(date +%s); bash tests/spells/lodgen_panel_run.sh > "$H/gate_panel_run.log" 2>&1
echo "panel_run rc=$? $(( $(date +%s) - S ))s: $(grep -E 'checks|RESULT' "$H/gate_panel_run.log" | tail -2 | tr '\n' ' ')"
echo "left running: $(tasklist 2>/dev/null | grep -ci nifskope)"
