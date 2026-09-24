#!/bin/sh
# IMPOSTORSHRUB1 census bake: every distinct model in bases.tsv, photographed by
# the WW_IMPOSTOR_BAKE hook, strictly serial, one harness NifSkope at a time,
# second monitor, absolute paths. bungo's own NifSkope (no --port) may stay open.
#
#   sh shrub_bake.sh OUTROOT [model-substring...]
#   env NS= (exe; default release/NifSkope.exe), OCT= (8), TILE= (512),
#       REF= (1326.5 = the largest extent of the Commonwealth's tree candidates,
#       TreeElmFree01, cands_trees.txt: the size ladder a real run gives them)
set -u
REPO="E:/Projects/NifskopeWildWastelandEdition"
S="$REPO/scratchpad/impostorshrub1_20260923"
NS="${NS:-$REPO/release/NifSkope.exe}"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
OCT=${OCT:-8}; TILE=${TILE:-512}; REF=${REF:-1326.5}
P=${PORTBASE:-29600}
R="$1"; shift
mkdir -p "$R"

tasklist | grep -i -q "Fallout4" && { echo "REFUSED: Fallout4.exe is up"; exit 1; }
n=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process -Filter \"Name like 'NifSkope%'\" | Where-Object { \$_.CommandLine -match '--port' }).Count" | tr -d '\r')
[ "$n" = "0" ] || { echo "REFUSED: $n harness NifSkope(s) with --port already running"; exit 1; }

tail -n +2 "$S/bases.tsv" | cut -f4 | tr 'A-Z' 'a-z' | sort -u | tr -d '\r' | while read -r model; do
	rel="$(echo "$model" | tr '\\' '/')"
	mesh="$DATA/meshes/$rel"
	b="$(basename "$rel" .nif)"
	if [ $# -gt 0 ]; then
		hit=0; for f in "$@"; do case "$b" in *"$f"*) hit=1 ;; esac; done
		[ $hit = 1 ] || continue
	fi
	if [ ! -f "$mesh" ]; then echo "$b MISSING $rel"; continue; fi
	O="$R/$b"; rm -rf "$O"; mkdir -p "$O"
	P=$((P+1))
	t0=$(date +%s%N)
	WW_IMPOSTOR_BAKE="$O" WW_IMPOSTOR_OCT="$OCT" WW_IMPOSTOR_TILE="$TILE" WW_IMPOSTOR_REF="$REF" \
	WW_WINDOW_AT=1960,40 \
		timeout 900 "$NS" "$mesh" --port "$P" > "$O/bake.stdout" 2>&1
	rc=$?
	ms=$(( ( $(date +%s%N) - t0 ) / 1000000 ))
	echo "$b rc=$rc ${ms}ms $(grep '^oct ' "$O/$b.txt" 2>/dev/null | cut -d' ' -f1-6)"
done
echo ALLDONE
