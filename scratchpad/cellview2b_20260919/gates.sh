#!/bin/bash
# CELLVIEW2B: the harness chain, ONE NifSkope at a time, each log its own file.
# Lock: a second copy is impossible, not merely unlikely.
set -u
REPO=/e/Projects/NifskopeWildWastelandEdition
OUT="$REPO/scratchpad/cellview2b_20260919"
mkdir -p "$OUT/logs"
mkdir "$OUT/.lock" 2>/dev/null || { echo "REFUSED: a chain is already running ($OUT/.lock)"; exit 8; }
trap 'rmdir "$OUT/.lock" 2>/dev/null' EXIT
cd "$REPO" || exit 9

SUM="$OUT/gates_summary_${1:-all}.txt"
: > "$SUM"
say () { echo "$@" | tee -a "$SUM"; }

run () {  # run <label> <secs> <cmd...>
  local label="$1"; shift; local secs="$1"; shift
  say "### $label start $(date +%H:%M:%S)"
  timeout "$secs" "$@" > "$OUT/logs/$label.log" 2>&1
  local rc=$?
  say "### $label rc=$rc  $(date +%H:%M:%S)"
  grep -E "checks, [0-9]+ failures|[0-9]+ steps, [0-9]+ failures|^PASS|^FAIL|^RESULT|^skips:" "$OUT/logs/$label.log" | tail -5 | tee -a "$SUM"
  say ""
}

case "${1:-all}" in
cell)
  run cell_pick         1800 bash tests/spells/cell_pick.sh
  run cell_open          900 bash tests/spells/cell_open.sh
  run cell_open_red      900 bash tests/spells/cell_open.sh --red
  ;;
controls)
  run render_shot       1800 bash tests/spells/render_shot.sh
  run harness_window     900 bash tests/spells/harness_window.sh
  run native_open       1200 bash tests/spells/native_open.sh
  run impostor_draw     2400 bash tests/spells/impostor_draw.sh
  run lodgen_octahedral 2400 bash tests/spells/lodgen_octahedral.sh
  ;;
misc)
  run gltf_gates        1800 bash tests/spells/gltf_gates.sh
  run gltf_export_opts  1200 bash tests/spells/gltf_export_options.sh
  run body_build        1200 bash tests/spells/body_build.sh
  ;;
cell2)
  # On the FINAL exe (18:14:53), and with the Sanctuary `.lodi` this lane baked
  # -- rows 4, 5 and 5r SKIP by name without it.
  L="$REPO/scratchpad/cellview2b_20260919/lodibake/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi"
  run cell_pick2      1800 bash tests/spells/cell_pick.sh
  run cell_pick_lodi  1800 env LODI="$L" bash tests/spells/cell_pick.sh
  ;;
impostor)
  # The fixtures are lane IMPOSTORFIX3's own bake, named by its green run
  # (gate_green.txt, 16:18:25, 24 steps 0 failures). The MESH is the card's
  # own `source` field: cards/000531b3_oct.lodm says "TreeMapleblasted05.nif".
  F="$REPO/scratchpad/impostorfix3_20260919/fixture"
  run impostor_draw 2400 env \
    IMPOSTOR_LODM="$F/blast_n4/cards/000531b3_oct.lodm" \
    IMPOSTOR_LODM_MORE="$F/blast_n8/cards/000531b3_oct.lodm" \
    IMPOSTOR_NIF="/e/Tools/Fallout 4/DataUnpacked/Data/Meshes/Landscape/Trees/TreeMapleblasted05.nif" \
    bash tests/spells/impostor_draw.sh
  ;;
rest)
  F="$REPO/scratchpad/impostorfix3_20260919/fixture"
  run impostor_draw 2400 env \
    IMPOSTOR_LODM="$F/blast_n4/cards/000531b3_oct.lodm" \
    IMPOSTOR_LODM_MORE="$F/blast_n8/cards/000531b3_oct.lodm" \
    IMPOSTOR_NIF="/e/Tools/Fallout 4/DataUnpacked/Data/Meshes/Landscape/Trees/TreeMapleblasted05.nif" \
    bash tests/spells/impostor_draw.sh
  run gltf_gates        1800 bash tests/spells/gltf_gates.sh
  run gltf_export_opts  1200 bash tests/spells/gltf_export_options.sh
  run body_build        1200 bash tests/spells/body_build.sh
  ;;
esac
say "CHAIN DONE $(date +%H:%M:%S)"
