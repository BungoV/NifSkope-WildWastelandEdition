#!/bin/bash
# WATER1 render pass. maps1 Boston camera (view 8, ortho 16384, region -5 -10 2 -3, 1600x1600 read back),
# LODI/sheets/levels exactly as maps1's render_all.sh. Resumable: skips a picture already on disk.
# usage: render_all.sh [group]   groups: gate pics flat v2 whole all
SP=/c/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad
ME=/e/Projects/NifskopeWWE-water1/scratchpad/water1_20260927
# one WATER1 pass at a time (2026-09-28: two chain4 copies ran at once): a second pass exits BUSY
if [ -z "${WATER1_PASS_HELD:-}" ]; then
  mkdir /e/Projects/NifskopeWWE-water1/scratchpad/water1_20260927/.water1_pass 2>/dev/null || { echo "BUSY: another WATER1 pass runs"; exit 3; }
  export WATER1_PASS_HELD=1; trap 'rmdir /e/Projects/NifskopeWWE-water1/scratchpad/water1_20260927/.water1_pass' EXIT
fi
P=$ME/pics; mkdir -p $P
T=$SP/ao2/terr_x1
OD=$SP/ao2/reg_x7/mod/FO4CSLOD/Commonwealth
V3NEW=$ME/bk_new_def/FO4CSLOD/Commonwealth/Commonwealth.lodl
V2NEW=$ME/bk_new_off/FO4CSLOD/Commonwealth/Commonwealth.lodl
V3RUNG=$ME/bk_rung_v3/FO4CSLOD/Commonwealth/Commonwealth.lodl
RUNG=$ME/run_rung/NifSkope.exe
NEW=$ME/run_new/NifSkope.exe
INST="/e/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth"   # read-only: sheets for the whole map
port=${PORTBASE:-42900}   # pass PORTBASE= a base checked unused (netstat) for each pass
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
G=${1:-all}
if [ $G = depth ]; then
  # follow-up 1: the depth-view exe ($DEPTHNS) -- the FL views again into pics_depth/ for the pixel
  # identity check against pics/FL_*, then the depth view flat (legend gate) and full (the picture)
  DNS=${DEPTHNS:?set DEPTHNS}
  [ -f $V3NEW ] && [ -f $DNS ] || { echo "missing $V3NEW or $DNS"; exit 1; }
  P=$ME/pics_depth; mkdir -p $P
  shot FL0_default_nowater NS=$DNS NOOBJ=1 LODL=$V3NEW WW_RENDER_FLAT=1 WW_LODL_WATER=0
  shot FL_default NS=$DNS NOOBJ=1 LODL=$V3NEW WW_RENDER_FLAT=1
  for pl in waterheight watertype bodyid flow shore cellflags depth; do
    shot FL_$pl NS=$DNS NOOBJ=1 LODL=$V3NEW WW_RENDER_FLAT=1 WW_LODL_PLANE=$pl
  done
  shot A_depth NS=$DNS NOOBJ=1 LODL=$V3NEW WW_LODL_PLANE=depth
  echo "RENDER PASS DONE $G"; exit 0
fi
if [ $G = slope ]; then
  # task 3 (sloped water): the sloped-water exe on the sloped-water writer's vanilla bake, the same
  # camera and names as pics/ (flat build, flat writer) and pics_depth/FL_depth, for pixel identity
  SNS=${SLOPENS:-$ME/run_slope/NifSkope.exe}; V3S=$ME/bk_slope_def/FO4CSLOD/Commonwealth/Commonwealth.lodl
  [ -f $V3S ] && [ -f $SNS ] || { echo "missing $V3S or $SNS"; exit 1; }
  P=$ME/pics_slope; mkdir -p $P
  shot FL_default NS=$SNS NOOBJ=1 LODL=$V3S WW_RENDER_FLAT=1
  for pl in waterheight watertype bodyid depth; do
    shot FL_$pl NS=$SNS NOOBJ=1 LODL=$V3S WW_RENDER_FLAT=1 WW_LODL_PLANE=$pl
  done
  shot A_waterheight NS=$SNS NOOBJ=1 LODL=$V3S WW_LODL_PLANE=waterheight
  # A1_default (objects on) dropped 20:10: its object input ($OD, ao2/reg_x7) no longer exists on disk
  port=$((port+1))
  if [ ! -s $P/WH_default.png ]; then
    env NS=$SNS NOOBJ=1 LV=3 SDIM=16 SHEETS="$INST" LODL=$V3S \
      bash $ME/shot.sh $P/WH_default.png $T Commonwealth -96 -96 95 95 8 570000 3200 1528 $port | tail -1
    [ -s $P/WH_default.png ] || { echo "STOPPED at WH_default"; exit 2; }
  fi
  echo "RENDER PASS DONE $G"; exit 0
