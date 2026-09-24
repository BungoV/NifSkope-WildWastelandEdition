#!/bin/bash
# AUDIT1 step 3: the REFUTERS for the .lodo/.lodi invariants, proven caught.
#
# Every case is applied to a COPY of a REAL product-written pair (no synthetic
# input), by tests/spells/lodgen_native_doctor.py, which re-signs every checksum
# the format carries so the mutation cannot be caught by a CRC. The independent
# Python decoder must then refuse it. A case the decoder accepts is a check that
# cannot fail, and is reported as such.
#   usage: bash refute_native.sh <lodo> <lodi>
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
PY="${PY:-/c/Users/bungo/AppData/Local/Programs/Python/Python39/python}"
S="$ROOT/tests/spells"
LODO="$1"; LODI="$2"
T="$(mktemp -d)"
missed=0
for case in lodi-wrap lodo-wrap agg-views agg-record; do
	rm -rf "$T/c"; mkdir -p "$T/c"
	cp "$LODO" "$T/c/a.lodo"; cp "$LODI" "$T/c/a.lodi"
	dout="$("$PY" "$S/lodgen_native_doctor.py" "$case" "$T/c/a.lodo" "$T/c/a.lodi" 2>&1)"
	drc=$?
	if [ $drc -ne 0 ]; then echo "$case  DOCTOR-FAILED  $(printf '%s' "$dout" | tail -1)"; continue; fi
	expect="$(printf '%s\n' "$dout" | sed -n 's/.*expect: *//p' | head -1)"
	out="$("$PY" "$S/lodgen_native_decode.py" "$T/c/a.lodo" "$T/c/a.lodi" 2>&1)"
	rc=$?
	if [ $rc -ne 0 ]; then
		echo "$case  CAUGHT (decoder rc=$rc)  expect=\"$expect\""
		printf '%s\n' "$out" | grep -aiE 'refus|FAIL' | head -2 | sed 's/^/        /'
	else
		echo "$case  NOT CAUGHT (decoder rc=0)  expect=\"$expect\""
		missed=$(( missed + 1 ))
	fi
done
rm -rf "$T"
echo "$missed refuter(s) the independent decoder did NOT catch"
