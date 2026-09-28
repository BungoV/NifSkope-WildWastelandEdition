#!/bin/bash
# WATER1 picture: a copy of the session's maps1/shot.sh. Differences, and only these:
#   NS defaults to WATER1's run copy (run_new); turn.sh name + settings scope = water1; sheet cache under water1/cache.
# usage: shot.sh <out.png> <lod dir> <EDID> <x0> <y0> <x1> <y1> <view 1|8> <ortho half-width> <W> <H> <port>
#   env: LV LI SLOT SDIM OBJ_REGION NOOBJ LODI_DIR SHEETS LREG LODL (as maps1)
OUT="$1"; F="$2"; E="$3"; X0=$4; Y0=$5; X1=$6; Y1=$7; VIEW=$8; ORT=$9; W=${10}; H=${11}; PORT=${12}
OUT="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"
LV=${LV:-3};LI=${LI:-0}; SLOT=${SLOT:-none}; SDIM=${SDIM:-16}
ME=/e/Projects/NifskopeWWE-water1/scratchpad/water1_20260927
NS=${NS:-$ME/run_new/NifSkope.exe}
LODL=${LODL:-$F/$E.lodl}
wp() { echo "$1" | sed -E 's#^/([a-zA-Z])/#\U\1:/#'; }
RES=$("$NS" -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --print-source 2>&1 | tr -d '\r' | sed -n 's/^resource [0-9]*: //p' | paste -sd ';')
CX=$(( (X0 + X1 + 1) * 2048 )); CY=$(( (Y0 + Y1 + 1) * 2048 ))
mkdir -p "$ME/cache"
CACHE="E:/Projects/NifskopeWWE-water1/scratchpad/water1_20260927/cache/sc_$(basename "$OUT" .png)_$(date +%s)"
OBJ=( WW_LODL_OBJECTS="$(wp "${LODI_DIR:-$F}")/$E.lodi" WW_LODI_REGION="${OBJ_REGION:-$X0,$Y0,$X1,$Y1}" WW_LODI_LEVEL=$LI )
[ "$SLOT" != none ] && OBJ+=( WW_LODI_SLOT=$SLOT )
[ -n "${NOOBJ:-}" ] && OBJ=( WW_BAKE2_NOOBJ=1 )
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh acquire WATER1 21600 || exit 1
# the wait for the turn can be long (a game flight holds it): check the game again once the turn is ours
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then
  bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh release WATER1; echo "GAME UP"; exit 1; fi
SCOPE=water1; REGKEY="HKCU\Software\NifTools\NifSkope 2.0 $SCOPE"
wipe() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
wipe; trap wipe EXIT
rm -f "$OUT" "${OUT%.png}.cam.log"
reg add "$REGKEY\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1
reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1
GM="$(dirname "$OUT")/gm_$$.reg"
python E:/Projects/NifskopeWWE-fix1/tests/spells/settings_scope_game.py "$SCOPE" "$(wp "$GM")" > /dev/null && reg import "$(wp "$GM")" > /dev/null 2>&1; rm -f "$GM"
env "${OBJ[@]}" WW_SETTINGS_SCOPE="$SCOPE" \
    WW_LODL_SHEETS="$(wp "${SHEETS:-$F}")" WW_LODL_SHEET_DIM=$SDIM WW_LODL_SHEET_CACHE="$CACHE" \
    WW_LODL_REGION="${LREG:-$X0,$Y0,$X1,$Y1},$LV" \
    WW_LODGEN_RESOURCES="$RES" \
    WW_RENDER_SHOT="$(wp "$OUT")" WW_RENDER_SIZE="${W}x$((H + 59))" \
    WW_RENDER_CENTER="$CX,$CY,${CZ:-0}" WW_RENDER_ORTHO="$ORT" WW_RENDER_VIEW="$VIEW" WW_RENDER_CLEAN=1 \
    WW_CAMERA_CENSUS="$(wp "${OUT%.png}.cam.log")" \
    WW_WINDOW_AT=1960,40 \
    timeout 1200 "$NS" --port "$PORT" "$(wp "$LODL")" > "${OUT%.png}.log" 2>&1
rc=$?
# Avast sandbox (2026-09-28 21:09): the launch returns rc 0 at once, but the sandboxed copy can go on
# running detached (pathless in Win32_Process) and write the picture minutes later with no log. Hold the
# turn while any NifSkope on OUR --port is alive (max 20 min); a picture made that way is flagged LATE.
late=""
if [ ! -s "$OUT" ]; then
  for i in $(seq 1 80); do
    n=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | ? { \$_.CommandLine -like '*--port $PORT*' }).Count" 2>/dev/null | tr -d '\r')
    [ "${n:-0}" = 0 ] && break; sleep 15
  done
  [ -s "$OUT" ] && late=" LATE(sandboxed, no log)"
fi
bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh release WATER1
if [ -s "$OUT" ]; then
  echo "OK $(basename "$OUT") $(stat -c %s "$OUT") B rc=$rc$late"
else
  echo "NO FILE $OUT rc=$rc"
fi
