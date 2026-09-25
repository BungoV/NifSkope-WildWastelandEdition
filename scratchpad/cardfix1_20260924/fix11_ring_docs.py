# CARDFIX1 step 5 (IMPOSTORRING1), part 7: the ring card contract in the three docs, and the FO4CS
# reader's owed change (docs only -- FO4CS is not this lane's tree). All three files are LF-only.
ROOT = 'E:/Projects/NifskopeWWE-cardfix1/docs/'


def patch(name, edits):
    b = open(ROOT + name, 'rb').read()
    assert b.count(b'\r') == 0, name
    s = b.decode('utf-8')
    for old, new in edits:
        c = s.count(old)
        assert c == 1, (name, old[:70], c)
        s = s.replace(old, new)
    out = s.encode('utf-8')
    assert out.count(b'\r') == 0
    open(ROOT + name, 'wb').write(out)
    print('patched', name)


RING_LODM = '''### 3.2 A horizon-RING card (2026-09-24, lane CARDFIX1 step 5)

bungo, 2026-09-23 04:4x, RULED: *"for fo4cs use the convention was 22.5 degrees
per take"*. **Tree** cards are photographed at 16 azimuths, 22.5 degrees apart, at
elevation 0 -- not over the hemi-octahedral grid. The card bake driver makes the
ring the default for `CANDIDATES=trees` (`RING=16`); the empty-slot run keeps the
grid (`RING=0`), and either can be forced.

A ring card uses **the aggregate's own layout keys (section 3a)**, not a third
layout:

| key | ring card | grid card |
|---|---|---|
| `oct` | **ABSENT** | N |
| `views` | V (16) | absent |
| `grid` | `[V, 1]` | absent |
| `frameOffset` | `2·V` numbers, view `v` at `[2v]`, `[2v+1]` | `2·N²` |

Every other key (`frame`, `half`, `pad`, `gap`, `mips`, `center`, `depthSpan`,
`coverage`, `projection`, `conv`, `auxDiv`) means exactly what it means on a grid
card. Frame `v` occupies pixels `[v·frameW, (v+1)·frameW) × [0, frameH)` and was
photographed from

```
eye(v)   = ( cos φ, sin φ, 0 )          φ = 2π·v / V
right(v) = ( −sin φ, cos φ, 0 )         up(v) = ( 0, 0, 1 )
```

under the `spec1` convention. **A reader blends the TWO frames that bracket the
camera's azimuth**, `f = φ_cam / (2π) · V`, `v0 = floor(f) mod V`, `v1 = v0 + 1
mod V`, weights `1 − t` and `t` with `t = f − floor(f)`; elevation selects
nothing (there are no frames above the horizon; what that costs is measured in
`tests/spells/impostor_ring.sh`, row M). At the slider's crisp end the stronger
of the two is drawn alone.

**Why there is no `oct` key, and why the sheet is one row.** A reader that knows
only the grid then finds no grid and refuses the set BY THAT KEY'S NAME (the
NifSkope reader before this change: *"oct is 0, outside the bake's own 2..16"*).
A 4 × 4 packing of the same 16 frames would carry a square sheet and an `oct 4`
that every existing reader would accept -- and draw hemisphere views from
horizon photographs without a word. The one-row sheet cannot be taken for any
N × N grid. Its width is `V·frameW`: 4096 at a 256 tile, 8192 at 512, 16384 at
1024 (the Direct3D 11 texture limit -- the ring does not go past a 1024 tile).
At one tile it holds a quarter of an N8 sheet's pixels.

**Card arrays** (section 4) carry the same keys on the array object: a ring set
groups only with ring sets of its own sheet size (the group key gains `|ring`).
**The aggregate** (section 3a) composites from grid cards only; a ring set given
to it is refused by name and counted (`aggregate cards: refused N horizon-ring
set(s) by name`). Teaching the aggregate the ring is owed.

**The `.lodm` stays version 1.** A reader that does not know `views` on a card
refuses the set; nothing is misread. Version 2 is reserved for the sway channel's
model-authored amplitude (lane CARDFIX1 step 6).

'''

patch('LODGEN_LODM_FORMAT.md', [
    ('| `oct` | int | **frames per side, N.** The sheet is **N × N frames = N² views**',
     '| `oct` | int | **frames per side, N. ABSENT on a horizon-RING card (§3.2), which says `views` and `grid` instead.** The sheet is **N × N frames = N² views**'),
    ('| `frameOffset` | float[2·oct²] | **per-frame positioning.**',
     '| `frameOffset` | float[2·oct²] | (`2·views` on a ring card, §3.2) **per-frame positioning.**'),
    ('\n## 3a. `kind: "aggregate"`', '\n' + RING_LODM + '## 3a. `kind: "aggregate"`'),
])

