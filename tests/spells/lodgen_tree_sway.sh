#!/bin/bash
#
# Tree sway weight in the identity profile's vertex ALPHA: 0 at the trunk base,
# 1 at the branch tips.
#
# Three properties, in the order they would break:
#
#   1. only TREES sway -- a shack or a warehouse carries an explicit zero, so a
#      consumer applying the channel blindly does nothing rather than something
#      wrong,
#   2. the weight RISES WITH HEIGHT -- this is what fails if the formula
#      regresses; a constant or inverted channel still passes (1),
#   3. BRANCH shapes are entirely non-zero while TRUNK shapes are not. That is
#      the signature of normalising across all of a placement's shapes rather
#      than per shape: branch cards sit above the trunk base, so they inherit
#      its scale instead of re-zeroing at their own and putting a step change
#      at the join.

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }

checks=0; fails=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then echo "  ok   $1"; else echo "  FAIL $1"; fails=$((fails+1)); fi; }

# Sanctuary (-20,24): maples, elms, blasted forest, shacks and a warehouse
# --objects takes its OWN chunk coords, exactly like --terrain; passing
# --terrain as well makes it silently bake a DIFFERENT chunk.
# Sway rides in the vertex ALPHA, which is identity data: OFF by default
# since 2026-09-12, so this harness spells --identity.
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -20 24 --dim 4 --identity \
	-o "$W/sway.bto" >/dev/null 2>&1
[ -f "$W/sway.bto" ] || { echo "FAIL: the object chunk did not generate"; exit 1; }

"$PY" - "$NS" "$W/sway.bto" <<'PYEOF' > "$W/r.txt" 2>&1
import os, re, subprocess, sys
NS, F = sys.argv[1], sys.argv[2]
listing = subprocess.run([NS, '-no-gui', 'list', F], capture_output=True, text=True).stdout
tree = nontree = 0
rising = falling = 0
branch_full = trunk_partial = 0
for m in re.finditer(r"\[(\d+)\] BSSubIndexTriShape", listing):
    blk = int(m.group(1))
    d = subprocess.run([NS, '-no-gui', 'dump', F, '-b', str(blk), '-f', 'Vertex Data',
                        '-d', '4', '-n', '400000'], capture_output=True, text=True).stdout
    a = [int(c[6:8], 16) for c in
         re.findall(r"Vertex Colors  <ByteColor4>  = #([0-9a-f]{8})", d)]
    zs = [float(z) for x, y, z in re.findall(
        r"Vertex\s+<\w+>\s*=\s*X\s*(-?[\d.eE+]+)\s+Y\s*(-?[\d.eE+]+)\s+Z\s*(-?[\d.eE+]+)", d)]
    if not a:
        continue
    dd = subprocess.run([NS, '-no-gui', 'dump', F, '-b', str(blk + 2)],
                        capture_output=True, text=True).stdout
    t = re.search(r"([^ \"\/]*\.dds)", dd)
    tex = (t.group(1) if t else "").lower()
    is_tree = "trunks" in tex or "branches" in tex
    share = sum(1 for v in a if v > 0) / len(a)
    if is_tree:
        tree += 1
        if len(a) > 20 and len(zs) == len(a):
            p = sorted(zip(zs, a)); q = max(1, len(p) // 4)
            lo = sum(x[1] for x in p[:q]) / q
            hi = sum(x[1] for x in p[-q:]) / q
            if hi > lo: rising += 1
            else: falling += 1
        if "branches" in tex and share > 0.999: branch_full += 1
        if "trunks" in tex and share < 0.999: trunk_partial += 1
    else:
        nontree += 1
        if max(a) != 0:
            print("NONZERO_NONTREE %s max %d" % (tex, max(a)))
print("TREE %d NONTREE %d RISING %d FALLING %d BRANCHFULL %d TRUNKPARTIAL %d"
      % (tree, nontree, rising, falling, branch_full, trunk_partial))
PYEOF
cat "$W/r.txt"
read -r _ tree _ nontree _ rising _ falling _ bfull _ tpart < <(grep '^TREE' "$W/r.txt")

check "the fixture has both trees and non-trees" \
	"$([ "${tree:-0}" -ge 3 ] && [ "${nontree:-0}" -ge 2 ] && echo 1 || echo 0)"
check "non-trees carry an explicit ZERO" \
	"$(grep -q '^NONZERO_NONTREE' "$W/r.txt" && echo 0 || echo 1)"
check "sway RISES with height on every tree shape (none inverted)" \
	"$([ "${rising:-0}" -ge 3 ] && [ "${falling:-0}" = "0" ] && echo 1 || echo 0)"
check "BRANCH shapes are wholly non-zero, TRUNK shapes are not (shared scale)" \
	"$([ "${bfull:-0}" -ge 1 ] && [ "${tpart:-0}" -ge 1 ] && echo 1 || echo 0)"

echo "$checks checks, $fails failures"
[ "$fails" = "0" ] && echo PASS || echo FAIL
[ "$fails" = "0" ]
