#!/bin/bash
# lane FOGHEIGHT2 offline gate (no window): the air's ground layer with the derived layer ON by default and a thinner
# groundFog file giving way to it. Builds tests/fogground/fogground_gate.cpp twice (this tree's cellvolair.cpp and
# BASE's), runs the arms over the 40 weather/hour K rows of bungo's load order (fixture/ks.txt), and judges.
#   bash tests/fogheight2/run.sh <outdir> [green|red] [BASE commit, default 2190b994]     (run from the tree root)
# green: judge this tree (exit 0 = pass). red: judge BASE's code and BASE's celllooks.cpp as if they were the fix
# (must exit 1: today's main lets every shipped 4-12 m file win and ships the row off).
O=${1:?outdir}
MODE=${2:-green}
BASE=${3:-2190b994}
T=$(pwd)
D=$T/tests/fogheight2
export PATH=/c/msys64/ucrt64/bin:$PATH
mkdir -p $O/new $O/base
git show $BASE:src/gl/cellvolair.cpp > $O/base/cellvolair.cpp || exit 3
git show $BASE:src/gl/celllooks.cpp > $O/base/celllooks.cpp || exit 3
python -I $D/mk.py $T/src/gl/cellvolair.cpp $O/new/cut.inc < /dev/null || exit 3
python -I $D/mk.py $O/base/cellvolair.cpp $O/base/cut.inc < /dev/null || exit 3
QI="-I/c/msys64/ucrt64/include/qt6 -I/c/msys64/ucrt64/include/qt6/QtCore"
SRC="$T/src/recx.cpp $T/src/wwswitch.cpp"
for v in new base; do
	g++ -std=c++17 -O1 -w -I$O/$v -I$T/src $QI $T/tests/fogground/fogground_gate.cpp $SRC -lQt6Core -o $O/$v/gate.exe > $O/$v/build.log 2>&1
	echo "BUILD-$v-RC=$?"
done
run() {	# run <name> <exe> <root> <row> <burn> [red]
	WW_RECX_ROOT=$3 GATE_ROW=$4 GATE_BURN=$5 WW_CELL_AIR_GROUND_DERIVED_RED=$6 timeout 120 $2 $D/fixture/ks.txt > $O/$1.txt 2> $O/$1.err
	echo "$1 rc=$?"
}
for v in new base; do
	for r in ship empty prec thick; do
		case $r in ship) R=$T/res/Data;; *) R=$D/fixture/$r;; esac
		run ${v}_${r}_off $O/$v/gate.exe $R 0 0
		run ${v}_${r}_on $O/$v/gate.exe $R 1 0
	done
	run ${v}_ship_onburn $O/$v/gate.exe $T/res/Data 1 1
done
run new_ship_onred $O/new/gate.exe $T/res/Data 1 0 filewins
if [ "$MODE" = red ]; then
	JUDGE_X=base JUDGE_LOOKS=$O/base/celllooks.cpp python -I $D/judge.py $O > $O/judge_red.txt < /dev/null; rc=$?
	tail -n 12 $O/judge_red.txt
else
	JUDGE_X=new JUDGE_LOOKS=$T/src/gl/celllooks.cpp python -I $D/judge.py $O > $O/judge.txt < /dev/null; rc=$?
	tail -n 12 $O/judge.txt
fi
echo "FOGHEIGHT2 $MODE JUDGE-RC=$rc"
exit $rc
