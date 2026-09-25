#!/bin/bash
# BAKE1: the FO4CS bake as the LOD panel runs it, on the command line (the panel cannot turn ground cover on under
# the FO4CS target without also writing the stock .BTR/sheet set into the mod folder, which R2 forbids).
# Order = the panel's run order: the .lodl (lodt row, default ON), the shadow heightmap (default ON), then the
# terrain virtual texture + the chunk queue with the native pair, cards and arrays.
# usage: bake.sh <mod folder> <stock scratch dir> <cards dir> <x0> <y0> <x1> <y1> <log dir>
set -u
MOD="$1"; SCR="$2"; CARDS="$3"; X0="$4"; Y0="$5"; X1="$6"; Y1="$7"; LOGS="$8"
L=E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925
NS="$L/run/release/NifSkope.exe"
P="E:/Projects/Fallout 4 Mods/profiles/Default"
mkdir -p "$LOGS" "$SCR"
stamp() { date +%H:%M:%S; }
run() {  # name, args...
	local name="$1"; shift
	local t0=$(date +%s)
	echo "$(stamp) start $name" | tee -a "$LOGS/stages.txt"
	"$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C "$@" > "$LOGS/$name.log" 2>&1
	local rc=$?
	echo "$(stamp) end $name rc=$rc $(( $(date +%s) - t0 )) s" | tee -a "$LOGS/stages.txt"
	return $rc
}
case ",${STAGES:-lodl,chunks}," in *,lodl,*) run lodl --lodl "$MOD" || exit 1 ;; esac
# The shadow heightmap (panel row default ON) is NOT baked: his load order moves land heights, so its corpus
# hash (cd9657c5) is not the one the FO4CS loader pins (d8337d02) -- "the map would be refused" -- and its file
# name is the FO4CS mod's own, so a FO4CSLOD copy would shadow the working vanilla map. STAGES=lodl,chunks.
case ",${STAGES:-lodl,chunks}," in *,heightmap,*) run heightmap --heightmap "$MOD" || exit 1 ;; esac
case ",${STAGES:-lodl,chunks}," in *,chunks,*) ;; *) exit 0 ;; esac
run chunks --terrain-region "$X0" "$Y0" "$X1" "$Y1" --dim 4 \
	--out-dir "$SCR" --tex-dir "$SCR/textures" \
	--native "$MOD" --vt "$MOD" --vt-height --vt-density 16 --cover \
	--impostors "$CARDS" --arrays || exit 1
echo "$(stamp) ALL DONE" | tee -a "$LOGS/stages.txt"
