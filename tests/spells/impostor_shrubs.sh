#!/bin/bash
#
# NO SHRUB BAKES AN EMPTY CARD (lane IMPOSTORSHRUB1, 2026-09-23).
#
# The population is derived from the PLUGIN every run, never typed: every base
# record of Fallout4.esm whose model path or editor id names a shrub, bush,
# sapling, undergrowth or hedge (tools/lod_emission_probe.py readPlugin; the
# look-behinds keep "Ambush" and "TrashEdge" out). On 2026-09-23 that is 60
# bases and 55 distinct models, 54 of them in the unpacked corpus (HedgeRow05,
# an MSTT placed nowhere in the Commonwealth, is not). None carries an MNAM
# slot, so none is an impostor candidate of `--list-impostor-candidates`
# today; they are the plants a card bake meets when it is handed one.
#
# Each model is photographed by the WW_IMPOSTOR_BAKE hook (N=8, TILE=512, the
# size ladder at REF 1326.5, the largest tree candidate of the Commonwealth) and
# its octahedral colour sheet is counted: COVERED TEXELS = alpha > 0, and the
# oct line's halfW. On the rung exe (c172ba9d) 27 of the 54 bake 0 covered
# texels at the fallback halfW 1.07733: the bake kept the BSMeshLODTriShape's
# LOD0 slot, which those models ship empty. This row is red there.
#
# Rows
#   S1 (floor) the census found >= 50 models on disk, so an empty list cannot pass
#   S2 every model baked (an albedo sheet and an oct line exist)
#   S3 no model's colour sheet has 0 covered texels, and no halfW is the 1.07733
#      nothing-measured fallback
#   S4 every model with no LOD0 part (every BSMeshLODTriShape ships the LOD0
#      slot empty) says which level served (`rangekept`), and no model with a
#      LOD0 part says one: the level is chosen per MODEL, so the maple
#      TreeMapleblasted05 (141/0/42 beside 0/75/5) keeps LOD0 as before
#
# USAGE
#   bash tests/spells/impostor_shrubs.sh            (~6 minutes, 54 bakes)
#   EXE=<exe> MATCH=<substring> bash tests/spells/impostor_shrubs.sh
#   KEEP=<dir> keeps the bakes there

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
. "$ROOT/tests/spells/_harness.sh"
if [ -n "${KEEP:-}" ]; then W="$KEEP"; mkdir -p "$W"; else W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT; fi
WA="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })"
RA="$(cd "$ROOT" && { pwd -W 2>/dev/null || pwd; })"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }
tasklist 2>/dev/null | grep -i -q "Fallout4" && { echo "REFUSED: Fallout4.exe is up"; exit 2; }
echo "exe: $NS ($(stat -c '%y %s' "$NS" 2>/dev/null))"

"$PY" - "$RA" "$ESM" "$DATA" "$WA/models.txt" <<'PYEOF'
import os, re, sys
root, esm, data, out = sys.argv[1:5]
sys.path.insert(0, root + '/tools')
import lod_emission_probe as P
PAT = re.compile(r'shrub|(?<!am)bush|sapling|undergrowth|(?<!tras)hedge', re.I)
_, _, _, baseOf, _ = P.readPlugin(esm)
models = set()
for form, (typ, edid, modl, mnam) in baseOf.items():
    if modl and (PAT.search(modl) or PAT.search(edid)):
        models.add(modl.lower().replace(chr(92), '/'))
onDisk = sorted(m for m in models if os.path.isfile(data + '/meshes/' + m))
open(out, 'w', newline='\n').write('\n'.join(onDisk) + '\n')   # LF: a CR rides into the path and the bake waits on a missing file
print('census: %d models named by the plugin, %d on disk' % (len(models), len(onDisk)))
PYEOF

