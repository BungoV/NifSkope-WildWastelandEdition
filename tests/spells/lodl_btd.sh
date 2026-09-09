#!/bin/bash
#
# Converting a Fallout 76 .btd to .lodl.
#
# The point of this gate is that the format was specified BEFORE either the
# writer or the converter existed, on the claim that a .btd would need no
# reshaping to fit it. What is tested:
#
#   1. the conversion runs with no plugin at all -- a .btd is the whole
#      landscape, so `lodgen --from-btd` takes no <file>
#   2. the file it writes passes the SAME reader as the Fallout 4 path, at a
#      different samples-per-cell and a different height quantum. Had either
#      been baked in as a constant rather than a header field, this fails.
#   3. heights round-trip to within HALF a quantum. Not exactly, unlike the
#      FO4 path: their grid starts at minHeight and ours is centred on zero, so
#      the two sit half a step apart -- and half a step is the bound, not one.
#      One was the first bound, and it hid a truncation bug through every run.
#   4. alpha words round-trip EXACTLY, since they are copied verbatim. This is
#      also the only read of plane 1 in a four-plane block.
#
# WHAT THIS CANNOT TEST, and why it says so out loud.
#
# EXM1PittWorldspace -- the only .btd small enough to run in a harness -- has
# ZERO land textures and a single constant terrain colour. So its LTEX alphas
# are all zero and its colour plane is one word. An earlier version of this
# script checked an "alpha invariant" here, watched it pass on 245 million
# samples, and reported the packing confirmed. It was confirming nothing: every
# word was zero. Appalachia then broke the same check on 72% of samples.
#
# A check that cannot fail on the input you run it against is not a check. So
# the alpha and colour paths are reported as UNCOVERED here rather than passed,
# and covering them means running Appalachia by hand:
#
#   NifSkope -no-gui lodgen --from-btd .../Appalachia.btd --lodl <dir>
#
# which takes ~25 minutes and ~2.5 GB.
#
# Skips cleanly when Fallout 76 is not installed.

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
F76="${F76:-E:/SteamLibrary/steamapps/common/Fallout 76 Playtest/Data/Terrain}"
BTD="${BTD:-$F76/EXM1PittWorldspace.btd}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$BTD" ] || { echo "no .btd at $BTD (Fallout 76 not installed) -- skipping"; exit 0; }

fails=0
check() { # check <name> <grep-pattern>
	if grep -q "$2" "$LOG"; then
		echo "  ok   $1"
	else
		echo "  FAIL $1"
		fails=$((fails + 1))
	fi
}

LOG="$W/out.txt"
"$NS" -no-gui lodgen --from-btd "$BTD" --lodl "$W" > "$LOG" 2>&1
RC=$?

if [ $RC -ne 0 ]; then
	echo "  FAIL the conversion exited $RC"
	cat "$LOG"
	echo "RESULT FAIL"
	exit 1
fi
echo "  ok   the conversion runs with no plugin file"

F="$(ls "$W"/Terrain/*.lodl 2>/dev/null | head -1)"
if [ -n "$F" ] && [ -s "$F" ]; then
	echo "  ok   a .lodl was written ($(wc -c < "$F") bytes)"
else
	echo "  FAIL no .lodl was written"
	echo "RESULT FAIL"
	exit 1
fi

# the tool reads its own output back through the independent reader and
# compares it against the .btd, so these lines are its verdict, not its intent
check "reader round-trips within HALF a quantum" '0 past half a quantum'
check "alpha words round-trip exactly (plane 1 of a four-plane block)" 'alpha words: 0 of'
check "colour words round-trip exactly (plane 2, A1R5G5B5 repacked)" 'colour words: 0 of'
check "ground cover round-trips exactly (plane 3)" 'ground cover: 0 of'
check "samples per cell came from the source, not a constant 32" 'rate 128'
check "no water invented where the source has none" 'water: none'

# the quantum must have come from the .btd's own height range; a file still
# saying 8 would mean the header field is being ignored
if grep -qE 'quantum 8($|[^.0-9])' "$LOG"; then
	echo "  FAIL height quantum is still FO4's 8 -- the header field is ignored"
	fails=$((fails + 1))
else
	echo "  ok   height quantum came from the source ($(grep -o 'quantum [0-9.]*' "$LOG" | head -1))"
fi

# Say plainly what this worldspace does and does not exercise, so nobody reads
# a pass here as coverage of the layer and colour paths.
LTEX="$(grep -o 'LTEX [0-9]*' "$LOG" | head -1 | tr -d 'LTEX ')"
COLW="$(grep -o '[0-9]* distinct words' "$LOG" | grep -o '^[0-9]*')"
[ "${LTEX:-0}" -gt 0 ] 2>/dev/null \
	&& echo "  ok   source has $LTEX land textures, so the alpha path is exercised" \
	|| echo "  NOTE source has no land textures -- the LTEX alpha path is UNCOVERED here"
[ "${COLW:-1}" -gt 1 ] 2>/dev/null \
	&& echo "  ok   source colour varies ($COLW distinct words)" \
	|| echo "  NOTE source colour is one constant word -- the colour path is UNCOVERED here"

[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
