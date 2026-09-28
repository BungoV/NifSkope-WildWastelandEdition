#!/bin/bash
# TILING5 picture: a copy of the session's maps1/shot.sh (original untouched). Differences, and only these:
#   turn.sh name = tiling5; settings scope = tiling5; sheet cache under this lane folder; terrain only (no objects);
#   NS defaults to this lane's run_rung copy (the rendering path is not touched by the lane).
# usage: t5_shot.sh <out.png> <sheet dir> <x0> <y0> <x1> <y1> <ortho half-width> <W> <H> <port>
#   env: LODL (landscape file, default the maps1 subject's Commonwealth.lodl)  LV (default 2)  SDIM (default 2)
#        any WW_* (e.g. WW_RENDER_FLAT=1) passes through to the exe.
OUT="$1"; SH="$2"; X0=$3; Y0=$4; X1=$5; Y1=$6; ORT=$7; W=$8; H=$9; PORT=${10}
OUT="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"
LV=${LV:-2}; SDIM=${SDIM:-2}
HERE=/e/Projects/NifskopeWWE-tiling5/scratchpad/tiling5_20260927
SP=/c/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad
NS=${NS:-$HERE/run_rung/NifSkope.exe}
LODL=${LODL:-$SP/ao2/terr_x1/Commonwealth.lodl}
wp() { echo "$1" | sed -E 's#^/([a-zA-Z])/#\U\1:/#'; }
CX=$(( (X0 + X1 + 1) * 2048 )); CY=$(( (Y0 + Y1 + 1) * 2048 ))
mkdir -p "$HERE/cache"
CACHE="$(wp "$HERE")/cache/sc_$(basename "$OUT" .png)_$(date +%s)"
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh acquire TILING5 21600 || exit 1
# the resource probe launches NifSkope too, so it runs inside the turn (moved here 2026-09-28)
RES=$("$NS" -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --print-source 2>&1 | tr -d '\r' | sed -n 's/^resource [0-9]*: //p' | paste -sd ';')
SCOPE=tiling5; REGKEY="HKCU\Software\NifTools\NifSkope 2.0 $SCOPE"
wipe() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
wipe; trap wipe EXIT
reg add "$REGKEY\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1
reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1
GM="$(dirname "$OUT")/gm_$$.reg"
python E:/Projects/NifskopeWWE-fix1/tests/spells/settings_scope_game.py "$SCOPE" "$(wp "$GM")" > /dev/null && reg import "$(wp "$GM")" > /dev/null 2>&1; rm -f "$GM"
DBG="${OUT%.png}.dbg"
# measured 21:44: the probe's NifSkope is still alive when $(...) returns (its pipe closed first); a second
# NifSkope launched beside it writes nothing and returns 0. Wait until ours is gone (never kill).
for i in $(seq 1 40); do
	left=$(powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | ForEach-Object { '{0}|{1}' -f \$_.ProcessId,\$_.ExecutablePath }" | tr -d '\r' | grep -i 'NifskopeWWE-tiling5')
	[ -z "$left" ] && break; echo "probe-linger $(date +%H:%M:%S) $left" >> "$DBG"; sleep 3
done
{ echo "pre $(date +%H:%M:%S.%N) port=$PORT resn=$(echo "$RES" | tr ';' '\n' | wc -l)";
  powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | ForEach-Object { '{0}|{1}' -f \$_.ProcessId,\$_.ExecutablePath }";
  netstat -ano -p UDP | grep ":$PORT " ; } >> "$DBG" 2>&1
env WW_BAKE2_NOOBJ=1 WW_SETTINGS_SCOPE="$SCOPE" \
    WW_LODL_SHEETS="$(wp "$SH")" WW_LODL_SHEET_DIM=$SDIM WW_LODL_SHEET_CACHE="$CACHE" \
    WW_LODL_REGION="$X0,$Y0,$X1,$Y1,$LV" \
    WW_LODGEN_RESOURCES="$RES" \
    WW_RENDER_SHOT="$(wp "$OUT")" WW_RENDER_SIZE="${W}x$((H + 59))" \
    WW_RENDER_CENTER="$CX,$CY,0" WW_RENDER_ORTHO="$ORT" WW_RENDER_VIEW=8 WW_RENDER_CLEAN=1 \
    WW_CAMERA_CENSUS="$(wp "${OUT%.png}.cam.log")" \
    WW_WINDOW_AT=1960,40 \
    timeout 1200 "$NS" --port "$PORT" "$(wp "$LODL")" > "${OUT%.png}.log" 2>&1 &
bpid=$!; echo "launched bash-pid $bpid winpid $(cat /proc/$bpid/winpid 2>/dev/null)" >> "$DBG"
wait $bpid; rc=$?; echo "post $(date +%H:%M:%S.%N) rc=$rc" >> "$DBG"
# a NifSkope of ours still alive after the wait = the wait did not cover it; wait for it (never kill)
for i in $(seq 1 40); do
	left=$(powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | ForEach-Object { '{0}|{1}' -f \$_.ProcessId,\$_.ExecutablePath }" | tr -d '\r' | grep -i 'NifskopeWWE-tiling5')
	[ -z "$left" ] && break; echo "linger $(date +%H:%M:%S) $left" >> "$DBG"; sleep 3
done
bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh release TILING5
if [ -s "$OUT" ]; then
  echo "OK $(basename "$OUT") $(stat -c %s "$OUT") B rc=$rc"
else
  echo "NO FILE $OUT rc=$rc"
fi
