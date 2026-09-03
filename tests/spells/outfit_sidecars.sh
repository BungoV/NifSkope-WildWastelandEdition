#!/bin/bash
#
# The FO4 outfit sidecars: .ssf segment ownership and .sclp bone scales.
#
# WHY THIS EXISTS
#
# Both writers turn data already in the NIF into a file the ENGINE reads, and
# every way of getting them wrong still produces valid-looking JSON.
#
# The .ssf's segment id is the sharp case. Its second byte has two plausible
# readings -- the subsegment's User Index, or its ordinal within the segment --
# which agree on most of the corpus and disagree on OutfitM's left arm, whose
# User Indices run 1, 2, 3, 160, 60. Only the ordinal is what
# BSGeometrySegmentFlagData::ApplyTo's lambda computes
# (SegmentStarts[segment] + 1 + ordinal, 1.10.155 @0x1b9eb50), so the shipped
# .ssf files are the external authority and this harness holds our output
# against them.
#
# WHAT IS MEASURED
#
#   1. .ssf generates for the sampled vanilla meshes (CLI rc 0)
#   2. every (shape, id) -> bone the SHIPPED .ssf asserts is reproduced, except
#      the ones vanilla authored as DISABLED, which are a decision the NIF does
#      not carry
#   3. no bone hash is left unresolved anywhere in the sample
#   4. CONTROL for 2: reading the second byte one subsegment along drops the
#      match rate from 56 of 56 to 22 of 56, which is what proves check 2 is not
#      vacuous. It does not drop to zero, and cannot: neighbouring subsegments
#      often belong to the same bone, so an off-by-one lands on the right name
#      about two times in five. The threshold is set against that, not against
#      an imagined clean failure.
#   5. .sclp identity table is 48 bones, vanilla's names in vanilla's order,
#      every scale exactly 1.0
#   6. .sclp measured against ITSELF is exactly 1.0
#   7. .sclp recovers a KNOWN scale: every vertex one bone influences is scaled
#      by 1.37 about that bone's own origin, which is exactly a 1.37 scale in
#      the bone's frame whatever its rotation, and must come back as 1.37
#   8. CONTROL for 7: a bone whose vertices were not touched stays at 1.0, so a
#      writer that scaled everything would fail
#
# WHAT IT DOES NOT MEASURE. Whether the game likes the files. No shipped outfit
# carries a body its .sclp can be re-derived from -- the "fitted" shapes in them
# are 58-vertex patches, not bodies -- so check 7 is a round trip through our
# own frame, not an external authority. The in-game gate is bungo's.
#
# NOTE ON PORTS: headless, no port needed.
#
# USAGE
#   bash tests/spells/outfit_sidecars.sh
#
# Check 7 spends about a minute: it rewrites 112 vertices one CLI call at a
# time, because `set` writes one field per invocation.

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
DATA="${DATA:-/e/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

pass=0; fail=0
ok()  { pass=$((pass+1)); echo "  ok   $1"; }
bad() { fail=$((fail+1)); echo "  FAIL $1"; }
check() { if [ "${1:-0}" = "1" ]; then ok "$2"; else bad "$2${3:+ -- $3}"; fi; }

winpath() { case "$1" in /[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;; *) printf '%s' "$1" ;; esac; }

