#!/bin/bash
# WATER1 render pass. maps1 Boston camera (view 8, ortho 16384, region -5 -10 2 -3, 1600x1600 read back),
# LODI/sheets/levels exactly as maps1's render_all.sh. Resumable: skips a picture already on disk.
# usage: render_all.sh [group]   groups: gate pics flat v2 whole all
SP=/c/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad
ME=/e/Projects/NifskopeWWE-water1/scratchpad/water1_20260927
P=$ME/pics; mkdir -p $P
T=$SP/ao2/terr_x1
OD=$SP/ao2/reg_x7/mod/FO4CSLOD/Commonwealth
V3NEW=$ME/bk_new_def/FO4CSLOD/Commonwealth/Commonwealth.lodl
V2NEW=$ME/bk_new_off/FO4CSLOD/Commonwealth/Commonwealth.lodl
V3RUNG=$ME/bk_rung_v3/FO4CSLOD/Commonwealth/Commonwealth.lodl
RUNG=$ME/run_rung/NifSkope.exe
NEW=$ME/run_new/NifSkope.exe
INST="/e/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth"   # read-only: sheets for the whole map
port=42900
shot() { # <name> <extra env...>
  local name=$1; shift
  port=$((port+1))
  if [ -s $P/$name.png ]; then echo "SKIP $name"; return; fi
  local r; r=$(env LODI_DIR=$OD LV=2 SLOT=0 SDIM=2 "$@" \
    bash $ME/shot.sh $P/$name.png $T Commonwealth -5 -10 2 -3 8 16384 1600 1600 $port | tail -1)
  echo "$(date +%H:%M:%S) $r"
  # a picture that did not appear = stop the pass (Avast sandbox or a crash); never go on launching
  case "$r" in OK*) ;; *) echo "STOPPED at $name"; exit 2;; esac
}
for f in $V3NEW $V2NEW $V3RUNG $RUNG $NEW; do [ -f $f ] || { echo "missing $f"; exit 1; }; done
G=${1:-all}
if [ $G = gate ] || [ $G = all ]; then
  # identity: rung exe vs new exe with WW_LODL_WATER=0, same camera, same files
  shot G_rung_L01_default NS=$RUNG
  shot G_new0_L01_default NS=$NEW WW_LODL_WATER=0
  shot G_rung_P_waterheight NS=$RUNG NOOBJ=1 WW_LODL_PLANE=waterheight
  shot G_new0_P_waterheight NS=$NEW WW_LODL_WATER=0 NOOBJ=1 WW_LODL_PLANE=waterheight
  shot G_rung_W3_bodyid NS=$RUNG NOOBJ=1 LODL=$V3RUNG WW_LODL_PLANE=bodyid
  shot G_new0_W3_bodyid NS=$NEW WW_LODL_WATER=0 NOOBJ=1 LODL=$V3RUNG WW_LODL_PLANE=bodyid
fi
if [ $G = pics ] || [ $G = all ]; then
  shot A1_default NS=$NEW LODL=$V3NEW
  for pl in waterheight watertype bodyid flow shore cellflags; do
    shot A_$pl NS=$NEW NOOBJ=1 LODL=$V3NEW WW_LODL_PLANE=$pl
  done
  # before, same camera and file: the rung exe painted the water numbers on the ground
  shot B1_default_before NS=$RUNG LODL=$V3NEW
  shot B_waterheight_before NS=$RUNG NOOBJ=1 LODL=$V3NEW WW_LODL_PLANE=waterheight
fi
if [ $G = flat ] || [ $G = all ]; then
  # legend gate: vertex colours only, so an opaque water pixel IS its legend rgb
  shot FL0_default_nowater NS=$NEW NOOBJ=1 LODL=$V3NEW WW_RENDER_FLAT=1 WW_LODL_WATER=0
  shot FL_default NS=$NEW NOOBJ=1 LODL=$V3NEW WW_RENDER_FLAT=1
  for pl in waterheight watertype bodyid flow shore cellflags; do
    shot FL_$pl NS=$NEW NOOBJ=1 LODL=$V3NEW WW_RENDER_FLAT=1 WW_LODL_PLANE=$pl
  done
fi
if [ $G = v2 ] || [ $G = all ]; then
  shot V2_default NS=$NEW NOOBJ=1 LODL=$V2NEW
  shot V2_waterheight NS=$NEW NOOBJ=1 LODL=$V2NEW WW_LODL_PLANE=waterheight
  shot V2_bodyid NS=$NEW NOOBJ=1 LODL=$V2NEW WW_LODL_PLANE=bodyid
fi
if [ $G = whole ] || [ $G = all ]; then
  # LIT1's whole-Commonwealth oblique: view 8, ortho 570000, look-at 0,0,0, 3200x1528; terrain + water only
  port=$((port+1))
  if [ ! -s $P/WH_default.png ]; then
    env NS=$NEW NOOBJ=1 LV=3 SDIM=16 SHEETS="$INST" LODL=$V3NEW \
      bash $ME/shot.sh $P/WH_default.png $T Commonwealth -96 -96 95 95 8 570000 3200 1528 $port | tail -1
    [ -s $P/WH_default.png ] || { echo "STOPPED at WH_default"; exit 2; }
  fi
fi
echo "RENDER PASS DONE $G"
