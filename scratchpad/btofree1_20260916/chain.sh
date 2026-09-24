#!/usr/bin/env bash
# Lane BTOFREE1, 2026-09-16 -- the chain, run strictly one at a time against the
# exe this lane built. ONE NifSkope instance ever: nothing here runs in parallel,
# and the GUI harnesses (native_open, lod_generation) get the machine to
# themselves in turn like every other step.
#
# Every step's whole output goes to its own log; this script prints only the
# line that carries the count, so the chain's summary is readable in one screen
# and the evidence is still on disk.
ROOT=/e/Projects/NifskopeWildWastelandEdition
L="$ROOT/scratchpad/btofree1_20260916/chain"
mkdir -p "$L"
cd "$ROOT" || exit 1
export PY=/c/Users/bungo/AppData/Local/Programs/Python/Python39/python

exe_line() {
	stat -c '%s bytes, %y' release/NifSkope.exe
}

step() {
	local name="$1"; shift
	echo
	echo "### $name  ($(date '+%H:%M:%S'))"
	"$@" > "$L/$name.log" 2>&1
	local rc=$?
	grep -E "^[0-9]+ checks,|checks, [0-9]+ failures|^PASS$|^FAIL$|files, [0-9]+ differ" "$L/$name.log" | tail -4
	echo "    rc=$rc  log=$L/$name.log"
}

echo "chain start $(date)"
echo "exe: $(exe_line)"

step native_open            bash tests/spells/native_open.sh
step lodgen_native          env KEEP="$L/nat" bash tests/spells/lodgen_native.sh
step lodgen_ladder          env KEEP="$L/lad" bash tests/spells/lodgen_ladder.sh
step lodgen_native_baseline bash tests/spells/lodgen_native_baseline.sh --check
step lodgen_defaults        bash tests/spells/lodgen_defaults.sh
step lod_generation         bash tests/spells/lod_generation.sh
step lodgen_byte_gate       env PHASES=bc bash tests/spells/lodgen_byte_gate.sh

# the independent decoder, on EVERY pair the format suite left on disk -- the
# same sweep NATIVE1c reported as 6 pairs / 36 checks / 0 failures.
echo
echo "### lodgen_native_decode  ($(date '+%H:%M:%S'))"
DEC=0; DFAIL=0
: > "$L/lodgen_native_decode.log"
for o in $(find "$L/nat" -name '*.lodo' | sort); do
	i="${o%.lodo}.lodi"
	[ -f "$i" ] || i="$(find "$(dirname "$o")" -maxdepth 1 -name '*.lodi' | head -1)"
	[ -f "$i" ] || { echo "  no .lodi beside $o" >> "$L/lodgen_native_decode.log"; continue; }
	if "$PY" tests/spells/lodgen_native_decode.py "$o" "$i" >> "$L/lodgen_native_decode.log" 2>&1; then
		r=ok
	else
		r=FAIL; DFAIL=$((DFAIL+1))
	fi
	DEC=$((DEC+1))
	printf '    %-6s %s\n' "$r" "${o#$L/nat/}"
done
echo "    $DEC pair(s), $DFAIL failing"
grep -oE "[0-9]+ checks, [0-9]+ failures" "$L/lodgen_native_decode.log" | sort | uniq -c

echo
echo "chain end $(date)"
echo "exe: $(exe_line)"
