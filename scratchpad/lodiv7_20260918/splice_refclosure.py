p = 'tests/spells/lodi_v7_refuters.py'
s = open(p, encoding='utf-8', newline='').read()

ANCHOR = '# ------------------------------------------------- identity untouched, ids dense'
assert s.count(ANCHOR) == 1, s.count(ANCHOR)

NEW = '''# ---------------------------------------------------------------- refuter (e)
# THE CLOSURE. Director, 2026-09-18 17:4x: the group IS the identity the far
# shadow pass keys on, and that pass excludes self-shadowing BY IDENTITY, so a
# house left in two groups shadows its own walls. A union-find that missed one
# pair does not announce itself -- it just leaves two halves of one house in two
# ids -- so the map from placement to group has to be a function of the CONNECTED
# COMPONENT and of nothing else, never of the order the pair search visited
# neighbours in. That is a closure property and it is checkable: no two
# architecture placements whose world boxes lie within the tolerance may sit in
# different groups.
#
# The boxes are REBUILT here from the shipped bytes -- the .lodo mesh AABB, the
# instance record's quantised rotation, scale and position -- rather than read
# from a side channel the writer filled, because a box handed over by the writer
# would grade the writer against itself.
#
# TWO THINGS THIS REBUILD CANNOT READ, both stated out loud rather than guessed:
# the drawn mesh is `bases[baseId].rep[mnamSlot]` and `mnamSlot` is NOT in the
# instance record (it survives only as the header's `slotInstances` histogram),
# and the record's position, rotation and scale are quantised where the writer
# grouped on the unquantised source. The first is answered by measurement: on
# this bake no architecture base has more than ONE distinct rep mesh, so the slot
# cannot change which mesh is drawn, and the count of bases where it COULD is
# printed. Any such base is skipped and counted, never guessed at. The second is
# bounded: position quantisation is 16384/65535 = 0.25 u, scale 1/8192, rotation
# 15-bit smallest-three -- all far under the 16-unit tolerance -- and the gap of
# every cross-group pair is printed so a near-boundary case could not hide.
TOL = 16.0
NO_MESH = 0xFFFF


def one_mesh(bid):
    """The mesh this base draws, or None when the four rep slots disagree and
    the file does not say which one was used."""
    b = L['bases'][bid]
    d = {b['rep%d' % k] for k in range(4) if b['rep%d' % k] != NO_MESH}
    return d.pop() if len(d) == 1 else None


def world_box(i):
    r = inst[i]
    mid = one_mesh(r['baseId'])
    if mid is None or mid >= len(L['meshes']):
        return None
    me = L['meshes'][mid]
    m, sc = r['m'], r['scaleF']
    lo = [1e30] * 3
    hi = [-1e30] * 3
    for c in range(8):
        lp = [(me['aabbMin'][k] + (me['aabbExtent'][k] if (c >> k) & 1 else 0.0)) * sc
              for k in range(3)]
        wv = [m[0] * lp[0] + m[1] * lp[1] + m[2] * lp[2] + r['x'],
              m[3] * lp[0] + m[4] * lp[1] + m[5] * lp[2] + r['y'],
              m[6] * lp[0] + m[7] * lp[1] + m[8] * lp[2] + r['z']]
        for k in range(3):
            lo[k] = min(lo[k], wv[k])
            hi[k] = max(hi[k], wv[k])
    return lo, hi


boxes = {}
ambiguous = 0
for i in arch_idx:
    b = world_box(i)
    if b is None:
        ambiguous += 1
    else:
        boxes[i] = b


def closure(key):
    """Every architecture pair within TOL that `key` puts in different groups,
    with the gap that separates them. The 1024-unit hash is an accelerator, the
    same one the writer uses; a pair in no shared cell is further apart than TOL
    in X or Y and cannot touch."""
    grid = collections.defaultdict(list)
    for i, (lo, hi) in boxes.items():
        for gx in range(int(math.floor((lo[0] - TOL) / 1024.0)),
                        int(math.floor((hi[0] + TOL) / 1024.0)) + 1):
            for gy in range(int(math.floor((lo[1] - TOL) / 1024.0)),
                            int(math.floor((hi[1] + TOL) / 1024.0)) + 1):
                grid[(gx, gy)].append(i)
    seen = set()
    touch = 0
    cut = []
    for v in grid.values():
        for a in range(len(v)):
            for b2 in range(a + 1, len(v)):
                i, j = (v[a], v[b2]) if v[a] < v[b2] else (v[b2], v[a])
                if (i, j) in seen:
                    continue
                seen.add((i, j))
                # a component cut by a chunk line IS two groups -- the brief's rule
                if chunk_of[i] != chunk_of[j]:
                    continue
                A, B = boxes[i], boxes[j]
                gap = max(max(A[0][k] - B[1][k], B[0][k] - A[1][k]) for k in range(3))
                if gap <= TOL:
                    touch += 1
                    if key[i] != key[j]:
                        cut.append((i, j, gap))
    return len(seen), touch, cut


def shortname(i):
    return base_model[inst[i]['baseId']].replace('/', BS).split(BS)[-1]


pairs, touching, cut = closure(gk)
ok('(e) the group map is closed under touching', not cut and touching > 0,
   '%d of %d architecture boxes rebuilt from the shipped bytes (%d skipped: the '
   'base draws more than one distinct mesh and the file does not say which), '
   '%d pairs examined, %d touching within %.0f u inside one chunk, %d of those '
   'in DIFFERENT groups%s'
   % (len(boxes), len(arch_idx), ambiguous, pairs, touching, TOL, len(cut),
      '' if not cut else ' -- closest: %s vs %s at a gap of %.3f u'
      % (shortname(cut[0][0]), shortname(cut[0][1]),
         min(c[2] for c in cut))))
# RED CONTROL: the same closure run against grouping SWITCHED OFF, each placement
# keyed on itself. If the test cannot see a missed join it would report 0 here
# too, and a closure test that examined no pairs would pass vacuously -- so this
# control proves both that pairs exist and that a cut component is visible.
_, touch_off, cut_off = closure([(chunk_of[i], i) for i in range(n)])
red('(e) control: grouping disabled', not cut_off and touch_off > 0,
    'with every placement its own group the same closure reports %d touching '
    'pairs cut across %d groups -- a test blind to a missed join would report 0'
    % (touch_off, len(cut_off)))

'''

s = s.replace(ANCHOR, NEW + ANCHOR)
open(p, 'w', encoding='utf-8', newline='').write(s)
print('closure refuter (e) added')
