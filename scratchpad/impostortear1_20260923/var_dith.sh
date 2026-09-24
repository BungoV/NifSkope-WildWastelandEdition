#!/bin/sh
# sh var_run.sh SWEEPS "T:tol:rule ..."   -- shader-only march variants into pop/m<T>_<tol>_<rule>
cd "$(dirname "$0")"
for v in $2; do
	T=${v%%:*}; r=${v##*:}; tol=${v#*:}; tol=${tol%%:*}
	n="dith${T}"
	python dither_shader.py run_fix/shaders/impostor_oct.frag run_var/shaders/impostor_oct.frag $T >/dev/null || exit 1
	for sw in $1; do sh sweep.sh "$PWD/run_var" "$PWD/pop/$n" $sw blast_n4 maple_n4 rock_n4; done
	python pop.py pop run_ship $n > "pop_$n.txt" 2>&1
	grep -E "FAIL|PASS|IoU [0-9]" "pop_$n.txt" | sed "s/^/$n /"
done
echo VARDONE