fi
if [ $G = real ] || [ $G = pin ]; then
  # task 3 on the real bake: the sloped placed water the Commonwealth has (real_cmp.py: cells 1,-4 and
  # -12..-11,27..28). BEFORE = what he had: the flat viewer (run_new) on the flat writer's file; AFTER =
  # the slope viewer on the slope writer's file. Same camera per pair, terrain + water only.
  SNS=${SLOPENS:-$ME/run_slope/NifSkope.exe}; V3S=$ME/bk_slope_def/FO4CSLOD/Commonwealth/Commonwealth.lodl
  [ -f $V3S ] && [ -f $SNS ] && [ -f $V3NEW ] && [ -f $NEW ] || { echo "missing inputs"; exit 1; }
  P=$ME/pics_real; mkdir -p $P
  cshot() { # <name> <x0 y0 x1 y1 ortho> <env...>
    local name=$1 x0=$2 y0=$3 x1=$4 y1=$5 ort=$6; shift 6; port=$((port+1))
    if [ -s $P/$name.png ]; then echo "SKIP $name"; return; fi
    local r; r=$(env LV=0 SLOT=0 SDIM=2 NOOBJ=1 "$@" \
      bash $ME/shot.sh $P/$name.png $T Commonwealth $x0 $y0 $x1 $y1 8 $ort 1600 1600 $port | tail -1)
    echo "$(date +%H:%M:%S) $r"
    case "$r" in OK*) ;; *) echo "STOPPED at $name"; exit 2;; esac
  }
  if [ $G = pin ]; then
    # the pond water-height pair with the ramp PINNED (WW_LODL_HEIGHT_RANGE, run_pin exe) so the sea keeps
    # its colour; plus the pin exe with no pin on the slope file = must equal E_waterheight_after (switch off = same)
    PNS=$ME/run_pin/NifSkope.exe; [ -f $PNS ] || { echo "missing $PNS"; exit 1; }
    cshot E_waterheight_pinexe_off 0 -5 2 -3 6144 NS=$PNS LODL=$V3S WW_LODL_PLANE=waterheight
    cshot E_waterheight_pinned_after 0 -5 2 -3 6144 NS=$PNS LODL=$V3S WW_LODL_PLANE=waterheight WW_LODL_HEIGHT_RANGE=${HR:-450,821.5}
    cshot E_waterheight_pinned_before 0 -5 2 -3 6144 NS=$PNS LODL=$V3NEW WW_LODL_PLANE=waterheight WW_LODL_HEIGHT_RANGE=${HR:-450,821.5}
    echo "RENDER PASS DONE $G"; exit 0
  fi
  for v in default waterheight; do
    pe=(); [ $v != default ] && pe=(WW_LODL_PLANE=$v)
    cshot E_${v}_after 0 -5 2 -3 6144 NS=$SNS LODL=$V3S "${pe[@]}"
    cshot E_${v}_before 0 -5 2 -3 6144 NS=$NEW LODL=$V3NEW "${pe[@]}"
    # the hills stand at ~7,000 units: centre the camera at that height (CZ), 2 cells across
    cshot N_${v}_after -13 26 -10 29 4096 CZ=7000 NS=$SNS LODL=$V3S "${pe[@]}"
    cshot N_${v}_before -13 26 -10 29 4096 CZ=7000 NS=$NEW LODL=$V3NEW "${pe[@]}"
  done
  echo "RENDER PASS DONE $G"; exit 0
fi
if [ $G = river ]; then
  # task 3: the synthetic sloped river (--water-slope-selftest's file) over its own 8x8 cells, view 8;
  # the flat-only exe (run_nodepth) on the SAME file is the before: it draws the river at its body height
  FX=$ME/fixture; [ -f $FX/slope.lodl ] || { echo "missing $FX/slope.lodl"; exit 1; }
  P=$ME/pics_river; mkdir -p $P
  rshot() { # <name> <env...>
    local name=$1; shift; port=$((port+1))
    if [ -s $P/$name.png ]; then echo "SKIP $name"; return; fi
    local r; r=$(env NOOBJ=1 LV=0 LODL=$FX/slope.lodl "$@" \
      bash $ME/shot.sh $P/$name.png $FX WaterSlopeFixture 0 0 7 7 8 20480 1600 1600 $port | tail -1)
    echo "$(date +%H:%M:%S) $r"
    case "$r" in OK*) ;; *) echo "STOPPED at $name"; exit 2;; esac
  }
  rshot R_default NS=$ME/run_slope/NifSkope.exe
  rshot R_waterheight NS=$ME/run_slope/NifSkope.exe WW_LODL_PLANE=waterheight
  rshot R_default_flatbuild NS=$ME/run_nodepth/NifSkope.exe
  echo "RENDER PASS DONE $G"; exit 0
fi
for f in $V3NEW $V2NEW $V3RUNG $RUNG $NEW; do [ -f $f ] || { echo "missing $f"; exit 1; }; done
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
