#!/bin/bash
# MERGE2 copy of IDENT2's Boston-box bake (the GPU1/INCR2 recipe), turn name MERGE2.
# usage: bake.sh <run dir holding NifSkope.exe> <out root> [extra chunk-stage args]
#   LIGHT=1  chunk stage with the native pair only (IDENT2's measuring bake).
# Box: the night rules' Boston box -8 -12 3 -1.
set -u
RUN="$1"; R="$2"; shift 2
NS="$RUN/NifSkope.exe"
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
P="E:/Projects/Fallout 4 Mods/profiles/Default"
CARDS=E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/cards
VR=( --vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" )
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
mkdir -p "$R/mod" "$R/scr"
R=$(cd "$R" && pwd)
bash $TURN acquire MERGE2 || exit 1
trap 'bash $TURN release MERGE2' EXIT
t0=$(date +%s)
if [ ! -f "$R/mod/.lodl_done" ]; then
	"$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --lodl "$R/mod" "${VR[@]}" --land-fill-vanilla > "$R/lodl.log" 2>&1
	rc=$?; echo "lodl rc=$rc $(( $(date +%s) - t0 )) s"; [ $rc -eq 0 ] && touch "$R/mod/.lodl_done"
fi
t1=$(date +%s)
if [ -n "${LIGHT:-}" ]; then
	"$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --terrain-region -8 -12 3 -1 --dim all \
		--out-dir "$R/scr" --native "$R/mod" "${VR[@]}" --land-fill-vanilla --fo4cs-one-root "$@" > "$R/bake.log" 2>&1
else
	"$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --terrain-region -8 -12 3 -1 --dim all \
		--out-dir "$R/scr" --tex-dir "$R/scr/textures" --native "$R/mod" --vt "$R/mod" --vt-height --vt-density 16 --cover \
		--vt-fill-vanilla "${VR[@]}" --land-fill-vanilla --impostors "$CARDS" --arrays --fo4cs-one-root "$@" > "$R/bake.log" 2>&1
fi
echo "chunks rc=$? $(( $(date +%s) - t1 )) s"
