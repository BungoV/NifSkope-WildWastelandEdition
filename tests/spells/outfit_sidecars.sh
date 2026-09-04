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
#   9. the bytes match vanilla's conventions: both formats CRLF, .ssf ends in a
#      newline (497 of 497 shipped) and .sclp does not (0 of 54), neither has a
#      BOM
#  10. a generated .sclp is byte-identical to a shipped one once the numbers are
#      blanked -- same key order, same bone order, same indent, same punctuation
#  11. `segments` resolves each SUBSEGMENT's own shared entry on a vanilla mesh:
#      MaleBody's segment 2 is RArm_UpperArm x3 then RArm_ForeArm1
#  12. CONTROL for 11: the top-level segments have no bone, so 11 is not just
#      "every row has a name". Together they pin the defect that prompted them --
#      reading "Parent Array Index" as the entry's own shared row instead of its
#      parent's returned the PARENT's entry for every subsegment, which is
#      exactly bone "none" on all four of those rows.
#  13. an authored DISABLED in an .ssf that is already there SURVIVES a
#      regenerate -- F_Arm_R hides its inner shell that way, and the mesh's own
#      Bone IDs name a real bone underneath, so a writer that only read the mesh
#      would silently throw the decision away
#  14. regenerating is idempotent: three passes over the same pair are
#      byte-identical, so the carry-forward cannot drift a file it re-reads
#  15. the Issue Manager's sidecar check finds each fault and stays QUIET on a
#      healthy pair -- a checker that fires on everything is worse than none
#  16. and the fix it offers actually resolves the finding: generate, re-check,
#      nothing left. A button that exists is not a fix; this is the same bar the
#      Repairs button failed once already.
#
# NOT COVERED: the matching writer change. riggingWriteSegmentLayout now emits
# vanilla's convention (0xFFFFFFFF on a segment, the parent's row on a
# subsegment), but every path that calls it lives in the Rigging Manager dock,
# which a headless run cannot drive. It was checked by hand against MaleBody,
# OutfitM and Deathclaw. The reader no longer depends on that field either way.
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
done		# the generated trees stay: check 9 reads MaleBody's back
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

echo "== bytes match vanilla's conventions =="
VAN_SSF="$DATA/Meshes/Actors/Character/CharacterAssets/MaleBody.ssf"
VAN_SCLP="$DATA/Meshes/Armor/ArmyFatigues/FatiguesM.sclp"
OUR_SSF="$W/gen/MaleBody/Meshes/MaleBody.ssf"
OUR_SCLP="$W/sclp/target.sclp"
"$PY" - "$OUR_SSF" "$OUR_SCLP" "$VAN_SSF" "$VAN_SCLP" > "$W/bytes.txt" 2>&1 <<'PYEOF'
import re, sys
ours_ssf, ours_sclp, van_ssf, van_sclp = (open(p, 'rb').read() for p in sys.argv[1:5])

def conv(blob):
    return (b'\r\n' in blob, blob.endswith(b'\n'), blob[:3] == b'\xef\xbb\xbf')

def blank(blob):
    return re.sub(rb'-?\d+\.?\d*(?:[eE][-+]?\d+)?', b'N', blob)

print('ssf  ours', conv(ours_ssf), 'vanilla', conv(van_ssf))
print('sclp ours', conv(ours_sclp), 'vanilla', conv(van_sclp))
print('RESULT',
      1 if conv(ours_ssf) == conv(van_ssf) else 0,
      1 if conv(ours_sclp) == conv(van_sclp) else 0,
      1 if blank(ours_sclp) == blank(van_sclp) else 0)
PYEOF
sed 's/^/     /' "$W/bytes.txt"
B1=$(awk '/^RESULT/{print $2}' "$W/bytes.txt")
B2=$(awk '/^RESULT/{print $3}' "$W/bytes.txt")
B3=$(awk '/^RESULT/{print $4}' "$W/bytes.txt")
check "$([ "${B1:-0}" = "1" ] && [ "${B2:-0}" = "1" ] && echo 1 || echo 0)" \
	"9. line endings, trailing newline and BOM match vanilla in both formats"
check "${B3:-0}" "10. a generated .sclp is byte-identical to a shipped one, numbers aside"

echo "== segment reader, against a vanilla mesh =="
"$NS" -no-gui segments "$(winpath "$W/sclp/reference.nif")" > "$W/segments.txt" 2>&1
sed -n '/segment 2 /,/segment 3 /p' "$W/segments.txt" | sed 's/^/     /'
SUBS=$(sed -n '/segment 2 /,/segment 3 /p' "$W/segments.txt" | grep -c "sub [0-3] ")
OWNED=$(sed -n '/segment 2 /,/segment 3 /p' "$W/segments.txt" \
	| grep -cE "sub [0-3] .*bone (RArm_UpperArm|RArm_ForeArm1)")
UNOWNED=$(grep -cE "^  segment [0-9]+ .*bone none" "$W/segments.txt")
check "$([ "${SUBS:-0}" = "4" ] && [ "${OWNED:-0}" = "4" ] && echo 1 || echo 0)" \
	"11. all $SUBS subsegments of MaleBody segment 2 resolve to their own bone ($OWNED named)"
check "$([ "${UNOWNED:-0}" -ge 7 ] && echo 1 || echo 0)" \
	"12. CONTROL: the $UNOWNED top-level segments carry no bone, as vanilla writes them"

