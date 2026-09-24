#!/bin/bash
# BLENDSEAM1 measurement bakes: the V9a region (-24 24 -17 31, four dim-4 chunks),
# cover OFF (the V9a-1 pair), the pyramid writer (--vt) and the stock writer (no --vt).
#   usage: bake4.sh <outroot> <exe> <variant...>   variants: V S Voff Soff
set -u
OUT="$1"; EXE="$2"; shift 2
OUTW="$(cd "$(dirname "$OUT")" && pwd -W)/$(basename "$OUT")"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
for v in "$@"; do
	rm -rf "$OUT/$v"; mkdir -p "$OUT/$v/obj" "$OUT/$v/tex" "$OUT/$v/mod"
	case $v in
		V)    extra=(--vt "$OUTW/$v/mod") ;;
		S)    extra=() ;;
		Voff) extra=(--vt "$OUTW/$v/mod" --blend-edges off) ;;
		Soff) extra=(--blend-edges off) ;;
	esac
	S0=$(date +%s)
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region -24 24 -17 31 --dim 4 \
		--out-dir "$OUTW/$v/obj" --data-root "$DATA" \
		--tex-dir "$OUTW/$v/tex" "${extra[@]}" > "$OUT/$v.log" 2>&1
	echo "BAKE $v rc=$? $(( $(date +%s) - S0 ))s exe=$(basename "$EXE")"
done
