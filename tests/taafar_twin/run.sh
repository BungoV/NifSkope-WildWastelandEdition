#!/bin/bash
# lane FARTOGGLE3: sub-pixel far objects through the Modern temporal AA on a still camera (twin.py, real taa2.frag
# through the driver, no NifSkope window). Every tier must PASS; the red (taa2Red 128 = WW_TAA2_RED=oldlock, the
# resolve before FARTOGGLE3) must FAIL on every tier. Also prints TAA off and the game copy (information).
# PY= the python with numpy (code1030 unit.sh puts MSYS2 first on PATH).
cd "$(dirname "$0")/../.." || exit 2
PY="${PY:-$(command -v python)}"
T=tests/taafar_twin/twin.py
ok=1
for e in 1 2 3 4; do
	g=$(timeout 600 "$PY" -I "$T" --engines "$e" --verdict "$e" < /dev/null 2>&1 | grep '^VERDICT')
	echo "green $g"
	case "$g" in "VERDICT PASS"*) ;; *) ok=0 ;; esac
	r=$(timeout 600 "$PY" -I "$T" --engines "$e" --verdict "$e" --red 128 < /dev/null 2>&1 | grep '^VERDICT')
	echo "red   $r"
	case "$r" in "VERDICT FAIL"*) ;; *) ok=0 ;; esac
done
timeout 600 "$PY" -I "$T" --engines off,game < /dev/null 2>&1 | grep '^engine'
if [ $ok = 1 ]; then echo "TAAFAR RESULT PASS"; exit 0; fi
echo "TAAFAR RESULT FAIL"; exit 1
