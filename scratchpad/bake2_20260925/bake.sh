#!/bin/bash
# BAKE2: BAKE1's bake.sh (lodl stage, then one chunks stage) with SEAM1's VT fill, for one worldspace.
# Flags copied from E:\Projects\NifskopeWWE-bake1\scratchpad\bake1_20260925\bake.sh and
# E:\Projects\NifskopeWWE-seam1\scratchpad\seam1_20260925\whole_vt.sh exactly; only the worldspace, the region
# (= that worldspace's own full LAND extent, measured with --dump-land) and the fill switch differ.
# FILL=on adds --vt-fill-vanilla --vanilla-lod-root (only where vanilla's dim-4 LOD is LOOSE in DataUnpacked).
# No heightmap stage (BAKE1: refused on a modded land corpus, and it would shadow the FO4CS mod's own map).
# usage: FILL=on|off bake.sh <ws formid> <x0> <y0> <x1> <y1> <name>
set -u
WS="$1"; X0="$2"; Y0="$3"; X1="$4"; Y1="$5"; NAME="$6"
L=E:/Projects/NifskopeWWE-bake2/scratchpad/bake2_20260925
NS="$L/run/release/NifSkope.exe"
P="E:/Projects/Fallout 4 Mods/profiles/Default"
MOD="$L/$NAME/mod"; SCR="$L/$NAME/scratch"; CARDS="$L/cards"; LOGS="$L/$NAME/logs"
mkdir -p "$LOGS" "$SCR" "$MOD"
stamp() { date +%H:%M:%S; }
gate() { if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "$(stamp) GAME UP" | tee -a "$LOGS/stages.txt"; exit 1; fi; }
run() {  # name, args...
	local name="$1"; shift
	gate
	local t0=$(date +%s)
	echo "$(stamp) start $name ws $WS region $X0 $Y0 $X1 $Y1 fill ${FILL:-?}" | tee -a "$LOGS/stages.txt"
	"$NS" -no-gui lodgen --mo2-profile "$P" --worldspace "$WS" "$@" > "$LOGS/$name.log" 2>&1
	local rc=$?
	echo "$(stamp) end $name rc=$rc $(( $(date +%s) - t0 )) s" | tee -a "$LOGS/stages.txt"
	return $rc
}
FILLARGS=()
case "${FILL:-}" in
	on) FILLARGS=( --vt-fill-vanilla --vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" ) ;;
	off) ;;
	*) echo "FILL must be on or off"; exit 2 ;;
esac
case ",${STAGES:-lodl,chunks}," in *,lodl,*) run lodl --lodl "$MOD" || exit 1 ;; esac
case ",${STAGES:-lodl,chunks}," in *,chunks,*) ;; *) exit 0 ;; esac
run chunks --terrain-region "$X0" "$Y0" "$X1" "$Y1" --dim "${DIM:-all}" \
	--out-dir "$SCR" --tex-dir "$SCR/textures" \
	--native "$MOD" --vt "$MOD" --vt-height --vt-density 16 --cover "${FILLARGS[@]+"${FILLARGS[@]}"}" \
	--impostors "$CARDS" --arrays --fo4cs-one-root || exit 1
echo "$(stamp) ALL DONE" | tee -a "$LOGS/stages.txt"