P=${PORTBASE:-29700}
while read -r rel; do
	rel="$(printf '%s' "$rel" | tr -d '\r')"
	[ -n "$rel" ] || continue
	b="$(basename "$rel" .nif)"
	[ -n "${MATCH:-}" ] && case "$b" in *"$MATCH"*) ;; *) continue ;; esac
	O="$WA/$b"; rm -rf "$O"; mkdir -p "$O"
	P=$((P+1))
	WW_IMPOSTOR_BAKE="$O" WW_IMPOSTOR_OCT=8 WW_IMPOSTOR_TILE=512 WW_IMPOSTOR_REF=1326.5 \
		timeout 900 "$NS" "$DATA/meshes/$rel" --port "$P" > "$O/bake.stdout" 2>&1
done < "$W/models.txt"

"$PY" - "$WA" "$DATA" "${MATCH:-}" <<'PYEOF'
import os, sys
import numpy as np
from PIL import Image
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tools/rigging_prototype')
import io, contextlib, struct
import nifparse
w, data, match = sys.argv[1], sys.argv[2], sys.argv[3]
fails = 0
def check(what, cond):
    global fails
    print(('  ok   ' if cond else '  FAIL ') + what)
    if not cond: fails += 1
models = [l for l in open(w + '/models.txt').read().split() if l]
if match:
    models = [m for m in models if match in os.path.basename(m)]
else:
    check('S1 (floor): the census found %d models on disk (>= 50)' % len(models), len(models) >= 50)
baked, empty, rows, needKept, saidKept, wrongKept = 0, [], [], [], [], []
for rel in models:
    b = os.path.splitext(os.path.basename(rel))[0]
    txt, png = '%s/%s/%s.txt' % (w, b, b), '%s/%s/%s_oct_albedo.png' % (w, b, b)
    if not (os.path.isfile(txt) and os.path.isfile(png)):
        empty.append(b + '(no bake)'); continue
    lines = open(txt).read().splitlines()
    octl = [l.split() for l in lines if l.startswith('oct ')]
    if not octl:
        empty.append(b + '(no oct line)'); continue
    baked += 1
    halfW = float(octl[0][4])
    cov = int((np.asarray(Image.open(png).convert('RGBA'))[..., 3] > 0).sum())
    rows.append((b, cov, halfW))
    if cov == 0 or abs(halfW - 1.07733) < 1e-4:
        empty.append('%s(%d texels, halfW %.5f)' % (b, cov, halfW))
    # does the model have NO LOD0 part (every BSMeshLODTriShape's LOD0 slot empty)?
    with contextlib.redirect_stdout(io.StringIO()):
        d, hdr, strings, blocks = nifparse.parse(data + '/meshes/' + rel)
    anyLod, anyL0 = False, False
    for i, t, s, z in blocks:
        if t != 'BSMeshLODTriShape':
            continue
        l0, l1, l2 = struct.unpack_from('<3I', d, s + z - 12)
        anyLod = anyLod or bool(l0 or l1 or l2)
        anyL0 = anyL0 or bool(l0)
    said = any(l.startswith('rangekept ') for l in lines)
    if anyLod and not anyL0:
        needKept.append(b)
        if said:
            saidKept.append(b)
    elif said:
        wrongKept.append(b)
for b, cov, hw in sorted(rows, key=lambda r: r[1])[:5]:
    print('  fewest covered: %-28s %7d texels  halfW %.2f' % (b, cov, hw))
check('S2: every model baked (%d of %d)' % (baked, len(models)), baked == len(models) and baked > 0)
check('S3: no model bakes an EMPTY card (%d empty%s)' % (len(empty), (': ' + ', '.join(empty[:12])) if empty else ''),
      not empty and baked > 0)
check('S4: every model with no LOD0 part names the level that served (%d of %d), none with one does (%d%s)'
      % (len(saidKept), len(needKept), len(wrongKept), (': ' + ', '.join(wrongKept)) if wrongKept else ''),
      len(saidKept) == len(needKept) and not wrongKept)
sys.exit(1 if fails else 0)
PYEOF
rc=$?
[ $rc -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
exit $rc
