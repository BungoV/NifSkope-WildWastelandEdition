#!/bin/bash
# Lane BUILD11: the harness chain. ONE NifSkope instance at a time -- a chain, never a
# fan-out (nifskope-ww-resume-pending s5). Each harness logs separately and its summary
# line is echoed as it lands, into scratchpad/build11_20260910/gates_summary.txt.
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
OUT=scratchpad/build11_20260910
mkdir -p "$OUT/logs"
SUM="$OUT/gates_summary.txt"
: > "$SUM"

run () {
  local label="$1"; shift
  local secs="$1"; shift
  echo "### $label start $(date +%H:%M:%S)" | tee -a "$SUM"
  timeout "$secs" "$@" > "$OUT/logs/$label.log" 2>&1
  local rc=$?
  echo "### $label rc=$rc  $(date +%H:%M:%S)" | tee -a "$SUM"
  grep -E "checks, [0-9]+ failure|^PASS$|^FAIL$|^RESULT" "$OUT/logs/$label.log" | tail -4 | tee -a "$SUM"
  grep -c "^  ok" "$OUT/logs/$label.log" | sed 's/^/    ok-lines: /' | tee -a "$SUM"
  grep "SKIP" "$OUT/logs/$label.log" | head -6 | sed 's/^/    /' | tee -a "$SUM"
  echo "" | tee -a "$SUM"
}

for g in "$@"; do
  case "$g" in
    skeleton_overlay) run skeleton_overlay 900 bash tests/spells/skeleton_overlay.sh ;;
    hkxmodel)         run hkxmodel_test    600 bash tests/spells/hkxmodel_test.sh ;;
    hkxfile)          run hkxfile_gates    900 python tests/spells/hkxfile_gates.py ;;
    animws)           run animws           900 bash tests/spells/animws.sh ;;
    files_tab)        run files_tab        900 bash tests/spells/files_tab.sh ;;
    hkxanim_ui)       run hkxanim_ui       600 bash tests/spells/hkxanim_ui.sh ;;
    hkxanim_play)     run hkxanim_play     600 bash tests/spells/hkxanim_play.sh ;;
    loaded_nifs)      run loaded_nifs      900 bash tests/spells/loaded_nifs.sh ;;
    top_bar)          run top_bar          600 bash tests/spells/top_bar.sh ;;
    ui_align)         run ui_align         600 bash tests/spells/ui_align.sh ;;
    *) echo "unknown gate: $g" ;;
  esac
done
echo "CHAIN COMPLETE $(date +%H:%M:%S)" | tee -a "$SUM"
