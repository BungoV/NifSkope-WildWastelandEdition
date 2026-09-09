#!/bin/bash
# VTFIX: the four bakes that separate the cover-gate coupling from dominantBase.
#
#   vt_cover   --vt + --cover   (the assembled chunk sheet, the harness's run1)
#   dir_cover  --cover only     (the direct chunk bake, the harness's novt)
#   vt_nc      --vt, NO cover
#   dir_nc     no vt, NO cover
#
# dominantBase reaches the colour composite unconditionally; the normal reaches
# it ONLY through the ground-cover slope gate and the tint it weights. So a
# difference that survives --no-cover is a dominantBase (or sampling) difference
# and one that does not is the cover gate.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
NS="${EXE:-$ROOT/scratchpad/vtfix_20260909/ns_run/NifSkope.exe}"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
W="${OUT:-$ROOT/scratchpad/vtfix_20260909/bake}"
X0=-24; Y0=24; X1=-17; Y1=31

run() {  # run <name> <extra...>
	local name="$1"; shift
	mkdir -p "$W/$name/mod" "$W/$name/obj" "$W/$name/tex"
	local t0=$SECONDS
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region $X0 $Y0 $X1 $Y1 --dim 4 \
		--out-dir "$W/$name/obj" --data-root "$DATA" "$@" > "$W/$name.log" 2>&1
	echo "$name rc=$? $((SECONDS - t0))s"
}

mkdir -p "$W"
run vt_cover  --vt "$W/vt_cover/mod"  --tex-dir "$W/vt_cover/tex"  --cover
run dir_cover                          --tex-dir "$W/dir_cover/tex" --cover
run vt_nc     --vt "$W/vt_nc/mod"     --tex-dir "$W/vt_nc/tex"
run dir_nc                             --tex-dir "$W/dir_nc/tex"

echo "== colour sheets =="
for n in vt_cover dir_cover vt_nc dir_nc; do
	f="$W/$n/tex/Commonwealth.4.-24.24.DDS"
	[ -f "$f" ] && echo "$n $(stat -c %s "$f") $(sha256sum "$f" | cut -c1-16)" || echo "$n MISSING"
done
echo "== the two comparisons =="
# never let cmp speak for a file that is not there: a missing input reported
# as "DIFFERS" is how a failed run looks exactly like a real result
pair() {  # pair <label> <a> <b>
	if [ ! -f "$2" ] || [ ! -f "$3" ]; then
		echo "$1: MISSING INPUT (a=$([ -f "$2" ] && echo y || echo n) b=$([ -f "$3" ] && echo y || echo n))"
		return 2
	fi
	if cmp -s "$2" "$3"; then echo "$1: IDENTICAL"; else echo "$1: DIFFERS"; fi
}
pair cover   "$W/vt_cover/tex/Commonwealth.4.-24.24.DDS" "$W/dir_cover/tex/Commonwealth.4.-24.24.DDS"
pair nocover "$W/vt_nc/tex/Commonwealth.4.-24.24.DDS" "$W/dir_nc/tex/Commonwealth.4.-24.24.DDS"
echo BAKE4-DONE
