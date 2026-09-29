#!/bin/bash
# MERGE1 copy of INCR2 bake_ws.sh; only change: --no-collapse-uniform on the chunk stage (FO4CS cannot read one-value tiles yet).
# INCR2 whole-worldspace bake into a staging tree (never the mod folder).
# usage: bash bake_ws.sh <exe abs> <stage root abs> <ws> [incremental]
#   ws = cw | fh | nw | pw.  Stage layout: <root>/mod/FO4CSLOD/<EDID> (the tree that gets installed),
#   <root>/scr/<ws> (the out-dir: BTR sheets + the raw chunk cache; keep it for quick rebakes).
#   A 4th arg "incremental" skips the lodl stage and runs the chunk stage with --incremental <root>/mod.
# Standing rulings: no stock .BTO/.BTR in the mod, --vt-height, --cover, crisp 8x8 cards, road detail 1 (default),
# vanilla fill + --land-fill-vanilla, his CURRENT MO2 profile (read only).
set -u
NS="$1"; R="$2"; W="$3"; MODE="${4:-full}"
P="E:/Projects/Fallout 4 Mods/profiles/Default"
C1=E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/cards
C2=E:/Projects/NifskopeWWE-bake2/scratchpad/bake2_20260925/cards
case $W in
	cw) WS=3C;       REG="-96 -96 95 95"; CARDS=$C1 ;;
	fh) WS=03000B0F; REG="-32 -32 20 31"; CARDS=$C2 ;;
	nw) WS=0600290F; REG="-32 -32 32 32"; CARDS=$C2 ;;
	pw) WS=000A7FF4; REG="-28 -12 2 25";  CARDS=$C2 ;;
	*) echo "unknown ws $W"; exit 2 ;;
esac
MOD="$R/mod"; SCR="${SCRROOT:-$R/scr}/$W"; LOGS="$R/logs"   # SCRROOT: put the out-dir on another drive
mkdir -p "$MOD" "$SCR" "$LOGS"
stamp() { date +%H:%M:%S; }
gate() { if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "$(stamp) GAME UP" | tee -a "$LOGS/stages.txt"; exit 1; fi; }
VR=( --vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" )
run() { local name="$1"; shift; gate; local t0=$(date +%s)
	echo "$(stamp) start $name exe $(sha1sum "$NS" | cut -c1-8)" | tee -a "$LOGS/stages.txt"
	"$NS" -no-gui lodgen --mo2-profile "$P" --worldspace "$WS" "$@" > "$LOGS/$name.log" 2>&1
	local rc=$?; echo "$(stamp) end $name rc=$rc $(( $(date +%s) - t0 )) s" | tee -a "$LOGS/stages.txt"; return $rc; }
INC=()
if [ "$MODE" = incremental ]; then INC=( --incremental "$MOD" ); TAG=inc_$(date +%H%M)
else TAG=full; run ${W}_lodl --lodl "$MOD" "${VR[@]}" --land-fill-vanilla || exit 1; fi
# shellcheck disable=SC2086
run ${W}_chunks_$TAG --terrain-region $REG --dim all --out-dir "$SCR" --tex-dir "$SCR/textures" \
	--native "$MOD" --vt "$MOD" --vt-height --vt-density 16 --cover --vt-fill-vanilla "${VR[@]}" --land-fill-vanilla \
	--impostors "$CARDS" --arrays --no-collapse-uniform --fo4cs-one-root "${INC[@]+"${INC[@]}"}" || exit 1
grep -E '^incremental|^raw chunk|^vt: kept|nothing moved|^card sets|^bto built|^timing' "$LOGS/${W}_chunks_$TAG.log" | sed 's/^/    /' | tee -a "$LOGS/stages.txt"
echo "$(stamp) DONE $W $MODE" | tee -a "$LOGS/stages.txt"
