#!/bin/bash
#
# G2: THE PANEL ROW "Rebake only what changed" (lane INCRGATE1, 2026-09-24).
#
# INCR1's `--incremental` became a row in the LOD Generation panel's Run
# section, OFF by default. Two questions, each a number, on the panel's own
# run (WW_LODGEN_RUN: one FO4CS chunk (-20,24) dim 4 with the native pair, then
# a heightmap-only run, into %TEMP%/Lodgen_LODUI1_run):
#
#   (a) ROW OFF IS THE OLD BAKE. The rung exe (which has no row) and this exe
#       with the row forced OFF (WW_LODGEN_INCREMENTAL=0) leave byte-identical
#       trees (tests/spells/lodgen_tree_digest.py).
#   (b) ROW ON ADDS THE LEDGER AND NOTHING ELSE. This exe with the row forced
#       ON: every file of the OFF tree is present with the same bytes, and the
#       only new files are the bake record FO4CSLOD/Commonwealth/Commonwealth.lodb
#       and the per-chunk caches FO4CSLOD/Commonwealth/*.lodj. The record names
#       one chunk, carries the switch token `--panel`, and its census says this
#       was the first run (no record yet, all chunks baked).
#
# RED CONTROL: the rung's tree, read as the ON tree -- it has no record and no
# cache, so (b) fails on it by name. The spell prints that verdict too, and
# exits 0 only when (a) and (b) pass AND the red control failed.
#
# NOT COVERED, and owed: a SECOND panel run over the first run's record (the
# 0-dirty replay). The WW_LODGEN_RUN harness in src/nifskope_ui.cpp wipes its
# output folder at every launch, and that file is not this lane's.
#
# USAGE
#   bash tests/spells/lodgen_panel_incremental.sh
#   EXE=... RUNG=... WORK=... PORT=... bash tests/spells/lodgen_panel_incremental.sh

set -u
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
RUNG="${RUNG:-$ROOT/release/NifSkope.before_incrgate1.exe}"
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
PORT="${PORT:-42419}"
WORK="${WORK:-$ROOT/scratchpad/panel_incremental_work}"
TMPD="${TEMP:-/c/Users/$USER/AppData/Local/Temp}"
RUNDIR="$TMPD/Lodgen_LODUI1_run"
DIG="$ROOT/tests/spells/lodgen_tree_digest.py"
LOG="$ROOT/release/ww_lodgen_test.log"
fails=0

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -x "$RUNG" ] || { echo "no rung exe at $RUNG"; exit 2; }
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>&1 | grep -qi "Fallout4.exe"; then
	echo "FAIL: Fallout 4 is running -- no GUI harness while the game is up"; exit 2
fi
rm -rf "$WORK"; mkdir -p "$WORK"

# one panel run; $1 = exe, $2 = tag, $3 = WW_LODGEN_INCREMENTAL value or "" to leave it unset
panel () {
	local exe="$1" tag="$2" inc="$3"
	rm -f "$LOG"
	# the harness log lands beside the exe that ran
	local elog="$(dirname "$exe")/ww_lodgen_test.log"
	rm -f "$elog"
	if [ -n "$inc" ]; then
		WW_LODGEN_TEST=1 WW_LODGEN_RUN=1 WW_LODGEN_INCREMENTAL="$inc" "$exe" --port "$PORT" "$SRC" >/dev/null 2>&1
	else
		env -u WW_LODGEN_INCREMENTAL WW_LODGEN_TEST=1 WW_LODGEN_RUN=1 "$exe" --port "$PORT" "$SRC" >/dev/null 2>&1
	fi
	cp -f "$elog" "$WORK/$tag.log" 2>/dev/null
	echo "  $tag: $(grep -a ' checks, ' "$WORK/$tag.log" 2>/dev/null | tail -1)"
	echo "  $tag: $(grep -a 'run 1 result line' "$WORK/$tag.log" 2>/dev/null | cut -c1-200)"
	if [ ! -d "$RUNDIR" ]; then echo "FAIL: $tag wrote no tree at $RUNDIR"; fails=$((fails + 1)); return; fi
	cp -r "$RUNDIR" "$WORK/$tag"
}

echo "== the three panel runs"
panel "$RUNG" rung ""
panel "$NS" off 0
panel "$NS" on 1

echo "== (a) row OFF is the old bake: rung tree == OFF tree"
if python "$DIG" "$WORK/rung" "$WORK/off"; then echo "  ok      (a) the rung and the row-OFF bake are byte-identical"
else echo "  FAIL    (a) the rung and the row-OFF bake differ"; fails=$((fails + 1)); fi

cat > "$WORK/on_check.py" <<'PYEOF'
import os, sys
off, on = sys.argv[1], sys.argv[2]
def walk(r):
    d = {}
    for dp, _, fs in os.walk(r):
        for f in fs:
            p = os.path.join(dp, f)
            d[os.path.relpath(p, r).replace('\\', '/')] = p
    return d
a, b = walk(off), walk(on)
same = [k for k in a if k in b and open(a[k], 'rb').read() == open(b[k], 'rb').read()]
lost = [k for k in a if k not in b]
moved = [k for k in a if k in b and k not in same]
extra = sorted(k for k in b if k not in a)
rec = [k for k in extra if k == 'FO4CSLOD/Commonwealth/Commonwealth.lodb']
lodj = [k for k in extra if k.startswith('FO4CSLOD/Commonwealth/') and k.endswith('.lodj')]
other = [k for k in extra if k not in rec and k not in lodj]
print('      %d file(s) of the OFF tree identical, %d lost, %d changed; extra: %d record, %d .lodj, %d other'
      % (len(same), len(lost), len(moved), len(rec), len(lodj), len(other)))
for k in (lost + moved + other)[:8]:
    print('        ' + k)
ok = True
def check(c, t):
    global ok
    print(('  ok      ' if c else '  FAIL    ') + t)
    ok = ok and c
check(len(a) > 0 and not lost and not moved, '(b1) every file of the row-OFF bake is there, byte for byte')
check(len(rec) == 1 and len(lodj) >= 1 and not other, '(b2) the only new files are the bake record and the .lodj caches')
chunks = switch = first = 0
if rec:
    for line in open(b[rec[0]], 'rb').read().decode('utf-8', 'replace').split('\n'):
        k, _, v = line.partition('\t')
        if k == 'chunk':
            chunks += 1
        elif k == 'switch' and v == '--panel':
            switch += 1
        if 'no bake record at' in line and 'all 1 chunk(s) baked' in line:
            first += 1
print('      record: %d chunk line(s), %d `--panel` switch, first-run census %d' % (chunks, switch, first))
check(chunks == 1 and switch == 1 and first == 1, '(b3) the record names 1 chunk, the switch --panel and the first-run census')
sys.exit(0 if ok else 1)
PYEOF

echo "== (b) row ON adds the ledger and nothing else"
python "$WORK/on_check.py" "$WORK/off" "$WORK/on" || fails=$((fails + 1))

echo "== red control: the rung's tree read as the ON tree (it must FAIL)"
if python "$WORK/on_check.py" "$WORK/off" "$WORK/rung"; then
	echo "  FAIL    the red control PASSED: the check cannot see a missing record"; fails=$((fails + 1))
else
	echo "  ok      the red control failed, as it must (no record, no cache)"
fi

echo
[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL ($fails)"
exit $([ $fails -eq 0 ] && echo 0 || echo 1)