[ -f "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 1; }
[ -d "$DATA" ] || { echo "no corpus at $DATA (set DATA=)"; exit 1; }

echo "== .ssf against the shipped files =="

# A spread: two bodies, full outfits, single-limb armour, a helmet, clothes.
SAMPLE="
Meshes/Actors/Character/CharacterAssets/MaleBody
Meshes/Actors/Character/CharacterAssets/FemaleBody
Meshes/Armor/ArmoredCoat/OutfitM
Meshes/Armor/ArmoredCoat/OutfitF
Meshes/Armor/CombatArmor/F_Arm_R
Meshes/Armor/CombatArmor/m_leg_mid_l
Meshes/Armor/ArmyFatigues/fatiguesm
Meshes/Armor/ArmyFatigues/fatiguesf
Meshes/Clothes/Suit/MOutfit
Meshes/Clothes/Wastelander/MOutfit
"

mkdir -p "$W/gen"
generated=0
for rel in $SAMPLE; do
	src="$DATA/$rel.nif"; van="$DATA/$rel.ssf"
	[ -f "$src" ] && [ -f "$van" ] || continue
	base="$(basename "$rel")"
	work="$W/gen/$base/Meshes"			# under Meshes\ so SSF File can anchor
	mkdir -p "$work"
	cp "$src" "$work/$base.nif"
	if "$NS" -no-gui cast "$(winpath "$work/$base.nif")" \
			-s "Rigging/Generate Segment File (.ssf)" \
			-o "$(winpath "$W/gen/$base/out.nif")" > "$W/gen/$base.log" 2>&1; then
		generated=$((generated+1))
	fi
	cp "$van" "$W/gen/$base.vanilla.ssf"
done
check "$([ "$generated" -ge 8 ] && echo 1 || echo 0)" "1. generated $generated sampled meshes"

if grep -lq "name a bone no block" "$W"/gen/*.log 2>/dev/null; then
	check 0 "3. every bone hash resolved" "$(grep -l 'name a bone no block' "$W"/gen/*.log | tr '\n' ' ')"
else
	check 1 "3. every bone hash resolved"
fi

"$PY" - "$W/gen" > "$W/compare.txt" 2>&1 <<'PYEOF'
import json, os, sys, glob

def load(path):
    with open(path, encoding='utf-8-sig') as fh:
        return json.load(fh) or {}

def pairs(doc, shift=0):
    """Every (shape, id, bone) a file asserts. shift emulates a wrong decode."""
    out = []
    for shape, body in doc.items():
        if not body:
            continue
        for delta in body.get('DeltaBones', []) or []:
            for value in delta['BoneDeltaList']:
                if shift:
                    seg, sub = value >> 16, (value >> 8) & 0xff
                    value = (seg << 16) | ((sub + shift) << 8)
                out.append((shape, value, delta['BoneName']))
    return out

root = sys.argv[1]
agree = miss = authored = 0
ctrl_agree = ctrl_total = 0
misses = []
for van in sorted(glob.glob(os.path.join(root, '*.vanilla.ssf'))):
    base = os.path.basename(van)[:-len('.vanilla.ssf')]
    ours_path = os.path.join(root, base, 'Meshes', base + '.ssf')
    if not os.path.exists(ours_path):
        continue
    ours = {(s, i): b for s, i, b in pairs(load(ours_path))}
    for shape, ident, bone in pairs(load(van)):
        if bone in ('DISABLED', 'Generic'):
            authored += 1          # an authored override the NIF cannot know
            continue
        if ours.get((shape, ident)) == bone:
            agree += 1
        else:
            miss += 1
            if len(misses) < 6:
                misses.append('%s %s 0x%06x want %s got %s'
                              % (base, shape, ident, bone, ours.get((shape, ident))))
    for shape, ident, bone in pairs(load(van), shift=1):
        if bone in ('DISABLED', 'Generic'):
            continue
        ctrl_total += 1
        if ours.get((shape, ident)) == bone:
            ctrl_agree += 1
print('agree', agree, 'miss', miss, 'authored', authored)
print('control', ctrl_agree, 'of', ctrl_total)
for m in misses:
    print('  miss:', m)
PYEOF
sed 's/^/     /' "$W/compare.txt"
AG=$(awk '/^agree/{print $2}' "$W/compare.txt"); MI=$(awk '/^agree/{print $4}' "$W/compare.txt")
CA=$(awk '/^control/{print $2}' "$W/compare.txt"); CT=$(awk '/^control/{print $4}' "$W/compare.txt")
check "$([ "${AG:-0}" -gt 20 ] && [ "${MI:-1}" -eq 0 ] && echo 1 || echo 0)" \
	"2. $AG shipped assignments reproduced, $MI missed"
check "$([ "${CT:-0}" -gt 0 ] && [ "${CA:-99}" -lt $(( ${CT:-2} / 2 )) ] && echo 1 || echo 0)" \
	"4. CONTROL: the shifted decode matches only $CA of $CT"

echo "== .sclp =="
mkdir -p "$W/sclp"
BODY="$DATA/Meshes/Actors/Character/CharacterAssets/MaleBody.nif"
cp "$BODY" "$W/sclp/reference.nif"; cp "$BODY" "$W/sclp/target.nif"
SHAPE=$("$NS" -no-gui list "$(winpath "$W/sclp/target.nif")" -t BSSubIndexTriShape 2>/dev/null \
	| head -1 | sed 's/^\[\([0-9]*\)\].*/\1/')

WW_SCLP_REFERENCE="" "$NS" -no-gui cast "$(winpath "$W/sclp/target.nif")" \
	-s "Rigging/Generate Bone Scale File (.sclp)..." -b "$SHAPE" \
	-o "$(winpath "$W/sclp/o1.nif")" > "$W/sclp/identity.log" 2>&1
read -r N FIRST LAST ONES <<< "$("$PY" - "$W/sclp/target.sclp" <<'PYEOF'
import json, sys
doc = json.load(open(sys.argv[1], encoding='utf-8-sig'))
ones = all(abs(e['Scale'][k] - 1.0) < 1e-9 for e in doc for k in 'xyz')
print(len(doc), doc[0]['Name'], doc[-1]['Name'], 'ones' if ones else 'NOT-ONES')
PYEOF
)"
check "$([ "${N:-0}" = "48" ] && [ "${FIRST:-}" = "Spine1_Rear_skin" ] \
	&& [ "${LAST:-}" = "LLeg_Calf_skin" ] && [ "${ONES:-}" = "ones" ] && echo 1 || echo 0)" \
	"5. identity table: $N bones, $FIRST .. $LAST, $ONES"

