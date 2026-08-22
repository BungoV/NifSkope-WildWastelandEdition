#!/bin/bash
#
# Rebuild a broad random sample of vanilla meshes and hold every one against the
# original. The whole-game regression, in one command.
#
# WHY THIS EXISTS
#
# The Concord test set is 114 meshes of interior clutter. It found three crashes
# and a door that would not open -- and the door was only found because it is a
# door. Anything the sample does not contain, the sample cannot tell you about,
# and a defect class that only shows on one kind of object (an ANIMATED one, as
# the keyframed body state was) will sit there until something loads it.
#
# So this samples across folder families rather than across one cell: SetDressing,
# interiors, Props, architecture, Furniture, Actors and Weapons, taking only
# files that carry compiled collision. Nothing is shipped anywhere -- the rebuild
# goes to a scratch tree and is compared, not installed.
#
# WHAT IT REPORTS
#
#   collision_ab.py       the stored solids: vertex sets, real planes, convex
#                         radius, capsule ends, sphere centres, body filter
#   hkcompound_sweep.py   compound AABBs, shape header words, body rest state
#
# On 2026-08-22 with 90 per family: 630 rebuilt, 630 accepted, 576 with every
# solid identical to vanilla (worst 0.5 mm), and the 54 that differ were all
# already-known buckets -- 33 mixed compounds, 16 ragdolls, 6 face
# decompositions. Keep that as the baseline: a NEW bucket is the thing to look at.
#
# USAGE
#   bash tools/corpus_verify.sh [per-family] [work-dir]
#
# Takes about a minute per 20 files with three workers.

set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
VAN="${VAN:-E:/Tools/Fallout 4/DataUnpacked/Data/Meshes}"
PER="${1:-30}"
WORK="${2:-$(mktemp -d)}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -d "$VAN" ] || { echo "no vanilla meshes at $VAN"; exit 2; }
command -v python >/dev/null || { echo "python is needed"; exit 2; }

mkdir -p "$WORK/out"
echo "sampling $PER files per family from $VAN"

NIFROOT="$ROOT" python - "$VAN" "$PER" "$WORK/list.tsv" <<'PYEOF'
import os, random, sys
sys.path.insert(0, os.path.join(os.environ['NIFROOT'], 'tools'))
from hkmatrun import nif_blobs, Pack

van, per, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
BS = chr(92)
picked = []
for fam in ('SetDressing', 'interiors', 'Props', 'architecture',
            'Furniture', 'Actors', 'Weapons'):
    root = os.path.join(van, fam)
    if not os.path.isdir(root):
        continue
    found = []
    for d, _, fs in os.walk(root):
        found += [os.path.join(d, f) for f in fs if f.lower().endswith('.nif')]
        if len(found) > 4000:
            break
    random.seed(11)          # a FIXED seed: the same sample every run, so two
    random.shuffle(found)    # runs of this script are comparable
    kept = 0
    for p in found:
        if kept >= per:
            break
        try:
            if not any(Pack(b).of_class('hknpPhysicsSystemData')
                       for _, _, b in nif_blobs(p)
                       if b[:4] == bytes([0x57, 0xE0, 0xE0, 0x57])):
                continue
        except Exception:
            continue
        picked.append(os.path.relpath(p, van).replace(os.sep, BS))
        kept += 1
    print('  %-14s %d' % (fam, kept), file=sys.stderr)
with open(out, 'w') as fh:
    for p in picked:
        fh.write('1\t%s\n' % p)
print('  %d files with compiled collision' % len(picked), file=sys.stderr)
PYEOF

n=$(wc -l < "$WORK/list.tsv")
[ "$n" -gt 0 ] || { echo "sampled nothing"; exit 2; }

# three workers, each with its own manifest: the resume check greps THIS file, so
# sharing one would make each worker skip the others' rows
REBUILD="$ROOT/tools/rebuild_collision.sh"
split -n r/3 -d "$WORK/list.tsv" "$WORK/part_"
echo "rebuilding $n files (3 workers)"
for i in 0 1 2; do
	( MANIFEST="$WORK/man_$i.tsv" bash "$REBUILD" "$WORK/part_0$i" "$WORK/out" >/dev/null 2>&1 ) &
done
wait

cat "$WORK"/man_*.tsv | grep -v '^status' > "$WORK/manifest.tsv"
echo "  rebuilt: $(grep -c '^ok' "$WORK/manifest.tsv") ok, $(grep -vc '^ok' "$WORK/manifest.tsv") not"

echo
echo "=== stored solids vs vanilla"
python "$ROOT/tools/collision_ab.py" "$WORK/manifest.tsv" "$WORK/out/Meshes/" "$VAN/" | head -6
echo
echo "=== compounds, header words, body rest state"
python "$ROOT/tools/hkcompound_sweep.py" "$WORK/manifest.tsv" "$WORK/out/Meshes/" "$VAN/" | tail -4
echo
echo "work dir: $WORK"
