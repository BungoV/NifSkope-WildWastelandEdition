#!/bin/bash
#
# Stable identity: the key of an object is its REFERENCE, carried in the
# manifest, so the same object in two rings (and in two bakes) is matched by
# (ref, part) and never by the per-chunk index or the shared base.
#
# Built here: the near Sanctuary chunk (-20,24) at dim 4, twice, and the dim-8
# chunk that contains it, (-24,24). Checks:
#   1. two bakes of one chunk are byte-identical (.bto and manifest)
#   2. the manifest's first line names the ring, the chunk and the columns
#   3. every row has the ref and part columns, and (ref, part) is unique
#   4. the dim-8 chunk shares objects with the dim-4 chunk by (ref, part), and
#      every shared object has the same base and the same position in both
#   5. SCOL parts carry their ordinal (part >= 0) - the case a bare reference
#      key would have collapsed
#
# USAGE
#   bash tests/spells/lodgen_identity.sh

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }

fails=0
ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }

build() {	# x y dim out
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects "$1" "$2" --dim "$3" --no-ao \
		--data-root "$DATA" -o "$4" >/dev/null 2>&1
}
build -20 24 4 "$W/a4.bto"
build -20 24 4 "$W/b4.bto"
build -24 24 8 "$W/a8.bto"
for f in a4 b4 a8; do
	[ -s "$W/$f.bto" ] && [ -s "$W/$f.bto.manifest.txt" ] || { bad "$f: chunk or manifest missing"; echo "RESULT FAIL"; exit 1; }
done
ok "three chunks built (dim 4 twice, dim 8 once)"

if cmp -s "$W/a4.bto" "$W/b4.bto" && cmp -s "$W/a4.bto.manifest.txt" "$W/b4.bto.manifest.txt"; then
	ok "two bakes of one chunk are byte-identical, chunk and manifest"
else
	bad "two bakes of one chunk differ"
fi

"$PY" - "$W/a4.bto.manifest.txt" "$W/a8.bto.manifest.txt" <<'PYEOF'
import sys
def load(path):
    lines = open(path).read().splitlines()
    header = lines[0] if lines else ''
    rows = {}
    parts = 0
    dup = 0
    for l in lines[1:]:
        t = l.split()
        if not t or not t[0].isdigit():
            continue
        if len(t) < 11:
            print('  FAIL a row lacks the ref and part columns: %r' % l); sys.exit(1)
        key = (t[9], int(t[10]))
        if int(t[10]) >= 0:
            parts += 1
        if key in rows:
            dup += 1
        rows[key] = (t[1], t[3], t[4], t[5])
    return header, rows, parts, dup
h4, r4, parts4, dup4 = load(sys.argv[1])
h8, r8, parts8, dup8 = load(sys.argv[2])
fails = 0
def check(what, cond):
    global fails
    print(('  ok   ' if cond else '  FAIL ') + what)
    if not cond: fails += 1
print('  header: %s' % h4)
check('the first line names ring, chunk and columns', h4.startswith('# lodgen manifest 2 ws Commonwealth dim 4 chunk -20 24 columns index base type x y z scale class height ref part'))
check('the dim-8 header names its own ring and chunk', h8.startswith('# lodgen manifest 2 ws Commonwealth dim 8 chunk -24 24 '))
print('  dim 4: %d rows, %d SCOL parts, %d duplicate keys; dim 8: %d rows, %d parts, %d duplicates' % (len(r4), parts4, dup4, len(r8), parts8, dup8))
check('(ref, part) is unique within a chunk', dup4 == 0 and dup8 == 0 and len(r4) > 100)
check('SCOL parts carry their ordinal', parts4 > 0)
shared = set(r4) & set(r8)
same = sum(1 for k in shared if r4[k] == r8[k])
print('  shared between the rings: %d objects, %d with the same base and position' % (len(shared), same))
check('the rings share objects by (ref, part)', len(shared) >= 100)
check('every shared object has the same base and position in both rings', len(shared) > 0 and same == len(shared))
sys.exit(1 if fails else 0)
PYEOF
[ $? -eq 0 ] || fails=$((fails + 1))

[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
