# LANE CLAMP: docs/LODGEN_TERRAIN_VT.md -- the ring is no longer a difference
# between the two paths.  Prose only; the provenance footer is re-derived last,
# by p05_anchors.py, per the ww-contract-provenance skill.
import sys

PATH = 'docs/LODGEN_TERRAIN_VT.md'
raw = open(PATH, 'rb').read()
cr_before = raw.count(b'\r')
s = raw.decode('utf-8')


def repl(old, new):
    global s
    if s.count(old) != 1:
        print('anchor count %d: %r' % (s.count(old), old[:70])); sys.exit(1)
    s = s.replace(old, new)


old = '\n'.join([
    'Where it differs, and only there: **the pyramid bakes a one-cell ring around',
    'every tile**, so its AO march and its outer-ring normals have real data, where',
    'the per-chunk path clamps both at the chunk edge. The `.btr` file itself is',
    'untouched'])
new = '\n'.join([
    'It differed in one place until 2026-09-10, and that difference is now gone:',
    '**the pyramid bakes a one-cell ring around every tile**, so its AO march and',
    'its outer-ring normals had real data where the per-chunk path clamped both at',
    'the chunk edge. The chunk path bakes on the SAME ring now, through the same',
    '`lodgenTerrainFillRing` and the same `lodgenTerrainGridSample`, so the',
    'assembled colour and `_msn` sheets are byte-identical to a direct bake with',
    'the ground-cover tint ON as well as off (V9a and V9b in',
    '`tests/spells/lodgen_terrain_vt.sh`), and the step in the normal across a',
    'chunk seam fell from 4.07x the interior step to 2.75x east/west and from 3.78x',
    'to 2.87x north/south, with the interior control unmoved at 1.80 and 1.60',
    '(V9c, same file).',
    '',
    '**One sheet is still not identical between the two paths, and it is named**:',
    'the `_data` sheet\'s wetness channel is a flow accumulation over the WHOLE',
    'sample grid its baker is handed, and a tile\'s grid is not a chunk\'s, so no',
    'ring can make those two agree. The chunk baker therefore keeps the CHUNK\'s',
    'grid for `lodgenTerrainChannels` (`chgt`) while its normal and its AO march',
    'read the ring, and the harness says so instead of pinning a bar it cannot',
    'hold. Fixing it means giving wetness a domain that is not the bake unit --',
    'a separate track. The `.btr` file itself is untouched'])
repl(old, new)

out = s.encode('utf-8')
if out.count(b'\r') != cr_before:
    print('CR MOVED %d -> %d' % (cr_before, out.count(b'\r'))); sys.exit(1)
open(PATH, 'wb').write(out)
print('ok  CR %d, lines %d' % (cr_before, len(s.split('\n'))))
