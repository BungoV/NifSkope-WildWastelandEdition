#!/bin/bash
# Lane LODIV7 -- what each grouping knob does to chunk 4.4.-12, measured by
# BAKING with the shipped code rather than by re-implementing the rule.
# Every path absolute. Writes one row a setting to knobtable.txt.
set -u
R=E:/Projects/NifskopeWildWastelandEdition
S=$R/scratchpad/lodiv7_20260918
OUT=$S/knobtable.txt
: > "$OUT"

run () {   # run <label> <component> <tolerance> <shape> <grid>
  local label="$1"
  if tasklist | grep -qi -E "Fallout4|NifSkope"; then echo "GAME OR NIFSKOPE UP -- stop"; exit 90; fi
  rm -rf "$S/knob"
  WW_LODI_GROUP_COMPONENT="$2" WW_LODI_GROUP_TOLERANCE="$3" \
  WW_LODI_GROUP_SHAPE="$4" WW_LODI_GROUP_GRID="$5" \
    bash "$S/bake.sh" "$R/release/NifSkope.exe" "$S/knob" > /dev/null 2>&1
  local rc=$?
  local line
  line=$(grep -o "groups [0-9]* over [^;]*" "$S/knob/bake.log" | tail -1)
  local sha
  sha=$(sha1sum "$S/knob/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi" 2>/dev/null | cut -c1-12)
  printf '%-34s %s  [lodi %s]\n' "$label" "${line:-BAKE FAILED rc=$rc}" "${sha:-none}" | tee -a "$OUT"
}

run "SHIPPED box/16u/grid1024"      architecture 16   box    1024
run "tolerance 0 u"                 architecture 0    box    1024
run "tolerance 4 u"                 architecture 4    box    1024
run "tolerance 64 u"                architecture 64   box    1024
run "tolerance 256 u"               architecture 256  box    1024
run "bound SPHERE not box"          architecture 16   sphere 1024
run "grid 256 u (must not change)"  architecture 16   box    256
run "grid 4096 u (must not change)" architecture 16   box    4096
run "component=buildings"           buildings    16   box    1024
rm -rf "$S/knob"
echo "--- table at $OUT ---"
