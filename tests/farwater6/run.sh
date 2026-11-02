#!/bin/bash
# lane FARWATER6 (FARWATER5 L4): one speed reference for both flow writers; the marking tool marks encoding 1.
#   bash tests/farwater6/run.sh <exe> <base exe> <out dir> [bake|selftest|judge|all]
# bake:     the Commonwealth .lodl from both exes, linear and WW_LODL_FLOW_LOG=1: lodgen must be byte-equal (G1).
# selftest: --water-mark-selftest on copies: green linear, green log, red (WW_WATER_SPEED_REF_RED=mean) linear and log,
#           base exe linear (the reference FAIL list).
# judge:    G2 green linear: L4 ok and no FAIL the base lacks; G3 green log: opens, L4 ok, encoding 1 kept;
#           G4 red linear: L4 FAILS; G5 red log: refused at open.
# Fallout4.exe must not run; ESM= overrides the master. Steps can run in separate calls (each one under ~5 min here).
set -u
EXE=${1:?exe}; BASE=${2:?base exe}; OUT=${3:?out dir}; STEP=${4:-all}
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
mkdir -p "$OUT"
O=$(cygpath -m "$OUT")
tasklist | grep -qi '^Fallout4.exe' && { echo "Fallout4.exe is running"; exit 3; }

bake() {   # bake <exe> <tag> <log 0|1>
	[ -s "$OUT/$2.lodl" ] && { echo "[$2] kept"; return 0; }
	env WW_LODL_FLOW_LOG=$3 "$1" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$O/b_$2" --water-report "$O/$2_report.txt" \
		< /dev/null > "$OUT/$2_bake.log" 2>&1
	cp "$OUT/b_$2/FO4CSLOD/Commonwealth/Commonwealth.lodl" "$OUT/$2.lodl" && rm -rf "$OUT/b_$2"
	echo "[$2] $(grep -h -o 'speed 15 on [^)]*)' "$OUT/$2_report.txt")"
}
selftest() {   # selftest <exe> <tag> <lodl tag> [env...]
	local exe=$1 tag=$2 src=$3; shift 3
	[ -s "$OUT/st_$tag.log" ] && { echo "[st $tag] kept"; return 0; }
	cp "$OUT/$src.lodl" "$OUT/st_$tag.lodl"
	env "$@" "$exe" -no-gui lodl "$O/st_$tag.lodl" --water-mark-selftest < /dev/null > "$OUT/st_$tag.log" 2>&1
	echo "rc=$?" >> "$OUT/st_$tag.log"
	rm -f "$OUT/st_$tag.lodl"
	echo "[st $tag] $(grep -c '^  ok ' "$OUT/st_$tag.log") ok, $(grep -c '^  FAIL ' "$OUT/st_$tag.log") FAIL, $(tail -1 "$OUT/st_$tag.log")"
}
fails() { grep '^  FAIL ' "$OUT/st_$1.log" | sed -E 's/[0-9]+(\.[0-9]+)?/N/g' | sort -u; }

case $STEP in bake|all)
	bake "$EXE" new_lin 0; bake "$EXE" new_log 1; bake "$BASE" base_lin 0; bake "$BASE" base_log 1
	g1=0
	cmp -s "$OUT/new_lin.lodl" "$OUT/base_lin.lodl" && echo "G1a ok   lodgen linear bake byte-equal to the base" || { echo "G1a FAIL linear bake differs"; g1=1; }
	cmp -s "$OUT/new_log.lodl" "$OUT/base_log.lodl" && echo "G1b ok   lodgen log bake byte-equal to the base" || { echo "G1b FAIL log bake differs"; g1=1; }
	echo "G1 $([ $g1 = 0 ] && echo PASS || echo FAIL)" > "$OUT/g1.txt";;
esac
case $STEP in selftest|all)
	selftest "$EXE" green_lin new_lin; selftest "$EXE" green_log new_log
	selftest "$EXE" red_lin new_lin WW_WATER_SPEED_REF_RED=mean; selftest "$EXE" red_log new_log WW_WATER_SPEED_REF_RED=mean
	selftest "$BASE" base_lin base_lin;;
esac
case $STEP in judge|all)
	bad=0
	cat "$OUT/g1.txt"; grep -q PASS "$OUT/g1.txt" || bad=1
	extra=$(comm -23 <(fails green_lin | grep -v ' L4 ') <(fails base_lin))
	l4=$(grep -c '^  ok   L4 ' "$OUT/st_green_lin.log")
	[ -z "$extra" ] && [ "$l4" = 1 ] && echo "G2 PASS green linear: L4 ok, no FAIL the base lacks ($(fails base_lin | wc -l) shared)" \
		|| { echo "G2 FAIL green linear (L4 ok lines $l4; new FAILs: $extra)"; bad=1; }
	l4g=$(grep '^  ok   L4 ' "$OUT/st_green_log.log" | grep -c 'flow encoding 1')
	[ "$l4g" = 1 ] && echo "G3 PASS green log: opened, L4 ok on encoding 1 ($(grep -c '^  FAIL ' "$OUT/st_green_log.log") FAIL)" \
		|| { echo "G3 FAIL green log: $(grep -m2 -E 'L4|encoding' "$OUT/st_green_log.log")"; bad=1; }
	grep -q '^  FAIL L4 ' "$OUT/st_red_lin.log" && echo "G4 PASS red linear FAILS L4: $(grep -o 'is -\?[0-9]* (needs 8' "$OUT/st_red_lin.log")" \
		|| { echo "G4 FAIL the red does not fail L4"; bad=1; }
	grep -q 'uses encoding 1' "$OUT/st_red_log.log" && echo "G5 PASS red refuses encoding 1" \
		|| { echo "G5 FAIL red log was not refused"; bad=1; }
	[ $bad = 0 ] && echo "FARWATER6 RUN PASS" || echo "FARWATER6 RUN FAIL"
	exit $bad;;
esac