WW_SCLP_REFERENCE="$(winpath "$W/sclp/reference.nif")" "$NS" -no-gui cast \
	"$(winpath "$W/sclp/target.nif")" -s "Rigging/Generate Bone Scale File (.sclp)..." \
	-b "$SHAPE" -o "$(winpath "$W/sclp/o2.nif")" > "$W/sclp/self.log" 2>&1
WORST=$("$PY" - "$W/sclp/target.sclp" <<'PYEOF'
import json, sys
doc = json.load(open(sys.argv[1], encoding='utf-8-sig'))
print('%.9f' % max(abs(e['Scale'][k] - 1.0) for e in doc for k in 'xyz'))
PYEOF
)
check "$("$PY" -c "import sys;print(1 if float(sys.argv[1])<1e-6 else 0)" "$WORST")" \
	"6. self-reference is identity (worst deviation $WORST)"

"$PY" - "$NS" "$W/sclp" "$SHAPE" > "$W/sclp/synth.txt" 2>&1 <<'PYEOF'
import json, os, re, shutil, subprocess, sys
ns, work, shape = sys.argv[1], sys.argv[2], sys.argv[3]
BONE_SCALE = 1.37
ref = os.path.join(work, 'reference.nif')
tgt = os.path.join(work, 'target.nif')
shutil.copy(ref, tgt)

def run(*args):
    return subprocess.run([ns, '-no-gui'] + list(args), capture_output=True, text=True).stdout

verts, cur = [], None
for line in run('dump', ref, '-b', shape, '-f', 'Vertex Data', '-d', '3', '-n', '4000').splitlines():
    m = re.search(r"Vertex  <HalfVector3>  = X ([-\d.]+) Y ([-\d.]+) Z ([-\d.]+)", line)
    if m:
        cur = {'p': [float(m.group(i)) for i in (1, 2, 3)], 'w': [], 'i': []}
        verts.append(cur)
        continue
    m = re.search(r"Bone Weights  <hfloat>  = ([-\d.]+)", line)
    if m and cur:
        cur['w'].append(float(m.group(1)))
    m = re.search(r"Bone Indices  <byte>  = (\d+)", line)
    if m and cur:
        cur['i'].append(int(m.group(1)))

