#!/bin/bash
# GPU1 profiled bake: the INCR2 whole-map switches (bake_ws.sh: lodl stage, then the chunk stage with the native pair,
# the terrain VT, cover, vanilla fill, cards, arrays) on a region, with wwprof sampling the chunk stage.
# usage: prof_bake.sh <run dir holding NifSkope.exe> <out root> <x0> <y0> <x1> <y1> [extra chunk-stage args]
# Every log line carries its epoch second. Takes and releases the machine-wide NifSkope turn.
set -u
RUN="$1"; R="$2"; X0=$3; Y0=$4; X1=$5; Y1=$6; shift 6
NS="$RUN/NifSkope.exe"
G=/e/Projects/NifskopeWWE-gpu1/scratchpad/gpu1_20260926
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
P="E:/Projects/Fallout 4 Mods/profiles/Default"
CARDS=E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/cards
VR=( --vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" )
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
mkdir -p "$R/mod" "$R/scr"
# NifSkope resolves relative output paths against its own folder, not this shell's: make R absolute.
R=$(cd "$R" && pwd)
bash $TURN acquire gpu1 || exit 1
trap 'bash $TURN release gpu1' EXIT
stamp() { while IFS= read -r l; do printf '%(%s)T %s\n' -1 "$l"; done; }
t0=$(date +%s)
"$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --lodl "$R/mod" "${VR[@]}" --land-fill-vanilla 2>&1 | stamp > "$R/lodl.log"
echo "lodl rc=${PIPESTATUS[0]} $(( $(date +%s) - t0 )) s"
t1=$(date +%s)
( "$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --terrain-region $X0 $Y0 $X1 $Y1 --dim all \
	--out-dir "$R/scr" --tex-dir "$R/scr/textures" --native "$R/mod" --vt "$R/mod" --vt-height --vt-density 16 --cover \
	--vt-fill-vanilla "${VR[@]}" --land-fill-vanilla --impostors "$CARDS" --arrays --fo4cs-one-root "$@" 2>&1 | stamp > "$R/bake.log"; echo "chunks rc=${PIPESTATUS[0]}" > "$R/rc.txt" ) &
if [ -n "${PROF:-}" ]; then
	RUNW=$(cd "$RUN" && pwd -W)/NifSkope.exe
	# The first pid seen can be a short-lived launcher (GPU1 run 1: OpenProcess error 87 on it), so re-query
	# until wwprof attaches. wwprof runs from the scratch copy (Avast holds fresh worktree exes).
	WWPROF=/c/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad/gpu1/wwprof.exe
	for i in $(seq 1 60); do
		[ -f "$R/rc.txt" ] && break
		pid=$(powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.ExecutablePath -eq '$(echo $RUNW | sed 's#/#\\#g')' } | Sort-Object CreationDate | ForEach-Object { \$_.ProcessId }" | tr -d '\r' | tail -1)
		[ -z "$pid" ] && { sleep 1; continue; }
		echo "profiling pid $pid (chunk epoch $t1, attach epoch $(date +%s))" | tee "$R/prof_pid.txt"
		"$WWPROF" "$pid" "$R/prof.txt" "${PROF_MS:-200}" 48 && break
		sleep 1
	done
fi
wait
cat "$R/rc.txt"; echo "chunk stage $(( $(date +%s) - t1 )) s"