RING_SHEET = '''### 2.1 The tree RING (2026-09-24)

Tree cards are not on this grid any more: they are 16 azimuths at elevation 0 in
ONE row, `views × 1` -- the aggregate's ring (§10.2) at 22.5 degrees, by bungo's
ruling of 2026-09-23. `docs/LODGEN_LODM_FORMAT.md` §3.2 is the contract: `views`
and `grid` instead of `oct`, frame `v` at `[v·frameW, (v+1)·frameW) × [0, frameH)`,
two neighbours blended by angle. Everything in §3 onward -- the gap, the padding,
the size ladders, per-frame positioning, the orthographic camera, the channels --
is the grid's, frame for frame. The bake: `WW_IMPOSTOR_RING=16` (it wins over
`WW_IMPOSTOR_OCT`); the driver: `RING=16`, the default for `CANDIDATES=trees`.

Measured against the N8 grid on the same model at the same tile: the grid's
horizon is 28 frames whose azimuths bunch toward the diagonals (per quadrant 0,
9.5, 21.8, 36.9, 53.1, 68.2, 80.5, 90 degrees; largest step 16.2), the ring's is
16 at a uniform 22.5. `tests/spells/impostor_ring.sh` prints both at the
in-between azimuths and at elevations 0/5/15/30/60.

---

## 3. Frame geometry'''

patch('LODGEN_CARD_SHEETS.md', [
    ('bakes at that fit.\n\n---\n\n## 3. Frame geometry', 'bakes at that fit.\n\n' + RING_SHEET),
    ('oct N frameW frameH halfW halfH cx cy cz depthSpan family base\n',
     'oct N frameW frameH halfW halfH cx cy cz depthSpan family base\n'
     'ring V frameW frameH ...     INSTEAD of the `oct` line on a horizon-ring bake\n'
     '                             (§2.1): the same fields, V frames in one row. An\n'
     '                             old lodgen finds no `oct` line and makes no set,\n'
     '                             rather than reading V as N\n'),
    ('lodm <candidate> <family|none|rejected> <diffuse>    one per textured shape\n',
     'lodm <candidate> <family|none|rejected> <diffuse>    one per textured shape\n'
     'ringview <v> <azim> <elev>   ring bakes only, one per frame: the camera the\n'
     '                             renderer HELD for frame v, in degrees, read back\n'
     '                             from the view at the moment it was drawn -- the\n'
     '                             echo impostor_ring.sh R1 checks against v x 360/V\n'),
])

patch('LODGEN_IMPOSTOR_SPEC.md', [
    ('Channel meaning is unchanged and the `.lodm` names files, not formats, so no\nversion moved.\n',
     'Channel meaning is unchanged and the `.lodm` names files, not formats, so no\nversion moved.\n\n'
     '**The tree ring, and what FO4CS\'s card reader must learn (2026-09-24, owed;\n'
     'FO4CS is built last).** Tree cards are now 16 azimuths at elevation 0, one\n'
     'row (`docs/LODGEN_LODM_FORMAT.md` §3.2). The `.lodm` says `views` 16 and\n'
     '`grid` [16,1] and has **no `oct`**. Until FO4CS reads that, a reader that\n'
     'requires `oct` must refuse the set by name -- never take `views` for N. The\n'
     'change, in order: (1) layout from `views`/`grid` when `oct` is absent, frame\n'
     '`v` at u `[v/V, (v+1)/V)`, v `[0, 1)`; (2) frame eye `(cos φ, sin φ, 0)`,\n'
     '`φ = 2πv/V`, right `(−sin φ, cos φ, 0)`, up `(0, 0, 1)`; (3) selection = the two\n'
     'frames bracketing the camera azimuth, weights `1 − t`, `t` by angle, no\n'
     'elevation term; the crisp end draws the stronger alone; (4) `frameOffset` is\n'
     '`2·V` long, view `v` at `[2v]`; (5) a `cardArray` carries the same keys.\n'
     'NifSkope\'s drawer (`src/gl/impostordraw.cpp`, `selectFrames` ring branch) is\n'
     'the reference.\n'),
])