skin = run('get', ref, '-b', shape, '-f', 'Skin').strip()
ptrs = [int(v) for v in re.findall(r"Bones  <Ptr>  = (\d+)",
        run('dump', ref, '-b', skin, '-f', 'Bones', '-d', '1', '-n', '400'))]
nodes = {int(m.group(1)): (m.group(2), [float(x) for x in m.group(3).split(',')])
         for m in re.finditer(r"\[(\d+)\] NiNode '([^']*)' T=\(([^)]*)\)", run('world', ref))}

# A bone with enough vertices to fit and few enough to rewrite one call at a
# time, and a control bone at the other end of the body.
BONE = 'RArm_UpperFat_skin'
CONTROL = 'LLeg_Calf_skin'
index = next(i for i, p in enumerate(ptrs) if nodes.get(p, ('',))[0] == BONE)
origin = nodes[ptrs[index]][1]
touched = [k for k, v in enumerate(verts)
           if any(v['i'][j] == index and v['w'][j] > 0.001
                  for j in range(min(len(v['i']), len(v['w']))))]

# Scaling uniformly about the BONE'S OWN ORIGIN is exactly a uniform scale in
# the bone's frame -- the rotation drops out -- so the expected answer needs no
# knowledge of the frame the spell will use, which is the point.
source = tgt
for n, k in enumerate(touched):
    p = verts[k]['p']
    q = [origin[a] + BONE_SCALE * (p[a] - origin[a]) for a in range(3)]
    out = os.path.join(work, 'step%d.nif' % (n % 2))
    r = subprocess.run([ns, '-no-gui', 'set', source, '-b', shape,
                        '-f', 'Vertex Data/%d/Vertex' % k,
                        '-v', '%.6f,%.6f,%.6f' % tuple(q), '-o', out],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print('set failed at vertex', k, r.stdout, r.stderr)
        sys.exit(1)
    source = out
shutil.copy(source, tgt)

env = dict(os.environ)
env['WW_SCLP_REFERENCE'] = ref
subprocess.run([ns, '-no-gui', 'cast', tgt, '-s', 'Rigging/Generate Bone Scale File (.sclp)...',
                '-b', shape, '-o', os.path.join(work, 'o3.nif')],
               capture_output=True, text=True, env=env)
doc = {e['Name']: e['Scale'] for e in json.load(open(os.path.join(work, 'target.sclp'),
                                                    encoding='utf-8-sig'))}
got = doc[BONE]
ctl = doc[CONTROL]
worst = max(abs(got[a] - BONE_SCALE) for a in 'xyz')
ctlworst = max(abs(ctl[a] - 1.0) for a in 'xyz')
print('moved %d vertices of bone %s' % (len(touched), BONE))
print('recovered %.5f %.5f %.5f (want %.5f, worst error %.6f)'
      % (got['x'], got['y'], got['z'], BONE_SCALE, worst))
print('control %s %.5f %.5f %.5f (worst %.6f)'
      % (CONTROL, ctl['x'], ctl['y'], ctl['z'], ctlworst))
print('RESULT', 1 if worst < 1e-3 else 0, 1 if ctlworst < 1e-6 else 0)
PYEOF
sed 's/^/     /' "$W/sclp/synth.txt"
R7=$(awk '/^RESULT/{print $2}' "$W/sclp/synth.txt"); R8=$(awk '/^RESULT/{print $3}' "$W/sclp/synth.txt")
check "${R7:-0}" "7. a known 1.37 bone scale is recovered"
check "${R8:-0}" "8. CONTROL: an untouched bone stays at 1.0"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