echo "== regenerating over an .ssf that is already there =="
PRES="$W/pres/Meshes"
mkdir -p "$PRES"
cp "$DATA/Meshes/Armor/CombatArmor/F_Arm_R.nif" "$PRES/F_Arm_R.nif"
cp "$DATA/Meshes/Armor/CombatArmor/F_Arm_R.ssf" "$PRES/F_Arm_R.ssf"
for attempt in 1 2 3; do
	"$NS" -no-gui cast "$(winpath "$PRES/F_Arm_R.nif")" \
		-s "Rigging/Generate Segment File (.ssf)" \
		-o "$(winpath "$W/pres/out.nif")" > "$W/pres/pass$attempt.log" 2>&1
	cp "$PRES/F_Arm_R.ssf" "$W/pres/pass$attempt.ssf"
done
"$PY" - "$W/pres/pass1.ssf" > "$W/pres/check.txt" 2>&1 <<'PYEOF'
import json, sys
doc = json.load(open(sys.argv[1], encoding='utf-8-sig'))
# vanilla hides 0x020100, the inner shell, while the mesh says RArm_UpperArm
hidden = [v for shape in doc.values() for d in shape.get('DeltaBones', [])
          if d['BoneName'] == 'DISABLED' for v in d['BoneDeltaList']]
print('hidden ids', [hex(v) for v in hidden])
print('RESULT', 1 if 0x020100 in hidden else 0)
PYEOF
sed 's/^/     /' "$W/pres/check.txt"
KEPT=$(awk '/^RESULT/{print $2}' "$W/pres/check.txt")
check "${KEPT:-0}" "13. the authored DISABLED on 0x020100 survives a regenerate"
if cmp -s "$W/pres/pass1.ssf" "$W/pres/pass2.ssf" \
	&& cmp -s "$W/pres/pass2.ssf" "$W/pres/pass3.ssf"; then
	check 1 "14. three regenerates are byte-identical"
else
	check 0 "14. three regenerates are byte-identical" "the carry-forward drifts"
fi

echo "== Issue Manager: the sidecar check =="
ISS="$W/issues"
mkdir -p "$ISS/absent" "$ISS/donor" "$ISS/orphan" "$ISS/healthy"
SRC_NIF="$DATA/Meshes/Armor/ArmoredCoat/OutfitM.nif"
SRC_SSF="$DATA/Meshes/Armor/ArmoredCoat/OutfitM.ssf"

# the .ssf is simply not there
cp "$SRC_NIF" "$ISS/absent/OutfitM.nif"
# a copied mesh still naming its donor's file
cp "$SRC_NIF" "$ISS/donor/MyOutfit.nif"; cp "$SRC_SSF" "$ISS/donor/MyOutfit.ssf"
# a shape the file has no key for
cp "$SRC_NIF" "$ISS/orphan/OutfitM.nif"
"$PY" - "$SRC_SSF" "$ISS/orphan/OutfitM.ssf" <<'PYEOF'
import json, sys
doc = json.load(open(sys.argv[1], encoding='utf-8-sig'))
doc['OutfitM_PW:0'] = doc.pop('OutfitM:0')      # the shape was renamed, the file was not
open(sys.argv[2], 'w', encoding='utf-8', newline='\r\n').write(json.dumps(doc, indent=3))
PYEOF
# and one that is fine
cp "$SRC_NIF" "$ISS/healthy/OutfitM.nif"; cp "$SRC_SSF" "$ISS/healthy/OutfitM.ssf"

sidecheck() { "$NS" -no-gui check "$(winpath "$1")" -t "Outfit Sidecars" 2>&1; }
A=$(sidecheck "$ISS/absent/OutfitM.nif" | grep -c "not beside this mesh")
B=$(sidecheck "$ISS/donor/MyOutfit.nif" | grep -c "still pointing at its donor")
C=$(sidecheck "$ISS/orphan/OutfitM.nif" | grep -c "finds no segment data for it")
H=$(sidecheck "$ISS/healthy/OutfitM.nif" | grep -c "no findings")
echo "     absent $A  donor $B  orphan $C  healthy-quiet $H"
check "$([ "${A:-0}" -ge 1 ] && [ "${B:-0}" -ge 1 ] && [ "${C:-0}" -ge 1 ] && [ "${H:-0}" -eq 1 ] \
	&& echo 1 || echo 0)" \
	"15. the check finds all three faults and stays quiet on a healthy pair"

# 16: the offered fix has to RESOLVE the finding, not merely exist.
resolved=0
for case in absent donor orphan; do
	nifname=OutfitM.nif
	[ "$case" = donor ] && nifname=MyOutfit.nif
	"$NS" -no-gui cast "$(winpath "$ISS/$case/$nifname")" \
		-s "Rigging/Generate Segment File (.ssf)" \
		-o "$(winpath "$ISS/$case/$nifname")" > "$ISS/$case.fix.log" 2>&1
	if sidecheck "$ISS/$case/$nifname" | grep -q "no findings"; then
		resolved=$((resolved+1))
	else
		echo "     $case still reports:"; sidecheck "$ISS/$case/$nifname" | sed 's/^/       /' | head -3
	fi
done
check "$([ "$resolved" -eq 3 ] && echo 1 || echo 0)" \
	"16. generating the .ssf resolves all $resolved of 3 findings"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
