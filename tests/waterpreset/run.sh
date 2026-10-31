#!/bin/bash
# lane FIX1101A (bungo 10-31 RULED: "water that looks good and is not expensive to render"): the water quality preset.
#   table   src/gl/waterpreset.h: Low no look, Ultra every look (the default), steps only add, the costly terms high;
#           red -DWATERPRESET_RED (today: no preset) must FAIL
#   budget  the MEASURED cost files (tests/watercost/cost_*_1080.txt, cost_ssr_march_1080.txt) per step vs Low 0.4 /
#           Medium 0.8 / High 1.2 / Ultra 1.8 ms at 1080p; red --red today (no preset, the full-normal ray pass) must FAIL
# PY= the python with numpy. Green: every arm as wanted, rc 0.
cd "$(dirname "$0")/../.." || exit 2
PY="${PY:-$(command -v python)}"
OUT="${TMPDIR:-/tmp}/waterpreset_$$"
mkdir -p "$OUT"
export PATH=/c/msys64/ucrt64/bin:$PATH
ok=1
g++ -std=c++17 -O1 -I src/gl tests/waterpreset/preset_test.cpp -o "$OUT/p.exe" || ok=0
g++ -std=c++17 -O1 -DWATERPRESET_RED -I src/gl tests/waterpreset/preset_test.cpp -o "$OUT/pr.exe" || ok=0
v=$(timeout 30 "$OUT/p.exe" | tail -1)
printf '%-30s %s (want PASS)\n' "table green" "$v"
case "$v" in *"TABLE PASS"*) ;; *) ok=0 ;; esac
v=$(timeout 30 "$OUT/pr.exe" | tail -1)
printf '%-30s %s (want FAIL)\n' "table red (no preset)" "$v"
case "$v" in *"TABLE FAIL"*) ;; *) ok=0 ;; esac
for a in "budget green|PASS|" "budget red today|FAIL|--red today"; do
	IFS='|' read -r n w args <<< "$a"
	v=$(timeout 60 "$PY" -I tests/waterpreset/budget.py $args < /dev/null 2>&1 | grep '^VERDICT')
	printf '%-30s %s (want %s)\n' "$n" "$v" "$w"
	case "$v" in "VERDICT $w"*) ;; *) ok=0 ;; esac
done
rm -rf "$OUT"
if [ $ok = 1 ]; then echo "WATERPRESET RESULT PASS"; exit 0; fi
echo "WATERPRESET RESULT FAIL"
exit 1
