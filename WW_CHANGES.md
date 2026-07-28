# NifSkope — Wild Wasteland Edition: Change Log

## 2026-07-28v — Polytope layout measured; encoder deliberately not started

The convex polytope's layout is now pinned: **76 vanilla polytopes, zero violations**
of every rule in TO_BE_IMPLEMENTED 4d-spec-polytope. Variable length, but everything
follows from the counts — vertices at +0x50 (a capsule's end points push its own out
to +0x70), `nv` always a multiple of 4, `np = roundup(nf, 4)`, the face array padded
to the same multiple, indices after that padding, total `align16(end)`. Face entries
are `u16 firstIndex, u8 numIndices, u8 minHalfAngle`, with `firstIndex` the running
sum and the counts summing to exactly `ni`.

**I stopped there rather than starting the encoder**, because measuring turned up
three pieces of content that are not yet understood, and an encoder built without
them would produce files that look right and are not:

- `minHalfAngle` is not a flags constant. It takes dozens of values with no
  clustering — a quantized angle. Capsules are the exception at a flat 4. It is now
  preserved through the decode as `HknpShape::faceAngles`, so a round-trip can carry
  it, but deriving it for *new* geometry needs Havok's quantization rule.
- The array padding has no single fill. Spare plane slots are `(0,0,1,0)` in 50
  cases, all-zero in 50, something else in 2 — unlike the capsule's, which is one
  sentinel corpus-wide. Harmless geometrically, fatal to a byte comparison.
- `hknpShapeMassProperties` still needs a writer: an hkPackedVector3 encoder, and
  for general hulls the plane-offset volume and inertia by halfspace intersection.

That is a better place to leave it than a half-built encoder. The layout is the
expensive part and it is now banked and validated; the remaining three are each a
self-contained question.

Self-tests green; capsules 819/819 and spheres 30/30 unchanged.

## 2026-07-28u — The sphere encoder: 30 of 30 byte-exact

`hknpEncodeSphereShape`, and `--roundtrip` now covers spheres too. **30 of 30
byte-exact — the whole 128-byte object, not just its structure.**

That stronger result is not better work than the capsule's, it is a simpler object.
A sphere derives *nothing*: it stores a centre and a radius, and every other byte is
one value corpus-wide. There is no core box, so no padding and no roll — the two
quantities that make a capsule unreproducible. Where the capsule's spec has to say
"take these from the decode or synthesize them", the sphere's says nothing at all.

The layout, measured over 23 spheres in the actor skeletons: 128 bytes, flag word
`11 01 00 01` at +0x10 (the capsule's is `c3 01 00 01`, so the low byte looks like a
shape-type tag), radius at +0x14, material at +0x18, one vertex array of four
entries at +0x40. The centre is repeated four times for SIMD and all four carry
index **0** in the w mantissa, not 0..3 — they are one vertex, not four corners.

Worth flagging for whoever writes the next primitive: a sphere has **no plane, face
or index arrays**, and its vertex payload begins at +0x40, exactly where a
polytope's plane descriptor sits. The decoder already documents that trap; the
encoder now has to respect it in the other direction.

Capsules unchanged at 819 / 819.

## 2026-07-28t — The capsule encoder, and three corrections it forced

`hknpEncodeCapsuleShape` writes the 432-byte object, and `collision <file>
--roundtrip` re-encodes every capsule in a file and checks it against the bytes it
came from. Over the 36 actor skeletons that carry them: **819 capsules, structure
byte-exact on all 819, worst vertex error 9.9e-07 m.**

The test reports two numbers on purpose. *Structure* — header, flag word, the four
hkRelArray descriptors, both end points, the index-tagged `w` components, the face
and index tables, the sentinels — must be byte-identical, and that is where a
misread layout shows up. *Geometry* is a distance, because byte-exactness is not
reachable: neither the core padding nor the roll about the axis is a function of
the stored parameters. One number covering both would let a real error hide inside
float noise.

**Three things yesterday's note got wrong, all caught by measuring rather than by
reasoning:**

- **The core is an OBB, not an AABB.** 07-28s called it `AABB(capA, capB)` padded
  by R/99, which is right only because the brahmin's 39 capsules are *all*
  axis-aligned. Across the corpus 195 of 778 are tilted, and the AABB reading
  misplaces their corners by up to **17 mm**, against 4.8e-07 m for the OBB. A
  sample that cannot distinguish two hypotheses will happily confirm the wrong one.
- **The frame is left-handed after all.** 07-28s recorded `u x v = -axis`, then a
  fresh script said right-handed and appeared to overturn it. Both were right about
  their own convention, and it is the convention that matters: with *bit set = +
  side*, it is `u x v = -e0`, 778 of 778, and bit 0 points toward **capA**.
- **`primRadius` was inflated by 0.35%, and the recorded reason was wrong too.**
  The old figure was not `convexRadius * 1.014288` but `1.0175`: the margin came
  from a closest-point-on-*segment* helper, and because the box overhangs the
  segment axially, the clamp folded that overshoot into what was meant to be a
  perpendicular distance — every corner reading `padding * sqrt(3)` instead of
  `padding * sqrt(2)`.

That last one deserves its own line, because I introduced it *this session* while
"fixing" the radius, and only the round-trip caught it. The ratio stayed a clean
constant across all 778 capsules, so every consistency check passed while the value
was 22% too large.

**What the radius should be is a real question, not a rounding detail.** The solid
is the core box offset by `convexRadius`, so its cross-section is a rounded
*square*: half-width `R + padding` facing a face, `R + padding*sqrt(2)` facing a
corner. One scalar cannot describe that. The circumscribed value never under-states
the solid, so no contact is missed. Measured A/B on the settle corpus, same command
throughout: inscribed 33/37 with Liberty Prime at **34.2 m/s**, circumscribed 34/37
at 3.11, the old accidental `sqrt(3)` 34/37 at 2.95. Liberty Prime moving 3 -> 34
on a 0.7% radius change says that rig is marginal; the fix for that is a
dissipation model, not a radius picked to hide it.

**Not reachable, and worth stating plainly.** Byte-exact reconstruction from
`(capA, capB, convexRadius)` is impossible, and that is measured rather than
assumed. `padding/radius` sits at 1/99 but scatters **1.6e-5 relative** — hundreds
of ULP, far beyond any rounding of a fixed formula. The roll is likewise not a
function of the axis: capsules whose axes agree exactly always agree on the roll
(34 of 34 groups), but capsules whose axes agree to **0.008 degrees** disagree, and
no ordering rule (argmin, argmax, cyclic) explains the split. Both are inherited
from the authored primitive, so the encoder takes them as optional inputs — fed
back from the decode to preserve a shape, synthesized deterministically for a new
one.

The 1/99 is itself most likely an artefact of that split: an authored outer radius
`Rout` divided as `convexRadius = 0.99*Rout` and `padding = 0.01*Rout` has exactly
that ratio.

Also decoded and now exposed: `HknpShape::coreVerts`, `corePadding` and
`rawOffset`. Plane order is `+u, -v, +v, -u, +e0, -e0` with `n.x + d = 0` on the
face — derived independently from the constant index table and from the stored
planes, agreeing on all 778.

Self-tests green; settle corpus 34/37, unchanged against baseline.

## 2026-07-28s — Capsule geometry solved; vertex ORDER is what is left

Prototyped the capsule encoder in Python and byte-diffed it against all 39 vanilla
capsules, which is the cheapest place to find out what is still unknown.

**The geometry rule is exact:** the core box is `AABB(capA, capB)` padded by
**R/99** on every axis — 0.01010101 measured on all three axes of all 39, 1/99 to
eight figures. That also corrects yesterday's note: the true outer radius is
`convexRadius * 1.010101`, not 1.014288. The larger figure came from measuring to a
box *corner*, which is the diagonal rather than the perpendicular half-width. The
kind of mistake that survives because both numbers look plausible.

**What is left is the ordering.** A capsule whose axis is Z reproduces all eight
vertices *exactly*; one whose axis is X comes out permuted. In both, bit 0 of the
vertex index tracks the capsule's own axis — so the box is built in a local frame
of axis-plus-two-perpendiculars and written into shape space, and matching vanilla
byte-for-byte needs Elric's rule for picking those perpendiculars.

This is not cosmetic: the 24-byte index table is a **constant** shared by all 778
vanilla capsules, so permuting vertices without permuting indices describes a
different solid. An encoder can either recover the basis rule, or emit its own
vertex order with a matching index table — geometrically identical, not
byte-identical, and validated by decoding the result and comparing geometry.

Both routes are written into TO_BE_IMPLEMENTED 4d-spec, along with a smaller trap:
some vertex components differ from a clean +-pad by 2 ULP, so even with the right
order a few values carry rounding from whatever order Elric computed them in.

## 2026-07-28r — The capsule encoder, fully specified

No code yet — the complete byte layout, written into TO_BE_IMPLEMENTED as 4d-spec
so the encoder is transcription rather than investigation.

Measured on the brahmin's 39 capsules, and every structural field is invariant:
the object is **always 432 bytes**, the four flag bytes at +0x10 are one value, and
the 24-byte face table and 24-byte index table are each a single distinct value —
matching 07-28d's finding across all 778 vanilla capsules. Those are constants to
embed verbatim.

Two things that were not previously pinned down:

**The value at +0x14 is the capsule radius**, and the 8-vertex hull is a *shrunk
core*, not the capsule. The real shape is that box Minkowski-summed with a sphere
of that radius, which is why the box is a sliver. Its dimensions are fixed ratios
of the radius, identical to five decimal places on all 39:

    half-width perpendicular to the axis = R * 0.014288      (= R / 69.99125)
    half-extent along the axis           = halfLength + R * 0.010101

That perpendicular ratio is exactly the Havok-to-game unit scale, which is
unlikely to be coincidence and worth understanding before hard-coding.

**Vertex w carries the vertex index** in the low mantissa byte of 0.5 — `00 00 00
3f`, `01 00 00 3f`, and so on — which is why the decoder's w handling looked odd.

Noted rather than changed: `primRadius` is decoded as `convexRadius + margin` with
the margin measured to a box *corner*, so it over-reads by about 0.5% (0.04522
stored against 0.04613 reported). The exact outer radius is `convexRadius *
1.014288`. It is the number the simulator collides with, so it is worth fixing, but
not worth churning a verified corpus for at the end of a session.

## 2026-07-28q — hknpShapeMassProperties decoded (the 4d blocker)

The object that gated the convex-polytope encoder. `simulate`/`collision` now
report every shape's volume, mass, centre of mass and inertia.

**Layout** (0x30 bytes: 16 of header, then `hkCompressedMassProperties`):

| offset | field |
|---|---|
| +0x10 | packed centre of mass |
| +0x18 | packed inertia diagonal |
| +0x20 | packed quaternion, major-axis frame (one constant value in every file seen) |
| +0x28 | float volume |
| +0x2c | float mass |

**`hkPackedVector3` is three int16 mantissas plus a shared power-of-two exponent**,
the exponent held in the fourth int16 as `(E + 96) << 7` — low seven bits zero
throughout. Each component is `i16 / 32768 * 2^E`. Proven by decoding the turret's
centres of mass and checking them against the same quantity computed from the hull
geometry: **8 of 8 to within 1e-5 m**. Reading that fourth slot as a half float is
the obvious guess, gives the right *direction* and a scale wrong by 4x or 16x, and
is exactly the sort of near-miss that reads as success.

**Three facts an encoder needs, all measured:**

- **Centre of mass is just the hull's centre of mass.** Exact on the turret's
  boxes and within 0.0004 m on the alien's 252-vertex and 64-vertex hulls.
- **Volume and inertia are computed with the face planes pushed out by the convex
  radius**, not on the rounded Minkowski sum. For a box that is the box grown by
  2r per edge, and it reproduces the stored volume to **0.000%** on all 8 turret
  shapes; the Steiner formula for a properly rounded box is out by up to 1.1%.
  Mass equals volume in every vanilla case, so density is 1.
- **The stored inertia is 1.5x the physical inertia.** Exactly — 8 shapes, 24
  components, ratio 1.5000 throughout, against the axis-aligned inertia of the
  expanded box at the stored mass. Why Havok scales it is not established; that it
  does is not in doubt, and `HknpShape::massInertia()` divides it back out.

Only convex polytopes carry these. The brahmin's 39 capsules and spheres have
**zero** such objects, which fits 07-28d's finding that a capsule is a fixed
template.

**Still open for a general encoder:** most vanilla polytopes are boxes (28 of 37
in a 400-file sample) and those are solved outright. General hulls — the alien
carries 252- and 64-vertex ones — need the plane-offset polytope built by halfspace
intersection before their volume and inertia can be computed exactly. Their
centres of mass already come out right.

## 2026-07-28p — Inverse inertia, named and encoded correctly

Paying off a debt recorded in 07-28f, before the ragdoll encoder inherits it.

`dyn_inertia +0x20` holds the **inverse** inertia diagonal, not the inertia. The
field has been called `inertia` since it was decoded; it is now `invInertia`, on
both `HknpBodyPhys` and `HknpSystem`. The evidence is threefold: +0x04 beside it is
plainly inverse mass (0.2, 0.05, 1.0 for 5, 20 and 1 kg); the reading makes the
tensor physical (the brahmin pelvis at 5 kg reads 4.16, giving I = 0.24 and a radius
near 0.22 m, where the other reading implies a 0.9 m pelvis); and the simulator
settled it from the far end, since reciprocating it made every ragdoll explode.

**Two genuine bugs followed from the name.**

`hknpencode`'s `dynamicInertia()` wrote the *true* tensor into that slot, and
computed its box fallback as `mass*(d^2+d^2)/12` — true inertia — straight into an
inverse field. Both now invert. This path only runs for dynamic bodies, which is
why nothing has misbehaved: anything written that way would read back with the two
quantities swapped.

At the NIF boundary the same confusion cancelled out and so hid itself.
`bhkRigidBody`'s **Inertia Tensor** field means the real tensor, and the decode was
filling it with the inverse; the encoder read it straight back, so a round trip was
self-consistent and wrong. Both ends now convert, which leaves the byte round trip
untouched — invert twice and you are where you started — while making the field a
user reads or edits mean what it says. The Collision Manager's physics panel shows
the true tensor for the same reason.

Verified: the solver is unchanged (36 of 37 settling, brahmin 0.91 m/s), and both
self-tests stay green.

## 2026-07-28o — Bone dragging works, verified without a window

Physics Sim mode will let you grab a ragdoll bone and drag it. The mechanic itself
is now tested: `simulate --drag <body>` pins a body, sweeps it round a circle at
the speed a hand moves, and reports whether the ragdoll follows or comes apart.

**184 drags across every ragdoll in the corpus — every fifth body of each — zero
divergences, worst joint separation 4.0 mm**, and sub-millimetre on everything but
the sentry parts kit. Dragging the root, a spine link or an extremity all behave.

This is the payoff from choosing XPBD in 07-28f: a drag needs no spring constant
and nothing to tune. Pin the body, put it where the cursor is, and the solver
resolves the rest — a pinned body simply has infinite mass for the substep. And
it needs no window to test, which is the point: only the mouse-ray plumbing is
left unverified, not the physics.

**The first version of the test was wrong, and worth recording.** It reported
0.41 m separations, and the instinct was to blame the solver. It pinned the root
*and* dragged a forearm a quarter of the ragdoll's height — asking the arm to span
further than an arm reaches. No solver satisfies that; it is a fact about arms. A
real drag grabs one body and lets the rest dangle, and `--drag` now does that
(0.41 m becomes 0.00015 m). The lesson is the recurring one this week: when a
measurement looks like a bug, check what the measurement is asking for.

### Rejected: alternating sweep direction

Joints are stored roughly parents-first, so a forward sweep carries a correction
from the pelvis outwards in one pass and from a grabbed hand *inwards* one joint
per pass. Alternating direction each iteration is textbook symmetric Gauss-Seidel
and should fix that. It measures worse — **32 of 37 settling against 36**, Liberty
Prime going from 1.5 m/s to 20.9 — and it was not needed once the test was fixed.
Reverted, and noted in the code so it is not tried a third time.

That is the sixth change this week that was principled, plausible and worse on
measurement. The corpus keeps earning its keep.

## 2026-07-28n — The scene bridge, and why it cannot be one transform

Groundwork for drawing the simulated pose (phase 3), done headlessly so the part
that can be verified is verified before any rendering code exists.

Each body's collision is drawn in its own node's space: the renderer's transform
for body i is `worldTrans(node_i)`, while the solver holds that body's rest pose in
the ragdoll's own space. The obvious bridge is to establish one ragdoll-to-scene
map, `worldTrans(node_i) * rest_i^-1`, which must be identical for every body —
one ragdoll, one scene.

**It is not.** Measured across the corpus, 11 of 37 models disagree by more than a
game unit:

| model | spread (game units) |
|---|---|
| Vertibird | **341.1** |
| Robot/SkeletonRef | 112.7 |
| Robot/sentry | 68.9 |
| Turret (standing) | 47.1 |
| Turret (workshop) | 41.1 |
| Turret (mounted) | 21.8 |
| Deathclaw | 14.3 |
| PowerArmor (x3) | 7.1 |
| MirelurkHunter | 1.5 |

The brahmin and the human agree to 0.0006 and 0.0003, and rotations agree to 1e-5
*everywhere* — it is purely translation. The turret's 47.1 game units is **0.672
Havok metres**, which is exactly the rest-pose pivot error 07-28h measured on that
same turret, so this is the same authoring inconsistency seen from the scene side.
`glnode.cpp` already says as much for stair helpers: the node transform is
authoritative for placement, cinfo's position is only a rest pose. This puts a
number on it across the whole corpus.

**So the viewport must not use a global map.** The formulation that works is
per-body relative motion:

    T_draw_i = worldTrans(node_i) * ( rest_i^-1 * sim_i )

Each body keeps its authoritative scene placement, and only how far the solver has
moved it since rest is applied. At rest the bracket is the identity, so the
simulated draw is byte-for-byte the static draw — the property worth having, and
one that holds however far the two disagree. A global map would have put the
Vertibird 341 game units (about 4.9 m) from where it belongs.

`simulate` reports the spread, so a model whose scene and packfile disagree is
visible before anyone wonders why its ragdoll is offset.

## 2026-07-28m — The exact angular solve is worse (negative result, reverted)

07-28l predicted the eyebot's remaining 17.2 m/s would fall to solving the angular
constraint as a 3-vector instead of per-axis. **It does fix the eyebot, and it is
worse overall.** Reverted; the corpus stays at 36 of 37 settling. Recorded because
the change is a textbook correction that anyone reading `applyAngular` will propose
again.

The maths is not in doubt. The relative rotation two bodies pick up from an angular
impulse `p` is `(Ia^-1 + Ib^-1) p`, so the impulse that removes a correction
exactly is `p = (Ia^-1 + Ib^-1)^-1 corr` — a 3x3 solve. What the code does instead
is project onto the correction's own axis,
`p = n * theta / (n.(Ia^-1 + Ib^-1).n)`, which is a Rayleigh quotient: exact for
isotropic inertia, and wrong in proportion to the anisotropy, because `I^-1 p` is
not parallel to `p`.

Measured (parts kits reduced to their real ragdoll, ten seconds, speed at the end):

| | settled | notes |
|---|---|---|
| per-axis (kept) | **36 / 37** | eyebot 17.2 m/s, the only failure |
| exact 3x3 everywhere | 29 / 37 | eyebot fixed; mosquito 420 m/s, mirelurk queen 136, bloatfly 66 |
| exact 3x3 for lopsided bodies only | 32 / 37 | mosquito still 395 — insects have thin limbs, so they are lopsided too |

**Why exact is worse.** The per-axis projection satisfies the constraint along
`corr` and leaves a residue perpendicular to it. That residue is not free — it is
dissipative, and the light-bodied creatures were quietly relying on it. Removing
it exposes energy the projection had been absorbing. Gating the exact solve on
anisotropy does not rescue it either, because "lopsided inverse inertia" describes
a mosquito's leg as readily as an eyebot's antenna: the 50:1 threshold that picks
out the antennae at 1200:1 also picks up half the insects in the corpus.

So the eyebot's hinges stay unsolved, and the honest description of the remaining
gap has changed: it is not "the angular solve is approximate" — it is that this
solver's stability partly rests on that approximation, and replacing it needs a
real dissipation model rather than a better linear algebra step. That is a bigger
piece of work than it looked from the outside, and it is one ragdoll out of 37.

## 2026-07-28l — Two of the three failures were not ragdolls at all

**36 of 37 settle.** The eyebot is the only vanilla ragdoll left that does not
come to rest, and worst penetration across the corpus is 0.08 mm.

07-28k blamed the last two failures on self-collision, on the strength of
`--no-self` fixing Robot/SkeletonRef. That was the right observation and the wrong
conclusion. **Those files are not one ragdoll.** A ragdoll is a tree, so it has
one fewer joint than it has bodies — and Robot/SkeletonRef has **48 bodies with 10
joints**, with 630 of its 1128 possible body pairs *already overlapping in the rest
pose*, because the interchangeable parts are authored stacked in the same space.
`skeletonSentryBodyPart` says it in the name: 15 of its 24 bodies are unjointed.

Pinning the bodies no joint touches settles both outright:

| | all bodies | actual ragdoll only |
|---|---|---|
| Robot/SkeletonRef | 9.0 m/s | **0.58** |
| Robot/skeletonSentryBodyPart | 72.4 m/s | **0.73** |

So there was never a self-collision bug to find here. Dropping 37 loose,
mutually interpenetrating spare parts produces a clattering heap, which is what
the data describes. `simulate` now reports the count — "bodies no joint touches (a
parts kit, not one ragdoll): 37 of 48" — and `--jointed-only` pins them, so the
distinction is visible instead of being read as instability. Three files in the
corpus are kits.

### A stale-build measurement, corrected

The 5°/10°/15° threshold sweep in 07-28k reported corpus-worst speeds of 34.6 /
20.1 / 46.3 and picked 10°. Those builds were stale — the sweep piped `make` to
`/dev/null`, so a build that did not run looked like a result. Re-measured with the
build verified each time:

| threshold | settled | corpus worst |
|---|---|---|
| 5° | **36 / 37** | 17.2 m/s |
| 10° | 35 / 37 | 10.0 m/s |
| 15° | 35 / 37 | 41.3 m/s |

5° is now the setting: it leaves the eyebot as the single failure, where 10°
improves the eyebot but pushes the mirelurk hunter just over the line. This is the
third stale-build trap in this repo; builds are verified before measuring now
rather than silenced.

### The one real remaining failure

The eyebot, at 17.2 m/s. It has no loose bodies and no rest-pose violations; it is
simply the corpus's hardest case — seven hinge joints on antennae whose inverse
inertia is **4417 along their own axis against 3.67 across it**, a ratio of 1200.
With that anisotropy the angular response to a correction points nowhere near the
correction axis, which is inherent to the single-axis formulation rather than a
mistake in it. Fixing it properly means solving the angular constraint as a
3-vector rather than per-axis.

## 2026-07-28k — Adaptive solver passes: the stiff hinges converge

The workshop turret is fixed — from 60 m/s to a clean settle — and the eyebot
drops from 87 m/s to 20. The worst-behaved body in the entire corpus went from
**87 m/s to 20**, with 32 of 37 ragdolls at rest and worst penetration 0.1 mm.

07-28j left two hinge models diverging and established that 16 solver sweeps fixed
them while breaking the sentry. The reason a global count cannot work: a shoulder
needs one pass and the turret's pelvis hinge, limited to **±1°**, needs several.
Sweeps are now spent per joint, and only while that joint is still visibly
violated — a joint doing its job stops after one pass and costs exactly what it
did before. That is the property the earlier attempts lacked: they paid for the
stiff joints by disturbing the thirty-odd ragdolls that were already correct.

**The distinction that made it work** is between a joint *resting on* its limit
and one being *driven through* it. Those are not the same thing, and a ragdoll in
motion has limits firing constantly. The first version treated any firing limit as
distress, spent eight passes on each, and unsettled five animals that had been
fine — Dogmeat, the FEV hound, a mirelurk hunter, a radscorpion and the behemoth.
`limitAngle` now returns *how far* out of range the angle was, and only a genuine
excess earns extra work.

Ten degrees is that line, and it is measured rather than chosen: across the corpus
the worst body ends at 34.6 m/s with a 5° threshold, **20.1 at 10°**, and 46.3 at
15°, with 32 of 37 settling in all three cases. Too tight and healthy joints are
treated as sick; too loose and the stuck ones get no help.

### Still moving: 3

Robot/SkeletonRef (9.0 m/s) and the sentry (2.8) are **self-collision** —
SkeletonRef drops to 0.11 m/s with `--no-self`, and body-body is still exact only
for single capsules and spheres, so compounds are the suspect. The eyebot (20) is
still hinge-limited; its antennae carry an inverse inertia of 4417 along their own
axis against 3.67 across it, so it is the hardest case in the corpus by a wide
margin. Liberty Prime (1.7) and the mirelurk hunter (1.1) are essentially settled.

## 2026-07-28j — Ragdolls settle the way the file says they should

**32 of 37** vanilla ragdolls now come to rest — under 1 m/s after ten seconds of
falling onto a plane — with nothing diverging and worst penetration 0.04 mm.

**Per-body damping was decoded all along and never used.** `dyn_motion` +0x18 and
+0x1C hold linear and angular damping, typically 0.1 and 0.05, and they are what
brings a ragdoll to rest in game rather than leaving it swinging. Using them took
settling from 27-29 to 32 of 37. The synthetic self-test rigs explicitly zero
them, since their whole purpose is conserved energy.

**The measurement was as much of a problem as the solver.** Speed at exactly the
five-second mark, on a chaotic falling body, is a noisy number to steer by — and
several earlier decisions were steered by it. Ten seconds plus real damping gives
a signal that actually distinguishes "settled" from "still going".

### Two plausible fixes the corpus rejected

Both are recorded because each looks obviously correct and neither is:

- **More solver sweeps.** 16 instead of 4 fixed the workshop turret outright
  (66 m/s → 0.02) and took the sentry from 8.7 m/s to 75. Net: 25 of 37 settling
  against 29. Per-model testing said yes; the corpus said no.
- **Scaling by corrections rather than joints.** `solverScale` divides a body's
  response by how many joints touch it. Weighting each joint by the corrections it
  really applies — a hinge aligns the axles *and* bounds the swing, so it is worth
  three — is strictly more accurate and measures worse: 30 of 37, with the turret
  going from 60 m/s to 149. Softening that hard leaves each sweep barely
  correcting, and the residual becomes velocity.

### Still moving: 4

Eyebot (87 m/s), workshop turret (60), Robot/SkeletonRef (9.0), sentry (4.1).
Liberty Prime at 1.2 m/s is essentially settled. Bisected with `--no-limits`,
`--no-self` and `--only-limit`:

- **Hinges** — eyebot and workshop turret. The turret is decisive: full run,
  `--no-self` and `--only-limit hinge` all give *identically* 66.1097, and
  `--no-limits` gives 0.008. Its Pelvis hinge is limited to ±1°, i.e. welded.
  This is a convergence problem, not a broken formulation — 16 sweeps fix it — but
  no global sweep count or scaling rule tried so far fixes it without breaking
  something else. Per-joint adaptive iteration is the obvious next thing.
- **Self-collision** — Robot/SkeletonRef goes from 23.6 m/s to 0.11 with
  `--no-self`. Body-body is still exact only for single capsules and spheres, so
  compounds are the suspect.

RadStag is off the list: at 0.30 m/s in the full configuration it was fine, and
the earlier reading came from a stale build.

## 2026-07-28i — A body is not one shape (and the machines were falling, not exploding)

**This corrects 07-28g and 07-28h.** Those entries attributed the machine
ragdolls' energy to hinge limits and to constraint data authored against a
different bind pose. That diagnosis was largely wrong. Two ordinary bugs in this
code accounted for most of it.

**They were in free fall.** The turret's kinetic energy climbed steadily while its
joint separation stayed at 1e-6 and its contact count stayed at zero — and after
five seconds its fastest body was doing 48.6 m/s, against 9.81 x 5 = 49. It was
not unstable. It was falling through the floor at exactly the rate gravity
predicts, because collision only ever handled capsules and spheres and every
machine is a convex polytope. Energy climbing is what falling looks like.

**A body carries several shapes, and the build kept only the last.** Liberty
Prime's decode reports *14 shapes across 12 bodies* and the workshop turret *6
across 3*; its body 1 alone is four polytopes. The build loop assigned rather than
accumulated, so most of a machine's geometry was discarded — taking its centre of
mass with it, which reintroduced the parallel-axis error 07-28f exists to remove.
A body is now the union of its shapes, reduced to spheres in bone space: a capsule
contributes both end points at its radius, a sphere its centre, a polytope its
vertices. That reproduces all three previous cases exactly and generalises.

**Contacts need the same sharing as joints.** A box landing flat puts eight
vertices through the floor, and eight full corrections lift it eight times as far
as one — the sentry left the ground at 24 m/s that way. Splitting each body's
contact correction by its number of touching points is the same remedy as
`solverScale`, applied to contacts.

Results, measured by **speed** rather than energy — energy scales with mass, and
Liberty Prime massing tens of tonnes reads as a blow-up next to a cat while moving
no faster:

| | before | after |
|---|---|---|
| Turret (standing) | 83,157 energy | settles, 1.0 |
| Turret (mounted) | 51,443 | 0.0004 |
| Liberty Prime | 45.8 m/s (free fall) | 2.2 m/s |
| Sentry | 24.5 m/s | 2.4 m/s |

Corpus: **27 of 37 at rest** (< 1 m/s after 5 s), 6 still moving gently (1.0-2.4
m/s, which is a landed ragdoll rocking), **4 genuinely wrong** — eyebot, workshop
turret, Robot/SkeletonRef, radstag. Nothing diverges; worst penetration anywhere
is 0.05 mm.

**New: a contact self-test.** `simulate --selftest` now drops a 1 kg box on the
plane and requires it to come to rest without sinking. It settles at 0.0000 m/s
with penetration converging as h² (8.8e-5 / 2.2e-5 / 6e-6 / 1e-6 / 0 at 4 / 8 /
16 / 32 / 64 substeps). Energy conservation says nothing about contacts — they
dissipate — so this needed its own criterion.

Also corrected: the pose-reconstruction diagnostic added while chasing 07-28h
asked an invalid question. It derived each child's orientation by assuming the two
joint frames coincide, i.e. that every joint rests at its own zero. A ball socket
pins position and leaves all three rotational degrees free, so nothing requires
that, and it duly reported the deathclaw as 0.74 m and 50° out when the deathclaw
simulates perfectly. It now propagates position only, taking orientations from the
reference pose.

## 2026-07-28h — Why some ragdolls simulate hot: the file, not the solver

Chasing the turret's 0.67 m rest-pose joint error from 07-28g. It is not a decode
bug and not a solver bug. **Some constraints ship with their parent-side
transform never filled in** — the raw bytes hold an identity rotation and a zero
pivot, which asserts that a child body's origin coincides with its parent's. On
the turret that is 0.67 m from true, and the solver spent every substep failing to
satisfy it.

Reading the packfile directly settled it. Turret joint 1 (`hkpRagdollConstraintData`):

    transformA rows: [1,0,0] [0,1,0] [0,0,1]  pivot [0,0,0]
    transformB rows: [1,0,0] [0,1,0] [0,0,1]  pivot [0,0,0]

while joint 5, the same class in the same file, carries a real basis and a pivot
of (0.2876, -0.1219, 0). So it is per-object, not per-type.

Havok fills these in at setup from the bodies' current transforms
(`setInBodySpace`), so deriving the missing pivot from the rest pose is what the
engine would have done. Doing that took the corpus from **28 to 31 of 37**
ragdolls settling calm, and the turret's rest-pose joint error from 0.67 m to
1.7e-5. 58 joints across the corpus needed it. The count is reported by
`simulate`, and the frames are deliberately left alone — they drive the angular
limits, and inventing those would change authored behaviour that nothing here can
check.

### New: cinfo +0x40 is the body's orientation

Sitting directly after the position at +0x30, and it is a genuine rotation —
quaternion norm reads **1.00000** on every body of every model tested, with
non-trivial angles (90°, 117°, 180°, 120°).

That gives the body pose a **triple validation**. cinfo's position equals the bone
origin accumulated from `hkaSkeleton`'s reference pose *exactly*, and cinfo's
orientation matches the accumulated rotation to within **0.1°**, on the brahmin,
the eyebot and the turret alike. Three independent parts of the file agreeing that
closely means body placement is not where any remaining trouble lives — which is
precisely what let this hunt move on to the constraints.

### Still hot: 6 of 37, all machines

Eyebot, Liberty Prime, a sentry and three turrets. Two things are now known about
them and one is not:

- Their constraint data is inconsistent with their reference pose beyond the
  unset transforms above: turret joint 2 has a properly authored pivotB of
  (0.7009, -0.1592, 0.2011) and still misses the rest pose by 0.22 m. The bodies
  are where three sources agree they should be, so it is the constraints that
  disagree.
- Rebasing fixes the pivots but not the frames, and the frames drive the limits.
  The eyebot is at 1.8 units of energy with `--no-limits` and 7,300 with
  `--only-limit hinge`, so the residual is angular.
- What is NOT known is whether these ragdolls are simply authored in a different
  bind pose from the one the skeleton stores. That is the next thing to test, and
  it is a data question rather than a solver one.

New diagnostic: `simulate --trace` now lists every joint that does not hold in the
rest pose with its pivots and Havok class name, which is what made all of the
above visible in one run.

## 2026-07-28g — Collision: ragdolls now fall over and land

Phase 2 of the simulation work. Ragdolls collide with a ground plane and with
themselves, and `simulate --drop` lets one fall instead of hanging from its root.
Across all 37 vanilla ragdolls: **none diverge**, worst penetration anywhere is
**0.4 mm** and worst ball-socket separation **6.9 mm**.

The narrow phase is one function. Every ragdoll shape is a capsule or a sphere,
and both are "all points within r of a segment" — a sphere just has a zero-length
one — so closest-point-between-segments plus a radius comparison covers all three
pairings exactly. No GJK, no iteration, no tolerance to tune.

Two exclusions make self-collision usable. Jointed bodies always overlap where
they meet, by construction. And any pair **already overlapping in the rest pose**
is excluded permanently — the authored pose has a thigh inside a pelvis in
places, and a solver told to separate those would tear the ragdoll apart on frame
one (18 such pairs on the brahmin). Havok expresses the same intent with filter
groups, honoured first where the file sets them.

Broad phase runs once per step rather than per substep: a body moves a fraction
of a millimetre in a substep, so the candidate set cannot meaningfully change.
Contact geometry is still recomputed every substep, because a contact point frozen
at the start of a step pushes bodies in a direction they have already left.

**A third instance of the centre-of-mass bug.** 07-28f fixed it for capsules and
spheres; every turret, Liberty Prime and every prop is a convex polytope, which
fell through to a centre of mass of (0,0,0) — the bone origin — and inherited the
whole parallel-axis error. Averaging the hull vertices took Liberty Prime from
10,318 units of energy to 209 and halved the corpus-worst joint separation.

Also: correction rotations are capped at 0.2 rad, because the XPBD rotational
update is the *linearised* quaternion step and feeding it radians does not rotate
by radians. And the hinge limit now projects its swing axis perpendicular to the
axle before measuring — the same requirement twist and plane already had.

### Known open: 9 of 37 settle hot

They hold together and stay bounded, but carry more energy than they should. All
are machines — three turrets, the eyebot, Liberty Prime, a sentry — and there
are at least two distinct causes, neither yet isolated:

- **Hinges.** The eyebot and radstag are near-perfect with `--no-limits` (1.8 and
  0.7) and bad with `--only-limit hinge` (7,300 and 10,476). The eyebot has seven
  antenna hinges where the brahmin has two knees, and antennae carry an inverse
  inertia of 4417 along their own axis against 3.67 across it. With anisotropy
  like that the angular response to a correction is nowhere near the correction
  axis, which is inherent to the single-axis formulation rather than a typo.
- **A rest pose that does not match the joints.** The turret starts with a
  ball-socket separation of **0.67 m**, before a single step runs; every healthy
  ragdoll starts at 1e-6. That is a decode question, not a solver one — the
  decoded pivots genuinely disagree with the accumulated skeleton rest pose for
  these models — and it is recorded as such rather than tuned around.

## 2026-07-28f — The ragdoll solver works

Every vanilla ragdoll now simulates: **37 of 37** actor skeletons that carry a
jointed collision system settle, none diverge, and the worst ball-socket
separation anywhere in the corpus is **0.4 mm**. The brahmin went from 741,122
units of energy to **1.0**, and its joint error now converges as the substep
count rises — 8.5e-4 / 6.1e-5 / 6e-6 at 8 / 32 / 96 — which is the h² behaviour a
correct solver is supposed to show. (The four skeletons that do not simulate have
no ragdoll at all: two first-person rigs, CreateABot and Robot.)

Five separate faults, found by measurement rather than by reading the code:

**Bodies sat on the bone origin instead of the centre of mass.** The inertia
tensor is expressed about the centre of mass, and a limb bone's origin is its
joint — 0.13 m away. By the parallel axis theorem that understates the real
inertia about the bone origin by m*d², a factor of ~27 on the brahmin thigh, so
every correction over-rotated the bone and whipped its far end. The file has no
centre-of-mass field, so the shape centroid is used; the decoded tensor confirms
it, giving body 8 a 0.083 m radius and a 0.59 m length against a 0.428 m bone plus
a radius at each end.

**cinfo +0x30 is the body POSITION, not a centre of mass** (renamed accordingly).
All 39 brahmin entries equal the bone origin accumulated from `hkaSkeleton`'s
reference pose, to every decimal printed — which incidentally re-validates the
skeleton decode from an unrelated part of the file.

**Joint frames were transposed.** `hkRotation` stores columns; they were read as
rows. This left **22 of 38** joints violating their own limits in the rest pose,
with cone angles up to 169°. Conjugating drops that to 2, and both survivors are
honest: knee hinges limited to [-60°, -20°], which a neutral skeleton is
legitimately outside.

**The plane limit measured an angle that does not exist.** It took the angle from
a0 to b0 about b2, but neither is perpendicular to b2, so the reading mixed in the
cone angle and the two limits undid each other every substep. On its own it drove
the brahmin to 4.7 million; measured properly as the angle out of the parent's
plane it settles at 94.

**One Gauss-Seidel sweep per substep is not enough here.** A ragdoll's inverse
inertia runs into the hundreds, so a correction at one end of a bone swings the
other end by about half as much again; with five joints on one pelvis the
round-trip gain exceeds one. Mass splitting plus **four** sweeps fixes it. Both
numbers are measured on synthetic rigs, not guessed — see below.

Also: limit ranges stored min-above-max are swapped at build (unsatisfiable
otherwise, and `std::clamp` with lo above hi is undefined behaviour), and the
twist axis bails out when the two frames approach opposite rather than
normalising rounding error into a rotation axis.

### The self-test is the reason this was findable

`simulate --selftest` builds eight synthetic rigs whose total energy is a
conserved quantity, so drift is the solver's own error with no decode involved.
It earned its keep repeatedly:

- `pendulum` / `chain3` / `chain8` / `fork` / `heavy` / `spun` all stayed
  bounded while the real ragdoll exploded — which ruled out topology, inertia
  magnitude and rest rotation as causes, and pointed at the decoded data.
- `forkh` (a shared parent with a *realistic* inverse inertia) finally reproduced
  the blow-up in 6 bodies instead of 39: +1,142% energy, worse with more
  substeps. That is what identified the coupling problem.
- It then measured the fix: four sweeps take `forkh` to -1.6% and `chain8h` from
  +20,632% to -0.1%, both converging. Sixteen sweeps buy nothing over four.

One caution recorded honestly: the rigs' first version started with a sideways
shove and no angular velocity, which is not a state a pinned body can be in. The
solver projected the impossible part away in the first substep at a cost
independent of h, and that looked exactly like a substep-independent leak in the
solver. It was not. The rigs now start as a rigid rotation about the anchor.

New diagnostics: `--iterations`, `--only-limit <twist|cone|plane|hinge>` (which
is what isolated the plane bug), `--trace`, a rest-pose limit report, and a
runaway report naming the first body to exceed 50 m/s and the joints on it.

## 2026-07-28e — How Fallout 4 cloth collision works (investigation, no code)

Documentation only — nothing in the app reads this yet. Recorded because it is
expensive to rediscover and it drops straight into the existing `collision` CLI,
whose `--extract` already pulls the blob out verbatim.

`BSClothExtraData` is a Havok **Cloth (`hcl`)** packfile, the same container and
version as collision but with a **0x50 file header instead of 0x40** — which is
exactly why the collision parser desyncs on it. Section headers have to be found
by name, not offset.

Cloth collides against a small set of named capsules, one `hclCollidable` each:
an `hkTransform`, a shape pointer, and an inline `Collidable_<Bone>NNN` name. The
PrewarDress uses five — both thighs, both calves, the spine — mirrored L/R.

`hclTaperedCapsuleShape` is the interesting one, a cone frustum with no ragdoll
equivalent. Only `(A, B, radiusA, radiusB)` is authored; the rest of the object is
precomputed SIMD scratch. Verified rather than assumed: across 40 cloth meshes and
43 tapered capsules, the length, apex distance, apex position, cos and sin² all
reproduce **43 of 43** from those four values, the apex landing exactly on
`A - (rA*L/(rB-rA)) * axis`. One taper sign disagrees, most likely a reversed
`rB < rA` case.

Two traps recorded in TO_BE_IMPLEMENTED 4b: cloth is in **game units** where
ragdoll collision is in **Havok metres** (~70x apart), and the cloth `hkaSkeleton`
**keeps all 277 bone names** where ragdoll packfiles strip every one to null.

## 2026-07-28d — Shape encoding is a fixed template

A `hknpCapsuleShape` is not a primitive. It is a full convex polytope *plus* the
two exact end points — which is why the decoder recovers its radius from the hull
rather than reading it. The question for the encoder was whether that hull has to
be generated in general, and it does not: across all **778** vanilla capsules the
topology is identical — 8 verts, 8 planes, 6 faces, 24 indices, 432 bytes — and
the index table and face table are **byte-identical in all 778**. Both are
constants to embed verbatim.

Only the geometry varies, and it is a square-section box around the capsule axis:
all 8 hull verts are equidistant from the A–B segment in all 778, so the corners
are `±margin` perpendicular to the axis at each end. The margin is genuinely
per-capsule (0.000125 to 0.0186, 70 distinct values) and must be carried rather
than defaulted; `convexRadius` is the capsule radius minus it.

`hknpSphereShape` is 128 bytes with 4 identical SIMD-padded verts and no plane,
face or index arrays at all — which is precisely why reading those three fields
on a sphere returns vertex bytes, the bug fixed in 07-27ad.

Layouts in TO_BE_IMPLEMENTED item 6. With this the encoder's remaining unknown
is only `hknpShapeMassProperties`, which capsule-and-sphere ragdolls do not use.

## 2026-07-28c — The ragdoll root's full layout, and the class hashes

Groundwork for the encoder: every member of `hknpRagdollData`, not just the ones
the decoder reads. Table in TO_BE_IMPLEMENTED item 6. The two new ones are a
**pointer to `hkaSkeleton` at `+0x78`** (a global fixup, which is how the skeleton
is reached) and a **bone → body index array at `+0x80`**.

That second one had a trap in it worth recording, because it caught me. Read at
the body count it looks like an identity map with garbage on the end for two
ragdolls. It is not: its count lives at `+0x88` and is the **bone** count, and
bones and bodies are not the same number — `Robot/SkeletonRef` has 48 collision
bodies but only 11 ragdoll bones, `skeletonSentryBodyPart` 24 and 9. Read at its
own length it is the identity map in **all 35** vanilla ragdolls with zero
exceptions. An encoder emits identity of length `bones`; one that assumed
`bodies` would write past the end of the skeleton on any actor whose collision
carries loose parts.

Class-name hashes for the encoder's table are now read out of the vanilla files
rather than guessed — 17 classes, byte-identical across all 35 ragdolls.
`hkRefCountedProperties` comes back as `0x7c574867`, which is exactly what
`hknpencode.cpp` already emits, so the existing table and the corpus agree.

## 2026-07-28b — hkaSkeleton decodes, and the joint frames were labelled backwards

`hkaSkeleton` is a root object of its own beside `hknpRagdollData`, one per
ragdoll. `hkArray`s at `+0x18` parentIndices (`hkInt16`), `+0x28` bones (16 bytes
each) and `+0x38` referencePose (`hkQsTransform`, 48 bytes: translation, rotation,
scale); the four arrays after those are empty in every vanilla ragdoll. Now on
`HknpSystem::bones`, and `collision` prints the rest pose per bone.

**Bone index equals body index.** All **757** constraint bindings name the same
parent that `parentIndices` does — zero disagreements — and every ragdoll has
exactly one more bone than it has joints, rooted at index 0. Those are two
separately written arrays, so agreeing edge for edge is evidence, not restatement.

The `hkQsTransform` stride was confirmed the same way: all **792** vanilla bones
have a unit-quaternion rotation (worst deviation 3.6e-07) and a scale of exactly
`(1,1,1)`.

**Names are not in the packfile.** Every `hkaBone`'s name pointer is null and
carries no fixup — Bethesda strips them. They have to come from the NIF's node
list, which is what the existing body→node mapping already does.

Havok stores a quaternion `xyzw` and NifSkope's `Quat` is `wxyz`. Note that a unit
norm does *not* check a reorder, since a permutation preserves it; the reorder was
verified directly against the raw bytes instead, **792 of 792** correct.

### The frame labels were the wrong way round

The previous entry called constraint `+0x30` the parent side and `+0x70` the
child side. It is the other way round, and `referencePose` is what settled it: a
joint sits at the child bone's origin, so in the child's own space its pivot is
`(0,0,0)` and in the parent's space it is the child's local translation. Measured
over 755 joints, frame A (`+0x30`) has a zero pivot **98.8%** of the time and
frame B (`+0x70`) equals the child's rest position **93.9%** of the time. So **A
is the child side, B the parent side** — which also matches the binding entry's
own order, child at `+0x08` and parent at `+0x0c`.

No decoded value changes; `collision` was already printing the useful one. The
labels on `rotA`/`rotB` were wrong, and would have sent the encoder's transforms
to the wrong slots.

Cross-checking the two decodes against each other end to end: of 707 joints whose
child bone is identifiable, **659 have a parent-side pivot equal to the child's
rest translation**. The remaining 48 differ by small authored offsets (a knee
6 mm out, the human head 29 mm) — a joint's rotation centre need not sit exactly
on the bone origin.

### Encoder readiness, audited rather than assumed

By class, **99.5% of ragdoll packfile bytes are now read** (1,094,944 of
1,100,864 across the 35 vanilla ragdolls). The remaining 0.5% is three small
classes, and they are not equally hard: `hkpPositionConstraintMotor` (35 objects)
and `hkRefCountedProperties` (53) have **exactly one distinct byte pattern each**
across the whole corpus — Bethesda presets an encoder can emit verbatim — while
`hknpShapeMassProperties` has **43 distinct patterns in 53 objects** and is real
per-shape data that must be decoded or recomputed.

All three appear exactly 53 times, one for one with `hknpConvexPolytopeShape`:
capsule and sphere mass properties are analytic, so only polytopes carry
precomputed ones. Details in TO_BE_IMPLEMENTED item 6.

## 2026-07-28a — Ragdoll joint limits decode: the atom chain

The piece the previous entry deliberately left undone. A constraint object is a
flat run of **atoms** from `+0x20` to its end; each opens with a u16 type and
carries no length, so walking needs a type→size table. That table now exists and
is checked rather than assumed:

| type | atom | size |
|---|---|---|
| `0x02` | `SET_LOCAL_TRANSFORMS` | 144 |
| `0x05` | `BALL_SOCKET` | 16 |
| `0x0c` | `2D_ANG` | 16 |
| `0x0e` | `ANG_LIMIT` | 16 |
| `0x0f` | `TWIST_LIMIT` | 32 |
| `0x10` | `CONE_LIMIT` | 32 |
| `0x11` | `ANG_FRICTION` | 16 |
| `0x12` | `ANG_MOTOR` | 40 |
| `0x13` | `RAGDOLL_MOTOR` | 96 |
| `0x17` | `SETUP_STABILIZATION` | 16 |

`HknpConstraint` gains `twist` / `cone` / `plane` / `hinge` (`HknpAngLimit`:
min, max, tau), plus `friction`, `motorEnabled` and `breakable`. `collision`
prints them in degrees — nobody authors a ragdoll in radians.

**Why the table is right, not merely plausible.** A size wrong by even four bytes
desyncs the walk and turns every later type into garbage. With this table all
**757 constraint objects across all 35 vanilla ragdolls walk to their exact end**
hitting only known types, and each class yields exactly **one** atom sequence with
no variants: ragdolls are `transforms, setupStabilization, ragdollMotor,
angFriction, twistLimit, coneLimit, coneLimit, ballSocket` (518 of them), hinges
are `transforms, setupStabilization, angMotor, angFriction, angLimit, 2dAng,
ballSocket` (239).

The field offsets were then corroborated separately from the sizes:

- all **3068** decoded angles land in `[-pi, pi]`;
- the **518** `-100` "unlimited" sentinels are exactly one per ragdoll constraint,
  matching the single bound a cone limit does not have;
- the tau factor takes just **two** values across 1793 atoms — `0.8` on ragdoll
  limits, `1.0` on hinges. Bytes at a wrong offset do not fall into a 518/239 split.

End to end, through the shipped CLI rather than the scratch parser: of 224 L/R
bone pairs, **75% get identical or sub-degree-identical limits** and 83% agree
within 5°. The brahmin's hips are the nice case — `RLeg1` plane `-28.0..9.0`
against `LLeg1` `-9.0..28.0`, mirrored *asymmetric* ranges. Nothing in the decoder
knows about bone names. The 6% that differ by more are all clean authored values
(Liberty Prime's ankles are `0..90` against `-180..-90`, the same span from a
different zero), not the `1e38` a bad read produces.

### Breakable wrappers, and the last two joints

`hknpBreakableConstraintData` is a wrapper holding a pointer to the real `hkp*`
data. The decoder follows it — taking the first pointer in the object that lands
on an `hkp*ConstraintData` rather than hard-coding the member offset from two
samples. Both are the **Vertibird's doors**, hinges meant to snap off, with exact
X-mirror pivots. So joints decoding limits is now **757 of 757**, up from the
755/757 that decoded frames.

### Two vanilla joints have min/max stored backwards

Both in the human skeleton, at exactly `+5.00/-5.00` and `+0.10/-0.10` degrees —
the same magnitude constants used correctly elsewhere, just transposed. That is
what the file says, so it is reported as-is (`hinge 0.1..-0.1`) rather than
silently swapped.

Vanilla has **no** motorised joints: every motor atom's enabled byte is zero,
matching the raw bytes. Ragdoll motors are switched on at runtime, not authored.

## 2026-07-27af — Ragdoll joint frames decode

Each constraint carries two `hkTransform`s — `+0x30` for the parent side, `+0x70`
for the child — four `hkVector4` apiece: three basis rows then the pivot. Now on
`HknpConstraint`, and `collision` prints the child-side pivot per joint.

**Checked before being written, not after.** A 4x4 read at a correct offset has an
orthonormal basis with determinant +1; bytes at a wrong one essentially never do.
Across all 35 vanilla ragdolls, **1514 of 1518** constraint frames pass. The four
that fail are all `hknpBreakableConstraintData` — a different wrapper class my
filter caught by name — so the hkp* classes are **1514/1514**, and the decoder
now skips non-`hkp` classes rather than misreading them. 755 of 757 joints decode
frames; the 2 without are those breakable wrappers.

A second, independent sign the offset is right: the brahmin's `RLeg1` pivot reads
`(-0.000, 0.000, -0.302)` and `LLeg1` reads `(0.000, -0.000, 0.302)` — exact ±Z
mirrors for mirror-symmetric bones.

### Still missing: the angular limits

They sit in the Havok **atom chain** after the frames. Each atom begins with a u16
type and carries **no size field**, so walking it needs a type→size table.
Observed on the brahmin: `0x05`→16, `0x0e`→16, `0x0f`→32, `0x10`→32,
`0x11`→16, `0x12`→40, `0x13`→96, `0x17`→16 — inferred from **two** objects,
which is not enough to build on. Deliberately not implemented on that basis;
today already produced two bugs from plausible-looking wrong reads.

Values a correct decode should recover: tail-base twist ±0.087266 rad (±5°), cone
0.69813 (40°), knee hinge -0.2618..0.69813 (-15°..40°).

## 2026-07-27ae — The ragdoll joint graph decodes

`hknpRagdollData`'s array at `+0x50` is the constraint binding table — 0x18 bytes
an entry, one per non-root body:

```
+0x00  void*   constraint data   (global fixup)
+0x08  uint32  child body
+0x0c  uint32  parent body
+0x10  uint64  padding
```

Now on `HknpSystem::constraints`, and printed by `collision`.

### Why this is more than a guess

The body → node map and the bone parent tree both come from the **NIF**,
independently of the packfile, so checking one against the other is evidence
rather than restatement. On the brahmin, **all 38 bindings name a bone and its
nearest ancestor that has a body** — bodiless intermediates such as `LNeckHub`
are skipped, which a ragdoll has to do since it cannot constrain to a bone with
no collision. My first check called those six "mismatches"; the check was wrong,
not the data.

The kinds corroborate it anatomically. The eight `hkpLimitedHingeConstraintData`
entries are exactly the **two knees, two elbows, two wrists and two toes** — every
genuine hinge joint on the animal — with ball-and-socket everywhere else.

All 35 vanilla FO4 actor ragdolls decode: **757 joints, 237 of them hinges**.

### What is still missing

This is the joint **graph**, not the joint **parameters**. The
`hkpRagdollConstraintData` / `hkpLimitedHingeConstraintData` /
`hkpPositionConstraintMotor` objects hold pivots, axes and angular limits, and
none of that is read. An encoder built on the graph alone would emit joints with
no limits, which is its own kind of broken.

## 2026-07-27ad — hknpSphereShape decodes: every vanilla ragdoll is now complete

A sphere's vertex payload starts at `+0x30 + 0x10`, which **is** `+0x40` — exactly
where a polytope keeps its plane, face and index relArrays. Those three fields
were therefore vertex bytes read as counts: on the Deathclaw ragdoll `+0x44` read
**0x7f20 = 32544 faces**, the shared sanity check tripped, and
`decodeConvexLike()` returned before the sphere branch below it could ever run.
Every `hknpSphereShape` decoded to nothing.

Fixed by reading the vertex array first and the polytope-only arrays only after
the sphere and capsule branches have returned.

**Pass 2 now reports what it drops.** A body whose shape failed to decode
vanished without a trace — pass 3 records failures in `unknownShapes`, pass 2 did
not — and that is exactly how this stayed invisible: once 27p made bodies
resolve, spheres reached pass 2 and stopped being reported at all, so the symptom
degraded from "not decoded hknpSphereShape" to a silently missing shape.

Across all 35 vanilla FO4 actor ragdolls there are now **zero undecoded shape
classes**, and 30 have a shape for every body — the other five have *more* shapes
than bodies, which is the already-documented case of bodies that no collision
object names.

| | before | after |
|---|---|---|
| Deathclaw | 27 shapes / 28 bodies | **28 / 28** |
| Robot/SkeletonRef | 41 / 48 | **48 / 48** |
| skeletonSentryBodyPart | 23 / 24 | **24 / 24** |

Deathclaw's sphere reads r = 0.392881, matching the 0.39288 at `+0x14` in the raw
bytes.

Also closed the GUI check owed since 27o: the Collision Manager on the brahmin
skeleton lists one row per bone with Bone = "ragdoll bone", Shape = Capsule and
Layer = Biped (8), over 41 bodies.

## 2026-07-27ac — One toolbar icon size

The "large" 36px alternative is gone; 16px, matching Blender's header, is the
only size.

Removed rather than merely re-defaulted, because **its Settings checkbox was
never wired to anything** — nothing in the codebase read or wrote `largeIcons`,
and the size came straight from `Settings/Theme/Large Icons` with a hardcoded
default. It was a control that appeared to offer a choice and did not, which is
worse than no control. The checkbox, the `ToolbarSize` enum, the `toolbarSize`
member and the settings read all go with it.

Installs carrying `Large Icons = true` are unaffected in the sense that nothing
reads it any more — everyone gets 16px.

## 2026-07-27ab — The top bar was still 36px icons

Flattening the buttons (27z) removed the chrome but the bar still looked the
same size, and the reason is that it was: **`setToolbarSize()`'s "large" value is
36px and large is the DEFAULT.** The glyphs, not the boxes, were setting the
height. Blender's header icons sit around 16.

- Small is 16 to match Blender.
- Large comes down 36 → 24. Still generous, still there for high-DPI and
  accessibility, but 36 made one row of ~15 controls taller than the menu bar and
  the tab bar put together.
- The **default** is now Small. Installs with `Settings/Theme/Large Icons`
  already written keep their choice and simply get the saner 24.
- Dropped the construction-time `iconSize * 3 / 4` shrink: `setToolbarSize()`
  runs afterwards from `restoreUi()` and overwrote it anyway, so it was a second
  opinion that never applied and only misled.

Worth noting for anyone reading 27z: the flattening was still worth doing — it
is what freed ~115px of width — but it was answering the wrong half of the
question on its own.

## 2026-07-27aa — Slimmer Block List and Block Details

**Block List: eight filter chips → one dropdown.** They were already mutually
exclusive, which is what a menu expresses, and the row they occupied was the
third strip of chrome above the list — search, chips, breadcrumb — before a
single block was visible. The button wears the active category's icon, so the
current filter stays readable without spending a row on it. `QButtonGroup`
becomes a `QActionGroup`, and the old `idClicked` lambda becomes
`setBlockListQuickFilter()` so the reset-to-All path shares it.

**Block Details: the Type column is off by default**, toggled from the header's
context menu. It was a permanent ~90px showing `Ref<NiTimeController>`-style
text — useful when authoring structure, not when editing values, which is the
common case by a wide margin. Value gets the room back.

One catch worth recording: setting the column hidden at construction does
nothing, because `restoreUi()` later calls `tree->header()->restoreState()`,
which carries its own column visibility and wins. The default has to be applied
*after* the restore.

**Not done: dropping the `[n]` suffix in the Block List Value column.** I
suggested it on the belief that it repeats the block number already in the Name
column, and could not confirm where it is produced — `NifModel::topItemRepr()`
has exactly that format but feeds item *paths* for diagnostics, not the Value
column. Left alone rather than changed on a guess.

## 2026-07-27z — Row icons were pinned to the top of the row, not centred

`NifDelegate::decoRect()` returned `QRect( opt.rect.topLeft(), opt.decorationSize )`
— comment and all: *"allways upper left"*. Anchoring at the row's top means an
icon shorter than the row sits high by half the difference. Rows in the Block
List are 20 px against a 16 px decoration, so every icon was **2 px above** where
it belonged while the text beside it centred itself.

Now centred vertically in the row. Computed from the row height rather than
hardcoded, so it stays correct across font size, DPI and style changes.

Measured on the `NiNode` row, before and after:

| | icon centre | text centre | offset |
|---|---|---|---|
| before | y 219.5 | y 222 | **2.5 px high** |
| after | y 221.5 | y 222 | 0.5 px |

The half pixel left is rounding on a 6 px dot in a 20 px row, not a bias.

This delegate serves the Block List, Block Details, the Header view and the KFM
tree, so it corrects icon alignment in all four. Render regression not run:
`nifdelegate.cpp` is item-view painting and no draw path touches it.

## 2026-07-27y — The Scene nodes filter chip uses the node dot

The Block List drew one category two ways: the tree row had the themed dot while
the "Scene nodes" filter chip still used `:/btn/blockNode`.

Factored the dot into `wwNodeCategoryIcon()` and pointed both at it, rather than
copying the drawing into the chip — same function, so they cannot drift.

Scope is deliberately just this one category. The earlier attempt to tint *every*
Block List icon (27w) was reverted: the ask was to make the node mark match the
skin, not to strip the colour out of the whole list. Geometry, material, texture,
collision, particles, light, camera, skin, animation and extra data keep their
stock art in both the tree and the chip row.

## 2026-07-27x — Animation transport on the View toolbar

Play/pause, sequence, loop, more, and a mini timeline, at the head of the View
toolbar — reachable without opening the Timeline dock or switching to the
Animation workspace. The common case is "play this and look at it", which should
not cost a layout change.

It drives the **existing** actions (`aAnimPlay` / `aAnimLoop` / `aAnimSwitch`) and
`Scene::animGroups` rather than keeping parallel state, so this, the Timeline
dock's transport and Space in the viewport cannot disagree.

- **Play/Pause** — the glyph shows what the button *does*, not what it is
- **Sequence** — icon-only dropdown of `animGroups`, current one checked
- **Loop**
- **More** — Play Reversed, cycle through sequences, speed (0.25–4x), and a
  toggle for the Timeline dock
- **Mini timeline** — 64 px scrubber over `[timeMin, timeMax]`

### Reverse playback did not exist

`advanceGears()` did a bare `time += dT`. It now scales by a new
`GLView::animSpeed`, which may be negative, and **both** ends carry the wrap
logic — a sequence run backwards finishes at `timeMin`. Written once and applied
to whichever end the current direction is heading for, so loop and cycle cannot
behave differently depending on the sign. Reverse is the sign on the rate, so it
and the speed choice share one group and cannot contradict each other.

### Two bugs found while building it

- **Gating on `animGroups` was wrong.** Plenty of files animate through
  standalone controllers with no named `NiControllerSequence` — every NiPSys
  effect in `Meshes/Effects` is like this, and they are exactly what someone
  opens to watch something move. The transport was greyed out on all of them.
  Animatable is a **time range**: `timeMax() > timeMin()`. The sequence *button*
  still needs a non-empty list, since with nothing to choose it would open an
  empty menu.
- **`setEnabled()` on a `QToolButton` with a `defaultAction` does nothing** — the
  button mirrors the action's state. Play and Loop stayed live with no animation
  until the actions themselves were disabled.

### Fitting it in

At 1920 px the toolbar had no room: the first version pushed **Panels and
Workspaces off the end**. The sequence button's text label was the expensive part,
so it is icon-only with the name in its tooltip and checked in its menu. Both
buttons survive now.

Greying needed doing by hand as well. Qt's own disabled-icon fade measured 232 vs
209 peak brightness — about 10%, which does not read as unavailable — and
`wwBoxedButtonQss` colours text, useless for icon-only buttons. The glyphs are
generated, so they are now generated in `textMuted` when disabled: **176 vs 235**.

Verified by measurement on both states, with an animated file and without.
Render regression 7/7 identical, including the time-driven `particles_mist`.

## 2026-07-27v — Block Details was bound to the empty model at startup

Reported: no block details, whatever is selected. **A regression from 27q**, mine.

`onLoadBegin()` and `onLoadComplete()` each call `swapModels()`, which **toggles**
the views between the real models and the empty ones that keep them quiet during
a load. The starter-scene path emitted only `completeLoading` — added in 27q to
fix a stale camera — so that toggle ran an **odd** number of times and left
`tree`, `header`, `list` and `kfmtree` bound to `nifEmpty`.

The tell was in the screenshot: Block Details showed a header reading **version
20.0.0.5, User Version 11, Num Blocks 0** for a document that is 20.2.0.7 / 12
with 4 blocks. Not a wrong root — a different model. Selection was never broken;
`setRootIndex()` was being called on a tree pointing at the wrong model.

Fix: emit `beginLoading` before building and `completeLoading` after, in that
order, exactly as `loadFile()` does, so the pair balances. `completeLoading` now
carries the real success flag rather than an unconditional `true`.

Verified with a whole-window grab: Block Details reads **20.2.0.7 / 12 /
Num Blocks 4**.

**Lesson: `swapModels()` is a toggle with no idempotence and no assertion.** Any
future path that fabricates a document must emit both signals or call neither.
Worth giving it an explicit `bool showEmpty` argument at some point so an
unbalanced call cannot silently invert the UI.

Also: `WW_UI_SHOT` no longer requires a file name, and is exempt from the
startup-cube guard alongside `WW_STARTER_SHOT`. The starter document emits
`completeLoading` now, so the whole-window harness works on the startup scene —
which is the only way to eyeball the docks in that state, and how this was
confirmed.

Render regression 7/7 identical.

## 2026-07-27u — The black cube: an unwritten bitangent, and another NaN

Closes the item 27t left open. **The starter scene never wrote Bitangent X/Y/Z.**

The lighting shader builds a tangent-space basis from normal, tangent and
bitangent and normalizes it. A bitangent of exactly `(0,0,0)` makes that a
`normalize(0,0,0)` — NaN — and the shape renders pure black however correct its
base map, lights and uniforms are. **The same failure mode as the 07-17 line
defect**: a zero that becomes NaN inside a shader. Two in one day.

Fix: `bitangent = cross( normal, tangent )`, written into the three fields.
Verified per face — the +Z face gets `(0, 1, 0)`, the +Y and +X faces `(0, 0, -1)`,
each the correct cross product.

### Why every comparison said "identical"

This is the useful part. The textures, uniforms, lights, shader program, blend
state and draw list genuinely *were* identical between the working and broken
renders — every table in 27t is accurate. The difference was in the **vertex
buffers**, which were the last thing dumped:

```
LOADED (grey):    bitangentVector  0.000 0.004 0.004
STARTUP (black):  bitangentVector  0.000 0.000 0.000
```

And that is also why the same document loaded from disk rendered correctly:
saving round-trips the bitangent through the `normbyte` encoding, where a stored
0 decodes to about **0.0039** — tiny, but non-zero, so the basis survives. Only
the freshly-built in-memory document carries exact zeros. A bug that repairs
itself on the first save is exactly the kind that looks impossible.

**Lesson, and it is a repeat.** 27t's tables compared everything *bound* to the
draw and nothing *fed* to it. Vertex buffers are shader input too. When output
contradicts inputs that all look right, dump the vertex buffers before concluding
the state is identical — `rdc script` with `GetVBuffers()` + `GetBufferData()`
does it in one call, and would have found this hours earlier.

### Two tool facts worth keeping

- **The inline-colour texture format is `#AABBGGRR`, not `#AARRGGBB`**
  (`gltexloaders.cpp:990`). The starter cube's grey is symmetric so it was right
  by luck; the renderer's own `default_n = "#FFFF8080"` decoding to RGBA
  `80 80 FF FF` — a flat normal — is what proves the order.
- **`rdc debug pixel` is as unusable as `rdc debug vertex` here**: every input
  reads `-107374176.0`, which is `0xCCCCCCCC` as a float. The backlog warned about
  the vertex path; it applies to the pixel path too.
- **`rdc diff --pipeline` times out** on these captures at the default 60 s; pass
  `--timeout`. `--draws` works fine.

Render regression 7/7 identical.

## 2026-07-27t — Fallout 4 by default, cube on the grid, themed node dot

Three reported items; two fixed, one **still open and honestly unresolved**.

### Fallout 4 is the default now

Two separate places had it wrong:

- **`NifModel::createNew()` left the document on the wrong game.** It calls
  `clear()`, which resolves `gameResources` from the bsVersion it has *at that
  moment* — the startup default — and then changed the version without
  re-resolving. `GameManager` keys the resource set off the model's version, so a
  document created as Fallout 4 kept whichever game the default implied and every
  texture and material lookup it made went to the wrong game for the rest of its
  life. Now re-resolves after setting the version.
- **The startup default itself was BS 100 (Skyrim SE).** This fork is a Fallout 4
  tool, so a new document should be one without being asked. Now 130. Only
  affects users who never set `Settings/Nif/Startup Defaults/User Version 2`.

### The cube sits on the grid

`Translation` is now `(0, 0, size/2)`, so it rests on the XY plane instead of
being half-sunk through it. The mesh stays symmetric about its own origin, so the
node — and the pivot every transform tool uses — remains at the cube's centre;
only the node is lifted. Verified: `Z = 69.991249`, exactly half of the
139.9825-unit cube.

### The Block List node dot follows the theme

`blockListCategoryIcon()` drew the `NiNode` dot with a hardcoded
`QColor( 214, 214, 210 )`. That matched nothing else in the palette and, worse,
ignored the theme — a near-white dot on a near-white row is invisible on the
light skin. Now `wwSkinColor( "text" )`, per the skin rule that colours come from
`skinVars[]` and never from literals. Cached per colour so a theme switch
rebuilds the pixmap instead of serving the old one forever.

### RESOLVED in 27u — see below. The account of the hunt is kept.

### The startup cube rendered black

The same document renders **grey when loaded from a file and black at startup**,
and I could not find the cause. What the captures rule out — all read at the cube
draw, in the grabbed frame, in both runs:

| checked | startup (black) | loaded (grey) |
|---|---|---|
| BaseMap texel | `80 80 80 FF` | `80 80 80 FF` |
| NormalMap texel | `80 80 FF FF` | `80 80 FF FF` |
| shader program | vs 123 / ps 122 | vs 123 / ps 122 |
| light diffuse / ambient | white / white | white / white |
| `toneMapScale` | 0.2364 | 0.2364 |
| PS `$Globals` (26 vars) | all sane, `alpha` 1 | — |
| VS `vertexColorOverride` | `1,1,1,1` | — |
| vertex input declarations | identical | identical |

`rdc rt 132` confirms the colour target is already black immediately after the
cube draw, so the fragment shader really is outputting black from inputs that
look correct.

Two dead ends worth recording so they are not repeated:

- **`rdc debug pixel` is unusable here**, exactly like `rdc debug vertex`: every
  input reads `-107374176.0`, which is `0xCCCCCCCC` as a float. The backlog
  already warned about this for the vertex path; it applies to the pixel path too.
- **`bsshape.cpp:90` fills the vertex-colour array with BLACK** for shapes whose
  descriptor has no `VA_COLOR`, and the capture shows `vertexColor` *is* a used,
  bound attribute. White is the multiplicative identity and black is the identity
  for nothing, so this looked like the answer. **It is not** — changing it to
  white left the cube black. Reverted rather than shipped, since it changes
  rendering for every uncoloured shape and fixed nothing observable. Still looks
  wrong; worth revisiting with evidence rather than on a hunch.

The next thing to try is a capture **diff** of the two runs across all state
rather than the hand-picked fields above — `rdc diff` exists and was not used.

Render regression 7/7 identical.

## 2026-07-27s — Starter cube: neutral grey, and two inside-out faces

Reported against `bc4656a`: the startup cube looked wrong and was pure black.

**Colour.** It now carries a neutral base map instead of nothing. `"#AARRGGBB"`
is the renderer's own inline-colour texture syntax — the same one its built-in
fallbacks use (`white` is `"#FFFFFFFF"` in `renderer.cpp`) — and a texture-set
slot resolves through the same `bind()` those go through, so this needs no file
on disk and cannot go missing. Slot 0 is `#FF808080`, slot 1 the flat normal
`#FFFF8080`, ten slots to match vanilla.

`0x80` is **linear** 0.5, which displays around `0xB8` and lands close to
Blender's default material. `"#FF808080s"` (the renderer's sRGB suffix, as `gray`
uses) would mean a perceptual 50% grey instead and reads noticeably darker — a
one-character change if that is preferred.

**Winding — a real bug, but not the one that was visible.** Each face winds its
quad CCW in its own `(u, v)` plane, so `u × v` has to *be* the outward normal:

| face | u | v | u × v | normal | |
|---|---|---|---|---|---|
| ±Z | X | Y | +Z | +Z | ok |
| ±Y | X | Z | **−Y** | +Y | **inside out** |
| ±X | Y | Z | +X | +X | ok |

`X × Z = −Y`, so both ±Y faces were wound backwards. Fixed by swapping to
`{ 2, 0 }` — `Z × X = +Y`. **The same bug is in `tlMakePrimitive()`**, so Add
Primitive (Shift+A) has been building cubes with two inverted faces all along;
fixed in both copies.

Be clear about what this did *not* explain: `bsshape.cpp:419` disables
`GL_CULL_FACE`, so nothing was ever culled and the viewport looked the same
either way — confirmed by rendering a pre-fix file and a post-fix file from the
same camera, both full squares, differing only in colour. It still matters for
anything that trusts winding (export, normal recalculation), and it is wrong
arithmetic regardless, but the black-blob appearance was the missing base map.

Verified: geometry read back from the written file — 24 vertices at ±h, 12
triangles, and the +Y face's first triangle now cross-products to `(0, +4, 0)`.
Rendered before/after from a pinned camera: magenta-no-texture → neutral grey.

## 2026-07-27r — The 07-17 line defect is FIXED: a zero viewport in the uniform block

Open since 07-17: the ground grid, origin axes and 3D cursor did not appear until
you clicked in the viewport. Ten days of probing eliminated the program cache
(GPU-verified), depth, colour, alpha, geometry, FBO, uniform-block binding,
clipping, GL errors and the geometry stage. It was none of those.

**`globalUniforms.viewportDimensions` reached the GPU as `0, 0, 0, 0`.**

`lines.geom` divides by `vpScale = viewportDimensions.zw * 0.5` and normalizes
`p1_ss - p0_ss`, where both are scaled by it. At zero that is `normalize(0,0)`
plus a divide by zero, so every emitted `gl_Position` is NaN and the rasteriser
drops the primitive. This accounts for every symptom that made the bug so hard to
place:

- the geometry **is** emitted — the capture shows 22 triangles out of the GS for
  the grid draw, and no pixels
- forcing the fragment colour to red gave `redPx=0`, because the vertices were
  NaN and colour was never involved
- **POINTS from the same streaming path still drew** — they never touch `vpScale`
- **a pick render healed it**: `indexAt()` calls `setViewport()`, so after one
  click the value is right for the life of the window

`setViewport()` is the only writer and is called from just `resizeGL()` and
`indexAt()`. `resizeGL()` returns early when the context is not yet valid,
**without** setting it, and if no later resize arrives the value stays zero until
something clicks.

Fix in `setGlobalUniforms()`: when `viewportDimensions[2] <= 0`, fill from the
live `GL_VIEWPORT` before uploading. `getViewport()` already carried exactly this
fallback — the upload path simply never got it. Read-only, so it cannot move the
viewport the way forcing `setViewport()` per frame did (that attempt shifted all
seven baselines and was reverted; that warning still stands and is unchanged).

Verified both ways round on the same scene:

| | `viewportDimensions` on the GPU | grid / axes |
|---|---|---|
| before | `0, 0, 0, 0` | absent |
| after | `0, 0, 1434, 730` | drawn |

**Render regression: 7/7 identical**, as expected — the harness's `indexAt()`
warm-up was already forcing lines on, and this produces the same state. That
warm-up is now redundant and can go once there is confidence the fix covers every
path; it is left in for determinism.

### The correction that mattered

`glview.cpp` carried a comment stating the zero-viewport/NaN theory "was wrong",
citing a probe that read `(0,0,395,517)` at the grid draw. **That probe read the
CPU-side struct**; the GPU had the zeros. The theory was right and the
measurement was aimed at the wrong side of the upload. The comment is corrected
in place rather than deleted, because it is the second time this exact trap has
cost days here — see the 07-11 startup-grid bug, whose lesson was already
recorded as "printf cannot see a cache-vs-GPU desync". It could not be seen
without a capture, which is why 07-27q's in-app RenderDoc plumbing had to land
first.

## 2026-07-27q — NifSkope opens on a cube, like Blender

Launching with no file gave an empty document, which is nowhere to click: most of
what this editor does needs a shape to do it to. **Add Primitive itself refuses
without one** — it clones an existing `BSTriShape` for its vertex layout and
material — so an empty document could not even make its own first shape.

Now a window with no file opens on a minimal Fallout 4 scene: `NiNode` "Scene
Root" holding a `BSTriShape` "Cube" with its own `BSLightingShaderProperty` and
`BSShaderTextureSet`. Version 20.2.0.7 / user 12 / **BS 130**.

`NifModel::createNew( fileVersion, userVersion, bsVersion )` is new and is what
makes the BS version stick. `clear()` takes its versions from
`Settings/Nif/Startup Defaults`, whose BS default is **100 (Skyrim SE)**, and
setting the header field afterwards is not enough — the cached copy that
`getBSVersion()` and every version condition read is only refreshed by `clear()`
and by loading a header, and `cacheBSVersion()` is protected. BS version
conditions whole block layouts, most visibly `BSTriShape`'s vertex data, so a
document built at BS 100 would have had the wrong vertex rows.

### Size: 2 metres, not 1 unit

`STARTER_CUBE_SIZE = 2.0f * 69.99125f` — Blender's default cube in FO4 units,
from the same Havok-metre constant the collision decoder is validated against
(`hknpdecode.h`, `glnode.cpp`), rather than a number picked to look right.

Deliberately **not** the viewport's Add Primitive default of 1.0. That one is
dropped at the 3D cursor into a scene that already has something in it, with Size
live in the operator panel. A document containing nothing else has to be visible
on its own, and at 1.0 it is not: `GLView::frameAll()` snaps any scene whose
radius is below the unit scale to a 1024-unit sphere, so a 1-unit cube stays
sub-pixel however the camera is framed. Measured — at 1.0 the viewport is empty,
at 100 the cube fills a third of it.

### Guards

- **background windows** — session/workspace tabs and `promoteBackgroundDocument()`
  load into their window immediately, so a cube would be built only to be thrown
  away.
- **`WW_*` environment** — every harness keeps the empty document it expects; a
  cube would shift block numbers under all of them. Same guard `saveUi()` uses.
  `WW_STARTER_SHOT` is exempt, being the harness for this path.
- **`Settings/Nif/Startup Defaults/New Document Cube`**, default true. Set it to
  false to get the old empty document back without a rebuild — worth knowing,
  since a fault on the startup path would otherwise leave nowhere to click.

The undo stack is cleared and marked clean afterwards: the cube is the document's
starting state, not an edit, so closing an untouched window must not ask about
saving (`clean 1` in the harness log confirms it).

Building the model directly also skips everything that reacts to a document
arriving, which showed up as the cube drawing with a stale camera. It now emits
`completeLoading` and calls `updateScene()` / `frameAll()`.

### Verified

`new -o OUT [--size N]` writes the same document the GUI opens with, so this is
checkable without a window: 4 blocks, 20.2.0.7 / 12 / 130, 24 verts, 12 tris,
Data Size 744 = 24x28 + 12x6, bounding radius 121.228 = sqrt(3)/2 x 139.98, and
**load + save is byte-identical at 1316 bytes**. `WW_STARTER_SHOT=<png>` then
confirmed the GUI path renders it and exits 0.

**Loose end:** the startup cube draws unshaded black, where the same document
opened from disk draws in the viewport's no-texture magenta — a loaded file has a
folder for texture lookup to fail against, a document that was never on disk has
none. Both are "no material"; only the appearance differs. Not chased further
because each build is currently a machine-stability risk (see below).

**Not run:** the 7-image render-regression set. It launches a NifSkope per image
and the machine hard-crashed three times today under sustained build load
(Kernel-Power 41, no bugcheck, no WHEA, no TDR — the signature of a power or
thermal cutout, not a driver fault). Builds are now `-j2`. The corpus passes a
file on the command line and sets `WW_RENDER_SHOT`, so the cube path is
double-guarded off for it and cannot change those pixels; run it to confirm when
the machine is trusted again.

## 2026-07-27p — `hknpRagdollData` decodes: ragdoll bodies are real, not inferred

Supersedes the positional inference in 27o (below), which stays only as a
fallback. **`hknpRagdollData` derives from `hknpPhysicsSystemData`** — the same
array offsets hold at the object base, so matching the class name was the entire
fix. Verified on the brahmin skeleton before changing anything: `+0x10`
body_props, `+0x20` dyn_motion, `+0x30` dyn_inertia, `+0x40` bodyCinfos and
`+0x60` shape entries all read **39** (its bone count), and every one of the 39
cinfos resolves through its global fixup to a distinct `hknpCapsuleShape` **in
exact index order**, body *i* → capsule *i*. That is also independent
confirmation of 27o's measured `shape index == body id`.

**All 35** vanilla FO4 actor ragdolls now decode their bodies from the packfile —
including the three (`Deathclaw`, `Robot/SkeletonRef`,
`Robot/skeletonSentryBodyPart`) that positional attribution had to refuse, since
attribution no longer depends on nothing having been dropped. The positional path
now fires on zero vanilla files; it is kept, gated as before, for a packfile whose
root is neither class. 72 SetDressing/Architecture meshes re-checked: unchanged.

Per-bone physics is real now rather than defaulted. The brahmin reads **39 bodies,
dynamic, layer 8** (was: 0 bodies, static, layer 1 by fallback), and a decompiled
bone body carries Mass 5, Friction 0.30, Motion System 3, Quality 4 — where before
it got Mass 0, Friction 0.5, Motion System 5.

One thing this exposes is recorded in TO_BE_IMPLEMENTED §4a rather than guessed
at: **`hknpRagdollData +0x50` holds 38 entries on a 39-bone ragdoll** — one per
non-root bone. That is the constraint binding array (38 bindings share 24 distinct
constraint objects at 0x18 bytes each), and it is the lead for decoding the joints.

### Per-body mass / inertia / damping

`HknpSystem` held these as scalars read from array element 0, which was right when
a system meant one body. A ragdoll has one per bone, so every decompiled bone got
bone 0's values. They now live on `HknpBodyPhys`, seeded from the system scalars
when the arrays are absent so callers never have to test.

Two things had to be measured rather than assumed, and the second was a mistake
caught only because it was checked:

- **Strides.** `dyn_inertia` is `0x70` and `dyn_motion` is `0x40` — the only
  candidates of 0x30/0x40/0x50/0x60/0x70/0x80 that read 39 plausible entries on the
  brahmin. A 15-body Halloween banner agrees independently (896/14 = 0x40,
  1568/14 = 0x70). Note the encoder writes a 0x40-byte `dynamicInertia` block; that
  stays byte-validated for single-body output, where stride is irrelevant.
- **The index.** `cinfo +0x0c` is the body's **motion index** into those arrays, not
  its own index — the same field `hasMotion` already tested against `0x7fffffff`.
  Indexing by body index is wrong in a way that looks plausible: it gave the
  brahmin 4 kg toes, 5 kg ears and a 1 kg head. By motion index both limbs taper
  **5 → 3 → 1 → 1** from thigh to toe and from upper arm to palm, and the 20 kg
  lands on Spine4. The arrays also carry their own counts (`+0x28`, `+0x38`) which
  need not equal the body count — the banner has 15 bodies and 14 dynamic entries,
  one being its anchor — and testing the motion index against those covers both
  that and the static case.

Verified after: 35/35 ragdolls decode bodies from the packfile, 72/72 physics
meshes unchanged, skeleton selftest passes, and a decompiled brahmin carries
distinct per-bone masses (Pelvis 5, Tail1 4, Sack 4, Spine4 20, RLeg2 3).

## 2026-07-27o — Per-bone ragdoll collision: attribution recovered, Bone column, sync

Follow-on to 27n, which measured that the ragdoll *is* the bone collision but
recorded a caveat: the decode "collapsed 41 bhkNPCollisionObjects into 2, with a
single bhkRigidBody owning a bhkListShape of 39 capsules", so per-bone body
association looked flattened. **That caveat is now resolved — it was a decoder
defect, not a property of the format.** Where 27n reports one bhkListShape / one
bhkRigidBody / one bhkCollisionObject, the correct result is 39 of each.

### The defect

A `bhkRagdollSystem`'s packfile root is **`hknpRagdollData`**, not
`hknpPhysicsSystemData`. `hknpDecode`'s body scan only understands the latter, so
on every ragdoll it found **0 bodies** and left all 39 capsules at `bodyId = -1`.
Three consequences, all measured on the vanilla brahmin skeleton:

- the **viewport** drew all 39 capsules stacked on whichever bone holds Body ID 0
  (`glnode.cpp` maps `bodyId -1` onto body 0) and nothing on the other 38
- **Decompile** grouped by `bodyId`, so 39 bones collapsed into one body
- the **Collision Manager** showed 39 identical rows, each listing all 39 shapes

### Positional attribution

The NIF still names a bone for every body id, and the shapes come out of the
packfile in body order, so **shape index == body id**. Proven rather than assumed:
pairing the brahmin's 15 mirrored bones (`LLeg1`/`RLeg1`, `LArm2`/`RArm2`, ...)
through this mapping matches their capsule radius and length to **0.5%**, where
the best wrong offset differs by **27% — 52x worse**. Six pairs are bit-identical.

Gated two ways so it can never invent data:

- only when the packfile yielded **no** bodies, so it cannot override real ids
- only when **`unknownShapes` is empty**. A shape the decoder skipped is a hole in
  the sequence and every later index is off by one — binding capsules to the wrong
  bones is worse than leaving them unbound, because wrong looks right.

`HknpSystem::positionalBodies` tells callers the ids are an inference.

Swept all 35 vanilla FO4 actor skeletons that carry a ragdoll: **32 now attribute
per-bone**. The 3 refused are `Deathclaw` (28 objects, 27 shapes — drops an
`hknpSphereShape`) and `Robot/SkeletonRef` + `Robot/skeletonSentryBodyPart`. Five
more (`Turret` 3 objects / 6 shapes, `Vertibird` 11/13, `LibertyPrime` 12/14,
`RadStag` 31/33, `MirelurkQueen` 59/60) have bodies that no collision object names
— already a documented case on the physics path — and every object there still
resolves to exactly one shape.

End-to-end check: decompiling the brahmin ragdoll now yields **39
bhkCapsuleShape + 39 bhkRigidBody + 39 bhkCollisionObject**, and the capsule radii
on the first six (0.2529 / 0.0460 / 0.3556 / 0.1241 / 0.2097 / 0.1241) match
shapes 0-5 — Pelvis, Tail1, SPINE2, RLeg1, Sack, LLeg1 — each on its own node.
Also swept 72 SetDressing/Architecture meshes with compiled collision: all 72
still attribute from the packfile body array, so the physics path is untouched.

### There is still no ragdoll encoder, and that is not an encoder task

`hknpEncodeCompressedMesh` writes one body holding one compressed mesh. Emitting a
ragdoll needs five classes NifSkope does not decode at all. From the brahmin's
53,920-byte packfile (`tools/hkparse.py` on `collision --extract`):

| class | count | decoded |
|---|---|---|
| `hknpRagdollData` | 1 | no — the root, and why no bodies decoded |
| `hknpCapsuleShape` | 39 | yes |
| `hkpRagdollConstraintData` | ~20 | no — the joints |
| `hkpLimitedHingeConstraintData` | ~4 | no — the joints |
| `hkpPositionConstraintMotor` | 1 | no |
| `hkaSkeleton` | 1 | no — the ragdoll's own skeleton copy |

An encoder cannot be validated before those decode, because the only usable test
is `decode(encode(decode(x))) == decode(x)`. Writing one now would emit a file
that loads with no joints and no ragdoll skeleton. So: **decode first**.

Meanwhile two live one-way trips are now guarded, both default-Cancel:

- **Decompile** a `bhkRagdollSystem` names what will not come back. Once per cast,
  so Decompile All asks a single time. Skipped headless (`-no-gui` builds a plain
  `QCoreApplication`, where `QMessageBox` is invalid).
- **Compile Selected** wrote a single static body as a compressed mesh in a
  `bhkPhysicsSystem` unconditionally. On a bone that triangulates the capsule and
  cannot restore the ragdoll; it now says so first.

### Collision Manager

- **Bone column.** Two sources, because the file kinds answer differently: a
  skinned mesh has a skin, so `skeletonAnalyse()` gives `deforming (N v)` or
  `unused bone`; a `skeleton.nif` has no skin at all — every node would read "not
  a bone" — but a `bhkRagdollSystem` existing *is* the statement that these bodies
  are bone collision, so it reads `ragdoll bone`. Logical column 6 moved beside
  Node with `moveSection`, which left the 42 literal column indices in this file
  alone. The State cell now distinguishes `RAGDOLL` from `COMPILED`.
- **Selection sync with the Skeleton Manager.** Both docks already pushed through
  `NifSkope::select()`, so the current block is the bus and no direct coupling was
  needed — the missing half was listening. Each side maps the incoming block into
  its own terms: the Collision Manager matches a node to the row that owns it, the
  Skeleton Manager follows a `bhkNPCollisionObject` to its `Target` bone. Both
  guard the echo.
- **One decode per packfile.** `addCompiled` called `hknpDecode` per collision
  object, so a skeleton decoded its 54 kB ragdoll **41 times** to build 41 rows.
  Now `hknpDecodeCached`, keyed on the data.

### Decompile All accepted only `bhkPhysicsSystem`

So it silently did nothing on every skeleton file, while the single-block
Decompile (which tests both classes) worked. Both paths now agree via
`isCompiledSystem()`.

**GUI checks owed.** Build is green and every model-layer claim above was measured
through the CLI, but the three UI-visible pieces — the Bone column and its
`moveSection` placement, the `RAGDOLL` state cell, and the two-way selection sync —
have not been looked at on screen. Deferred deliberately, not forgotten: launching
NifSkope mid-session steals focus (07-26 rule). Verify on the next GUI pass.

### New: `collision` CLI command

```
collision <file>                        node -> body -> system bindings, and what
                                        the hknp decode found in each packfile
collision <file> --extract -b N -o F    write a system's Binary Data verbatim
```

The per-shape body id is the column that matters; `--extract` is what lets
`tools/hkparse.py` read a vanilla packfile without a NIF parser in Python. Every
measurement above came from it.

## 2026-07-27n - Bone collision: viewing and editing already work (investigation, no code)

Goal: edit bone collision in the Collision Manager. Measured what already exists
before building anything, and most of it does.

**The ragdoll IS the bone collision.** Decoding the brahmin skeleton's
bhkRagdollSystem (block 8) with the existing Havok / Decompile Compiled Collision
spell yields **39 bhkCapsuleShape** blocks - one capsule per bone - plus a
bhkListShape, a bhkRigidBody and a bhkCollisionObject. All ordinary editable
legacy blocks. 97 blocks in, 99 out.

**The Collision Manager already enumerates it.** Its browser loop guards only on
 and then follows that object's Data
link to whatever system block it points at - there is NO system-type filter
anywhere ( appears zero times in collisiontools.cpp). Decoding
is generic too: .
So a skeleton's 41 bone-collision objects populate the browser as-is, and the
existing translucent-solid + wire viewport display draws them with no renderer
work. **Step 1 of the plan needed no implementation.**

**Corrects my own earlier claim** that nothing called the decoder for ragdolls:
havok.cpp:1570 accepts bhkRagdollSystem explicitly, and it works.

**The single real gap: compiling back.** After a decompile you hold legacy blocks
and can edit capsule radius, endpoints, layer, material and rigid-body physics -
but nothing re-emits a bhkRagdollSystem packfile. hknpencode was written for
bhkPhysicsSystem compressed-mesh and convex shapes.

**One caveat to check before trusting the round trip:** the decode collapsed 41
bhkNPCollisionObjects into 2, with a single bhkRigidBody owning a bhkListShape of
39 capsules. So the per-bone BODY association may be flattened by the decode even
though the shapes survive. Whether each capsule can still be attributed to its
bone matters for a bone-collision editor, and is the next thing to verify.

Also still true: Decode All only accepts bhkPhysicsSystem, so it silently skips
ragdolls; only the single-block Decompile handles them. Cheap inconsistency to fix.

## 2026-07-27m — The regression guard was never deterministic: it was the line defect

Chasing why all seven baselines drifted found the answer, and it corrects two
earlier explanations of mine.

**Cause.** Streaming LINE geometry — ground grid, origin axes, 3D cursor — draws
nothing until a pick render has run (the open 07-17 defect). Whether that had
happened varied between harness runs, so the grid was present in some captures
and absent from others. The diff sizes prove it: largest on 
(54,581 px, a near-empty scene where the grid IS most of the content), small on
 (3,805 px, where the duct covers it).

**Fix (a workaround, not a fix for the defect):** the harness now runs one
 before grabbing, which performs the pick render and puts the line path
deterministically ON. Re-baselined once; 7/7 identical on two consecutive
compares. Remove this when the defect itself is solved.

**Two of my earlier explanations were wrong, and are corrected here:**
- 07-27j/k blamed  cold-vs-warm drift on a particle-sim warm-up.
  It was the grid.  bytes was the grid-present render and  the
  grid-absent one — nothing to do with the sim.
- Earlier today I suspected persisted view state, then AA/anisotropic settings.
  Both were tested and eliminated (removing the AA keys changed nothing).

**Also eliminated along the way**, so nobody re-tests them: persisted dock layout
(sizes matched throughout), lighting settings (no  key exists —
all defaults),  (no  key, as the palette migration
intends), and a skeletonView leak into normal renders (checked a current capture:
duct renders, no bones).

**Process note.**  was committed without running the regression set,
which is how a whole commit went by with the guard red. The set is cheap; run it
before every commit that touches rendering, the harness, or a dock.

## 2026-07-27l — Harness runs no longer overwrite the user's dock layout (regression fix)

**Reported:** NifSkope launches with Block List, Block Details, Header and NIF
Browser all toggled off.

**Cause, mine, from 07-27j.** The `WW_RENDER_SHOT` harness hides every dock so the
framebuffer size depends only on `WW_RENDER_SIZE`. It then quits — and
`NifSkope::saveUi()` runs on close, so the hidden-dock layout was written straight
into `UI/Window State` and became the layout every subsequent normal launch
restored. The harness change was right; persisting its side effects was not.

**Fix:** `saveUi()` returns early when any `WW_*` environment variable is set. The
guard is deliberately generic rather than a check for that one variable — this
session alone made roughly thirty harness launches across a dozen harnesses,
several of which hide docks, switch modes or resize the window, and none of them
should leave a trace in the user's settings.

Also cleared the already-polluted `UI/Window State` and `UI/Window Geometry` keys
so the docks come back without the user having to restore them by hand.

Verified both directions: a harness run no longer writes `Window State`, and a
normal launch still does.

**Lesson worth keeping:** a test harness that drives the real application shares
its persisted settings. Anything a harness changes that is saved on exit —
layout, camera, view mode, workspace, toggles — leaks into the user's next
session. This is the second such leak today; the first was the render baselines
moving when the persisted dock layout changed.

## 2026-07-27k — Blender-style armature rendering in the Skeleton Manager

The headline ask: render the whole skeleton — bones with a visible direction,
their names, and each bone connected to its parent.

**Octahedral bones** (`GLView::drawOctahedralBone`). Blender's shape: a square
collar at ~15% of the bone's length with radius ~10%, four edges fanning back to
the head and four converging on the tail. The taper is the point — it shows which
way the bone faces, which a plain head-to-tail line cannot. Twelve line segments,
so it needs no new shader or render state. Used in the Skeleton Manager; Pose Mode
keeps the plain stick, where bones are drag targets and a dense octahedral cluster
would fight with picking.

**`skeletonView` mode on GLView** — the armature draws while the Skeleton Manager
dock is up, without entering Pose Mode. Names are always on there (reading the rig
is the whole job); Pose Mode keeps them behind its Names toggle. The existing
dashed parent-relationship lines, depth fade, hover and picking all come along.

**Skeleton files now populate.** `refreshPoseBones()` built its list purely from
skinned shapes, so a `skeleton.nif` — a file that is nothing *but* bones — drew
nothing and the dock reported "0 bone(s)". In skeleton view it now falls back to
the node hierarchy: every non-geometry `NiAVObject` is a bone for display.

Verified on `Brahmin/CharacterAssets/Skeleton.nif`: 50 nodes, 50 bones drawn, and
the screenshot reads unmistakably as a two-headed brahmin — two neck chains, four
legs, named joints. `WW_SKELETON_TEST` gained `WW_SKELETON_SHOT=<png>` for this,
because the render-regression harness hides every dock and so switches the
skeleton view off.

### The open line-geometry defect blocks this, and characterises it better

The first screenshot showed **names and joint dots but no bones and no
relationship lines**. That is the 07-17 defect: before the first pick render,
streaming *line* geometry draws nothing. Sharper than anything from rounds 1–3:

> **Points render. Lines do not.**

`drawPoints` (selection.prog) works while `drawLines` (lines.prog) does not, in
the same frame, through the same streaming path. Checked and eliminated: it is
*not* a missing geometry stage — `GL_ATTACHED_SHADERS` on lines.prog reports
`attached=3 linked=1 [vert(1) GEOM(1) frag(1)]`, all compiled.

Interactively this is invisible to the user: their own screenshot has the grid, so
lines were already working there — the pick render on their first click had healed
it. Only a cold frame is affected. `WW_SKELETON_SHOT` therefore runs one
`indexAt()` first as a **harness warm-up**, clearly not a fix, and with that the
bones render correctly.

Regression set 7/7 identical (warm). Note again that `particles_mist` differs by
54,581 px on a cold first run and matches warm — the same effect recorded in
07-27j, now seen twice.

### Not done from the same request

Asked for in one go, not delivered: multi-select with Block-List colours, pin
bones, weight overlay, load-skeleton-from-file/archive, the numeric bone transform
panel, safe bone rename, bone-count/partition-limit warnings, symmetry tools, and
moving the hierarchy tree into the Pose Manager. Only the rendering and the
no-skin fix landed. Also visible in the screenshot and still open: **name labels
overlap badly** in dense regions — Blender declutters by screen distance, and this
does not yet.

## 2026-07-27j — Skeleton Manager workspace, phase 1 (read-only)

Backlog item #1, the largest un-started feature, per `SKELETON_AND_POSE_PLAN.md`
§A.7 phase 1: **read-only**. Hierarchy, bone classification, per-bone influence,
selection sync, rest-pose toggle. It writes nothing, which is why it goes first —
the dangerous part (§A.3 bone transforms with inverse-bind rebinding) waits for
its gauntlet.

**`src/skeletontools.{h,cpp}`** — `skeletonAnalyse()` is deliberately independent
of the dock so the CLI and the UI can never disagree about which nodes are bones.
It handles both skin backends: FO4 `BSSkin::Instance` with per-vertex
`Bone Weights`/`Bone Indices`, and classic `NiSkinInstance` + `NiSkinData` with
per-bone `Vertex Weights`. Influence comes from the vertex data, never from the
bone list, because a bone being *listed* says nothing about whether anything is
bound to it — which is exactly the unused-bone case the dock exists to surface.

Classification: **deforming** (moves vertices), **unused** (listed by a skin but
no vertex uses it — prunable, shown in `textBright`), **not a bone** (no skin
references it at all — a camera or attach point, shown in `textMuted`). Colours
come only from `skinVars` via `wwSkinColor()`.

Two validation findings come free because the pass has to detect them anyway:
dangling skin bones (a `Bones` entry that resolves to no block — real corruption,
the game looks it up by index and finds nothing) and duplicate node names (every
tool here looks bones up by name, so duplicates are ambiguous).

**Dock** — `Workspaces ▸ Skeleton`, appended LAST because the persisted
`UI/Workspace` index maps positionally onto the managers list; inserting anywhere
else would silently reopen a different workspace for every existing user. The
"Skeleton Manager (Planned)" placeholder is gone.

**CLI** (§A.8) — `skeleton <file>` prints the tree with influence; `--validate`
prints findings only and **exits 1 if any fire**, which is the real payoff: a
pre-export gate for a build script. `--prune-unused` is deliberately absent until
phase 2 ships with its bone-index remap tests.

**Verification.** `WW_SKELETON_TEST` drives the dock and checks every filter and
the search against `skeletonAnalyse()` — PASS on the FO4 faceBones head: 70
nodes, 68 bones, 68 deforming, 0 unused, and the filters return 70/68/68/0.
Independently, the CLI's summed weight is **1688.98 against 1689 vertices** —
0.02 of float drift across 6254 influences, which confirms the weight reading
rather than just the plumbing. 68 bones also matches the count recorded in the
FO4 CustomizationRemap reference notes. A non-skinned mesh reports 1 node / 0
bones without complaint.

**Honest gap:** the *Unused* filter has no positive test case. A 40-mesh sweep of
vanilla FO4 armour found zero unused bones — vanilla exports are clean — so the
filter is only shown returning 0 correctly. Its classification feeds the verified
counts, but a file that actually has an unused bone has not been through it.

### Two harness problems found on the way, both fixed

**The dock shrank the viewport and broke every baseline.** A new `QDockWidget`
parented to the main window shows at startup, so the GL viewport lost space and
all seven baselines became "size mismatch". Fixed the dock (register with a dock
area, then `hide()`, as the UV Editor dock already did).

**But the deeper problem was the harness.** Pinning the *window* to 1280×800 is
not enough — the GL viewport gets whatever the docks leave it, so any change to
the persisted dock layout silently resizes the framebuffer. The Skeleton dock
moved it from 517 to 695 px tall. The harness now hides every dock before
rendering, so the framebuffer depends only on `WW_RENDER_SIZE`. Baselines re-cut
once (they are also now much larger — `lit_head` 50 KB → 95 KB — because the
scene gets the whole window). Verified stable: 7/7 identical on two consecutive
compares.

Noted for the future: `particles_mist` renders differently on the **first** run
after a rebuild (27,945 bytes cold vs 7,868 warm) and is stable thereafter. So
re-baseline warm, and do not trust a single cold comparison of that case.

## 2026-07-27i — Startup grid/axes: NOT FIXED, but four theories killed and a headless repro found

Re-opened the 07-17 defect (grid + origin axes invisible until the first
viewport click). **Still open.** No behaviour change shipped — only env-gated
diagnostics. What is now known that was not before:

**1. There IS a headless reproducer.** A clean `WW_RENDER_SHOT` capture of
`ACDuctConnector01.nif` has no grid; the `lit_setdressing` baseline has never had
one. 07-17 concluded probe captures were "an imperfect proxy" and that live GUI
verification was required — but the plain harness reproduces it exactly. The
reason the old instrumentation kept seeing healthy frames is that **`WW_PERF_TEST`
masks the bug**: its `update`+`processEvents` round trips are precisely what
heals the grid, so the probe could never observe a broken frame. Any future
attempt should use `WW_GRID_PROBE`, not `WW_PERF_TEST`.

**2. Program-cache desync is disproven — at the GPU, not the cache.** This was
the leading theory, and the one the earlier startup-grid fix was built on. The
07-17 probe logged `prog=(useProgram("lines.prog") != nullptr)`, which is the
*cache*: `Scene::useProgram` (gltools.cpp:432) returns the cached pointer
**without rebinding** when `getCurrentProgram()` already names the program, so
that log could never detect a stale binding. Querying `GL_CURRENT_PROGRAM`
directly gives `gpuProg=47 expected=47 MATCH` on every single grid draw, with the
VAO bound and `modelViewMatrix` resolving on the actually-bound program. Both raw
`glUseProgram` sites in renderer.cpp do update `currentProgram`, and the raw
`glUseProgram(0)` at glview.cpp:2872 is preceded by a proper `cx->stopProgram()`
at :2843, so there is no desync to find.

**3. Not depth rejection.** `WW_GRID_NODEPTH=1` disables `GL_DEPTH_TEST` for the
line draws. The grid stays invisible.

**4. Not colour, alpha, contrast or geometry.** `WW_GRID_RED=1` forces the line
geometry opaque red at 8 px. **No red appears anywhere.** Logged values are all
sane: vertex-colour alpha 0.5, a ±1024 plane at Z=0 (`p0=(-1024,-1024,0)`,
`p1=(-1024,1024,0)`), viewport 395×517, colour mask open, blend on.

**Where that leaves it.** A draw is issued, with the right program, the right VAO,
valid uniforms, depth off and forced opaque red — and none of it reaches the image
that gets presented. So the instrumented draws are not the draws that produce the
visible frame. Combined with 07-17's note that `grabFramebuffer()` re-renders
offscreen, and the drawGrid trace reporting `fbo=0`, the sharpest remaining
hypothesis is a **render-target / pass mismatch**: the streaming line pass writes
to a different framebuffer than the one presented. **Next step: log the bound
draw-FBO in the line pass and in the presented pass and compare** — that is a
much narrower question than 07-17's "diff all GPU state in RenderDoc", and it can
be answered headlessly.

Also checked and *not* a bug: the harness looked unpinned (`vp=395x517` rather
than 1280×800), but `nifskope_ui.cpp:3269` pins the *window* to 1280×800 and
395×517 is just the GL viewport's share of it once the docks take their space.

Diagnostics kept, all inert unless their env var is set (verified: regression set
7/7 identical with them compiled in). Note two of them deliberately *change*
rendering when enabled, so they are debug switches, not probes: `WW_GRID_NODEPTH`
(disables depth test) and `WW_GRID_RED` (forces red, 8 px). `WW_GRID_PROBE` only
logs, to `ww_grid_probe.log` beside the exe, capped at 40 lines.

### Round 2, same day — five more eliminations and two wrong turns

Kept going. Everything below is measured, not reasoned.

**Also eliminated:**
- **Render-target mismatch.** Every `paintGL` frame and every line draw reports
  `drawFbo=0`/`readFbo=0`, and the grab reads FBO 0. Same buffer. This had been
  the leading hypothesis at the end of round 1; it is wrong.
- **The uniform block.** `viewportDimensions` reads `0,0,395,517` and
  `projectionMatrix` is a sane perspective matrix *at the grid draw*. So the
  NaN-from-zero-viewport theory below is disproven.
- **Clipping / camera.** Reproducing the shader's near-plane test on the CPU for
  the first grid line: `zw0=2325.6`, `zw1=-1772.9`, world origin `zwMid=276.3`.
  One endpoint in front, one behind — exactly the case `drawLine()` clips and
  emits. `mvTrans=(30.06, ~0, -139.09)`, a sane camera.
- **Silent driver rejection.** `glGetError()` immediately after the draw is clean.
  Worth stating because release builds compile out the *only* glGetError loop in
  the paint path (`glview.cpp` guards it with `#ifndef QT_NO_DEBUG`), so a
  rejected draw would otherwise be invisible.
- **Primitive/stage mismatch.** `drawLines` defaults to `GL_LINES`, matching
  `layout( lines ) in` in lines.geom; `.geom` files are mapped to
  `GL_GEOMETRY_SHADER` (glcontext.cpp:870) and `lines.geom` deploys. The program
  links (the draw proceeds), so the geometry stage is present.
- **Not a framing artifact.** Forced red in a *top* view — where the Z=0 grid
  faces the camera and must fill the frame — still yields `redPx=0`.

So: a draw with the correct program, a content-addressed VAO, sane matrices
producing in-frustum clip coordinates, depth disabled, opaque red at 8 px, no GL
error, correct primitive type and a linked geometry stage — produces **zero
fragments**. That is the state of knowledge. Still open.

**Wrong turn 1 — a "harmless" fix that broke all seven baselines.** Added a
per-frame `setViewport( 0, 0, pixelWidth, pixelHeight )` to `paintGL`, believing
the block was zero. It moved the viewport (pixelWidth/pixelHeight need not match
the viewport a given frame draws with — `indexAt()` sets its own for the pick
render, and DPR differs) and changed every baseline. Reverted, with a DO-NOT
comment left at the site so the next person does not retry it.

**Wrong turn 2 — I blamed the harness for my own stale deploy.** After reverting
`drawline.glsl` the diffs persisted, and I concluded the harness was polluted by
persisted view state, "confirmed" by the two mismatched hashes noted earlier in
the session. Both claims were wrong. A pristine-tree run is **7/7 identical**, so
the harness is reproducible, and the harness pins the view to `ViewFront`
regardless of persisted state (`nifskope_ui.cpp:3285`). The real cause: reverting
only a shader changes no `.cpp`, so `make` relinks nothing, so `QMAKE_POST_LINK`
never re-copies `res/shaders/` — `release/shaders/drawline.glsl` kept the modified
version. **This project's own notes already document that trap and I walked into
it.** Lesson, again: after a shader-only revert, force a relink (touch a `.cpp`)
and confirm the deployed copy, or verify with `grep` against `release/shaders/`.

Consequence worth keeping: the `drawline.glsl` NaN guard is **not** a safe no-op —
it materially changed output — so it is reverted rather than kept as hardening.

### Round 3 — RenderDoc cannot currently see the scene at all

"Take a RenderDoc capture" has been the recommended next step since 07-17. Tried
it via `rdc-cli` (headless, `E:\Tools\rdc-cli\rdc.cmd`). **It does not work on
this app as-is, and that is why the recommendation never paid off.**

Every capture, at every frame number tried (3, 7, 12), on both a static prop and
a continuously-animating particle file, contains the same thing:

```
1 draw calls (0 indexed, 0 dispatches, 1 clears)
EID  TYPE  TRIANGLES  INSTANCES
44   Draw  2          1
```

Four events total, and that one draw binds a single `textureSampler`. That is
**Qt compositing a textured fullscreen quad** — two triangles — not NifSkope's
scene. The 3D work never appears in the capture.

Cause: the viewport is a native child window with its own GL context (which is
also why `ogl->grabFramebuffer()` is required instead of `skope->grab()`, and why
the scene reports `drawFbo=0` — FBO 0 *of its own context*). RenderDoc's frame
boundary follows the presenting surface, so it captures the compositor's swap and
the scene's rendering falls outside the captured frame. Frame numbering does not
correspond to `paintGL` calls either: my probe counted 9 `paintGL` frames during
startup, and RenderDoc frames 3 and 7 both contained only the composite blit.

Also learned: `rdc capture` cannot attach to the `WW_RENDER_SHOT` harness at all
("failed to connect to target") — the harness renders and calls `qApp->quit()`
faster than RenderDoc's target-control handshake completes. Capturing needs
`--keep-alive` against a normally-launched instance.

**So the next step is no longer "capture it in RenderDoc" — it is "make RenderDoc
able to capture the viewport context".** Options, cheapest first: capture with
`--trigger` plus `capture-trigger` timed while the viewport is actively
redrawing; capture a long run and search for the frame whose draw count is more
than one; or give the harness a hold-open env var so a capture can be triggered
deliberately at a known point. Until one of those lands, `rdc mesh --stage gs-out`
— the geometry-shader output that would settle whether the line primitives are
emitted at all — is unreachable.

Nothing in the tree changed for round 3; the two `.rdc` files are scratch.

## 2026-07-27h — Refraction was dead for every BGSM-backed FO4 mesh (fixed)

Chasing "the refraction regression fixture guards nothing" found a real renderer
bug rather than a bad fixture.

**Root cause.** `BSLightingShaderProperty::updateParams` assigns `hasRefraction`
only inside the `} else { // m == nullptr` branch — when the shape has *no* valid
BGSM/BGEM. With a material present, which is nearly every FO4 mesh, it kept
`resetParams()`'s `false` and the NIF's `SLSF1_Refraction` bit was never read.
The screen-space refraction preview had been dead for all material-backed FO4
content since it shipped on 07-06. Now read in the material branch too.

**The material is OR'd with the NIF flag, not preferred over it.** nif.xml says
FO4 flags are "mostly overridden if Name is a path to a BGSM/BGEM file", which
argues for material-wins — and measurement kills that: of **6899** vanilla FO4
materials under `Data\Materials`, **zero** set `bRefraction`. Material-only would
leave the feature dead for all vanilla content. Offsets were validated before
trusting that zero: every boolean field decodes as strictly {0,1} while
`iAlphaTestRef` shows a real range (37–200), which a misaligned read cannot do.

**Wrong first attempt, corrected.** The first version read the material alone,
on the strength of the nif.xml comment. The corpus scan is what showed it was
backwards.

**Hypothesis 1 from RENDERER_MATCH_PLAN §0 is disproven.** FO4 bit 15 *is*
`Refraction` (nif.xml:7015); Skyrim's and FO4's layouts agree, so
`SLSF1_Refraction = 1 << 15` was never the problem.

**Why the symptom was so misleading.** `fo4_default.frag:383` ends with
`color.rgb = bg`, replacing the shape with the framebuffer behind it. Over a
featureless background a refracting shape is **invisible, not distorted** — so
the fix's first visible effect was the mesh vanishing, which reads as a
catastrophic regression. I mistook the vanishing head for proof the fix worked
("the bytes differ"), then for proof it was broken; it was neither. Only looking
at the pixels settled it.

**Fixture rebuilt.** `tests/render/refraction_fixture.nif` is now
`CA-PowerArmorVisorGlass01.nif` with the flag and Refraction Strength 0.8 set on
block 5 — an A/B pair with the `glass_visor` case, which is the same mesh with
the flag off. Any regression that stops refraction engaging flips it back to
looking like its own control. **Known limit:** it proves refraction *engages*,
not that it *distorts*; that needs geometry behind the shape, and a plain `merge`
does not do it (the pieces land at unrelated scales and never overlap).

**Harness counter bug, also fixed.** `capture.ps1`'s summary used
`($results | Where-Object {...}).Count`. When exactly one case matches,
Where-Object returns the hashtable itself rather than a 1-element array, and
`.Count` on a Hashtable is its *key* count — so a single DIFF row printed
"diff=4" (name, status, pixels, max). The totals only looked right while every
status matched more than one case, which is why yesterday's "ok=3 diff=4" passed
unquestioned. Wrapped each filter in `@()`.

**Baselines re-cut** at this commit, so the set is 7/7 identical instead of
carrying four permanent expected-diffs from the 07-27a spec/gloss change. A guard
that always prints four diffs is a guard nobody reads.

## 2026-07-27g — Backlog re-verified against the code (docs only, no code change)

`TO_BE_IMPLEMENTED.md` was last checked on 07-21 and had drifted over five
shipping days. Verified claim by claim; **five stale, six missing entirely.**

**Stale — filed as open, actually done:**
- **Pose mirror / paste-flipped poses** — `PoseMirrorButton` +
  `ogl->poseMirrorBone()` (`posetools.cpp:192`, `:491`). 07-22i even said
  "Pose-bone mirror already existed"; the backlog was never updated.
- **String-index derived display** — `nifmodel.cpp:1385` already returns
  `"<string> [<idx>]"` with distinct invalid-index messages. **Stock NifSkope.**
- **nif.xml tooltips on field labels** — `nifmodel.cpp:1512`, `ToolTipRole` on
  `NameCol`, from `NifItem::text()` (= the nif.xml description,
  `src/data/nifitem.h:111`). **Stock NifSkope.** So two of the four "ready to
  build" Block Details editors were never work at all.
- **"Proportional editing — explicitly declined"** — reversed by the user and
  shipped in 07-22i with all 8 Blender falloff curves. The decline sat in the
  do-not-implement list for five days after the feature existed.
- **"Eight `WW_*_TEST` harnesses"** — there are **22**. The CLI-port item was
  sized against a number frozen at 07-21.

**Missing entirely:**
- **`src/collisiontools.cpp` is a committed orphan** — not in `NifSkope.pro`;
  the live file is `src/spells/collisiontools.cpp`. Editing the orphan compiles
  clean and does nothing. Found by diffing the `.pro`'s exact paths against
  `find src -name '*.cpp'`; matching on basename hides it, and my first attempt
  did exactly that and reported "no orphans".
- **Separate ▸ By Material / By Loose Parts** — disabled at `glview.cpp:7241`,
  *"not implemented yet (Blender parity later)"*, never filed.
- Four **Pose Manager refinements** deferred in 07-22i and never carried across:
  pose-mode bone proportional, topology-based mirror pairing, the weight overlay
  drawing at bind positions, and non-destructive posing being a mode-boundary
  guarantee rather than a display overlay.
- §12 **Rendering** said "future/disabled" while spec/gloss, the `.pbrm` reader,
  resolution, the shader and the mode all shipped over 07-27a..e.
- The **refraction fixture guards nothing** (`RENDERER_MATCH_PLAN §0`) — worth
  saying plainly in the backlog, since §1/§2 were built anyway despite the plan
  saying not to until it was resolved.
- **Repo state**: ~54 files uncommitted across 07-25..07-27.

**Corrections:** Skeleton Manager entry is `nifskope_ui.cpp:6137`, not ~L4747.
The collision cap is 4096 *sections* of ≤255 verts/tris each — the old
"255-section cap lifted" conflated the two. "Nothing built" still sat in the Pose
Manager section four days after the dock landed.

**Also corrected an error of my own mid-sweep:** I first reported the NifItem
slab pool as unbuilt, having grepped `nifmodel.cpp` and `nifitem.h` but not
`nifitem.cpp`. It shipped. Filed as lesson 5 in the file's own how-to-use list,
along with the basename-vs-path trap.

The "Awaiting GUI verification" section was rewritten: its opening claim ("the
agent does not run GUI sessions") was superseded on 07-22 (agent verifies via
harnesses) and 07-26 (no interactive launches while the user is working). A new
**verification record** at the end of the file distinguishes what this sweep
actually re-tested from what it carried forward on the 07-21 sweep's word.

## 2026-07-27f — PBR parked: modes and toggle greyed out

At the user's call, until the empty-frame bug in 07-27e is fixed:

- **PBR** and **Legacy and PBR** are greyed out, labelled "(unfinished)", with a
  tooltip saying why. **Legacy** stays enabled and is the active mode.
- **Auto-replace** is greyed out and unchecked too — substituting a material that
  nothing can draw would only hide the legacy one.
- **Gated at the accessors, not just in the UI:** `pbrmFeatureEnabled` in
  `glproperty.cpp` makes `pbrmMode()` return Legacy and `pbrmAutoReplaceEnabled()`
  return false regardless of what is stored. A stale QSettings value, a settings
  restore or a future caller therefore cannot switch it on behind the greyed-out
  menu. **Flip that one constant and re-enable the two menu entries to resume.**

Verified: with `Settings/Render/PBRM Mode` still set to PBR in the registry, the
render is a full frame (96.8 KB) rather than the 5.2 KB empty one — the gate holds
against stored state. Regression set unchanged (3 identical, 4 expected spec/gloss
diffs).

Everything from 07-27b–e stays in the tree — reader, resolution, `pbrm-resolve`,
the shader and the program routing are all still there and still compile. Only the
user-facing switches are off.

## 2026-07-27e — Three lighting modes; PBR mode draws nothing (OPEN)

Shading menu, per the user's spec. Exactly one of three, plus an independent
checkbox below:

| mode | behaviour |
|---|---|
| **Legacy** | spec/gloss only; a resolved PBRM is ignored |
| **PBR** | PBR only — *every* shape through `pbrm_default`; shapes with no PBRM are driven from their legacy material (rough = 1 − smoothness, metal 0, F0 0.04) so the scene sits under one BRDF |
| **Legacy and PBR** | per shape: PBR where a PBRM resolved, spec/gloss elsewhere; PBR always overrides legacy when available |

Persisted as `Settings/Render/PBRM Mode` (enum `PbrmLightingMode`, default Legacy).
**Auto-replace** stays separate and independent — it governs `.bgsm`/`.bgem` →
same-name `.pbrm` substitution only, and is not part of the mode group.

**Mode plumbing verified** on `ACDuctConnector01.nif` (no PBRM available):
Legacy `f2ee399cb5`, **Legacy-and-PBR byte-identical to Legacy** (correctly falls
through when nothing resolved), PBR distinct. Renders are deterministic — mode 0
twice gives the same hash — so those comparisons mean what they say.

### OPEN: PBR mode renders an empty frame

In PBR mode the shape does not draw at all (5.2 KB frame vs ~49 KB legacy). The
failing branch is specifically the **legacy-derived fallback** — PBR mode applied
to a shape with no PBRM. The PBRM branch proper is untested, because nothing
resolves through this profile's configured resources.

Eliminated, so do not re-check these:

- **Shader compiles and links.** `.prog`s link at startup; no compile error, no
  program info-log output.
- **No GL errors logged**, and the frame is produced (harness writes a PNG).
- **Per-draw GL state** — blend/`alphaFlags`/`alphaThreshold` via
  `AlphaProperty::glProperty`, depth test/func/mask, polygon mode — replicated
  from the tail of `setupProgramCE1`. Added; did not fix it.
- **Cubemap completeness** — the fallback was `cube_sk` (Skyrim's
  `bleakfallscube_e.dds`), absent from FO4 archives, leaving an *incomplete*
  cubemap bound to a sampler the shader references, which can invalidate a draw.
  Now picks `cube_fo4` by BS version; warning gone, still empty. Worth keeping
  regardless — that fallback was simply wrong for FO4.
- **Uniform block binding** — `globalUniforms`/`skinningUniforms` are bound
  generically at link time for any program declaring them, so `pbrm_default` gets
  them automatically.

### What three RenderDoc captures established (2026-07-27)

Captures at `%TEMP%\RenderDoc\NifSkope_2026.07.26_23.24.45_frame{504,527,691}.rdc`,
analysed with `rdc-cli`. `frame504` is Legacy (bound samplers `BaseMap CubeMap
EnvironmentMap GlowMap GreyscaleMap NormalMap RefractionSrc SpecularMap` =
`fo4_default.frag`); `frame527` is PBR (`BaseMap CubeMap EmissiveMap NormalMap
RmaosMap` = `pbrm_default.frag`); `frame691` has no indexed draws.

**Confirmed working:** the mode routing, program selection and texture binding.
`pbrm_default` is bound on 7 indexed draws, and `glDrawElements` with 272
triangles is genuinely issued at EID 87. Blend is disabled with write mask 15, so
blending is not eating it. Effect-shader shapes (the rings, the lightning preview)
still render correctly in PBR mode — only `BSLightingShaderProperty` shapes vanish.
In the PBR render target the **grid is continuous** where the geometry should be,
so nothing is being occluded: fragments never land.

**Two rdc-cli avenues are dead ends on this capture, don't retry them:**
`debug vertex` reports every input as `-107374176.0` (`0xCCCCCCCC`) and output
`0.0` — but it reports *exactly the same* for the **working legacy draw**, so it
is a replay artifact, not evidence (this cost a wrong conclusion until the control
was run). `pixel` history refuses with "MSAA pixel history not supported".

**Eliminated by experiment:** per-draw blend/depth state; the cubemap fallback
(was Skyrim's `cube_sk`, absent from FO4 — genuinely wrong, now version-picked,
did not fix this); and unconditional base-alpha-as-opacity, which *was* a real bug
— the spec's `overrideOpacity` was being ignored, so a legacy diffuse map with
zero alpha would discard every fragment. Fixed via a derived `OpacityTexture`
feature bit. Still empty afterwards, so that was not the cause either.

**Next step is the qrenderdoc GUI**, which can do what the CLI cannot: select
EID 87, **Mesh Viewer → VS Out** to see whether clip-space positions are sane
(that single view splits "vertex stage broken" from "fragment stage broken"), and
if positions are fine, right-click a covered pixel → **Debug Pixel** to watch the
discard. Everything cheaper than that has been tried.

**Default is Legacy and the regression set is unchanged with it**, so this is
inert for normal use — but PBR mode is not usable yet and should not be described
as working.

## 2026-07-27d — PBR renders: `pbrm_default` program + the mode is live

`res/shaders/pbrm_default.frag` + `.prog`, `Renderer::setupProgramPBRM()`, and the
**PBR: Roughness / Metallic** action is no longer a disabled placeholder.

BRDF is the editor's, same port as the FO4 path but with the metal term FO4
cannot have: `f0 = mix( dielectric, base, metal )`, `diff = (1-F)(1-metal)base/PI`.
Feature bits go to the shader verbatim, so a clear bit means "use the constant" —
the shader never guesses whether a channel is real, it just honours the override
semantics the reader already resolved. Normal Z is always reconstructed, never
read from B, because B is height when the material says so.

- **The vertex stage is `fo4_default.vert` as-is.** It already supplies every
  varying this pass needs; a private copy would only drift out of step.
- **`.prog` has no conditions.** Whether a shape uses this depends on a runtime
  material resolution and a UI mode, neither of which a `.prog` check can express
  (they evaluate NIF data). `setupProgram()` selects it by name **before the hint
  path**, so flipping the mode cannot leave a shape on a cached program; an empty
  condition list keeps it out of the automatic scan.
- **Textures bound by explicit path**, not `uniSampler` — that resolves slots out
  of the shader property's texture list, and a PBRM's paths live in the material
  document. A slot that is off falls back to a neutral, not magenta: off is a
  legitimate authoring state, not an error.
- AO is applied to ambient and env, not to the direct lobe — occlusion describes
  what the surface cannot see of the environment, and putting it on direct light
  double-darkens contact shadows. Env reflection is `f0`-scaled split-sum
  **without** the BRDF LUT, which over-darkens grazing angles on rough metals:
  an honest approximation, not the editor's split-sum.
- Tonemap is the same Hable curve the FO4 path uses, so a PBRM material and a
  BGSM material in one scene are graded alike instead of one looking washed out.

**Also fixed: log spam.** Same-name discovery probed with `getResourceFile`,
which warns "not found in archives" on a miss — and a miss is the *normal* case,
so every BGSM in every scene emitted a warning on every load. Now probed quietly
with `findResourceFile` first. Verified 0 warnings where there had been one per
material.

**Verified:**
- The shader **compiles and links** — `.prog` files link at startup, so a GLSL
  error surfaces whether or not a shape uses the program. Clean.
- Resolution runs live in the renderer: before the quiet-probe fix the log showed
  it deriving `basehumanfemaleskinhead.pbrm` from the head's `.bgsm` and probing
  the VFS, which is same-name discovery working end to end.
- **The regression set is unchanged with the mode off** — identical pixel counts
  and max deltas to the 07-27a comparison (5,926 / 33,730 / 44,424 / 33,730), so
  the PBR wiring is inert until switched on.

**Not verified: a shape actually rendering as PBR.** Nothing reachable through
this profile's configured resources has a `.pbrm` — `pbrm-resolve` derives the
right candidate for every material and the lookup runs, but the PBR mod folder is
not a configured resource path, and neither is the unpacked corpus (NIFs there are
opened by path while textures come from the real install's archives). Add the
folder holding the `.pbrm` files under Settings ▸ Resources and it will adopt;
until something resolves, the mode has nothing to draw differently.

## 2026-07-27c — `.pbrm` material resolution + auto-replace toggle

`BSShaderLightingProperty::resolvePbrm()` (called at the end of `setMaterial`)
resolves a PBRM two ways, priority order mattering:

1. **Direct link** — the material name ends in `.pbrm`. **Unconditional**: the
   asset asked for it explicitly.
2. **Same-name discovery** — a `.bgsm`/`.bgem` with a `foo.pbrm` beside it, only
   while auto-replace is on.

Reads through `nif->getResourceFile( data, path, "materials", "" )` — the same VFS
call `Material::openFile` uses — so a PBRM inside a BA2 resolves exactly like a
BGSM does. A discovery miss is the normal case and is silent; a malformed PBRM
leaves the BGSM in charge; an unsupported-but-valid one sets `pbrmUnsupported` and
**fails closed** rather than quietly rendering the BGSM as if nothing were wrong.
Lives on the base class so `.bgem` effect materials resolve the same way.

**Toggle:** Shading menu ▸ *Auto-replace BGSM/BGEM with .pbrm*, persisted as
`Settings/Render/PBRM Auto Replace` (default on) and cached into a file-scope
accessor (`setPbrmAutoReplace`/`pbrmAutoReplaceEnabled`) so resolution never
touches QSettings per property. It is **not** in the workflow radio group — it is
independent, and it governs discovery only. Toggling rebuilds the scene, since
materials resolve when a property is built.

Free functions rather than a static member on purpose: the member would have to
sit in a public section of `BSShaderLightingProperty`, and inserting an access
specifier mid-class silently changes the access of every member after it — the
same trap that bit `glview.h` three times in the 07-22i batch.

**Verified** with a second new CLI command, `nifskope-cli pbrm-resolve <file.nif>`,
which reports the route and outcome per shader property. On the PBR mod's
`x01_torso.nif`: 6 shader properties, all six deriving the correct same-name
candidate (`x01_torso.bgsm` → `materials\actors\powerarmor\x01\x01_torso.pbrm`),
lookups running through the VFS. They report *not found* because that mod folder
is not a configured resource path in this profile — the derivation itself is
provably right: block 14's derived
`materials\actors\powerarmor\x01\PBRSpheres.pbrm` matches
`mods\PBR\Materials\actors\powerarmor\x01\PBRSpheres.pbrm` exactly. Add the mod
folder to the configured resources and it adopts.

Note `pbrm-resolve` restates the resolution rule rather than calling
`BSShaderLightingProperty`, which lives in the GL layer that `-no-gui` never
builds. The rule is four lines; keep the two copies in step.

**Still not rendering.** Resolution populates `pbrm`/`pbrmValid` on the property
and nothing consumes them yet. Remaining: `pbrm_default.{vert,frag,prog}` with the
editor's PBR core (the GGX/Smith half is already in `fo4_default.frag`), renderer
texture binding for BaseColor/Normal/RMAOS/Emissive, and connecting the disabled
**PBR: Roughness / Metallic** action.

## 2026-07-27b — `.pbrm` reader (PBR, step 1 of 2)

`src/io/pbrmfile.{h,cpp}` reads PBRM v5, scoped to the spec's own **"Minimal
Standard runtime slice"** — shader `Standard` plus the four Primary UV sockets.
Spec: `PBRMaterialEditorQt/docs/PBRM-v5.md`.

- Envelope: `PBRM` magic, version 5 (4 accepted), uint32 payload size that must
  consume the rest of the file **exactly** — truncation and trailing bytes are
  both hard errors — 64 MiB cap.
- **Fails closed**, as the spec demands: an unknown shader family or a
  `requirements` entry this build does not provide leaves the document *valid*
  but unrenderable (`ok` false, `unsupported` true). It is never coerced to
  `Standard` and never partially rendered. This build provides
  `standard.primaryUv` v1.
- Override semantics: a missing or unusable texture forces its positive
  `override*` on so the constant applies — **except `overridePorosity`**, the
  documented exemption, because an absent porosity source has a meaningful
  derived value. RMAOS alpha is F0 only while `alphaCarries` is `Dielectric F0`.
- Path contract: `/`→`\`, drop leading `.\`, collapse separators, lowercase,
  leading `textures\` optional; drive-qualified, UNC, root-qualified and
  parent-traversal paths are rejected — which disables that slot with a
  diagnostic rather than failing the whole envelope. Rejection happens *before*
  collapsing so a bad shape cannot normalise into an accepted one. The authored
  string is never rewritten.
- Feature mask derived, not serialised, using the spec's bit assignments.

**Verified against a real material** via a new CLI command, `nifskope-cli pbrm
<file.pbrm>` (exit 0 ok / 1 parse error / 3 unsupported, so scripts can tell them
apart). On `PBRSpheres.pbrm`: envelope v5, `Standard`, features `0x7b`
(BaseColor+Normal+Rmaos+Roughness+Metallic+AO), paths normalising as specified,
emissive slot correctly disabled, and `overrideF0 = 1` with alpha carrying
Dielectric F0 — matching what the editor's own UI shows for that material.

**Not done yet — step 2:** nothing renders from this. Still needed are material
resolution from the NIF (`nifextfiles.cpp`), a `pbrm_default.{vert,frag,prog}`
carrying the editor's PBR core, and enabling the reserved
**PBR: Roughness / Metallic** entry in the shading menu (`nifskope_ui.cpp` ~5648,
currently a disabled placeholder with no connection).

Note: `NifSkope.pro` needed a `qmake6` re-run for the new source to enter the
build.

## 2026-07-27a — FO4 spec/gloss lighting on the material editor's BRDF

First half of the renderer match (`RENDERER_MATCH_PLAN.md` §1). The FO4 lighting
shader now uses the PBR Material Editor's specular BRDF, ported rather than
approximated — `D_GGX`/`G1` in `res/shaders/fo4_default.frag` are the editor's
own functions:

    spec = D_GGX( NoH, a ) * G1( NoL, k ) * G1( NoV, k ) * F / (4 NoL NoV)
    a = rough^2      k = (rough+1)^2 / 8      rough = 1 - smoothness

What changed, and why each was wrong:

- **F0 was hardcoded to 0.2** — about 5x too reflective for a dielectric. Now
  0.04, the editor's dielectric default. FO4 encodes **no metallicity**, so `_s.R`
  stays what it is authored as, a specular *mask*, and never becomes a metalness
  channel. Applied to the ambient Fresnel rim term too, which had the same 0.2.
- **Normalized-Phong Torrance-Sparrow** (`exp2( smoothness * 10 + 1 )`) replaced
  by GGX + Smith. `TorranceSparrow`/`VisibDiv` are left in the file, unused, for
  reference.
- **Specular was gated on `hasSpecularMap`**, so a material with only the scalar
  Smoothness / Specular Strength authored rendered completely matte. Ungated.
- **No energy conservation**: the diffuse term now carries `(1 - F)`, so light
  taken by the specular lobe is not also counted as diffuse. The lobe shape stays
  Oren-Nayar rather than the editor's Lambert — it is roughness-aware and the
  rest of the FO4 path is tuned against it. Swapping it is a separate decision.

Channel semantics were already correct and are unchanged: G is gloss, R is the
specular mask, and a flat white `_s.G` falls back to the material scalar.
`fresnelSchlick` still honours the authored **Fresnel Power** rather than
hardwiring `^5` — that field exists in FO4 and the editor has no equivalent.
Not ported: the editor's `multiScatter` energy compensation, which needs a BRDF
LUT that NifSkope has no equivalent for.

**Verified by the render-regression set, which is the point of having built it:**
`particles_mist`, `particles_glow` and `glass_shader` are **byte-identical** — the
particle simulation and effect-shader paths are untouched — while only
lighting-shader surfaces moved (`lit_setdressing` 44k px at max channel delta 22,
`lit_head` 33k px, `glass_visor` 5.9k px). That is exactly the intended blast
radius.

**Two harness defects found while doing it, both fixed:**

- **The guard was silently not guarding.** Nothing pinned the window size, so the
  framebuffer followed whatever geometry the session restored and every
  comparison came back "size mismatch" rather than passing or failing. Now pinned
  to 1280x800, overridable with `WW_RENDER_SIZE=WxH`.
- **Shader-only edits do not deploy.** `copyDirs` in `NifSkope_functions.pri`
  hangs off `QMAKE_POST_LINK`, so `res/shaders` is copied to `release/shaders`
  only when the exe **relinks** — editing just a `.frag` changes nothing that
  make considers a dependency, and the old shader keeps running. Copy manually or
  force a relink.

To isolate the change honestly, the baselines were captured with
`git show HEAD:res/shaders/fo4_default.frag` installed, then the new shader
dropped in and compared — same binary on both sides, so the diff is the shader
and nothing else.

## 2026-07-26d — Procedural lightning reads the controller it was given

`BSProceduralLightningController` preview (added 07-06) ignored three authored
fields and misread a fourth. Measured against `X01_Torso_Tesla_VFX.nif` block 36
(Subdivisions 7, Num Branches 2, Num Branches Variation 1, Length 32, Length
Variation 0, Width 4, Child Width Mult 0.5, Arc Offset 12.5).

- ~~**Subdivisions is a recursion depth, not a segment count.**~~ **WRONG —
  tried, disproven, reverted.** Reading it as a depth (2^7 = 128 segments) makes
  each quad 0.2 units long on a bolt that is 4 units wide, i.e. 20x wider than
  long; consecutive quads then fan out as separate blades rather than a beam.
  Rendering the same file with Width forced to 20 made it unmistakable — a spray
  of loose rectangles. The original `Subdivisions + 1` rounded up to a power of
  two (8 segments here) is right. Kept, with the reasoning recorded so it does
  not get "fixed" again.
- **Num Branches Variation** was unread; the branch count was pinned to Num
  Branches. Now `Num Branches ± Variation`, re-rolled each mutation.
- ~~**Length / Length Variation** drive branch length.~~ **WRONG — tried,
  disproven, reverted.** Length is 32 on bolts that span 25.3 units, so branches
  came out *longer than the bolt itself* and shot off in directions unrelated to
  the end node (user: "why does the lightning extend into directions other than
  the end node?"). Branch length is back to a fraction of the main bolt
  (`0.2..0.45 ×`). Length most likely sets the main bolt length for controllers
  with no `_Start`/`_End` pair, where there is no node distance to span;
  `Child Width Mult 0.5` agrees that children are subordinate. Both fields are
  still read, just not used for this.

Verified correct, do not re-investigate: Generation/Mutation are
`NiBlendBoolInterpolator` stubs (blocks 37/38), so reading the real bool keys
from the sequences' Controlled Blocks — not the controller's own interpolator
links — is right; `_Start`/`_End` node resolution; BGEM tint and UV scroll.

Follow-up in the same batch closed the rest:

- **Interpolators 3–9 are now read** (`NiFloatInterpolator` → `NiFloatData`) and
  evaluated per frame into `eff*` values that feed generation; a change in a
  shape parameter forces a rebuild even when the bolt is not mutating. Width
  animates without a rebuild. `NiBlendFloatInterpolator` stubs are **skipped on
  purpose** — like Generation/Mutation their real keys live in the sequences, and
  with no asset here that uses one, guessing would animate the bolt wrongly
  rather than leave it static.
- **Interpolator ID partitioning bug.** Any Controlled Block whose ID was not
  `Mutation` fell into `genKeys`, so a sequence-driven *parameter* curve (Width,
  Arc Offset, …) would have been read as the Generation flag and switched the
  whole bolt on and off. Now only an empty ID or `Generation` counts as
  Generation — empty is how the shieldtesla sequences author it, which is why
  this file worked by accident.
- **Amplitude decay 0.55 → 0.5**, the classic midpoint-displacement halving. At
  the old 3 effective levels the difference barely showed; at the correct 7 it
  compounds into a visibly too-noisy bolt.

**Deliberately unchanged: `Animate Arc Offset` still gates re-mutation.** On
reflection that is a defensible reading of the field — re-randomising the arc
offsets over time *is* animating them — and the alternative (oscillating the
offset continuously) is invented behaviour with nothing to verify it against.
The mutation cadence stays a hardcoded 1/24 s for the same reason.

**Rigs without a `_Start`/`_End` pair now draw.** `updateTime` bailed when it
could not find a `*_Start` ancestor, so any bolt not following the
shieldtesla/edison_pa naming convention rendered **nothing at all**. When there is
no pair, the bolt is emitted from the target along its local **+Y** for `Length`.
+Y is the authored axis on the rigs that *do* have a pair (shieldtesla's End sits
at local `(0, +25, 0)` from its Start), and this is the reading of `Length` that
makes sense — it is redundant when two nodes already define the span, which is
why using it as a branch length was wrong.

**Sequence-driven parameter curves.** Interpolators 3–9 only resolved as direct
`NiFloatInterpolator`s. When the controller holds `NiBlendFloatInterpolator`
stubs — as Generation/Mutation do — the real keys live in the sequences'
Controlled Blocks, so each parameter now has its own bucket keyed by Interpolator
ID (matched case- and space-insensitively against the controller's field names;
unrecognised IDs are dropped, not guessed). A direct curve still wins.

**Recursive branching.** Branches now fork: `drawPreview` builds a world polyline
per bolt, parents before children, and a child roots on its parent's *jittered*
path taking its frame from the parent's **tangent** there — so a branch-of-a-branch
nests off the strand it grew from instead of being laid out in the main bolt's
frame. `Child Width Mult` compounds per level, which is presumably why the field
is a multiplier; about half of level-1 branches fork and depth stops at 2.

**Correction to the determinism work above:** seeding from an incrementing
mutation counter was wrong — it made the bolt depend on how many times
`regenerate()` happened to run before the frame was observed, which varies with
frame timing, and two runs of the same frame disagreed. Seeding from the
**quantised time** (`time × 24`) fixes it: verified three runs of the same frame
byte-identical, and t=2.0 still differs from t=1.0 so it animates.

**Tiling aspect comes from the texture.** `texAspect` was hardcoded to 8.0; it
now reads the bound texture's dimensions via the existing public
`TexCache::getTextureInfo()` and uses `height / width`, falling back to 8.0.
**Unconfirmed on this asset:** the render is byte-identical to before, which is
consistent with the beam sheet genuinely being 256x2048 (exactly 8:1, as the old
comment claimed) but is equally consistent with the lookup returning null and
falling back. Distinguishing the two needs one probe of what
`getTextureInfo( shaderProp->fileName( 0 ) )` actually returns — the name is a
material-resolved path, so it may not be the cache key. Do that before trusting
this on a non-8:1 texture.

**Bolt generation is deterministic.** Every `random()` call in generation is now
`tlBoltRandom()` (xorshift32), re-seeded per mutation from
`(block number, mutation index)`. Previously the global RNG made each rebuild
unique, so scrubbing the timeline backwards drew a *different* bolt and the
render-regression harness could not pixel-compare lightning at all. Verified: the
same file/time/sequence rendered twice is now byte-identical (matching MD5) —
impossible before. Zero-state guard included, since xorshift cannot escape 0.

**Ribbon UVs: arc-length parameterised, whole-tile wrap.** The subdivision fix
exposed a latent bug — V was derived from `t`, the parameter along the straight
Start→End *axis*, not from distance along the ribbon. That only survived because
the old 8-segment bolt was nearly straight, so `t` ≈ arc length. At 128 segments
the jagged path is far longer than the axis, so one span of texture smeared over
the whole zigzag (reported as "giga stretched"), and because each segment took V
straight from `t`, lateral jumps stretched V unevenly so the tiles never lined up
("not one UV strip, not seamless").

V now comes from accumulated arc length along the world polyline, and the tile
count is `round( totalLength / (width × aspect) )` — a whole number, so the strip
wraps seamlessly instead of ending mid-texture. The tip fade moved onto the same
normalised arc position, so neither tiling nor fade depends on how jagged the
bolt is. `bLen` survives only as the degenerate-length fallback.

**Branches jagged in the wrong plane.** `boltPoint` displaced every bolt along
the MAIN bolt's `v`/`w` axes, so a branch heading along `v` had its lateral
offset pushed down its own direction: it folded back through itself and read as
disconnected spikes rather than a continuous bolt (user: "the geometry of bolts
does not actually connect to each other"). Each bolt now gets an orthonormal
frame perpendicular to its own direction (`frameFor`), which is what the
midpoint displacement assumed all along — the main bolt was correct only because
its frame *is* `v`/`w`. Invisible at 8 segments and short branches; obvious once
subdivision and authored Length made branches long.

**Arc Offset was compounding, and the ribbon was flipping.** Measured: block 82
(`LightningBolt_01_End`) is a child of 81, so the bolt spans its local offset —
`|(2.43, 25.0, -2.90)| ≈ 25.3` units — while Arc Offset 12.5 was fed in as the
FIRST level's amplitude and then accumulated down the recursion
(12.5 + 6.25 + 3.125 + … ≈ 2 × Arc Offset). Lateral wander could therefore equal
the entire bolt length, so the path doubled back on itself continuously. Two
symptoms from one cause: it cannot read as a bolt spanning two points, and
`cross( camZ, d )` **flips sign** at every reversal, twisting the strip into
bowties that pinch to zero width — which is what "the bolts do not connect to
each other" looked like.

Arc Offset is now treated as the maximum excursion from the straight line: the
starting amplitude is divided by the geometric series so the accumulated
displacement sums to Arc Offset. And the ribbon carries its previous side
forward (`dot( perp, prev ) < 0 → negate`) so the winding stays consistent
through a reversal.

Exact engine parity remains unverified: everything here is derived from the NIF
field semantics and standard midpoint-displacement lightning, not from the game's
algorithm. If it still reads wrong beside the game, the decay constant, the
mutation cadence and `texAspect` (the assumed 8:1 beam sheet) are the knobs.

## 2026-07-26c — Play implies animation enabled (reported as "all animations broken")

User report: no animation plays, in any file. **Not a code regression** — the
cause was a persisted setting, `GLView/Enable Animations` = `false`, i.e. the
**View ▸ Animations** toggle (`aAnimate`). `restoreUi()` applies it after the
`toggled` connect is established in `initToolBars()`, so it really does reach
`GLView::updateAnimationState`, clearing `AnimEnabled` and setting
`scene->animate = false`. Both effects together match the report exactly:
`advanceGears()` stops advancing time *and* controllers stop being evaluated, so
the scene looks frozen rather than merely unplaying.

What made it undiagnosable rather than merely wrong: **every play path was a
silent no-op** while the toggle was off. The Timeline dock's transport and Space
in the viewport were both wrapped in `if ( ui->aAnimate->isChecked() )` and did
nothing, with no message; triggering `aAnimPlay` from the menu set `AnimPlay` on
an `animState` that still lacked `AnimEnabled`. One stray click in a menu
disabled playback permanently across sessions, with nothing to find.

Fix: **Play now implies animation enabled.** A lambda on `aAnimPlay::triggered`
checks `aAnimate` first, connected *before* the `updateAnimationState` slot so
`AnimEnabled` is set by the time `AnimPlay` arrives. The two `aAnimate` gates at
the call sites are gone, so the dock button and Space can no longer silently do
nothing.

Diagnosed entirely from the CLI, the registry and the source — the user was
working in Blender, so no GUI launches (see the note in 07-26b about a running
instance swallowing launches over IPC).

**Second cause, same report: the default sequence is the dead one.** FO4 VFX
files ship two sequences — a one-shot `autoPlay` and the real `autoLoop`.
In `X01_Torso_Tesla_VFX.nif`: `autoPlay` is Start 0.0 / **Stop 0.0333333** /
CycleType 2 (CLAMP), `autoLoop` is Start 0.0 / Stop 4.93333 / CycleType 0 (LOOP).
With `autoPlay` selected, Play advances one tick, exceeds the stop time, resets
and clears `AnimPlay` — one frame, then stop, which looks exactly like nothing
happening. `Scene` picks `animGroups.first()` (glscene.cpp:277), i.e. **block
order**, so these files open on the one-shot every time.

Confirmed sound while chasing this, so don't re-investigate: the
NiControllerManager path is fine — `Node::findChild` is recursive (nesting is
not a problem), the controlled blocks carry valid interpolator/controller refs
into the `NiMultiTargetTransformController`, and `Scene::setSequence` propagates
to it.

**Still open (offered, not built):** default to the longest or a CYCLE_LOOP
sequence instead of `animGroups.first()`, and say something in the status bar
when a sequence completes instantly instead of silently popping the play button.

## 2026-07-26a — Skin: the PBR Material Editor's palette

First step toward merging the **PBR Material Editor** (`PBRMaterialEditorQt`) into
NifSkope — planned for next month, renderer included. The two tools now share one
visual identity so the merge is a code move, not a restyle. **Skin only this
round: palette, surfaces, control styling. No layout change, no widget
restructuring** (the material editor's sidebar-navigator + framed-section
structure is deliberately deferred until the merge, when all the pages are known).

Two edits carry it:

- **`defaultsDark[6]`** (`nifskope_ui.cpp`) is now the material editor's palette:
  base `#303236`, alt `#2d3034`, text `#e6e8eb`, highlight `#3d6f9f`, highlight
  text white, bright text `#f0a54a`. Was 60/60/60 grey with a `#cccccc`
  highlight and red bright-text.
- **`res/style.qss`** rewritten in that language — panel/bar/card surfaces, view
  and header plates, inputs with focus borders, flat buttons, dock titles,
  splitters, scrollbars, tabs, group boxes, menus. The toolbar toggles' radial
  gradients are gone in favour of the editor's flat accent fill.

**`${...}` skin variables, not literals.** `loadTheme()` already substituted
`${theme}`/`${rgb}` into the sheet; it now also substitutes a 21-entry
`skinVars[]` table with a dark and a light column. One stylesheet serves both
themes — hardcoding the dark hexes would have left the light theme as dark
widgets on a light window. Verified both: light theme renders light surfaces with
dark text throughout.

**Palette migration (the thing that would otherwise have made this invisible).**
The six colour keys under `Settings/Theme/` are *persisted* — `setTheme()` writes
them and the General pane round-trips them — so on any install that has run
before, the stored values **outrank** `defaultsDark[]` and a new default palette
never appears. `loadTheme()` now refreshes the keys once per
`themePaletteVersion` (currently 2). **Bump it whenever the defaults change.**

Deliberately not styled, each for a reason recorded in the sheet's header
comment: fonts (a `font-family` in QSS outranks `setFont()` and would silently
kill the Settings ▸ General view-font picker), `QWidget` as a blanket selector
(it fills custom-painted widgets — GL container, timeline views), and `::item`
colours plus checkbox `::indicator` images (the palette already carries
Highlight/HighlightedText, and the item delegates paint their own foregrounds —
link blue, diff grey, pinned star — so leaving both alone means nothing fights
over them; Fusion draws a palette-correct tick without needing a check image).

Verified by screen capture of the real window (dark and light), not
`QWidget::grab()` — **`grab()` returns white for the GL viewport** because the
native surface isn't in the widget backing store, which makes an offscreen shot
look like a broken skin. `-platform offscreen` is worse: it crashes on GL context
creation (exit 139), so the `WW_UI_SHOT` comment recommending it is stale for any
run that has to show the viewport.

### 07-26b — the rest of it (same batch, user: "you have not changed the color everywhere")

The global sheet only reached widgets Qt styles. Two classes of surface were
still on the old greys, and both are now on the skin:

**The GL viewport** — the largest surface in the window. `cfg.background`
defaulted to a neutral `QColor(46,46,46)` which read cold beside the
blue-charcoal chrome; it now defaults to the skin's `viewport` colour
(`#2b2d31`, a step darker than the chrome so the model still reads as content).
Its stored key `Settings/Render/Colors/Background` is a *Render* setting with its
own persistence, so the palette migration now removes that override too —
otherwise the largest surface stays off-skin forever. **`themePaletteVersion` is
3**: revision 2 had already been written by the 07-26a binary, so the viewport
change needed its own revision to fire at all. Grid/Highlight/Wireframe colours
are untouched (independent settings).

**The per-widget stylesheets** — ~40 hardcoded hexes across seven files. The skin
table now lives at the top of `nifskope_ui.cpp` and is exposed to C++ as
**`wwSkinColor( "name" )`** (`src/wwskin.h`), returning the current theme's
column, so a sheet built in code follows Dark/Light like `style.qss` does.
Converted: the Block List breadcrumb/footer and diff banner, both DragSpinBoxes,
the gizmo/operator/box-select redo panels, the collision operator panel and
create-shape toggles and budget/preview labels, the UV operator panel, the Pose
Manager folder label, the shading channel toggles, the toolbar separators
(`#7a7a7a` → `border`; they were the brightest thing in the toolbar), and the
mode / Panels / Workspaces selectors — those three were **three copies of the
same greys** and are now one `wwBoxedButtonQss()`.

Four new entries carry semantics the QSS didn't need: `accentText`, `accentBg`
(the amber collision toggle), `danger` (invalid/error text), `viewport`.

**The flags dialog got smaller, not re-parameterised.** `wwFlagListDialog`
hardcoded an entire theme — dialog, inputs, buttons, header sections, tree — all
of which the global sheet now provides, and all of which was overriding it. Only
the 26px flag rows and their hover are dialog-specific, so that is all that is
left. Same idea applies wherever else a dock re-states the default surfaces.

Remaining literals are deliberate: `selection-color: #ffffff` (white on the
selection fill, correct in both themes) and uvtools' `"#FF393939"` placeholder
*texture* colour, which is image data, not chrome.

**Note:** `src/collisiontools.cpp` is a stale duplicate of
`src/spells/collisiontools.cpp` — identical colour lines, and **only the
`spells/` copy is in `NifSkope.pro`**. The un-built copy was left alone.

Verification note: `SetForegroundWindow` from a background process is refused by
Windows' foreground lock, so a screen capture of the window can silently grab
whatever app is actually in front — the first attempt captured Blender. Either
`PrintWindow(hwnd, hdc, PW_RENDERFULLCONTENT)` (misses alpha-blended GL passes)
or the harnesses' own `ogl->grabFramebuffer()` (true GL frame) is the right tool;
if using a screen grab, assert `GetForegroundWindow() == hwnd` before trusting
the pixels.

## 2026-07-22l — Load skeleton from a game archive (reuses the NIF Browser)

The Pose Manager's **Load skeleton** button is now a menu with **From file…** and
**From game archive…**. The archive path reuses the existing NIF Browser rather
than adding a new archive UI: it opens a modal picker backed by the **same
`bsaProxyModel`** (with a private filter proxy chained on top so filtering there
doesn't disturb the dock's own view), and extracts the chosen NIF's bytes with
the browser's own **`extractConfiguredNifBytes`** — the exact path the NIF
Browser uses to open an archived NIF. No unpack-to-disk needed.

Supporting changes:
- **`nifMergeData(target, bytes, label, dedupe, result)`** — merges a NIF held in
  memory. `nifMergeFile` and `nifMergeData` now both funnel through one internal
  `mergeDonor()` splice, so the file and archive paths are identical apart from
  where the donor bytes come from.
- **`NifSkope::pickNifFromBrowser()`** — the modal picker (public), and
  **`populateConfiguredNifBrowserNow()`** — the tree rebuild minus the
  dock-hidden deferral gate, so the picker gets a populated tree even when the
  NIF Browser dock is closed. (The public slot `populateConfiguredNifBrowser()`
  keeps its no-arg signature — a default-arg `bool` broke its Qt `connect()`s.)

Verified by `WW_MERGEARCH_TEST`: the "From game archive" menu action exists;
`nifMergeData` merging the current NIF into itself matches `nifMergeFile` exactly
(same blocksAdded / nodesReused / nodesAdded) and undoes cleanly; and
`pickNifFromBrowser` opens and cancels without hanging. The archive extraction
itself is the NIF Browser's already-proven path; end-to-end needs configured game
archives (user-validated with real BSAs/BA2s).

## 2026-07-22k — NifSkope Library folder in Settings (General → NifSkope Library)

Generalised the pose-library folder into a reusable **NifSkope library** root, so
future features can share one configurable location. **Settings → General →
NifSkope Library** has a "Library folder" field with a **Browse…** button. The
Pose Manager stores its poses in the library's **`Poses`** subfolder; future
features get their own subfolders under the same root, so nothing collides.

The settings field and the dock's Folder… button write the **same** key —
`Settings/Library/Library Folder` (the humanized form of the `libraryFolder`
line-edit object name, persisted by the General pane's generic read/write
machinery). Leaving it empty falls back to `Documents/NifSkope` (poses →
`Documents/NifSkope/Poses`). The dock's folder row now shows the library root's
name, with the actual Poses path in its tooltip.

Changing the folder in Settings and clicking Apply **live-updates** an open Pose
Manager dock (it listens to the dialog's `saveSettings` signal and re-lists).

Verified by `WW_POSESETTINGS_TEST` (settings tab + line edit + Browse exist; Apply
persists to exactly `Settings/Library/Library Folder`; the dock label updates to
the same root on Apply) and `WW_POSELIB_TEST` (save → `<root>/Poses` → list →
Apply → Delete round-trip through the real dock, against the new key).

## 2026-07-22j — Pose library is a folder of files

The Pose library section of the Pose Manager is now backed by a **folder on
disk** instead of poses buried inside the NIF. Default is
`Documents/NifSkope Poses` (falls back to the home dir if there's no Documents
location); the folder is created on first use. A **Folder…** button picks a
different one, remembered in `QSettings` under `Pose/LibraryFolder`, and a grey
label shows the current folder's name (full path on hover).

The library list **keeps its selection across refreshes** — the list rebuilds on
every model edit (a bone pose fires the undo-stack signal), and it now re-selects
the same file by path afterwards instead of clearing, so a chosen pose stays
selected while you work. (Found while verifying: without this the harness's
Apply/Delete hit a null selection.)

The library list shows the `*.xml` pose files in that folder (base name shown,
absolute path carried in `UserRole`, sorted by name). **Save current…** prompts
for a name, sanitises it for a filename, and writes an Outfit Studio pose XML
into the folder. **Apply** (or double-click) imports the selected file with the
Blend strength. **Delete** removes the file (with a confirm). Import/Export pose
dialogs default to the library folder too. Because the files are plain OS pose
XML, they interoperate with BodySlide/Outfit Studio and can be shared between
projects — nothing is stored in the model.

Verified end-to-end by `WW_POSELIB_TEST`: the `QSettings` folder override
persists; a bone is posed and Saved into the folder; nudging the dock's bone
search re-lists the folder and the saved pose appears; selecting it and clicking
the dock's real **Apply** button reproduces the posed bone rotation
(matrix diff ~0); clicking **Delete** removes the file. Also confirmed by
screenshot: dropping the three PA sample poses in the folder lists all three
("69 bone(s), 3 pose file(s)").

## 2026-07-22i — Pose Manager batch: weights, hover name, multi-select, pin, non-destructive, proportional editing, mirror axis

Large batch (user-requested, worked autonomously; all verified by harness/screenshot).

**Hover bone name** — the bone under the cursor is always labelled (below the
joint), even with the global Names toggle off.

**Search bar at the top** of the dock (was under the Bones heading).

**Multi-select with Block-List colours** — the bone list is ExtendedSelection
and two-way-synced with the viewport (guarded against a feedback loop); the
active bone shows orange (#FF9D00) text, other selected bones a blue row, exactly
like the Block List. Shift/Ctrl-click in the viewport accumulates a multi-bone
selection and G/R/S transforms them together (the object gizmo already handles
`objSelection`).

**Weight-influence overlay** (Weights toggle) — highlights the vertices the
hovered/selected bone drives, heat-coloured by weight (5 buckets, blue→red),
rebuilt only when the inspected bone changes so a 38k-vert mesh draws in a few
point batches. Verified: bone 14 → 79 influenced vertices. NOTE: drawn at BIND
positions (shape world transform × vertex) — exact for an unposed mesh, may lag
the deformed surface once bones are posed (a refinement, not a bug).

**Pin bones** — Pin/Unpin locks the selected bone(s): excluded from gizmo
transforms in pose mode (skipped in `addNode`), shown pale grey with a 🔒 marker
in the list. Unpin all clears them.

**Non-destructive posing** (default on) — leaving Pose Mode restores the real
bone nodes to the originals captured on entry (`poseResetBone(-1,7)`), so the
saved NIF is never left altered; the pose persists via a pose file / the library.
"Bake to bones" opts out (commits + re-captures rest). `poseBaked` guards it.
NOTE: this is the mode-boundary guarantee; a display-overlay version (nodes never
touched even transiently) would be a larger rearchitecture — flagged for the user.

**Proportional editing (edit mode)** — Blender's O. A vertex transform spreads
to unselected vertices within a radius by a falloff curve. All 8 curves ported
from Blender (Smooth/Sphere/Root/Inverse Square/Sharp/Linear/Constant/Random),
`O` toggles, `Shift+O` cycles the curve, context-menu entry. Implemented by
extending the existing follower-vertex pattern (same mechanism as X-mirror): a
`falloff` field on `ElemVert`, neighbours gathered in `gizmoBeginElement` within
the auto/explicit radius, and the three transform loops scale by falloff (move ×,
rotate by angle×falloff, scale interpolates toward 1). **Crucially a no-op when
off** — falloff 1.0 gives identical math, so normal transforms are unchanged
(all pose harnesses still pass). Verified: 256 neighbours gathered with a
distance-decreasing falloff. Auto radius = 25% of the shape's bounds.
(Reverses the earlier "proportional editing declined — do NOT implement" note.)
**Deferred: pose-mode bone proportional** — the object-gizmo path is more
intricate (parent-space) and I won't ship an unverified transform-path change;
the falloff infra is shared, so it's a bounded follow-up.

**Mirror editing axis choice** — the edit-mode X-mirror generalised to X/Y/Z
(Blender mirror axis): `mirrorAxis` + a context-menu submenu (Enabled + axis
radio), cache invalidated on axis change, both the pairing and the follower
generalised to negate the chosen component. Pose-bone mirror already existed
(`poseMirrorBone`). Topology-based pairing deferred (position pairing works for
symmetric meshes).

Harness `WW_POSEEXTRAS_TEST` covers the weight overlay + proportional gather;
`WW_POSEDRAW/OSPOSE/POSE/POSEDOCK` all still PASS (transform path unregressed).

## 2026-07-22h — OS pose XML: rotation is a VECTOR, not Euler (checked the source, fixed)

The 07-22g Outfit Studio import/export got the rotation math WRONG, and checking
Outfit Studio's actual source (`ousnius/BodySlide-and-Outfit-Studio`,
`src/components/PoseData.cpp`) proved it: a pose's `rotX/rotY/rotZ` is a
**rotation VECTOR** (axis × angle, Rodrigues) passed to `nifly::RotVecToMat`, not
three Euler angles. The 07-22g code used `Matrix::fromEuler` — which happens to
agree only for single-axis rotations (a finger curl `(0,0,1.69)` equals
`Rz(1.69)`), so it looked fine on finger-heavy poses but was wrong for any
multi-axis bone (HEAD, RArm_UpperArm...). The round-trip test couldn't catch it
because it used `fromEuler` in both directions — self-consistent, wrong transform.

**This is exactly why the 07-22g caveat said "the round-trip proves the
encoding-inverse, not cross-tool parity — verify visually." Checking the source
resolved it properly instead.**

Fix: `osRotVecToMat` / `osRotMatToVec` in animationsetup.cpp are **verbatim ports
of nifly's `RotVecToMat` / `RotMatToVec`**. Confirmed nifly's `Matrix3` and
NifSkope's `Matrix` use the IDENTICAL convention — `m[row][col]`, applied `M*v`,
row-major (checked `nifly/include/Object3d.hpp` `operator*` and NifSkope
`niftypes.h:1005`) — so the ported formula produces **bit-identical rotations to
Outfit Studio**, with no handedness/transpose guessing. Import uses
`base * osRotVecToMat(v)` (local post-multiply — a finger curl about the bone's
own axis, the correct pose semantics), export uses `osRotMatToVec(rest⁻¹·cur)`.

**Verified**: `WW_OSPOSE_TEST` round-trip now compares the bone ROTATION MATRIX
(representation-independent) — diff 4.5e-07. Importing the real `PAActionPose.xml`
and re-exporting reproduces the multi-axis HEAD bone
(`0.26999998,-0.02,0.039999999` → `...,0.039999988`) to float precision, and now
the underlying matrix is the correct one.

**Remaining (low-risk) assumption for a user visual check**: the pose delta is
applied as `rest * delta` (bone-local). This is the standard and is what makes a
finger curl work about its own axis, but only a look at a known PA pose on a real
PA body fully confirms the composition side; if off, it's a one-line swap in
`applyOutfitStudioPose`. The euler-vs-rotvec question — the real risk — is now
resolved.

## 2026-07-22g — Pose Manager: load skeleton, multi-select, depth fade, Outfit Studio pose XML

Four additions to Pose Mode.

**Load skeleton from file** (dock button): a QFileDialog → `nifMergeFile` (the
same de-dup merge as CLI `merge`), so a `skeleton.nif` or another armour piece
comes in with same-named bones SHARED. Reports bones shared / added; warns if 0
matched (pieces won't pose as one rig).

**Multi-select + transform together**: pose-mode click passes Shift/Ctrl to
`objectSelectClick`, so it accumulates a multi-bone selection — and the object
gizmo already transforms all of `objSelection`, so G/R/S moves/rotates/scales
every selected bone at once. The dock bone list is ExtendedSelection and
two-way-syncs with the viewport (guarded against a feedback loop). Verified:
2 Shift-clicks → 2 selected.

**Depth fade**: bones are drawn brightest near the camera, dim far away
(camera-space Z across the drawn set, near=1.0 → far=0.35 on colour + alpha, and
a slightly larger near joint dot). Selected/hover bones ignore depth so they
stay clear. Makes a dense cluster far easier to read and pick.

**Outfit Studio pose XML** (BodySlide `.xml`) — import and export, dock buttons
+ CLI `pose --import-os` / `--export-os`. The format (from real PA pose samples
the user supplied): `<PoseData><Pose name><Bone name rotX/Y/Z transX/Y/Z/></Pose>`,
where rotations are **Euler radians** and everything is a **delta from rest** —
only posed bones are listed, names are the FO4 skeleton NiNode names.
`AnimSetup::applyOutfitStudioPose` / `writeOutfitStudioPose`: import composes
`target = base * fromEuler(delta)` (local post-multiply, `base` = the rest
captured on pose-mode entry, or the current bind pose standalone); export writes
`base.inverted() * cur` → `toEuler`, only for bones that actually moved, sorted
by name to match OS. Export rest is keyed by BLOCK (not name) so same-named
bones can't diff against the wrong node — a bug the first name-keyed version hit
(`LArm_Collarbone_skin` exported a bogus delta).

**Verified**: `WW_OSPOSE_TEST` round-trips a posed bone through export→reset→
import to 6e-08. And decisively — importing the user's real `PAActionPose.xml`
and re-exporting reproduces the source: HEAD `0.26999998,-0.02,0.04` →
`0.26999995,-0.019999998,0.040000007` (float-exact). All prior pose/pinned
harnesses still PASS.

**Honest caveat (in the docs, needs a user check)**: the round-trip proves the
import↔export is a faithful inverse of OS's *encoding*, but not that NifSkope's
`fromEuler` axis order matches Outfit Studio's when applied to a real skeleton.
If a known PA pose looks twisted on a real PA body, the fix is the Euler
order/compose side (a localized change in `applyOutfitStudioPose`). Only a
visual check on a real PA skeleton (user has these) can confirm cross-tool
parity. CLI export standalone diffs against the current pose (rest == current →
writes nothing) unless it follows `--import-os` in the same run; OS export is
primarily a GUI action, where pose mode captures the rest on entry.

## 2026-07-22f — Pose Mode: viewport skeleton, click-to-pose, reset, labels, filter, X-mirror

The Pose Manager grew from a bone LIST into a real viewport posing tool. New
**Pose Mode** — a viewport mode alongside Object / Edit / the paint modes,
listed in the mode dropdown and activated by the Pose workspace (like the paint
docks drive their modes). GLView members `poseMode` + friends; `posetools.cpp`
dock rebuilt around it.

**Draw** (`GLView::drawPoseSkeleton`): every skinned-shape bone is drawn as a
short capped shape from its head toward its tail (mean of child origins, or
local +Y for a leaf), with a joint dot, plus dim dashed parent-relationship
lines — Blender's bone + relationship-line display. The tail length is capped to
a characteristic bone size (median nearest-neighbour distance × 0.6), so a bone
parented to a far-off root doesn't stretch across the screen; the first cut did
exactly that and looked like spaghetti.

**Pick** (`poseBoneAt` + a branch in mouseReleaseEvent): a click resolves the
nearest bone segment in screen space and selects that node, so the existing
G/R/S poses it. Hover highlights the bone under the cursor. Picking and drawing
share `poseBoneTail`, so what you see is what you click.

**Reset** (`poseResetBone`): bone transforms are snapshotted on entering pose
mode = the "rest". Reset bone / Reset all restore it, with a channel selector
(All / Rotation / Location / Scale — Blender's Alt+R/G/S). Snapshot-undoable.

**Labels + filter**: bone-name labels in the viewport (QPainter overlay), a
relationship-line toggle, and a filter (All / Deforming / Face sculpt =
`skin_bone_*`). On the 68-bone facial rig, Face-sculpt + Names turns 70 bones of
clutter into a clean labelled set of the sculpt handles.

**X-mirror** (`poseMirrorBone`): copies a bone's pose to its L/R counterpart,
mirrored across X. Mirrors the source's motion RELATIVE TO ITS REST and applies
that mirrored delta relative to the counterpart's rest (Mx·R·Mx, translation.x
negated), so it works on rigs that are symmetric in motion. `riggingFlipBoneName`
gained the FO4 facial `_L_`/`_R_` INFIX pattern (it only had prefix/suffix/Left-
Right) — a strict improvement that also helps weight-paint mirror.

**Verified** by `WW_POSEDRAW_TEST` (log `ww_posedraw_test.log`, framebuffer +
screenshot): 70 bones drawn, overlay changes ~9.5k px, `poseBoneAt` resolves a
bone at its own drawn position, a synthetic click selects it, reset returns a
posed bone to rest (delta 0), and mirroring `skin_bone_L_Cheek` moves
`skin_bone_R_Cheek` symmetrically. `WW_POSE_TEST` / `WW_POSEDOCK_TEST` /
`WW_POSEHIER_TEST` / `WW_PINNED_TEST` all still PASS.

**Bugs found and fixed along the way**:
- **HiDPI pick offset**: `poseBoneAt`'s pick radius multiplied by
  `devicePixelRatioF()`, but `worldToScreen` works in LOGICAL pixels (uses
  `width()`/`height()`), same space as the event position — so the dpr factor
  was wrong. Dropped it. (The symptom only showed via a synthetic click that
  also had a dpr conversion error; both are now logical-space.)
- **Reset wrote zeros**: the first `poseResetBone` built the new value with
  `nv = oldVal; nv.set(value, model, item)` and it came out `(0,0,0)`. Rather
  than chase the `NifValue::set` subtlety, switched to the known-good
  `nifSnapshotOp` + `model->set<T>` path used everywhere else in glview.cpp.
- **modelChanged refresh**: entering pose mode during dock init (empty model)
  left a stale/empty bone list; `GLView::modelChanged` now re-runs
  `refreshPoseBones` (and captures rest if none yet) so a reload repopulates it.

`WW_UI_SHOT` gained `WW_UI_SHOT_DOCK=<objectName>` and `WW_UI_SHOT_POSE=1` to
open a dock / enter pose mode before the grab.

## 2026-07-22e — Verified: bone-by-bone posing is usable (hierarchy + cumulative)

`WW_POSE_TEST` proved a bone moves the mesh; this proves the two properties that
make posing bone-by-bone *practical* rather than tedious. New harness
`WW_POSEHIER_TEST` (log `release/ww_posehier_test.log`):

- **Hierarchy** — rotating a bone's ANCESTOR carries the bone and its skinned
  armour along (shoulder→arm→hand). Proven by composing each bone's world
  transform from the model and confirming a child bone's world position moves
  when only an ancestor is rotated. (On the flat facial-rig fixture the only
  ancestor is the root; a real body `skeleton.nif` has the deep spine/limb
  chains, where this is what lets you pose a whole arm from the shoulder.)
- **Cumulative** — transforming several bones stacks into one pose rather than
  overwriting: three bones rotated in turn each further moved the skinned bounds
  (3 of 3).

No code change to the engine — this is characterisation of existing behaviour,
kept as a regression guard.

## 2026-07-22d — Pose Manager dock + pose blending

The Pose Manager workspace, finishing the two backlog entries (#2) that used to
be a disabled menu placeholder. It is presentation over the shared pose API from
07-22c, so there is little new model-layer risk.

**Blending** first, in `AnimSetup::applyPose( nif, name, blend, error )`: `blend`
< 1 interpolates each bone from its current transform toward the pose
(translation/scale lerp, rotation `Quat::slerp`) — Blender's pose-strength
slider. Verified numerically: with a bone at the origin and a pose at Z=91.2848,
`--blend 0.5` lands it at Z=45.6424 (exactly half) and `--blend 1.0` at the full
value. `readPose()` was factored out so both apply and the dock read a pose the
same way. CLI gains `pose --apply NAME --blend F`.

**The dock** (`src/posetools.cpp`, `tlCreatePoseManagerDock`): a bone list that
drives block-list/viewport selection (click a bone, then G/R/S poses it — the
practical form of "bone picking"; clickable 3D bones are Skeleton Manager work),
plus a pose library with Save current / Apply / Delete and the blend slider.
Flat NifSkope visual language, no web-app chrome. Delete hands the sequence to
the existing `Block/Remove Branch` spell rather than open-coding block removal.
Enabled as the 8th workspace (Workspaces ▸ Pose); the "Pose Manager (Planned)"
placeholder is gone, added to both the workspace and mutual-exclusion manager
lists.

**Verified** by `WW_POSEDOCK_TEST`: dock found, bone list populated 69 rows,
"Save current" (auto-answering the name dialog) added a pose to the library AND
created a real `DockPose` sequence in the model, then Apply at 50% ran. A
`WW_UI_SHOT` capture confirms the layout renders correctly. `WW_POSE_TEST` and
`WW_PINNED_TEST` still PASS.

Also: `WW_UI_SHOT` now honours `WW_UI_SHOT_DOCK=<objectName>` to open a dock
before the grab, so any dock can be screenshot for verification.

## 2026-07-22c — Pose library (`pose`), and a silent-corruption fix in `set`

The last gap in the load-screen workflow. Poses are stored exactly as
`SKELETON_AND_POSE_PLAN.md` §B.1 specified — **a `NiControllerSequence` with one
key per bone at t=0**, the NIF equivalent of a Blender Action holding a single
frame — so they live in the file, appear in the Timeline, and export like any
other animation. No new format was invented.

New API in `AnimSetup` (`src/spells/animationsetup.h`), so the dialog, a future
Pose Manager dock and the CLI all share one implementation:

- `poseBoneNodes()` — every node some skinned shape is bound to.
- `savePose()` — capture the current bone transforms into a named sequence.
- `applyPose()` — write a pose's t=0 transforms back onto the bone nodes.

CLI: `pose <file> --list | --save NAME | --apply NAME -o OUT`.

Key writing follows `NiKeyframeData`'s real layout: `Translations` and `Scales`
are `KeyGroup`s, but rotation is **not** — `Num Rotation Keys` + `Rotation Type`
gate a `Quaternion Keys` array, and the type must not be 4 (XYZ) or that array
is conditioned out. The interpolator's own `Transform` is filled as well as the
keys, so a one-key pose is unambiguous to any reader; `applyPose` prefers the
first key and falls back to `Transform`.

**Verified** on the merged file: 69 bones detected, `--save TPose` created the
sequence and it listed back; then `set` moved the `Chest` bone to
(99, 42, 7) and `--apply TPose` restored it to exactly
`X 0.000024 Y 0.539375 Z 91.284805` — the original, to the last digit.

### Silent-corruption fix in `set` (found by that test)

Passing `-v "X 99.0 Y 42.0 Z 7.0"` **wrote zeros and reported success.**
`Vector3::fromString` (`niftypes.cpp:91`) splits on **commas**, and on a
mismatch it simply `return`s, leaving the default-constructed all-zero value —
while `NifValue::setFromString` still returns `true`. Every compound type
behaves this way.

`set` now validates the component count and numeric parse for the vector /
quaternion / colour families *before* writing, and refuses with the expected
format instead of silently zeroing a field:

```
error: Vector3 takes 3 comma-separated numbers, e.g. -v "0.0,0.0,0.0"  (got: X 99.0 Y 42.0 Z 7.0)
```

Worth remembering beyond the CLI: **`setFromString` returning true does not mean
the string was understood.** Any code path that accepts user text for a compound
value needs its own shape check.

## 2026-07-22b — Posing already works: verified, Pose Manager scope cut

Before building Part B of `SKELETON_AND_POSE_PLAN.md`, checked whether posing
needed building at all. **It does not.** `Shape::updateBoneTransforms()`
(`glshape.cpp:110`) derives each bone matrix from
`bone->localTrans( skeletonRoot )` — the **live** node transform, not a baked
bind pose — and `BSShape::transformShapes()` re-runs it whenever the scene
transform is dirty. Combined with block-list selection resolving any
`NiAVObject` (so G/R/S already transforms a bone node), **selecting a bone and
rotating it poses the mesh today, with no new feature.**

New harness `WW_POSE_TEST=1` (log `release/ww_pose_test.log`) proves it on the
merged file from 07-22a, and proves the merge's de-duplication in the same run:

```
2 skinned shape(s) in the scene
posing bone block 1 'Chest'
skinned bounds delta: 6.71363          before c(-0.004,1.76,118.33) r14.38
                                       after  c(-0.006,3.83,115.39) r17.50
framebuffer pixels changed: 7938
  shape 73 'BaseFemaleHead_faceBones:0' uses the bone, moved 6.71363
  shape 81 'skin_bone_C_Adam'sApple'    uses the bone, moved 6.33418
pieces bound to that bone: 2, pieces that moved: 2
after restore, bounds delta vs original: 0
PASS
```

Three independent signals: the skinned **bounds** move (the maths saw it), the
**framebuffer** changes (it reaches the screen), and restoring the bone returns
the delta to **exactly 0** (so it was the edit, not frame noise). The
per-shape check is the one that matters for an armour set — *both* merged
pieces follow the shared bone, by different amounts because they are weighted
differently. Had merge's node de-duplication failed, the second piece would have
been bound to a private copy and stayed put.

`Shape::boneTransforms` and `boundSphere` are protected; the public
`Node::bounds()` is recomputed by `updateBoneTransforms()` from the same live
matrices, so it is the right observable from outside.

**Consequence for the plan:** Pose Manager Part B loses its largest item. What
remains is the *library* — capture the current bone transforms as a one-key
`NiControllerSequence`, then list / apply / blend / mirror — plus viewport
convenience (picking bones in the viewport rather than the block list). The
posing engine is done.

**Working agreement changed** (user, 2026-07-22): *"Do it yourself please, I'm
busy."* The agent now performs its own GUI verification via harnesses rather
than handing the user a test checklist. `CURRENT_STATUS.md` updated.

## 2026-07-22a — Merge NIFs into one poseable file (`merge`)

Requested for the load-screen workflow: open an armour piece, bring in the rest
of the set and a `skeleton.nif`, then pose the whole thing as one rig. Merging
was the missing first step. New `src/nifmerge.{h,cpp}` + CLI `merge`.

The splice recipe is the proven one from
`spCollisionManager::importDonorCollision` (collect branch → `saveIndex` each
block → `insertNiBlock` + `loadAndMapLinks` through a donor→target block map).
**The addition is de-duplication by node name**, and it is the whole point of
the feature: a naive splice gives every piece its own private copy of the bones
it is skinned to, which renders fine but cannot be posed — moving "Chest" would
mean moving five copies. Mapping a donor `NiNode` onto the target's same-named
node makes the link-remap re-point every skin's `Bones` array at the shared
bones **for free**, with no skin-specific code.

Only `NiNode`s de-duplicate; shapes always import (two pieces may legitimately
share a shape name). Imported blocks whose donor parent de-duplicated away are
explicitly re-parented via `blockLink`, since the surviving parent's `Children`
array lives in the target and knows nothing about the newcomer.

**A bare `skeleton.nif` is just NiNodes, so loading a skeleton is the same
command.** Worth stating because `Rigging ▸ Import Donor Bone Nodes...` cannot
do it — that spell requires the donor to contain *skinned shapes*, and a
skeleton file has no meshes at all.

**Verified** on two skinned FO4 fixtures: 9 nodes reused by name + 1 new node
added; the merged shape's skin `Bones` resolve to blocks 1/2/3/8/9/10/11 — the
*target's* originals — plus 80, the one genuinely new bone; Skeleton Root → 0;
geometry preserved at 1689 verts / 3230 tris; and `verify_join.py` **PASSES** on
the result, so skin counts are consistent and every per-vertex bone index is in
range (the Join-era corruption check).

**Perf note applied up front:** the merge is a bulk load into a live model, so
it wraps the splice in `setState(Loading)` + `holdUpdates`. `loadAndMapLinks`
does not suppress signals for you and `holdUpdates` alone does not stop per-leaf
`dataChanged` — only the model state does. Without it a 38k-vertex piece would
look like a hang.

Reported counts make a silent failure loud: if **0 nodes are reused** the pieces
do not share a skeleton and posing as one rig will not work, so the command says
so explicitly.

## 2026-07-21f — Animation rigging from the CLI (`anim-setup`)

Backlog §13's top item. **Setup Controllers is no longer dialog-bound**: its
implementation moved out of `spSetupControllers::cast` into
`AnimSetup::setupControllers()` behind the new `src/spells/animationsetup.h`,
with the dialog as one caller and the CLI as another. The block-graph work was
always GUI-free — controller + interpolator + data, ControlledBlock in a
sequence, `NiDefaultAVObjectPalette` entry, manager/palette created if missing —
only the parameter entry was stuck behind a modal.

`CtlrKind` / `CtlrOption` and the per-block-type controller table moved to the
header as `AnimSetup::controllerOptions()`, so the CLI offers exactly the same
set the dialog does rather than a parallel list that could drift.

**One behavioural improvement fell out of the split:** the core resolves the
target sequence **by NAME**. The dialog passed a combo *index*, which is
meaningless to any caller that never saw the combo.

New command:

```
anim-setup <file> -b N --list
anim-setup <file> -b N --controller TYPE [--controller TYPE ...]
      [--sequence NAME] [--new-sequence] [--standalone]
      [--effect-var 0..9] [--int-var N] -o OUT
```

**Verified end to end** on `donor.nif` (details in `CLI.md`): rigging a
NiTransformController into a new `autoLoop` on an unrigged file takes it 80 → 87
blocks with exactly the right scaffolding (`NiControllerManager`,
`NiControllerSequence`, `NiDefaultAVObjectPalette`,
`NiMultiTargetTransformController`, `NiTextKeyExtraData`, +1 interpolator/data
pair); the ControlledBlock points at interpolator 85 and controller 81; the
palette entry carries the node name and resolves to block 0; adding a second
node by sequence name takes controlled blocks 1 → 2 and palette objs 1 → 2; an
unknown sequence name errors with exit 1. Independent check:
`Animation/Fix Invalid AV Object Refs` is a **no-op** on the result (file hash
unchanged), so the generated refs are valid by the project's own validator.

Regression: `WW_PINNED_TEST` and `WW_VERTEXFLAGS_TEST` both still PASS.
**GUI verification of the Setup Controllers dialog is pending** — it now routes
through the shared core, and only the user can exercise the modal.

## 2026-07-21e — Headless CLI (`NifSkope -no-gui`)

Batch mode for the same binary, filling the `// Future command line batch tools
here` slot upstream left in `main.cpp`. New `src/nifcli.{h,cpp}`, wrapper
`release/nifskope-cli.cmd`, full docs in **`CLI.md`**.

Commands: `spells` (list the 195 registered spells by `"Page/Name"`, tagged
instant/constant), `info`, `list`, `dump`, `get`, `set`, `cast`. Field paths are
`/`-separated with numeric segments indexing arrays —
`-f "Bone List/0/Bounding Sphere/Radius"`, the same convention as the Block
Details sticky state and pinned fields.

**Verified end to end** on `tests/rigging/fixtures/donor.nif`: `get` reads
1689 verts; `set` writes Flags 14→15 and a reload of the saved file reads 15;
`cast "Mesh/Update Bounds"` recalculates bone[0] radius 4.89446→4.75802 and
changes the file hash; non-applicable targets and bad paths error with exit 1.

**Scope, and it is a hard boundary.** Spells and model edits work — which
covers animation rigging (controllers, sequences, interpolators, keyframe
arrays, palette entries), skinning, bounds, sanitising. The viewport modelling
tools (extrude, loop cut, knife, join, separate, bevel) do NOT: they live on
`GLView`, need picked-element state and a GL context, and are GUI-bound by
architecture, not by omission.

**Three things that bit during the build, all recorded in `CLI.md`:**
- `Game::GameManager::get()` is deliberately NOT initialised — its scan builds a
  `QProgressDialog` (`gamemanager.cpp:150`), fatal without a `QApplication`.
  Nothing in the model layer needs it.
- The exe is linked `-subsystem,windows`, so it has **no console** and neither
  shell waits for it: a bare `> out.txt` returns before the work happens and
  leaves an empty file. Handled by `AttachConsole(ATTACH_PARENT_PROCESS)` (only
  when not already redirected, so pipes still work) plus the `start /b /wait`
  wrapper. Piping in PowerShell also forces the wait.
- `dump` filters rows by `evalVersion`/`evalCondition` like the GUI's row
  hiding. Without it a `BSVertexData` row prints BOTH precision variants of
  `Vertex` — the live one and a zeroed dead one — and a healthy mesh reads as
  corrupt.

**Testing trap worth knowing** (cost a false "the spell did nothing"): on a
*skinned* shape `spUpdateBounds` writes a ZERO bounding sphere on the block by
design (`mesh.cpp:2032` — `calculateBoneBounds` succeeds so the vertex branch is
skipped), because FO4 keeps real bounds per bone in `BSSkin::BoneData`. Reading
`Bounding Sphere/Radius` shows 0 before and after and looks like a no-op; the
evidence is in `BoneData`.

**Follow-up worth taking:** the eight `WW_*_TEST` harnesses each hand-roll
load → act → verify → quit in `nifskope_ui.cpp`. Most of that is now expressible
as CLI calls plus a script, which would shrink that file substantially.

## 2026-07-21d — One backlog: seven plan docs consolidated

Doc-only, no code. `TO_BE_IMPLEMENTED.md` is now **THE backlog** — a single
verified inventory of everything left, replacing seven documents that each
independently claimed to know what was open and several of which were wrong.

**Why.** Over 07-21 a single review turned up **nine** stale claims across
these files, in both directions: six Blender-batch items marked open that were
fully implemented; rest-pose display marked open when it had shipped; a "model
landmine" that was an untested conjecture and proved false; and — the largest —
**all 43 of `TIMELINE_PLAN.md`'s unticked boxes, every one of which shipped in
timeline v2**. Plus the converse failure: Skeleton Manager and Pose Manager,
the two biggest un-started features, appeared in *no* backlog file at all.

**Structure.** The new file leads with usage rules (verify against code; the
code is the only complete inventory; disabled UI entries are load-bearing
backlog), a summary table of all 12 open areas by size, then a section each —
ordered Skeleton Manager, Pose Manager, Performance 15c+16, Collision P4, the
Block Details / Block List / rigging / UV / animation / rendering remainders.
It closes with three lists that stop the failure recurring: **Verified SHIPPED
— do not rebuild**, **Explicitly declined — do not implement**, and **Awaiting
GUI verification (not implementation work)**. 674 lines → 485.

**Nothing was lost.** The Collision Manager feature spec existed only in
`TO_BE_IMPLEMENTED.md`, so it is preserved verbatim in that file's appendix.
The seven plan docs stay on disk as **design detail / history**, each now
carrying a banner that points at the backlog and warns against trusting its own
status claims. `TIMELINE_PLAN.md`'s banner states outright that its `[ ]` boxes
are wrong. `COLLISION_MANAGER_HANDOFF.md` keeps its role as required technical
reading. `WW_CHANGES.md` (history) and `CURRENT_STATUS.md` (handoff) keep
theirs; the latter's "Deferred/future work" list is explicitly retired as a
backlog location.

## 2026-07-21c — Backlog: the two missing workspaces, and a doc-scope lesson

Doc-only. **Skeleton Manager and Pose Manager** — reserved as disabled
workspace entries since the workspace batch (`nifskope_ui.cpp` ~L4747) — were
recorded ONLY in `CURRENT_STATUS.md`'s "Deferred/future work" list and were
missing from `TO_BE_IMPLEMENTED.md` entirely. A backlog review driven by the
backlog file therefore missed the two largest un-started features in the
project. Both now have real entries there, including the observation that the
open "independent persistent skeleton reference" rigging item is really a
Skeleton Manager prerequisite, and that Pose Manager depends on Skeleton
Manager (building it first would duplicate the bone-transform machinery).

Also corrected: **Rest-pose display in edit mode** was still listed as open
("`Scene::restPoseBlock` already stored" — implying no consumer). It shipped:
`Node::viewTrans`/`worldTrans` return `restWorldTrans()` for the edited shape
(`glnode.cpp:337/353`, five writers in `glview.cpp`). That is the 8th stale
backlog claim found on 07-21.

**Lesson, worth more than the entries:** 07-21a established "verify against the
code before building what a plan lists as open." This adds the converse —
**a doc can be wrong by omission, and the code is the only complete inventory.**
The disabled/planned UI entries (`setEnabled(false)` workspace actions, greyed
menu items) are load-bearing backlog that no markdown file listed.

## 2026-07-21b — Block Details: pinned fields

`BLOCK_DETAILS_OVERHAUL_PLAN.md` §4's pinned fields, built on the "improve the
existing tree in place" decision rather than the curated-sections concept.

Star the handful of fields you actually tune on a block type, then filter down
to just those on every block of that type: select block, scrub, next block.

- **Pin / Unpin Field** in the Block Details context menu. Offered for any field
  under a block, not only leaves — starring a whole compound (a Bounding Sphere,
  say) is a legitimate thing to want.
- **Per block TYPE, stored as a field PATH** — `NifSkope::wwFieldPath` reuses the
  sticky-expansion convention (`'\x1f'`-joined; array elements identify by row,
  everything else by name), so a pin set on one `BSLightingShaderProperty`
  resolves on every other one. Item pointers and row numbers would not survive
  the block switch, which is the entire point of the feature.
- **★ marker** on the pinned row's Name cell (`PIN_QSTRING` in nifmodel.cpp) —
  a marker, not a badge, per the binding visual rules.
- **★ toggle** beside the details filter shows only pinned rows: flat,
  auto-raise, matching the Block List header buttons. It reuses the proven
  `NifTreeView` keep-set (`setDetailsFilter`) rather than inventing a second
  hiding mechanism — the keep set is the pinned rows, their subtrees (a pinned
  compound must expand) and their ancestors. With text also typed, the search
  narrows *within* the pinned set.
- **Persisted** under `QSettings` `BlockDetails/PinnedFields` (type → sorted
  path list), loaded at dock construction.
- `NifModel::pinnedItems` holds only the CURRENT block's resolved items,
  recomputed on every block switch — the same window-owned pattern as
  `diffItems`/`selHighlight`; the model just serves it per-role.

**Harness** `WW_PINNED_TEST=1` (log `release/ww_pinned_test.log`): pins a field
on block A, asserts the pin follows to block B of the same type, that the star
reaches the Name column's display text, that the filter leaves exactly the
pinned row visible, that unpinning undoes all of it, and that a settings
save/load round trip preserves the pin. Green on `donor.nif` (NiNode ▸ Flags:
11 top-level rows → 1).

**Harness gotcha (cost one false failure).** `NifTreeView::isRowHidden(int row,
const QModelIndex & index)` marks `row` `[[maybe_unused]]` and reads
`index.internalPointer()` — it expects the ROW'S OWN index, not `(row, parent)`
as the QTreeView signature it shadows implies. Asking it `(r, blockRoot)`
reports on the block row itself and reads "everything hidden". To check what the
user actually sees, call the base explicitly: `tree->QTreeView::isRowHidden(r,
parent)`.

## 2026-07-21a — Vertex Flags landmine: tested, DISPROVEN (no code change)

`WW_CHANGES 2026-07-18b` closed the Create Skin corruption with a warning that
*"any spell doing `set<BSVertexDesc>` + `updateArraySize` on an unchanged vertex
count is suspect — incl. the stock Vertex Flags spell (flags.cpp)"*. That
warning propagated into `CURRENT_STATUS.md` and `TO_BE_IMPLEMENTED.md` and has
sat there since as an open data-corruption risk. **It was a conjecture and was
never tested. It is now tested, and it is wrong: the stock Vertex Flags spell
is correct.**

**New harness** `WW_VERTEXFLAGS_TEST` (nifskope_ui.cpp) + verifier
`tools/vertexflags_test/` (README has the full recipe). It casts the REAL spell
through `NifSkope::castSpell` — the path the Block Details context menu uses —
with a timer ticking its modal checkbox dialog, so the whole shipping path runs
including the `getVertexPositions`/`setVertexPositions` round trip.

Two independent layers: in-model (row layout matches the new desc, and every
vertex position survives), and on-disk — the verifier does not trust the model
that wrote the file, instead asserting against the header's **Block Sizes**
table that `saved − original block size == numVerts × (new stride − old stride)`.
Had the rows been left in the old layout, that delta would be 0.

**Gauntlet, all green** on `tests/rigging/fixtures/donor.nif` (FO4 bs130,
1689 verts, half precision, skinned, no colours):

| case | toggle             | stride | block size    | result |
|------|--------------------|--------|---------------|--------|
| A    | Colors ON          | 32→36  | 73828→80584   | PASS   |
| B    | Full Precision ON  | 32→40  | 73828→87340   | PASS   |
| C    | Colors OFF         | 36→32  | 80584→73828   | PASS   |
| D    | Full Precision OFF | 40→32  | 87340→73828   | PASS   |

0 of 1689 positions moved in every case, and **both round-trips (A→C, B→D)
reproduce the input file byte for byte** (SHA-256 identical, 108,302 B).

**Why the early return is harmless here.** `updateArraySizeImpl` does bail on an
unchanged count (`nifmodel.cpp:626`), but it is not the rows that need
rebuilding — the row *count* genuinely does not change. Every `BSVertexData`
field variant is already materialised as a `NifItem` in every row; the
`#ARG#`-gated conditions only decide which are live. Writing `Vertex Desc`
re-evaluates them (the `Vertex Data` array's `arg="Vertex Desc #RSH# 44"` names
the field, so `invalidateDependentConditions` reaches it) and the serialiser
writes the new layout. The spell also carries positions across a precision
change by discriminating on `valueType()` rather than by name — the correct
handling of the two `Vertex` variants.

The Create Skin bug was a genuinely different failure: it *created* skin arrays
that had zero children until the deferred cascade at `holdUpdates(false)`, not
flip a live/dead bit on already-materialised fields. **Do not "fix" the Vertex
Flags spell.**

**Harness gotcha worth keeping.** The probe that reads the row layout back is
subject to the very hazard under test: `"Vertex"` names BOTH precision variants,
so `getIndex(row0, "Vertex").isValid()` is true either way. The first cut of
this harness reported a false FAIL for exactly that reason. The Full Precision
probe now asks the live item for its `valueType()`; only the Colors probe can
lean on a unique name. Harnesses also now leave an app-owned dialog answerer
running past their own scope, so a save-changes prompt on quit cannot strand the
process (the earlier version hung after logging its verdict).

**Scope tested:** FO4 `bs130` `BSSubIndexTriShape`. Not exercised: SSE `bs100`
(the `NiSkinPartition` mirror branch at flags.cpp:1531) and `BSDynamicTriShape`
(`MakeDynamic`). Those paths remain unverified, not known-bad.

## 2026-07-20o — Loop Cut v3: single-vertex edge cut on tris, full adjust panel, edge-mode A

- **Plain triangles: single-vertex cut (Blender)** — when the hovered edge
  has no marked-quad ring, Ctrl+R degenerates to Blender's single-vertex
  placement: the preview is a yellow dot on the hovered edge (so the
  preview now FOLLOWS the mouse everywhere instead of freezing at the
  last quad ring), and confirming splits just that edge — `tlApplyEdgeCut`
  adds the cut vert(s) and fans the ≤2 adjacent triangles with winding
  preserved. Its own "Edge Cut" panel: Cuts / Factor / Clamp / Flipped.
- **Full Loop Cut panel** — the ring path's panel now carries the
  supported set of Blender's Loop Cut and Slide: Number of Cuts,
  Smoothness (normal-direction bulge, scaled by edge length), Falloff
  (Inverse Square / Sharp / Linear / Sphere / Smooth — shapes the bulge
  across multiple cuts), Factor, Flipped (mirrors the slide), Clamp
  (off allows a mild ±0.5 overshoot past the edge ends). Not carried:
  Even (needs per-edge arc-length slide), Correct UVs (UVs are always
  interpolated correctly here), Mirror Editing.
- **A selects all in edge mode** — selectAll built verts or faces only;
  edge pick mode fell into the vertex branch. It now builds every unique
  non-degenerate edge as a proper edge pick.

## 2026-07-20n — Loop Cut v2: Blender parity (quads-only, stays quads, centered + panel, typed count, orange loop)

User feedback on the first modal: cuts triangulated visibly, passed
through plain triangles, the new loop wasn't highlighted, and there was
no numeric input. All addressed:

- **Quads only, like Blender**: the ring walk hops through MARKED quad
  diagonals only (Make Face / Tris to Quads) — plain triangles stop the
  loop. On an all-tri FO4 mesh, run Tris to Quads first; that is Blender's
  own behavior on triangulated meshes.
- **The cut stays quads**: every new ladder cell's diagonal gets a quad
  mark (setQuadMarks joins the undo macro), so the result reads as quads,
  not a triangulated strip. Cell diagonals are derived deterministically
  from the same rows tlApplyLoopCut builds (nv + i*cuts + k), shared by
  the confirm and the panel re-run.
- **Confirm = centered cut + adjust panel** (the interactive mouse-slide
  phase is gone per user spec): LMB/Enter places the loop dead-center and
  arms the "Loop Cut" panel with Number of Cuts + Factor (the supported
  subset of Blender's Loop Cut and Slide panel); Factor slides the loop
  after the fact, re-run as one macro (cut + marks = one undo step).
- **New loop lands selected as EDGES** — orange selection lines across
  every cut loop, edge pick mode active, ready for G/S.
- **Numeric input while armed**: type digits for an exact count
  (multi-digit, Backspace edits), +/- adjusts, wheel still works.
  Keys are routed both through GLView::keyPressEvent and the event-filter
  interception so they work regardless of keyboard focus.

## 2026-07-20m — Alt+J from any select mode, isolate menu verified + harness, Render dedup, UI polish

**Tris to Quads from vertex/edge modes** (glview.cpp): Alt+J only read face
picks; selections made in vertex or edge mode silently produced "select
the faces". Blender's implicit rule now applies: a face counts as
selected when all three of its verts are covered by the selection
(pickedVertexRefs), so Alt+J works in every select mode.

**Isolate/visibility menu VERIFIED WORKING + regression harness**
(WW_ISOLATE_TEST=1 in nifskope_ui.cpp): user reported the viewport
visibility menu "doesn't seem to work". A new headless harness drives
Isolate Selected → Restore All on a real NIF and checks both state
(hiddenNodes, per-shape isHidden) and pixels (framebuffer diffs with
pumped repaints): PASS — 6 nodes hidden, viewport visibly changes,
restore recovers. HARNESS GOTCHA for future tests:
QOpenGLWindow::grabFramebuffer does NOT repaint; pump update() +
processEvents twice before grabbing or you diff two copies of the same
stale frame. Note for users: isolating with EVERYTHING selected hides
nothing (all blocks are relevant) — that reads as "didn't work".

**Render menu dedup** (user request): removed the entries that live
elsewhere — Solo Selected (display dropdown; Alt+Q re-registered
window-scope so the shortcut still fires), Show Transform Gizmo (display
dropdown), and the 3D Cursor & Elements submenu (all entries exist in the
viewport right-click menus / display dropdown). Auto-Key, Gizmo Snap
Distance, Update View, Save Current Lighting stay — not duplicated.

**UI polish**: the mode selector (Object Mode / Edit Mode / …) keeps a
constant width across all mode labels (max label metric + icon), so the
toolbar row no longer shifts on mode change; the Block List's NiNode
category dot is a quiet grayish-white code-drawn dot instead of the
orange resource (read as a status light).

## 2026-07-20l — Type chips show matches ONLY (flat while filtered)

User: "if I filter by these types, show me them only." Hierarchy mode
fundamentally can't — a tree row can never display without its ancestors,
so a chip filter always dragged the parent chain along. A quick-filter
chip now temporarily switches the Block List to the FLAT list view (every
block of the chosen category, nothing else); clicking All restores the
hierarchy — but only if the chip is what left it (a user already in flat
mode stays there). goToBlock's programmatic chip reset restores too.

## 2026-07-20k — Block Details search actually filters; boxed viewport mode buttons

**Search fix** (nifview.h/cpp, nifskope.cpp): the field filter LOOKED broken
("can't search for scale / data size / type") because it was: the view
re-derives row visibility from isRowHidden() on every relayout
(doItemsLayout, the expansion hook, resets — the row-hiding machinery from
the version-condition fixes), and the filter's one-shot setRowHidden calls
were clobbered by the very next pass, often triggered by the filter's own
row hiding. The filter now computes a KEEP set (matching rows + ancestors
+ matches' whole subtrees) handed to NifTreeView::setDetailsFilter, and
isRowHidden() enforces it alongside the condition/version rules — every
derivation pass now preserves the filter instead of fighting it. Also:
the Type column is searchable now ("Ref<BSShaderProperty>", "Vector3"...),
and a matching compound keeps its subtree visible when expanded (and skips
the per-member stringify — a matched Vertex Data no longer walks 38k
elements building text).

**Boxed mode buttons** (nifskope_ui.cpp makeMenuButton): the Select / Add /
Object / Mesh / Vertex / Edge / Face / Paint dropdowns on the mode toolbar
now use the same boxed style as the Panels / Workspaces selectors (1px
#555 border, #383838 well, rounded 4px, hover brighten) with slimmer
padding since up to eight share the row — they read as buttons instead of
floating text.

## 2026-07-20j — Reference drag polish: no shimmer, drop = Value cells only

Two user reports on 07-20i's drag: (1) the ghost shimmered — QToolTip
re-shows (with fade) on every reposition; replaced with a parentless
frameless QLabel (Qt::ToolTip flag, flat #383838 style) created once and
only ever move()d, on a 16 ms tick. (2) releasing the drag anywhere on the
row applied the value — even back over the donor Reference cell itself.
Drops now apply ONLY when released over a Value cell; anywhere else
(Reference/Name/Type columns, other rows without a type match, off-row)
cancels cleanly. Tooltip wording updated to match.

**Follow-up (d2552fc), "nothing happens on drop": Qt item views only route
a mouse RELEASE to the delegate's editorEvent when it lands on the pressed
cell — a drag by definition releases on a different cell, so the
Value-cell drop never fired. The ghost's tracking timer now detects
button-up itself, resolves the cell under the cursor via view->indexAt,
and applies there. Lesson for any delegate-driven drag: editorEvent is
press/same-cell-release only; cross-cell gestures need view-level or
timer-level completion.**

## 2026-07-20i — Modal Loop Cut, quad fill fix, Bevel reachability, reference drag v2

Four user-reported issues from hands-on testing.

**Loop Cut is now Blender's modal (Ctrl+R)** (glview.cpp/h, nifskope_ui.cpp):
Ctrl+R arms the modal — the ring under the cursor previews as a yellow loop
glued to the surface (re-projected per frame, so MMB orbiting works
mid-modal), the scroll wheel sets 1–64 cuts, LMB applies the cut and
chains straight into a slide phase where the mouse moves the new loop(s)
along the ring; LMB places, RMB/Esc recenters, Esc in phase 1 cancels.
The whole gesture is ONE undo step (macro: TlShapeStateCommand cut +
optional slide command), and the adjust panel arms afterwards with Number
of Cuts + Factor, re-run as a single command with the factor baked into
the lerp. Implementation notes: the ring walk moved into loopCutProbe with
a per-shape adjacency cache (a probe runs per mouse move — no O(T) hash
rebuild per move on big meshes; same-edge hovers skip recompute);
`tlApplyLoopCut`'s new-vert indices are deterministic (nv + i*cuts + k) so
the slide phase can address them without plumbing; the live slide is
preview-only writes, then the commit restores center and pushes the slide
command so undo captures the right pre-state; the new loop lands SELECTED
in vertex mode. Coexistence: startModalTransform refuses while a modal
tool owns the mouse; edit-mode exit mid-slide finishes centered (never
strands the open macro); knife-style key interception in the event filter.

**Quad selection fill artifact FIXED** (glview.cpp fill overlay): the two
halves of a marked quad rendered different orange tones — the active
face's lighter fill (0.36α warm) vs the selected fill (0.30α orange) split
along the hidden diagonal, because only one tri of a pair can be the
active element. The active face's quad partner now counts as active too,
so a quad reads as one uniformly-lit face (one quadPartnerTri lookup per
frame, only while marks exist).

**Bevel made reachable** (glview.cpp, nifskope_ui.cpp): the implementation
was complete but (a) Ctrl+B had NO pointer-over-viewport event-filter
routing — it only fired when the GL window held keyboard focus, unlike
E/F/I/K/etc. — now routed as op 8 in the shared modeling-ops block;
(b) it was missing from the W Specials menu — added ("Bevel…\tCtrl+B");
(c) it silently bailed unless edge-mode picks formed the path — a
vertex-mode fallback now derives the edge path from a selected vertex run
(edges whose both endpoints are selected), with the same chain guards.

**Reference drag v2** (nifdelegate.cpp): click-to-apply REMOVED per user —
a plain click on a Reference cell now only selects the row. The drag arms
on press but only becomes one past QApplication::startDragDistance();
the ghost then follows the cursor, and the drop applies the dragged
NifValue to whatever row it lands on IF the value type matches (so a
reference Glossiness can be dropped onto another float field too).
Release elsewhere/no match = clean cancel; self-heals via the physical
mouse-button check.

Build: green. GUI checks owed: modal loop cut end-to-end (preview, wheel,
slide, undo as one step, adjust panel), quad fill uniformity, Ctrl+B with
focus in a dock, vertex-run bevel, drag-threshold feel on the Reference
column.

## 2026-07-20h — Diff reference gets its own column (supersedes 07-20g's in-cell suffix)

User feedback on 07-20g: the reference value painted inside the Value cell
read as clutter — they wanted a real second value column. Replaced.

**Reference column** (`WwRefCol` = column 10, basemodel.h; NumColumns 10→11):
a real tree column headed "Reference", shown in Block Details only while a
diff reference is set (slides in right after Value, 160px default), hidden
again on clear. Differing rows show "◆ <reference value>" in grey; other
rows are empty. Served straight from NifModel's diffRefText — the
WwDiffRefTextRole + delegate-painted suffix from 07-20g are REMOVED.
Column-count fallout handled: pasteArray's hardcoded size assert →
NumColumns; the new column starts hidden in tree/header/kfmtree (incl.
after header restoreState from pre-column layouts, which would otherwise
surface it) and in the block list's list-mode.

**Drag ghost**: pressing a reference value picks it up — a cursor-following
label shows exactly what you're carrying (QToolTip re-anchored by a 50ms
timer, the AutoCloseMenu polling pattern, since item views don't route
mouse-move to editorEvent; self-heals by watching the physical button
state). Release anywhere on the same row applies it as one undoable
change; release elsewhere cancels. Handled BEFORE the delegate's
editable-flag guard — the column itself is read-only.

**Alt+J note**: user asked for Tris to Quads — it already exists (Alt+J,
edit mode, pointer over the viewport, faces selected; adjust panel with
Max Face/Shape Angle). No change made; documented the gating instead.

## 2026-07-20g — Diff values side-by-side + drag/copy the reference; Blender scrub on Details editors

Follow-ups to 07-20f from the user's first hands-on session.

**Reference value painted beside the current value** (nifdelegate.cpp,
WwDiffRefTextRole = UserRole+44): in diff mode every differing row now
shows "◆ <reference value>" right-aligned in its Value cell — orange
diamond, grey text — so current and reference read as two columns with no
tooltip digging. Skipped when the column is too narrow for both; the flag
suffix yields space to it; link rows inset it clear of the hover glyphs.
The stored reference NifValues moved from NifSkope into NifModel
(`diffRefValues`, beside diffItems/diffRefText) so the delegate can apply
them directly.

**Grab the reference value**: press the painted "◆ value" and release
anywhere on the same row — a plain click or a drag-onto-the-value both
write the reference's value onto the field, as one undoable
ChangeValueCommand. The diff recompute then un-highlights the row.

**Copy/paste it**: right-click a differing row → "Copy Reference Value"
fills the SAME field clipboard as Copy Field Value (path + the reference's
NifValue) — so the reference's Glossiness can be pasted onto the current
block, or onto five blocks at once via the Block List "Paste … to N
Block(s)". Leaf rows also gained "Paste Field Value (<label>)" in the
Details menu for single-row pastes without touching the Block List.

**Blender scrub on Block Details editors** (valueedit.cpp `WwScrubFilter`
+ `wwAttachScrubbers`): the DragSpinBox press-drag behavior from the
viewport redo panels, as an installable event filter on the editor's line
edit — press-drag scrubs (per-pixel step scales with the value's
magnitude; Shift = ×0.1 fine), plain click selects-all for typing, cursor
is the horizontal-drag arrow. Attached to every numeric editor ValueEdit
creates: FloatEdit (floats), QSpinBox/UIntSpinBox (ints), and the numeric
children inside VectorEdit/RotationEdit/ColorEdit/TriangleEdit — so
Translation X/Y/Z and Euler rotations scrub too. Model commit still
happens on editor close (Enter/focus-out), one undo step per edit, so a
scrub is a single undoable change, not a stream. Links/text/enums
untouched.

Build: green. GUI checks owed: ref-value legibility on narrow Value
columns, click-vs-drag feel on the ◆ zone, scrub feel (step scaling) on
Scale vs Translation vs int fields, and that click-to-type still lands
in the editors' select-all state.

## 2026-07-20f — Block Details batch: link jump/pick, decoded flags, field paste-to-many, sticky state, diff-vs-reference

Five additions to the Block Details dock from the overhaul concept
(BLOCK_DETAILS_OVERHAUL_PLAN.md), all keeping the existing Name|Value|Type
tree exactly as it is at rest. Pre-batch backup: git branch
`backup/pre-details-batch-20260720` + `release/NifSkope_backup_pre_details_20260720.exe`.

**Link rows: hover ↗ jump / ▾ pick** (nifdelegate.cpp): hovering a Ref/Ptr
row's Value cell shows two quiet grey glyphs at its right edge. ↗ selects
the linked block (via the delegate's parent NifSkope window, invokeMethod
"select"); ▾ pops a menu of every type-compatible block in the file
("12 — BSLightingShaderProperty \"name\"", checkmark on the current target,
None on top) — retargeting a link never means memorizing block numbers.
The pick is one undoable ChangeValueCommand. Compatibility = the link
field's template type via blockInherits. Invisible at rest; the glyph zone
backfills with the row's base/alternate/highlight colour for legibility.

**Decoded flags inline** (flags.cpp `wwFlagFieldSummary`, wwflagsummary.h,
nifmodel.cpp, nifdelegate.cpp): flag fields show a grey suffix after the
raw value — "14 — Selective Update, Sel. Upd. Transforms, +1". Served by
the model as WwFlagSummaryRole (basemodel.h, UserRole+43) gated on
name Flags/Integer Data + isCount, decoded next to the flag dialogs so bit
interpretations stay in one place (NiAVObject + BSX by named-bit list —
3 names then "+N"; Node/Shape/Billboard, Controller, Alpha blend/test,
ZBuffer, VertexColor, TexDesc by mode summary). Painted by the delegate
after the value text, elided, dropped entirely when the column is narrow.

**Field copy → paste-to-many** (nifskope.cpp, nifskope_ui.cpp): right-click
any leaf field in Block Details → "Copy Field Value" (name path from block
root + NifValue, shared across windows). Then right-click the Block List →
"Paste \"Glossiness\" = 80 to 5 Block(s)" writes it onto every selected
block that has the field (path resolved by name, arrays by row; value type
must match — mismatches are skipped, status bar reports applied/total).
One undo step via ChangeValueCommand transaction merging; old values are
captured before each push (push() itself applies — note pasteTo() in
nifview.cpp pre-sets and then records item->value() as "old", which makes
its undo a no-op; deliberately not copied here).

**Sticky Block Details state per block type** (nifskope.cpp, select()):
switching to a block of a type you already visited restores that type's
expansion set and scroll position instead of resetting + auto-expanding
(auto-expand still runs for never-visited types). Expansion paths are
name-based ("name|row" segments, row numbers inside arrays); capture
skips arrays >2000 rows so a 38k-vertex shape stays O(top-level rows).
Session-only (QHash keyed by block type name).

**Diff-vs-reference** (nifskope.cpp/h, nifmodel.cpp/h, nifskope_ui.cpp):
Block List right-click → "Set as Diff Reference" pins a block (marked ◆ in
the list); from then on whatever Block Details shows gets every differing
Value cell accented in the standard selection orange (#FF9D00), with the
reference's value in the tooltip. A flat grey banner above the field
filter says "Diff vs: 31 BSLightingShaderProperty — 6 row(s) differ"
(✕ clears; also via list right-click). Right-click a differing row →
"Take Reference Value", or "Take All Reference Values (N)" — one undo
step. The diff walk runs once per block switch / edit burst (dataChanged →
single-shot coalesced recompute), never per paint: sets of differing
NifItem pointers live in NifModel (diffItems/diffRefText/diffRefBlock,
same pattern as selHighlight) and data() just looks them up. Guardrails:
arrays >500 elements compare by length only; different-type blocks compare
matching field names only (banner notes it); stored reference values cap
at 4000 leaves. Reference invalidation (block deleted, file reload) clears
cleanly via QPersistentModelIndex + beginLoading.

Build: green (qmake6 re-run for the new header + role). GUI checks owed:
hover glyphs on link rows (incl. a proxy-hierarchy edge: glyphs are
Value-cell-hover only), flag suffix legibility on narrow columns, paste-
to-many undo as one step, sticky expansion across same-type blocks, diff
accent + Take Theirs round-trip.

## 2026-07-20e — Right-click flag copy/paste, foldable Block List, toolbar grips → separators

**Copy Flags / Paste Flags in the context menu** (flags.cpp): two new
instant spells on every flag field the Flags/Shader Flags spells handle.
Copy puts the raw value(s) on the same kind-tagged clipboard the dialogs
use; Paste only appears when the clipboard holds a COMPATIBLE kind — so
right-click → Copy Flags on one block, right-click → Paste Flags on
another, fully interoperable with the dialogs' buttons, across files and
windows. Details: shader properties copy F1+F2 together (keyed by enum
types, one snapshot undo step on paste); every spEditFlags type has its own
kind (alpha, controller, rigid body incl. the Col Filter Copy mirror,
zbuffer, BSX, NiAVObject, …) — even the combo-based editors' raw values
copy/paste safely between same-type blocks. `getFlagIndex`/`queryType`
became static for reuse.

**Block List folds to a sliver** (nifskope.cpp): the header rows dictated
the dock's minimum width. The Links button is now compact counts ("2/0",
meaning in the tooltip), the search field's minimum dropped to 48px, and
the nav row / filter-chip row / breadcrumb / footer labels all allow
clipping (minimum width 1) — so the dock can be dragged nearly closed for
a bigger viewport, elements clipping from the right as it narrows.

**Toolbar drag grips removed** (nifskope_ui.cpp): tRender/tMode/tView/tLOD
are now fixed (setMovable(false)) like tFile — the dotted grips read as
sliders and wasted row width. Group boundaries are drawn by the thin 2px
separator line instead (the tRender separator style now applies to all top
toolbars; tMode and tView start with one where their grips used to sit).

Build green, startup + join harness smoke PASS. GUI checks owed: right-click
Copy/Paste Flags round-trip (incl. dialog interop), folding the Block List
dock to minimum, and the toolbar row reading cleanly without grips.

## 2026-07-20d — Generic flag dialog (Node/BSX get the Shader Flags UI) + flag copy/paste

The modern Shader Flags dialog (filterable checkbox tree, "Stored in"
column, live hex footer) is now a reusable component
(`wwFlagListDialog`, flags.cpp) driven by a field list + bit-entry list,
and two more flag editors use it:

- **Node Flags** (`niavFlags` — NiAVObject flags on 20.2.0.7 files, the old
  plain 32-checkbox column) and **BSX Flags** (`bsxFlags`). Both list all
  32 bits (named where known, "Bit N" otherwise — BSX previously hid bits
  10–31 entirely; they were preserved on write but invisible).
- The editors with multi-bit enum fields behind combo boxes (node/legacy,
  controller, rigid body, Z-buffer, stencil, billboard, vertex color,
  TexDesc, alpha) deliberately keep their dialogs — a raw bit checklist
  would be a usability downgrade for e.g. a 2-bit collision mode.

**Copy/Paste between compatible flags.** The dialog has Copy and Paste
buttons wired to the system clipboard with a kind-tagged MIME payload
(`application/x-nifskope-flags`): Paste only enables when the clipboard
holds values copied from the SAME kind — shader flags paste between shader
properties of the same enum types (Skyrim vs FO4 shader flags do NOT
cross-paste; different bit meanings), node flags between NiAVObjects, BSX
between BSXFlags blocks. Works across windows/instances (system
clipboard); the plain-text fallback ("F1 0x8040028B  F2 0x00000031") is
for humans.

Write-back semantics unchanged per editor (shader = both fields in one
snapshot undo step; node/BSX = the existing direct set). Build green,
startup + join harness smoke PASS; the dialogs themselves need a GUI pass
(open each of the three, filter, toggle, copy → paste onto another block,
verify the hex footer matches the Block Details value).

## 2026-07-20c — Performance batch 3 (PERFORMANCE_PLAN.md Tier 3)

**15a — NifItem slab pool (shipped).** `NifItem` now allocates from a
chunked free-list pool (class-level `operator new`/`delete`,
`nifitem.cpp`): 8192-slot 16-aligned chunks, freed slots recycle through an
intrusive free list, mutex-guarded because the XML checker parses on worker
threads; chunks live for the process. NifItem has no subclasses so every
slot is `sizeof(NifItem)` (a defensive guard throws if that invariant ever
breaks). **Measured** (launch→completeLoading, 2 runs each):
missilesilocontrolroom04twofloors (2.0 MB, block-heavy) 1490–1561 →
1442–1463 ms; PBR x01_torso (1.8 MB, vertex-heavy) 3062–3106 →
2911–2942 ms (~5% of wall clock incl. fixed startup overhead; the parse
share is larger, plus ongoing locality gains that this metric can't see).

**17 — snapshot-undo retirement (completed to its safe boundary).** Delete
and Merge-by-distance were ALREADY in-place (per-shape
`TlShapeStateCommand`, snapshot only for legacy NiSkinData/NiSkinPartition
meshes where blocks really are removed) — the plan/memory note was stale.
The one remaining value-only snapshot op, **Snap → Verts to 3D cursor**
(`movePickedVertsToCursor`), now pushes merged `ChangeValueCommands` via
`tlPushPositionCommands` (signal-batched, lookup-memoized) instead of
serializing the whole file twice and reloading on Ctrl+Z. Every op still on
snapshot undo removes/adds blocks (Join, object-mode delete, Triangulate,
parenting, decal creation) — snapshot is the *correct* mechanism there
(block renumbering + model-wide link rewrites), per the standing design
note.

**15b — statically-dead field skipping: REJECTED with evidence.** Every
field of `BSVertexData`/`BSVertexDataSSE` is `#ARG#`-gated (nif.xml — the
Vertex Desc), and the desc IS editable post-load (Vertex Flags spell,
Create Skin, ensure-vertex-colors). Skipping dead fields at build time
would require rebuilding every row on desc change — inside the exact
machinery that produced the 2026-07-18b Create Skin corruption and the
0-length-conditional-array landmine. No per-field version conditions exist
there to exploit safely. Folded into the 15c design instead.

**15c / 16 — flattened packed vertex storage + off-thread parse: DEFERRED
as one joint project** (design constraints recorded in
PERFORMANCE_PLAN.md). Measurements above show item construction dominates
load; the safe off-thread split (raw block buffers off-thread, items on the
UI thread) moves only IO/decompression, so it doesn't pay until flattened
storage makes the raw buffer *be* the model's storage. Doing them together
is the real Tier-3 endgame; doing either alone under-delivers.

Verification: build green; WW_JOIN_TEST=1, WW_SEP_TEST (+undo, which
exercises TlShapeStateCommand round-trips over the pooled items),
WW_BROWSER_TEST, WW_COPYPASTE_TEST all PASS on the final binary. GUI check
owed: Snap → Verts to 3D cursor undo/redo (no reload flash expected now).

## 2026-07-20b — Performance batch 2 (PERFORMANCE_PLAN.md Tier 1 rest + Tier 2)

Three commits (2a/2b/2c), each built + harness-verified before the next.

**2a — remaining Tier-1 quickies + model-layer wins:**
- `Scene::draw` read `CollisionManager/CollisionOnly` from QSettings — a
  registry access — **every frame**; now a cached static the collision
  panel pushes to on toggle. `drawGrid`'s per-frame env-var probe hoisted.
  The per-frame `glGetError` drain in paintGL is debug-only now (it is a
  client-server sync point, and release already silenced its message —
  a stall with no output). Other `glGetError` users are event-driven only.
- `applyBlockListFilter` early-outs when no filter is active and none needs
  clearing (it walked all blocks building formatted searchable strings on
  every rows signal, empty search box included) and the rows signals are
  coalesced to one deferred run per event-loop turn.
- `updateHeader` block-type dedup via QHash (was `indexOf` per block =
  O(blocks × types) string scanning on every structural edit).
- **Batch multi-block removal**: every `removeNiBlock` outside Loading
  state runs a full-model `updateLinks` + `updateFooter`. The NiKeyframe
  purge (skeleton.cpp) and the unreferenced-interpolator cleanup
  (animationsetup.cpp) now use the same Loading + `updateModel()` batch
  pattern havok/optimize already had — one rebuild instead of M. The
  riggingtools single-node removal and optimize's property-merge loop are
  deliberately NOT batched: both consult live link lists between removals.
- **Named-lookup memoization in per-vertex loops**: `getIndex(name)` is an
  uncached linear scan; fixed-compound rows are structurally identical, so
  field row numbers resolve once per shape. `tlVertexValueIndex` gained a
  cached overload (`TlVertexFieldCache`) used by the gesture commit, the
  redo-panel re-apply and `tlPushPositionCommands`; `tlPushNormalCommands`
  resolves the Normal row once; the rigging vertex-color init and
  paint-stroke commit resolve "Vertex Colors" once (and the stroke commit
  now also runs under Processing with one span emit — same storm as the
  init loop had).

**2b — overlay soup caches:** the edit-mode overlay rebuilt its
unique-edge / quad-adjacency / filled-tris sets (O(T) hashing + fresh
allocations) on **every repaint**, camera orbits included, and the
wireframe overlay repeated the same edge dedup per shape per frame.
Index-space structures now persist across frames (`EditOverlaySets`,
`wireEdgeCache`): positions stay per-frame (they carry the toward-eye
pull), so gestures/deforms need no invalidation; topology growth is caught
by size fingerprints; `filledTris` follows the selection via a per-frame
FNV fingerprint over `pickedElems` (no hooks at the many mutation sites);
explicit `invalidateOverlayCaches()` covers what fingerprints cannot see
(hide/unhide, solo-restore, quad-mark undo/redo, any model dataChanged).

**2c — scene transform early-out:** `Scene::transform` cleared the
transform cache and re-walked every node (and re-evaluated every
controller, and re-queried the model for every `bhkRigidBody`) on every
repaint. The propagation is a pure function of (scene content, camera,
time, animate flag) — it now returns immediately when none of those
changed. Dirty hooks: `update()`/`clear()`/`setSequence()` (make routes
through the first two), the five `restPoseBlock` writers (rest-pose swaps
change `worldTrans` derivation), and paintGL forces the full pass while a
modal gesture or paint stroke is live. Camera moves, animation time, and
the animate flag are compared directly. LOD/billboard camera dependence is
safe: any view change runs the full pass.

**Deliberately deferred, with reasons** (documented in PERFORMANCE_PLAN.md):
- T2.13 draw sorting by program/texture: real batching needs the recursive
  draw restructured into sortable lists plus per-shape uniform caching
  (Tier-3-scale), and micro-guards on `glUseProgram` are exactly the
  renderer cache-desync landmine of the startup-grid bug.
- T2.14 frustum-culling particle/lightning systems: the sim must keep
  running for correct resume, draw-only culling has minimal payoff, and a
  wrong-space plane test would silently blank VFX previews with no
  headless way to catch it.

Verification: build green per stage; `WW_JOIN_TEST=1`, `WW_SEP_TEST`
(+undo), `WW_BROWSER_TEST` (incl. fast-path check) PASS per stage (2c run
below). GUI checks owed: edit-mode overlay correctness after hide/unhide +
quad-mark undo, rest-pose toggle, LOD/billboard behavior during orbit,
animation playback, and the collision-only toggle.

## 2026-07-20 — Performance batch 1 (PERFORMANCE_PLAN.md Tier 1, items 1–4)

First slice of the performance plan: the four highest-value / lowest-risk
fixes from the holistic survey. No behavioral changes intended — the same
work happens, it just stops happening when nothing needs it.

**1. NIF Browser stops re-indexing the game archives on every load**
(`nifskope.cpp populateConfiguredNifBrowser`). Every successful
`completeLoading` used to delete the `BA2File` VFS, re-scan every configured
resource path, re-enumerate the merged file list, and rebuild the whole
`QStandardItem` tree — even with the dock closed. Now:
- Two-level signature cache: the archive **index** is keyed by
  (game, resource-path list) and reused when unchanged; the **tree** is keyed
  by that plus the Load Archives / Load Loose toggles. An unchanged-tree
  populate only refreshes the Loaded NIFs group (the one per-load part). A
  toggle flip re-filters the tree without touching the disk.
- Visibility gate: while the browser dock is hidden (closed or a background
  tab — it ships tabified behind Header) the populate is deferred
  (`nifBrowserPopulatePending`) and replayed by a new
  `dockVisibilityChanged` hook on show.
- The explicit open-archive mode is untouched: it sets `currentArchivePath`,
  which disqualifies the cached index (`configuredIndexLive`), so the next
  configured populate rebuilds fully, as before. **Refresh** now clears both
  signatures first — it still means "re-read the disk".

**2. Hidden docks stop reacting** — the collision dock's gate pattern applied
to the two remaining offenders:
- Timeline (`timeline.cpp refresh`): `scanModel()` walks every block of the
  file on each coalesced dataChanged/reset; it now defers while the
  Animation Manager dock is hidden (`refreshPending` + `showEvent` replay).
- Rigging Manager (`spells/riggingtools.cpp`): the `refresh` lambda fires on
  every selection change and cleared + rebuilt both bone trees even hidden.
  Now it defers while the panel is hidden; the dock's existing
  `visibilityChanged` hook replays the pending refresh *before* re-applying
  overlays/heatmap (painting is already force-stopped on hide, so no mode
  can be stranded).
- The post-load `refreshRowHiding()` calls for Block Details/Header skip
  hidden views — `NifTreeView::doItemsLayout` re-derives hiding before the
  first layout when the dock next shows anyway.

**3. Block Details layout costs**:
- `uniformRowHeights=true` on the Block Details tree (`nifskope.ui`) — it was
  the only big view with it off, forcing a delegate `sizeHint` (a
  text-measure) per row on every relayout. All rows are single-line.
- `NifTreeView::updateConditions` (the dataChanged reactor) no longer does a
  full array descent: a block-level field edit used to walk **every element
  (and every element's members) of the block's arrays** — O(38k × fields)
  per qualifying edit, twice (Block Details + Header both connect). New
  `updateConditionsLazy` re-derives exactly the *visible* rows, descending
  only into the invisible root / view root / expanded rows — the same lazy
  one-level-per-expansion rule the expansion hook and `refreshRowHiding`
  already use, so closed subtrees still re-derive when opened.

**4. Remaining per-leaf dataChanged storms killed**:
- `riggingEnsureVertexColors`: the 38k-vertex opaque-white init loop ran
  under `holdUpdates`, which defers header/link refresh but does **not**
  suppress per-leaf `dataChanged` — one signal + dependent-condition sibling
  scan per vertex. The loop now runs under `setState(Processing)` with one
  span emit; the Vertex Desc / Data Size writes stay outside so their
  dependent-condition invalidation still runs.
- First application of big gestures: `QUndoStack::push` runs each command's
  `redo()` **before** merging, and a size-1 `ChangeValueCommand::redo` does
  not enter Processing — so committing a 38k-vert move emitted 38k signals
  (only the undo/redo *replay* was batched). New `TlCommandBatch` RAII
  (glview.cpp) suppresses per-leaf signals around the push loops and emits
  one span per touched shape: applied to the element gesture commit, the
  redo-panel re-apply, `tlPushPositionCommands`, `tlPushNormalCommands`
  (covers Edge Slide / Smooth / extrude-chain normals), and Flip Faces.
  The model state is a stack, so the nesting is safe.

Verification (build green 2026-07-20 14:50, all on X01_Torso_Tesla.nif):
- `WW_BROWSER_TEST` (extended): folder expansion preserved through a genuine
  full rebuild (signatures cleared first), **and** the new fast path proven —
  an unchanged-signature populate reused the tree (a `QPersistentModelIndex`
  into it survived; a rebuild would have killed it) — **PASS**.
- Regressions re-run on the new binary, all unchanged: `WW_JOIN_TEST=1`
  (merged 4031 v / 4529 t, bones {3,4}, 0 zero-weight appended verts,
  segments contiguous) — **PASS**; `WW_SEP_TEST` — **PASS + UNDO PASS**;
  `WW_COPYPASTE_TEST` (delta 9 blocks, 2/2 roots slotted) — **PASS**.
- `TlCommandBatch` is RAII over the model's own state *stack* (restore +
  span-emit cannot be skipped by an early return); the interactive gesture
  paths it wraps need a GUI pass — G/R/S commit + redo panel re-apply on a
  big selection, Edge Slide, Smooth, Flip/Recalc Normals, and Vertex Paint
  on a colourless mesh (the ensure-colors init).
- GUI checks still owed for the rest of the batch: Block Details relayout
  feel on a 38k-vert shape (uniformRowHeights + lazy condition walk — check
  version-gated rows still hide right while editing fields with arrays
  expanded), Timeline/Rigging docks catching up when re-shown, and the NIF
  Browser after changing resource folders in Settings (must rebuild) vs
  plain loads (must not).

## 2026-07-19g — NIF Browser keeps its expanded folders when you load a nif

Loading a nif rebuilds the NIF Browser's "Available NIFs" tree from scratch in
`populateConfiguredNifBrowser` (every successful load fires it via
`completeLoading`). The rebuild replaced every item and only re-expanded the top
"Available NIFs" node, so any folders the user had opened (e.g. armor ▸
armoredcoat) snapped shut on each load.

- Folder items are now tagged with their accumulated path
  (`NifBrowserFolderPathRole`). Before the rebuild, `populateConfiguredNifBrowser`
  walks the live tree and records which tagged folders are expanded; after the new
  tree is built and sorted, it re-expands the folders whose paths match — so the
  browser stays exactly where the user left it across loads, Refresh, and the
  archive/loose toggles.

Verified headlessly (`WW_BROWSER_TEST=1`, a new harness): expand the first
Available-NIFs folder, repopulate (what a load does), and the folder is still
open — **PASS** (`actors` expanded before=1, after=1).

## 2026-07-19f — Viewport mode menus dock into the toolbar; animation bar retired

The Blender-style viewport header menus — **Select · Add · Object** in object
mode, **Select · Mesh · Vertex · Edge · Face** in edit mode, **Select ·
Weights/Segments/Paint** while painting — were a floating translucent overlay
pinned to the bottom of the 3D viewport. They now live in a **docked toolbar
(`tMode`)** in the top toolbar row, in the slot the animation bar used to hold.

- **Animation bar removed.** The old `tAnim` toolbar (Play / Loop / Switch +
  timeline slider + animation-group combo) is gone: every one of its functions
  already exists in the **animation workspace** (Timeline dock) — the transport
  triggers `aAnimPlay`, the playhead replaces the slider (`setSceneTime` ↔
  `sceneTimeChanged`), and the sequence selector replaces the group combo
  (`setSceneSequence` / `sequenceChanged`), with Loop / Switch mirrored in via
  `TimelineWidget::addAnimActions`. The `.ui`'s `tAnim` toolbar was repurposed
  and renamed `tMode`; the `aAnimate` / `aAnimPlay` / `aAnimLoop` / `aAnimSwitch`
  **actions** and the `GLView` animation state machine (`updateAnimationState`)
  are untouched — only the redundant main-window widgets and their wiring went.
  `aAnimSwitch`'s show-only-with-multiple-sequences rule is now recomputed
  straight from the scene (`getScene()->animGroups.count() > 1`) on
  `sequencesUpdated`, so the dock's Switch button still appears correctly.
- **Overlay machinery deleted.** As ordinary window chrome the menus need none
  of the floating overlay's workarounds, all now removed: the frameless
  `WA_ShowWithoutActivating` tool window, its hand-rolled dark stylesheet,
  `positionViewportMenuBar()`, and the `eventFilter` Show / Move / Resize /
  WindowStateChange hooks that kept it glued to the viewport and dodged the
  focus-follows-mouse deactivation bug. A docked toolbar clicks without
  deactivating the main window, so that bug can't recur.
- **Per-mode collapse preserved.** Each button is added with
  `QToolBar::addWidget`, and it's the returned `QAction`'s visibility that's
  toggled per mode (the idiom the old anim-group combo already used) — so the
  toolbar slot collapses cleanly rather than leaving a gap, which hiding the
  `QToolButton` alone would not.

The Render-toolbar **mode selector** ("Object Mode" dropdown) and the viewport
visibility dropdown are unaffected — only the header *menus* moved.

In-app verified (`WW_UI_SHOT=1`, a new harness that grabs the whole main window
to `ww_ui_shot.png` and quits): with a mesh loaded in Object Mode the top
toolbar row shows **Select · Add · Object** at its right end and there is no
animation bar; the app starts cleanly (exit 0), so none of the removed
overlay/animation symbols break startup.

## 2026-07-19e — Separate (P) is skin- and segment-aware for FO4 meshes

Blender-style **Separate** (`GLView::separateSelection`) split a skinned
`BSSubIndexTriShape` incorrectly, the mirror of the old Join bug:

- **Shared skin (fixed).** `tlCloneShapeWithProps` clones a single block, so the
  split-off shape's `"Skin"` link still pointed at the **original's**
  `BSSkin::Instance` / `BSSkin::BoneData` — two shapes driving one skin, which is
  a malformed NIF (the binder already rejects shared BoneData). New
  `separateCloneSkin` gives the clone its **own** Instance + BoneData and relinks;
  the Skeleton Root and per-bone node pointers stay shared (the common skeleton,
  correctly referenced by both halves).
- **Stale segments (fixed).** `tlKeepTriangles` rewrites only the triangle array,
  leaving every segment's `Start Index` / `Num Primitives` pointing past the new,
  smaller triangle buffer on **both** halves. New `separateBuildSegments` rebuilds
  each slot (and subsegment) for the kept subset: since FO4 segments are
  contiguous in triangle order and `tlKeepTriangles` preserves order, a range's
  new position is the count of kept triangles before its original start
  (prefix-sum). The **slot count and shared Segment Data (Per-Segment-Data / SSF)
  are kept intact** — a slot that lost all its triangles just becomes empty
  (`Num Primitives` 0), so the dismemberment structure and SSF alignment stay
  valid.
- **Orphan-vertex trim.** `tlCompactVertices` then drops the verts each half no
  longer uses and reindexes its triangles (the compaction `tlDeleteGeometry`
  already does in Faces mode), so each piece is vertex-optimal like Blender's
  Separate rather than carrying the whole shared vertex buffer. FO4 skin weights
  are inline in the vertex record (moved with it) and there is no NiSkinData /
  NiSkinPartition to reindex, so no skin fix-up is needed; segments are triangle-
  indexed and unaffected.
- **Undo.** `TlShapeStateCommand` also snapshots the `Segment` subtree +
  `Num Primitives`, so Ctrl+Z restores the source's segment ranges (not just its
  verts/tris), and now pre-sizes each grown-back row's conditional skin arrays
  before the value restore so a vert-removing op's undo can't hit the 0-length-
  array landmine. The clone + new skin blocks are dropped by the existing
  `TlBlockAppendCommand`.

Vertex colours / alpha survive the split AND the compaction: each vertex record
(RGBA — alpha = the colour's A channel) moves verbatim with its vertex, and the
triangle remap is validated to keep the mesh geometry identical.

Verified headlessly (`WW_SEP_TEST`; `WW_SEP_BLOCK` targets a specific shape):
- Vault 111 suit (3106 v / 5033 t / 7 seg / 20 subseg, **no** vertex colours),
  split the first half of the faces → source 2515 t (skin 69/70, kept), clone
  2518 t (skin **79/80, its own**); both halves: 7 segments,
  `Σ Num Primitives == Num Triangles`, contiguous, orphan verts trimmed to
  source **1624** / clone **1557** (from 3106; `geomOk`, `noOrphan`), **0**
  zeroed weights.
- Coloured shape (torso block 111, 68 v / 64 t / 4 seg, **VF_COLORS**): split
  in half → both halves keep all 4 segments (`sumPrim == tris`, contiguous),
  distinct skins (112/113 vs 118/119), and **every vertex colour/alpha preserved**
  (`colorOk`) on source AND clone.
- Byte level (`verify_join.py`): both output shapes internally consistent — 62 /
  1 bones (Instance == Bones == BoneData), segments cover [0, tris), Data Size
  consistent — **PASS** on both the vault and coloured splits.
- Undo restores verts / tris / segment ranges and drops the appended clone + skin
  blocks (block count back to the original) — **UNDO PASS**.

## 2026-07-19d — Paint-mode viewport selector auto-acquires a target

Picking **Weight / Vertex / Segment Paint** from the viewport mode dropdown did
nothing — and gave **no feedback** — unless a mesh was already selected. The
handler opens the manager, auto-picks the first bone, and clicks its Start
Painting button; with nothing selected the bone list is empty, that button is
disabled, and the handler silently flipped back to Object Mode
(`nifskope_ui.cpp`). This bit right after a Join, which leaves no live selection.

- **Auto-acquire.** A shared `acquirePaintTarget( requireSkin )` helper now runs
  when no target is set: it selects the object the user is looking at (the active
  object) if it qualifies, else the file's **sole** qualifying shape (never
  guessing between several). `requireSkin` gates Weight / Segment paint (need a
  skin) vs Vertex paint (any tri-shape). It drives `NifSkope::select()`, whose
  synchronous `currentNifIndexChanged` repopulates the manager before the handler
  re-checks it — so the mode engages in one click.
- **Feedback.** When it still can't start, the status bar now says why —
  "no skinned mesh to paint…", "select which mesh… (this file has several)", or
  "select a bone / segment row, then Start Painting" — instead of silently
  reverting.

Verified headlessly (`WW_WP_TEST`, vault suit, nothing pre-selected):
`objActive -1` → trigger `ViewportWeightPaintAction` → `weightPaintModeActive 1`
— **PASS** (previously it would have reverted). Join regression re-run in the
same binary: unchanged — **PASS**.

## 2026-07-19c — Join: donor segments merge by dismemberment *slot*, not appended

The 07-19b merge appended each donor's non-empty top-level segments as **new
top-level segments** past the active's. That's wrong for FO4: `BSSubIndexTriShape`
segments are **indexed dismemberment slots** — the shared Segment Data / SSF
maps slot *i* to a body-part / cut. A donor's SEG 3 (e.g. shape 74 — segments
SEG 0–3 with all 230 faces in SEG 3, but **no** subsegments and **no** shared
Segment Data of its own) was landing as a brand-new slot the receiver's SSF
never defines, growing the vault suit to 9 segments the game can't address.

`GLView::joinBuildSegments` now merges **donor segment i into receiver segment i**:

- Triangles are **reordered** so every slot stays one contiguous `[Start, Start+
  count)` range after donor faces fold in — donor slot *i*'s faces are inserted
  at the end of receiver slot *i*'s run, and all later slots (and their
  subsegments) shift by that delta.
- The receiver's **subsegments, Per-Segment-Data (body-part Bone IDs + cut
  offsets), and SSF are preserved** — only `Start Index` / counts move; every
  `Parent Array Index` still resolves.
- Donors with more slots than the receiver flatten their surplus into the last
  common slot (reported), so no undefined slot is ever created.

Verified — vault 111 suit (receiver 7 seg / 20 subseg) + donors 74 & 63:
- **Num Segments 7 / Total 27, unchanged** (was ballooning to 9). seg3 grew
  2052 → 2356 = donor 74's 230 + 63's 74 folded into slot 3.
- seg2's 5 subsegments untouched (`parentPSD=2`); seg4/5/6 + their subsegments
  shifted +304 tris, all `parentPSD` intact. Coverage 5033 contiguous, SSF kept.
- Byte verify (`verify_join.py vault_joined.nif 3106 5033`): verts 3106, tris
  5033, bones 62, segments 7, desc `0x5b00050430208` — **PASS**.
- Torso regression unaffected: **mode 1** 5684 tris / Num 1 / contiguous — PASS;
  **mode 2** 5748 tris / Num 4 / contiguous — PASS.

## 2026-07-19b — Join (Ctrl+J) is rigging-aware: skin, segments, colors merge

Object-mode **Join** (`GLView::joinSelectedObjects`) previously did a
geometry-only merge (append verts/tris, transform normals) that **corrupted**
FO4 skinned meshes: per-vertex Bone Indices were copied verbatim but still
pointed into each source's own bone list, the active's `BSSkin::Instance` /
`BoneData` were never extended, and `BSSubIndexTriShape` segments described only
the original triangle count. Now it merges the rig:

- **Bones/weights/BSSkin::Instance.** Each source's bones are unioned into the
  active's `BSSkin::Instance` + `BSSkin::BoneData` (matched by bone **NiNode
  block number** — same file, so identity, no name ambiguity), missing bones
  appended (Ptr + BoneData transform, reusing the Transfer-weights append
  pattern), and every appended vertex's Bone Indices remapped into the merged
  list. Weights carry over verbatim. 256-bone (uint8) overflow skips a source.
- **Segments (incl. subsegments / dismemberment).** The active's own segments —
  and its shared **Segment Data** (subsegments, Per-Segment-Data with body-part
  Bone IDs + dismemberment cut offsets, SSF file) — are preserved unchanged
  (its triangles keep positions `[0, activeTris)`). Each donor's non-empty
  top-level segments are appended as new top-level segments with `Start Index`
  shifted past the active, plus a default Per-Segment-Data row **at the end** of
  the PSD array — so every existing subsegment `Parent Array Index` / Segment
  Start still resolves, and the receiver's dismemberment stays intact. Donor
  subsegments are flattened (reported). (Earlier this collapsed `Num < Total`
  shapes to one segment, destroying dismemberment — the Vault 111 suit's
  7 seg / 20 subseg regressed to 1 seg / 0 subseg; fixed.)
- **Vertex + alpha colors** ride along with the verbatim vertex copy.
- **Superset formats.** Compatibility was `identical desc`; now a source merges
  if it shares the active's structural layout and carries **no attribute the
  active lacks**. A source *missing* a fillable attribute the active has is
  promoted with a default — **opaque-white vertex color**, single-bone bind
  (weight [1,0,0,0] index 0), zero eye data — so e.g. a colourless mesh joins a
  coloured active and gets white. A source **richer** than the active is skipped
  with a message telling the user to make the richest mesh the active object
  (no risky in-place vertex-desc rebuild).
- **Perf.** The merge wraps its bulk writes in `setState(BaseModel::Processing)`
  + `restoreState()` + one `dataChanged`, like the other topology ops — without
  it every one of thousands of vertex writes made the live scene react
  (quadratic; a multi-second freeze / apparent hang on a 4k-vertex join).
- **Skin-copy landmine (fixed).** A freshly grown `BSVertexData` row leaves its
  `#ARG#`-conditional arrays (`Bone Weights` / `Bone Indices`) **0-length** until
  a deferred cascade — so `tlCopyItemValues` silently dropped the donor skin and
  every appended vertex came out weight-0 (rendered at the origin, "weights don't
  transfer"). The merge now `updateArraySize`s each new row's weight/index arrays
  to the source's length *before* copying, so the skin actually carries over.
  (Neither Default's `onItemValueChange` nor Processing builds these per-row.)

Verified headlessly (WW_JOIN_TEST in nifskope_ui.cpp, x01tesla_torso):
- **mode 1** (biggest shape active): the four same-desc `Edison_Torso`/glow
  shapes → merged 4309 verts / 5684 tris, bone list unioned to {5,6,7,8}, 0
  vertex bone indices out of range, 4 segments covering all triangles.
- **mode 2** (richest-format shape active): the coloured shape absorbs the four
  colourless ones → 4377 verts / 5748 tris, 8 segments, and the 4309 appended
  verts are all opaque white while the active's own verts keep their colours.
Both green live-model and byte-level (`tools/join_test/verify_join.py`: skin
Num Bones == Bones == BoneData, all indices < Num Bones, segments cover
[0, tris), Data Size consistent). Both complete in ~5 s (was a freeze).

Also verified on **vault111suit.nif** (mode 1): receiver `Vault111Suit_YanEdits:0`
has 7 segments / 20 subsegments + SSF; after absorbing two donors the merged
shape is 3106 v / 5033 t, 62 bones (unioned, all indices in range), and
**9 segments / 29 total with all 20 original subsegments, cut offsets and the
SSF preserved** — the two donors added as segs 7-8 with default PSD rows at the
end. verify_join.py now checks Segment Data self-consistency (Num/Total match,
Segment Starts == Num, Per Segment Data == Total). The harness also asserts the
appended donor verts keep non-zero skin weight (0 zero-weight verts) — the check
that would have caught the skin-copy landmine above.

## 2026-07-19a — Copy/Paste Branch (Ctrl+C / Ctrl+V) go multi-selection

Copy Branch and Paste Branch already own Ctrl+C / Ctrl+V (their
`QKeySequence::Copy` / `::Paste` hotkeys). They now handle a **multi-block
selection** and slot every branch back in — "copy these five blocks, paste
them, have them land in the right place".

**Copy Branch** unions every *selected* block's branch. A spell is only handed
one index by its shortcut/menu, so the block-list selection is published to the
spell: `NifSkope`'s `list` selectionChanged handler calls the new
`setBlockListSelection(blockNumbers)` (spells/blocks.h), and `spCopyBranch::cast`
unions all of them (via `copyBlockBranchesToClipboard`, `populateBlocks` dedups
overlaps) when the cast lands on one of the selected blocks; otherwise it copies
just that one branch (unchanged single-block behaviour).

**Paste Branch** links *each* root of the pasted set (a block not childed by
another pasted block), not just the first — the old code linked only `iRoot`.
`blockLink` picks the correct slot per pair (NiAVObject under a NiNode ->
Children, NiProperty under a shape -> Properties, ...). New: if a scene-object
root can't attach to the chosen target — the realistic Ctrl+V case, where the
current block is a *shape*, not a node — it falls back to the nearest NiNode
ancestor of the target so it still slots in instead of being orphaned. Narrowly
scoped: only fires for a still-orphaned (`getParent < 0`) NiAVObject root, so
property/extra-data/single-node pastes are unaffected.

Dropped the earlier dead-end: a dedicated Ctrl+C / Ctrl+V event filter on the
block list (nifskope_ui.cpp). It fought the real spell QActions — Ctrl+V is a
WindowShortcut processed in `QApplication::notify` before a widget event filter
sees the key, so the filter's paste-under-nearest-node branch never ran and the
spell pasted onto the raw (non-node) selection, orphaning the copies. Enhancing
the spells themselves is the right layer.

Verified headlessly (WW_COPYPASTE_TEST, x01tesla_torso, 116 blocks / 5
BSSubIndexTriShapes): selecting the two smallest shapes, casting Copy Branch
(union = 10 blocks = both shape+skin+shader branches), then casting Paste Branch
onto a *shape* — blocks 116->126 (delta 10 = the union, so both branches really
were copied); the nearest NiNode "Scene Root" gained exactly 2 children whose
types match the roots in order (both slotted in via the fallback, not just the
first); 0 pasted child links point back into the original range (internal links
remapped). Saved + reparsed clean (`tools/copypaste_test/verify_copypaste.py`):
Scene Root children `[…,111,116,121]`, 0 dangling / out-of-range.

## 2026-07-18h — Confirm popups open with the action button under the cursor

Blender opens a confirm with its action button beneath the pointer for an
instant click. New reusable `tlPlacePopupAtCursor(box, button)`: adjustSize +
show() (lays the dialog out without painting — paint waits for exec()'s event
loop, so it's flicker-free), measures the button's real screen position, and
shifts the whole window so the button lands on the cursor, clamped to the
cursor's screen. Applied to the "Delete selected objects?" confirm (Delete
button) and the over-cap duplicate confirm (its accept button, relabelled
"New Shape" / "Cancel" so it reads as an action). The operator pop-ups
(edit-mode Delete/Merge/Separate/Snap/Set Origin menus) already exec at the
cursor. Verified headlessly (WW_DELETE_TEST): action-button centre lands 0 px
from the parked cursor.

## 2026-07-18g — Delete confirm pops at the cursor (Blender)

The "Delete selected objects?" box opened screen-centre; now it opens
centred on the cursor like Blender's confirm. deleteBlocksWithConfirm
adjustSize()s the box, centres its rect on QCursor::pos(), clamps to the
cursor's screen availableGeometry so it never opens partly off-screen, and
moves it before exec(). Verified headlessly (WW_DELETE_TEST parks the
cursor, harness logs popup-centre vs cursor): 8 px offset (window frame),
i.e. at the cursor.

## 2026-07-18f — Object-mode / Block-List delete (Blender X)

New `GLView::deleteBlocksWithConfirm(blocks)` — the shared core for deleting
whole objects. Shows Blender's "Delete selected objects?" confirm (Delete /
Cancel), computes each selected block's branch closure (the block plus every
descendant it parents — matches Remove Branch; shared refs and blocks owned
by OTHER shapes are left alone), removes them all as ONE snapshot-undo step
(block removal renumbers + rewrites links model-wide, so snapshot undo like
Join), tracks everything by QPersistentModelIndex so order/​renumbering can't
bite, and prunes the dangling -1 child links removeNiBlock leaves in
surviving parents (tlRemoveNullChildLink now returns bool + loops).

Wired to three entry points, all multi-selection aware:
- **Object-mode X / Delete** (viewport, GLView::keyPressEvent): deletes the
  object selection. `deleteSelectedObjects()`.
- **Object context menu**: "Delete  X".
- **Block List X / Delete** (NifSkope::eventFilter, guarded to `o == list`):
  deletes the selected block(s) — gathers block numbers from the list
  selection (proxy-mapped) and calls the same core, same prompt. The list is
  already ExtendedSelection so Shift/Ctrl multi-select works.

Verified headlessly (WW_DELETE_TEST harness): selecting two shapes on the
37-block torso and confirming removed exactly their 9-block branch closure
(2 shapes + owned skin/shader/segment blocks), 0 dangling child links, and
all five surviving shapes kept valid renumbered skin links; saved + reparsed
clean. Confirm text verified = "Delete selected objects?".

## 2026-07-18e — Clone freeze fixed: tlCloneBlock now loads with signals suppressed

User: duplicating into a new shape (18d) froze NifSkope after the confirm.
Headless repro (WW_DUPFREEZE_TEST harness in nifskope_ui.cpp — drives the
real edit-mode duplicate on the 38,450-vert PBR mesh, over-cap path, confirm
auto-answered) reproduced a >45 s hang, and step logging pinned it INSIDE
tlCloneShapeWithProps → tlCloneBlock. Object-mode duplicate (=2) hung the
same way, so this was a latent bug in the shared clone helper, NOT the 18d
code — object-mode Duplicate/Separate/Add-Primitive on any high-poly shape
would have hung too.

Root cause: NifModel::loadIndex() (unlike the full-file load()) does NOT set
Loading state, so populating a 38k-vertex clone emitted a change signal per
value/array write, and with doCompile==0 the live scene ran scene->update()
tens of thousands of times → quadratic. Fix: tlCloneBlock brackets the
insert+loadIndex in setState(Loading)/restoreState (mirroring the real
loader) and emits one dataChanged for the new block. Clone of the 38k shape
now ~0.8 s; full edit-mode duplicate-into-new-shape ~1.3 s (was a freeze).
Also gave tlKeepTriangles the standard setState(Processing) write-guard so
its large triangle-array rewrite doesn't re-storm signals (helps Separate
too). Verified end to end: PBR.001 written with 38450 verts / 76800 tris,
correct format, saved + reparsed clean. The clone shares the source's
BSSkin::Instance, same as object-mode Duplicate and Separate.

## 2026-07-18d — Duplicate over the cap now offers "duplicate into a new shape"

User feedback on 18c: the hard refusal blocked the actual workflow
(duplicating most of a 38k-vert mesh is legitimate). Edit-mode Duplicate
now, when the copy would cross the 65,535-vertex cap, pops a confirmation
("Duplicate the selection into a NEW shape instead?"). On Yes it reuses the
Separate recipe minus the source edit: tlCloneShapeWithProps clone, linked
to the same parent, tlUniqueNodeName, tlKeepTriangles keeps only the
selected faces, original untouched — the clone inherits the source's vertex
array so this path can never itself exceed the cap. Undo =
TlBlockAppendCommand inside the existing Duplicate macro. If everything
went to new shapes, edit mode exits and the new shape is selected (press G
to move — same handoff as Separate); mixed selections keep the normal
coincident-move gesture with a status note. Loose-vert-only over-cap
selections (no faces to carry) still refuse. Like Separate, the clone keeps
the full vertex array with only the selected triangles — unused verts can
be pruned later with the existing cleanup spells.

## 2026-07-18c — Duplicate/Join: 65,535-vertex cap enforced (user-reported corruption)

User repro: edit-mode Duplicate of many separate areas on the 38,450-vert
PBR sphere-grid mesh (x01_torso.nif) → 76,900 verts → giant orange blobs and
cross-mesh streaks. Root cause: BSTriShape stores Num Vertices and every
Triangle corner as uint16; `duplicateElements` had NO cap guard, so
`quint16( vremap.value(...) )` wrapped every new triangle onto unrelated
early vertices (38,450 × 2 = 76,900 > 65,535). Every OTHER growth op already
guarded (Extrude/Subdivide/Split/Rip/Bevel/Knife/Symmetrize refuse; Bridge
and Loop Cut clamp) — Duplicate and Join were the two that slipped through.

- `duplicateElements`: refuses a shape whose duplicate would cross the cap
  (status message; other selected shapes still duplicate, with a note).
- `joinSelectedObjects`: skips sources that would push the active shape past
  the cap AND now removes only the sources actually merged — previously a
  skipped/empty source block was DELETED without being merged (silent data
  loss on the same code path).

Recovery note for the repro file: the wrapped duplicate is a normal undo
step — Ctrl+Z restores the mesh; do not save the corrupted state.

## 2026-07-18b — Create Skin: corruption found by automated test, spell rewritten (byte-patch + reload)

Built a headless test harness (WW_CREATESKIN_TEST=1 env hook in
nifskope_ui.cpp, same pattern as the extrude/perf hooks) that drives the REAL
spell through NifSkope::castSpell with an auto-answer timer for its dialogs,
then verifies the saved NIF with an independent Python parser
(tools/createskin_test/). Test asset: X01_ArmLeft.nif with the skin stripped
at byte level (donor = the untouched original, so a Nearest-Vertex transfer
must reproduce the original weights exactly).

**The shipped spell corrupted the mesh.** Diagnosis, all empirically proven:

- `updateArraySize` early-returns when the vertex count is unchanged, so the
  "layout rebuild" after adding VA_SKINNING never happened.
- BSVertexData rows carry BOTH precision variants of "Vertex"/"Bitangent X"
  as children, and **condition checks pass for all of them** (`getIndex` by
  index or name cannot tell the real field from its dormant twin). The
  capture/restore-by-name therefore wrote the zero-valued full-precision
  twins over the real half-precision values: positions and bitangent X
  zeroed in-model.
- The per-row Bone Weights/Indices arrays had 0 children at write time (the
  deferred update cascade instantiates them only at holdUpdates(false)), so
  the weight-1.0 writes silently no-opped.
- The save path then wrote 40-byte full-precision-shaped records while desc
  and Data Size claimed 32 — the output file was corrupt beyond the zeros
  (triangles land at nv*40, header block size disagrees with Data Size).

**Fix — the spell no longer touches vertex rows through the model.** It
builds BSSkin::BoneData/Instance and the Skin link against the untouched
layout (block-level ops, proven good), serializes the model (faithful for
the unchanged layout), patches the BYTES (desc, Data Size, header block
size; each record gains 12 skin bytes = weight 1.0/0/0/0 float16 + indices
0/0/0/0 at the skinning offset), and reloads through the loader — the one
code path proven to build skinned rows correctly. Every byte walk
self-validates and any failure restores the pre-op serialization. The
bounding sphere is written after reload (plain value ops). The mesh bound
now comes from decoded record positions (the old code read the zero Vector3
twin — the shipped bound was a degenerate point at the skin-to-bone origin).

**Second find — Bind Donor Bones refused fresh skins.** With zero bones in
common (a Create Skin target knows only its bind node) the "shared BoneData
entry" bind-space check had nothing to compare and hard-refused, so the
atomic transfer rolled back (the rollback worked as designed). Added the
sound fallback: when no entry is shared, verify the two shapes occupy the
same skeleton-root-relative space directly — same guarantee, and per-node
rest poses of every bone actually bound are still verified individually
downstream.

**Verification (all green, tools/createskin_test/README.md has the recipe):**
- Create Skin, default parent node: 961/961 non-skin vertex bytes
  byte-identical, 961/961 exact weight-1.0-slot-0 records, triangles
  untouched, stored skin-to-bone == inv(boneWorld)·shapeWorld exactly,
  max |skinned − static| = 0.000000 over all verts.
- Deliberately bound to a rotated, translated NON-parent bone
  (LArm_ForeArm1): max deviation 8e-6 (float16 noise) — the compensation is
  right for any node choice.
- Full pipeline (Create Skin → Transfer Bones and Weights, Nearest Vertex):
  5 bones bound, 961 verts transferred, remap blob written; 940/961 weights
  match the donor exactly, all 21 outliers are coincident seam vertices
  (distance-0 ties between position twins on different bones — inherently
  ambiguous for any position-based mapping, not a defect). Geometry
  byte-identical through the whole pipeline.

Model-machinery landmine for LATER (not fixed here, affects the stock
Vertex Flags spell in flags.cpp too): after a Vertex Desc edit the model's
row conditions/save layout diverge as described above. Any spell that edits
vertex-layout flags through set<BSVertexDesc> + updateArraySize on an
unchanged count is suspect until that is fixed.

## 2026-07-18 — Cross-area batch: rigging 3B + paint mirroring, multi-section hknp encoder, link jump

User asked to "finish the remaining percentages" across rigging, collision,
and the UI overhauls. Everything below is build-green and probe-passed
(287 ms load, 1.0-1.2 ms/frame) but GUI/asset-untested. EXPLICITLY NOT
attempted (cannot be validated blind): compound/instance Havok encoding (no
reference pairs), CustomizationRemapNewBonesData (open leaked-pointer
question), in-game validations, and the remaining timeline-overhaul and
block-list cosmetic items (summary COLUMN, inline flag editor, collapsible
sections — tooltips/Flags-spell near-equivalents already exist).

- **Rigging: weight-paint Mirror (X)** — checkbox in the paint panel. The
  brush emits a second, mirrored sample stream (position-paired partners via
  mirrorPartnerOf, already-brushed verts skipped); the panel accumulates it
  separately and commits it onto the **L/R counterpart bone** resolved by
  name (riggingFlipBoneName: Left/Right words, FO4 LArm_/RArm_-style
  prefixes, _L/_R suffixes; same bone when no side marker). Main + mirrored
  halves are ONE undo macro. No live heatmap for the mirrored side (the
  heatmap shows the selected bone).
- **Rigging: transfer options** — Mapping combo (Closest Face = barycentric
  default / Nearest Vertex = verbatim copy for identical topology) and Max
  Bones (1-4) spin in the workspace's advanced group; persisted QSettings
  read by riggingTransfer itself, so the atomic flow AND direct spells all
  honor them.
- **Rigging Phase 3B: Create Skin (bind to node)** — new spell + workspace
  button "0.": gives an entirely UNSKINNED BSTriShape a complete FO4 skin.
  VertexDesc gains VA_SKINNING (ResetAttributeOffsets + Data Size + array
  rebuild) with every existing attribute preserved by name (positional
  leaf-value capture/restore per field); all verts bind to one chosen node
  (weight 1.0, slot 0); BSSkin::BoneData (bounding sphere in bone space,
  skin-to-bone = inv(boneWorldAbs) * shapeWorldAbs so the mesh does not
  move) + BSSkin::Instance (Skeleton Root -> block 0) wired to the shape.
  The donor pipeline then works on it unchanged. Snapshot undo. Shader
  skinned-flag NOT touched (message tells the user to check the material).
  VERIFY: mesh must not move after casting, then in-game.
- **Collision: multi-section compressed-mesh encoder** — the 255-vert/tri
  cap is gone. Meshes partition into spatial slabs (longest-axis sort,
  greedy fill to the u8 section budgets), each section quantizes against its
  OWN domain (11:11:10 bits — precision now scales with density), quads
  hold section-relative u8 indices, boundary verts duplicate (the
  shared-vertex table stays unused, like every decoded Elric sample).
  Single-section output stays byte-identical to the in-game-validated
  writer (including its +0x4c quirk; multi-section writes the semantically
  correct 0 there). Compile's round-trip gate now also requires the decoded
  triangle count to equal the input. Up to 4096 sections (~1M tris).
  VERIFY: compile a >255-vert collision, then walk on it in-game.
- **Block Details: link jump** — double-clicking a link field's NAME column
  selects the linked block; the Value column keeps its inline editor.

## 2026-07-17 — Modeling backlog mega-batch (9 features), build green, GUI-untested

The user green-lit the whole modeling backlog in one order. Everything below
compiled clean per feature and the WW_PERF_TEST probe passes (load, edit-mode
entry, select-all, 0.77-0.87 ms/frame — unchanged), but NONE of it has had an
interactive GUI pass yet. Test priority: in-place undo of Delete/Merge on a
real mesh, Rip, X-mirror, quad Subdivide, knife pokes.

- **Checker Deselect** (Select menu, unbound-by-default rebindable key):
  drops every Nth selected element, Nth/Offset in a Redo Panel v2 panel.
  Order = selection order, not Blender's connectivity walk (documented).
- **Repeat Last (Shift+R)**: re-runs the last adjust-panel operator with its
  current parameter values on the CURRENT selection (`lastOpExRerun` was
  already a plain re-run — repeat is that, minus the undo-and-adjust).
  Gesture transforms and pre-v2 panels are not repeatable (documented).
- **F9**: moves the visible redo panel next to the cursor (screen-clamped;
  new `GLView::redoPanelToCursor` signal, handled beside the panel glue).
- **Quad stage 2**: box AND circle select now select/deselect marked quads
  as whole faces via a new cached tri→partner map
  (`GLView::quadPartnerMap`, invalidated by the marks command, vert-count
  guard and file load). Subdivide treats a fully-marked quad as a QUAD:
  four sub-quads around a center vert on the diagonal midpoint, diagonal
  not cut, sub-quad diagonals re-marked; surviving marks now RE-RECORD
  against the new vertex count (marks used to die on every subdivide), all
  in one undo macro (`setQuadMarks` grew a vert-count override for this).
- **Rest-pose display**: `Scene::restPoseBlock` finally has a consumer —
  Node::viewTrans/worldTrans return `restWorldTrans()` (authored transforms
  straight from the NIF, whole parent chain) for the edited shape. Bone-level
  GPU skinning is deliberately unaffected (node-level rest pose only).
- **In-place undo, 5 of 6**: Delete / Merge / edit-Duplicate use per-shape
  TlShapeStateCommands in a macro (legacy-skin shapes fall back to snapshot —
  NiSkinData rewrites and partition removal aren't captured in place);
  object-Duplicate / Add Primitive use the new `TlBlockAppendCommand`
  (redo re-runs the append-only creation closure, undo removes the created
  block range highest-first and prunes the null child links via
  `tlRemoveNullChildLink`); Separate = state-capture command (empty apply)
  + append command per shape. **Join stays snapshot DELIBERATELY** (it
  REMOVES blocks — restoring renumbered blocks + model-wide links in place
  is the real corruption risk; commented at the call site). The command
  helpers moved above the operator functions (qmake re-run needed).
  Redo closures capture everything BY VALUE (cursor target, selection
  lists) so a later redo is deterministic; counters report via shared_ptr.
- **Split (Y)**: detaches the face selection in place — boundary verts
  duplicated, selected faces re-pointed, selection follows the detached
  side. Per-shape in-place undo.
- **Rip (V)**: rips along a selected interior manifold edge path (no
  branches, >= 2 edges; one mesh per rip). Per interior path vertex the
  incident-face fan is split into side arcs (union-find, path edges are the
  cuts); the side under the cursor floods from the nearest path face,
  interior verts with faces on both sides duplicate, move-side faces
  re-point, and a chained move starts on the ripped verts (Esc keeps them
  coincident). Path ENDPOINTS stay welded (the slit tapers closed) — that is
  also why single-edge rips are rejected.
- **Mirror editing (X)**: Mesh ▸ Mirror Editing (X), persisted
  (GLView/Edit/MirrorX). Mirror partners pair by position (1e-3 tol,
  27-cell spatial hash probe; center-line verts pair with nobody; cache per
  topology). Unselected partners join modal G/R/S gestures as FOLLOWER
  ElemVerts (`mirrorOf`): excluded from pivot + direct transform, they take
  their source's new local position X-negated each update — raw/authored
  space, so deformed cages mirror sanely. Commit/cancel handle them through
  the normal elemVerts paths (per-vertex value commands / preview restore).
  If both sides are selected, both transform directly (no double-apply).
- **Knife v2**: interior cut points become real POKED vertices (barycentric
  attributes via new `tlWriteBaryVertex`, host tri fanned; bary clamped
  inward so fans can't degenerate; one poke per triangle); multi-mesh
  polylines apply per shape inside one undo macro; **Z toggles cut-through**
  while armed (off = a crossing must be visible: the ray at the crossing
  must hit one of the edge's own faces first). Handled in both key paths
  (GLView keyPressEvent + the app-level modal swallow).
- **Redo Panel v2 migration**: Merge by Distance and Select Linked by Angle
  left the old single-value panel (`lastOpKind` 1/2 retired; 3 = floating
  decal keeps it). Remove Doubles' default now comes from
  `lastMergeDistance`. This also makes both Shift+R-repeatable.
- **Bevel (Ctrl+B) — IMPLEMENTED after all** (user said continue): the
  rip + offset + bridge construction. Same input contract as Rip (one mesh,
  interior manifold path, no branches, >= 2 edges or a closed loop; both
  sides must not connect around the path). The path rips, both rows offset
  half the width into their own side's surface plane (centroid-based side
  direction ⊥ the local path direction — a v1 approximation of Blender's
  edge-slide directions), and the slit bridges with a MARKED-QUAD strip that
  tapers closed into the welded endpoints via one triangle each — the
  corner-termination minefield never opens. Strip winding = one whole-strip
  decision against side A's surface normal. Old quad marks touching the path
  are dropped (side B edges re-pointed), strip diagonals marked, all in one
  undo macro (state command + marks, vert-count override). Width scrubs in a
  Redo Panel v2 panel (default = ¼ average path edge length); Edge menu +
  rebindable Ctrl+B. Segments = 1 only in v1. The HIGHEST-risk item of the
  batch — test it after Rip, on a copy of the mesh.

## 2026-07-17 — Open in place: files replace the current document (new-window on request)

User request: opening a NIF (File ▸ Open, Recent Files, Recent Archive Files,
the NIF browser) used to spawn a new window whenever the current one held a
file. Every open path now loads INTO the current window, replacing the
document; unsaved changes prompt the existing Save/Discard/Cancel
(`saveConfirm()`) first. Opening a new window is still available, on request:

- **Right-click a Recent Files / Recent Archives / Recent Archive Files entry**
  (including the toolbar Open flyout's recent list) → "Open in New Window".
  Implemented as a strictly-scoped eventFilter on exactly those four menus
  (the filter also sits on qApp, so scope by pointer, not by cast). The
  popup opens OVER the still-open menu chain (Qt stacks popups like
  submenus); the chain is closed only when the choice is actually made —
  dismissing drops back into the open menu. (First cut closed the chain
  BEFORE popping up, which read as "all the menus disappear" — user report,
  fixed same day.) Recent-archive entries open a fresh window with the
  archive loaded in its browser. Status tips advertise the right-click.
- **NIF browser context menu** grew "Open NIF in New Window" next to
  "Open NIF" (routes per source: configured resource / loose file / archive
  member, via `openArchiveFile( index, newWindow )`).

Plumbing: `openFile`/`openArchiveFile`/`openArchiveFileString`/
`openConfiguredNif` return bool = "not cancelled", so batch opens can abort
cleanly: multi-select Open and the browser's Load Selected open the FIRST
file in place (one prompt; cancel aborts the batch) and every additional file
in its own window, as before. Dropping NIFs on the viewport follows the same
rules (it routes through `openFiles`). The bsaView doubleClicked connect
became a lambda (PMF connects don't tolerate the added default arg).

Deliberate exceptions: **Reload** of an archive-loaded file re-extracts in
place with NO prompt (`confirmReplace=false` — Reload is an explicit discard,
matching the loose-file reload), and the OS/single-instance handoff still
opens its own window.

Verified: build green; WW_PERF_TEST probe full pipeline on X01_Torso_Tesla
unchanged (0.8-0.9 ms/frame). The prompt flows themselves need a quick
interactive check (open-over-dirty via each path, cancel, right-click menus).

## 2026-07-17 — Review fix batch: cross-file state bleed, quad/knife hardening, O(T²) quad ops

Fixes from a code review of the recent quad/knife/grid work. Build green;
WW_PERF_TEST probe re-run on X01_Torso_Tesla.nif (full load + select + edit
mode + frame benches, 0.7-0.8 ms/frame — unchanged). The knife/quad paths
themselves still want interactive GUI verification.

- **Cross-file state bleed (worst find)**: `editHiddenTris`, `scene->hiddenTris`,
  `quadDiagonals`/`quadMarkVerts` were never cleared on file load (only
  `savedElemSelections` was) — all keyed by block number, and `hiddenTris` is
  consulted by the NORMAL render (bsshape.cpp): hide triangles in file A, open
  file B, and a same-numbered shape silently rendered with holes; quad marks
  bled whenever the vertex count coincided (likely across variants of the same
  mesh). All cleared in the same completeLoading hook now. Snapshot-undo
  reloads (nif->load()) intentionally keep the state — completeLoading only
  fires on real file loads.
- **Make Face (F)**: a triangle can only be half of ONE quad — F now refuses
  with a status hint when a picked tri already carries a marked diagonal
  (previously a second diagonal could hide two edges of one tri and made the
  partner lookup edge-order dependent).
- **Tris to Quads / Triangulate O(T²) freeze fixed**: both called
  `quadPartnerTri()` (full triangle scan) once per selected face — select-all
  on a marked mesh scaled quadratically. New `tlBuildEdgeTris()` builds an
  edge→(two tris, saturating count) adjacency once per shape; partner lookups
  are O(1) via `tlQuadPartnerVia()` (same semantics incl. the non-manifold
  no-unique-partner rule). `quadPartnerTri()` stays for the single-pick path.
- **Triangulate undo**: flip modes pushed TWO entries per shape (snapshot +
  marks) — one Ctrl+Z restored the marks but left the flipped triangles.
  Wrapped per shape in a QUndoStack macro: one Ctrl+Z per shape now. The
  adjust-panel undoSteps counting is macro-aware by construction (index delta).
- **Knife vs undo**: undo could still fire while the knife was armed (Edit
  menu by mouse; Ctrl+Z with the pointer off the viewport) and invalidate the
  cut points' vertex indices. beginKnife now watches
  `QUndoStack::indexChanged` and cancels the knife on any stack activity
  (watcher disarmed before knifeApply pushes its own command); applyKnife
  additionally drops any cut whose vertices no longer exist before writing
  lerp rows (belt and braces — also covers a redo after external changes).
- **Knife stale picks**: the cut rebuilds the triangle list (face indices
  shift, cut edges are replaced), so edge/face picks on the cut shape are now
  dropped after apply; vertex picks survive (indices only ever appended).
- **Knife overlay**: committed points that fail to project after an MMB orbit
  (behind the camera) drew lines to a (-1e6,-1e6) sentinel — a spurious line
  shooting across the viewport. Segments/markers touching an unprojectable
  point are now skipped.
- **drawGrid hygiene**: the ortho grid pass restores GL_LESS before returning
  (the coplanar-fix LEQUAL no longer leaks into later passes; benign today
  since every pass sets its own depth func, but cheap to make airtight).

Reviewed but deliberately NOT changed: the per-frame edit-overlay rebuild
(O(V) transform + O(T) edge dedup every frame — 0.6-0.8 ms/frame on real
meshes; a revision-keyed cache is the fix if profiling ever demands it, and a
missed invalidation there would corrupt the edit wireframe), and the
load-time attribution (the probe's reload peel-off shows ~⅔ of the attached
overhead is the Block Details + Header tree views — that investigation is
in flight).

## 2026-07-17 — Ortho grid: two depth bugs fixed (user-confirmed), Blender-matched

Two long-standing ortho-view grid defects found via the headless render probe
(framebuffer PNG grabs through the real numpad path + pixel histograms):

- **Coplanar depth suppression**: the decade grid levels and the origin axis
  lines are coplanar; with GL_LESS the first (faint fine) level to write
  depth suppressed every co-linear line drawn after it — the strong
  mid/major levels and the red/green axes were invisible (measured ~0.055
  effective alpha instead of 0.5). Fixed with GL_LEQUAL for the grid pass.
- **Far-plane clipping in "opposite" views (Ctrl+7 bottom, back, right)**:
  glProjection() extends the clip range by an origin sphere when Show Axes
  is on, but the grid plane's z-push used scene-only bounds. In views where
  the origin sits nearer the camera than the model, the far plane shrank and
  the plane (pushed to 1.45× the radius) landed beyond it — grid and axes
  clipped wholesale. The grid now mirrors glProjection's bounds exactly
  (new GLView::axisMarkerRadius() accessor). User-confirmed fixed.

Grid cosmetics settled with the user against Blender 4.5 references:
grid default 100,100,100 @ 0.5 (all earlier defaults migrate), near-pure
red/green axis colours with a 0.9 alpha floor, UV editor lines softened to
neutral grey, and the UV grid made zoom-adaptive (gridBaseDiv, powers of 8).

## 2026-07-17 — A-key fix (menu bar stole activation); Blender-matched grids

- **"A stopped working in edit mode"**: the floating viewport menu bar is a
  separate Qt::Tool window — clicking any of its menu buttons ACTIVATED that
  window, deactivating the main one. Focus-follows-mouse is gated on
  isActiveWindow(), so every viewport key (A, G/R/S, …) went dead until the
  next click inside the viewport. The bar now carries
  Qt::WindowDoesNotAcceptFocus: its menus work as before but never steal
  activation.
- **3D viewport grid dimmer than Blender**: the grid draws with
  FRAMEBUFFER_SRGB off, so the old default (99,99,99 @ 0.8 alpha) displayed
  much darker than intended. New default 150,150,150 @ 0.92; values saved
  with the old default migrate automatically (Settings ▸ Render color picker
  default updated to match).
- **UV editor grid vanished when zoomed out**: the shader's grid levels were
  FIXED subdivisions (8/64/512 per tile) — zoomed out they packed sub-pixel
  and washed out to nothing. The grid base is now zoom-adaptive
  (`gridBaseDiv` uniform, powers of 8 keeping the coarsest spacing in a
  readable 24–192 px band at any distance, coarsening beyond one line per
  tile when far out) with the ×8 finer level crossfading in on zoom — the
  Blender behaviour. The legacy uvedit widget keeps its classic fixed grid
  (it passes gridBaseDiv = 8).

## 2026-07-17 — Quad feedback batch: diagonal in selection outline; redo panels

User feedback on stage 1, all three points addressed:

- "F does not immediately create a visual quad / quads re-show their
  triangles when selected again": the base wireframe hid the marked diagonal
  correctly, but the selected-face OUTLINE drew all three edges of every
  filled triangle — putting the diagonal right back on top while the quad (or
  its verts) were selected, i.e. immediately after F. The outline builder now
  skips valid marked diagonals, so a quad reads as a quad the moment F lands
  and whenever it is selected.
- "No popup with triangulation options": Triangulate (Ctrl+T) now arms the
  Blender-style adjust-last-operation panel with a Quad Method dropdown
  (Keep Diagonals / Beauty / Shortest Diagonal / Longest Diagonal) — switch
  the method live after the cut, F9-style. Tris to Quads (Alt+J) got its
  panel too (Max Face Angle° / Max Shape Angle°, scrubbable).

## 2026-07-17 — Knife tool (K) + THE STARTUP GRID BUG FIXED FOR REAL

**Grid bug root cause found at last** (while adding the knife preview): the
QPainter overlay cleanup at the end of paintGL called a RAW `glUseProgram(0)`
every frame, desyncing the renderer's cached current program. When the last
cached program of a frame was `lines.prog` (exactly what the ground grid and
origin axes leave behind), the next frame's grid draw hit the cache, skipped
the rebind, and rendered with program 0 — invisible. With a selection, later
overlay passes bind other programs, the cache mismatches, the rebind happens,
and everything "healed" — which is why only the first click ever fixed it.
Fix: unbind through `renderer->stopProgram()` so the cache stays coherent.
This also explains every earlier falsified theory; the WW_PERF_TEST probes
were chasing state that was identical because the desync lived in a cache the
traces never printed.

**Knife (K)** — Blender-style modal cut tool (v1):
- K arms it in edit mode (cross cursor, status hints). LMB places cut points
  with Blender snapping (vertex 11 px, then edge 8 px, else a free point on
  the face); MMB orbits mid-cut (the line is re-projected each frame and
  stays glued to the surface); Enter applies; Esc / RMB cancels; leaving edit
  mode cancels. While armed, all other single-key viewport shortcuts are
  inert (modal, like Blender).
- Preview: white cut line, dashed rubber band to the hover point, green
  squares on committed points (filled when snapped), green/white hover ring.
- Apply splits every edge the polyline crosses (screen-space crossing test
  against front-facing, non-hidden triangles), inserting lerped vertices at
  the exact crossing (attributes interpolated via tlWriteLerpVertex) and
  re-splitting affected triangles with the proven Subdivide splitter (1/2/3
  cut edges per tri). One undo step (TlShapeStateCommand "Knife").
- v1 limits: points snapped to a face interior are waypoints only (cuts land
  on the crossed edges); occluded front-facing edges can still be cut
  (Blender's "cut through" behaviour, permanently on); one mesh per cut.
- Edge menu gained "Knife… K"; binding `viewport.knife` is rebindable.

## 2026-07-17 — Quad modeling, stage 1: quad layer, Make Face (F), Tris to Quads (Alt+J), Triangulate (Ctrl+T)

Blender-style quads over the triangle-only NIF format. A quad is a pair of
adjacent triangles whose shared edge is *marked* as a diagonal
(`GLView::quadDiagonals`, per shape): the edit-mode wireframe hides the
diagonal, face picking selects both halves as one face, and Loop Cut walks
marked diagonals in preference to its parallel-direction guess. **The NIF
data stays triangles at all times, so saving needs no triangulation step** —
the requested "triangulate on save" holds by construction. Mark sets are
undoable (TlQuadMarksCommand) and validated against the live topology: a
changed vertex count invalidates a shape's marks, non-manifold or hidden
diagonals draw normally again.

- **F — Make Face**: two adjacent face-picked triangles (or exactly the four
  corner vertices of a tri pair) form a quad; anything else falls back to the
  existing Fill / Bridge, so F keeps all its old uses. Registered binding
  renamed "Make Face (quad) / Fill / Bridge".
- **Alt+J — Tris to Quads**: greedily pairs the face-selected triangles,
  Blender-style: candidates rejected above a 40° face-angle (fold) or 40°
  corner deviation from 90° (shape), best-cost pairs win.
- **Ctrl+T / Face ▸ Triangulate Faces**: splits selected quads back to
  visible triangles with diagonal options — Keep Diagonals (Ctrl+T default),
  Beauty (Delaunay max-min angle), Shortest Diagonal, Longest Diagonal. Flips
  rewrite the two triangles (undoable, winding preserved via the quad loop).
- Face menu and the W Specials carry the new entries.

Known v1 limits: box/circle select still add quad halves individually (the
fill highlight can show a half-quad); Subdivide is not yet quad-aware; the
Knife (K) is the next stage.

## 2026-07-17 — Startup grid/axes missing until first click: investigation + tentative fix

Reported: after loading a NIF the ground grid (and origin axis lines) do not
appear until something is clicked in the viewport. Reproduced headlessly with
the WW_PERF_TEST probe (framebuffer PNG dumps). Established facts:

- Every guard passes on the broken frames: scene options intact, drawGrid
  executes, buffers allocate, lines.prog resolves, colors/line widths sane,
  modelview + projection matrices and all queried GL state (FBO, depth, blend,
  masks, viewport) are BIT-IDENTICAL between a broken and a working frame.
- The model renders fine in broken frames; only the streaming line geometry
  (grid, axes) is invisible — the failure is inside the streaming-draw path,
  not scene state.
- In probe runs, a frame rendered after the selection state became valid (or
  after an update+processEvents round trip) always showed the grid, and it
  stayed visible after deselecting again.
- grabFramebuffer re-renders offscreen, so probe captures are an imperfect
  proxy for the live window; final verification must be done in the GUI.

Tentative fix shipped: `GLView::postCompileRepaints` — after a scene compile,
two follow-up repaints are scheduled (16 ms apart), so the user never sits on
the stale post-load frame. Harmless regardless; whether it cures the live
symptom needs a GUI check. If the grid is still missing on load, next step is
diffing the streaming VAO/program state with a GPU debugger.

UPDATE (00:30): repaints alone were NOT enough — user confirmed live that the
grid appears only once something is selected. A synthetic-currentBlock
workaround was tried next (point currentBlock at the root for the corrective
repaints, emulating the first click — probe frames with a valid currentBlock
always showed the grid); it first broke initial camera centering
(setCenter() frames the current block's node; the root has zero-radius
bounds) and, re-sequenced after centering, STILL did not heal the grid live.
REVERTED (00:45). Conclusion: none of the CPU-visible state theories survive
— the difference between a real first click and everything emulated so far is
still unidentified (the click path also runs the pick render + full selection
sync; probe grabs conflated several of these).

**STATUS: OPEN.** Symptom: grid + origin axes (all streaming line geometry
inside the scene pass) invisible after loading a NIF until the first click in
the viewport; harmless otherwise. Everything checked and ruled out is listed
above; `WW_PERF_TEST=1` re-arms the full instrumentation (stage timings,
drawGrid guard/GL-state traces, ShapeData creation trace, framebuffer PNG
dumps). Next step: RenderDoc capture of the first post-load frame vs the
first post-click frame and diff the grid draw call's GPU state (actually
bound program, VAO, vertex attribute bindings, uniform values) — printf
instrumentation has been exhausted.

Diagnostics kept (all inert without WW_PERF_TEST=1): drawGrid guard + GL-state
traces, ShapeData creation trace, probe stages with PNG dumps in createWindow.

Build-infra lesson recorded: `make ... | tail` reports TAIL's exit code — a
compile failure sailed through unnoticed. Build commands now echo make's own
status; PowerShell 5.1 also mangles bash -lc strings containing double quotes
(use simple redirects instead of quoted pipelines).

## 2026-07-16 — Slow click FOUND AND FIXED: the Block Details filter (round 3)

The row-hiding walks (rounds 1-2) were real but not the 3 seconds. A new
WW_PERF_TEST probe (env-gated stage timing in NifSkope::select + the
selection-changed handlers, driven headlessly on the user's x01_torso.nif)
attributed it exactly:

- `applyBlockDetailsFilter()` ran on EVERY `currentNifIndexChanged` and — even
  with an EMPTY filter box — recursed the entire current block and
  **stringified every value column** (38k vertices × members ≈ 480 ms), and it
  fired TWICE per viewport click (direct select + the list-mirror echo).
  Fix: early-out when the filter is empty and no filter was previously active
  (`blockDetailsFilterWasActive`); clearing a real filter still walks once to
  un-hide.
- The UV editor rebuilt itself from the viewport on every object-selection
  change even while hidden (~55 ms): now defers to its next showEvent
  (`viewportRebuildPending` + `deferredRebuildCb`).

Probe before → after: objectSelectClick 545 ms → 5 ms; NifSkope::select
485 ms → 3 ms; pick render and paints were always 1-2 ms. Rigging /
vertex-paint / meshtools handlers measured 0 ms (already visibility-guarded
or cheap).

The WW_PERF_TEST instrumentation stays in (zero cost unless the env var is
set): run `WW_PERF_TEST=1 NifSkope.exe <file>` to re-measure; it writes
ww_perf_test.log next to the exe and quits.

## 2026-07-16 — Row hiding is now derived lazily (round 2 of the slow-click fix)

The first fix only made the doItemsLayout pass shallow; two more full-subtree
walks still ran per click: `currentChanged` (fires on every click, full
descent of the current row's subtree — selecting the sphere block descended
its 40k-vertex arrays) and `refreshRowHiding` (every block switch). All three
hot paths now pass `descendArrays=false`; hiding INSIDE arrays is derived
lazily, one level per expansion, by the (now also shallow) expansion hook —
expanding an element derives that element's members via the same hook.
Full walks remain only where they are correctness-critical and rare: the
explicit "show non-applicable rows" toggle and the dataChanged condition
update (e.g. VertexDesc edits re-deriving member visibility).

Known tradeoff: an interior array node that stays expanded across block
switches keeps its earlier-derived hidden set (the rot-prone storage) until
it is re-expanded or its data changes; the depth-1 version-gated rows the
original bug was about are still re-derived on every switch/layout.

NOTE when verifying: the reporter's screenshot showed "build ca555d7" in the
title bar — a stale running instance from BEFORE both perf fixes (NifSkope
hands new files off to a running process). Fully close every NifSkope window
before judging a fix.

## 2026-07-16 — Slow click-select on high-poly blocks fixed; legacy render hotkeys removed

**Perf:** selecting a high-poly shape in the viewport (reported on the
x01_torso spheres) had become very slow. Cause: the row-hiding
`doItemsLayout()` re-derivation walked the Block Details root's ENTIRE
subtree — including a big shape's vertex/triangle arrays, tens of thousands
of model visits — and layouts fire constantly (selection change, scroll,
auto-expand), multiplying the walk several times per click.
`updateConditionRecurse()` gained a `descendArrays` flag: the frequent
doItemsLayout pass no longer descends into arrays (array element rows are
not individually version-gated), while the root-change path
(`refreshRowHiding`) keeps the full walk that derives array-member hiding.
The original rot fix stays intact for the rows it was built for.

**Legacy hotkeys removed** (10 shortcut properties stripped from
nifskope.ui): Lighting Only Alt+L, Update View Alt+U, Silhouette Alt+D,
Show Grid Shift+G, Textures Alt+T, Vertex Colors Alt+V, Frontal Alt+F,
Specular Alt+H, Glow Alt+G, Cube Mapping Alt+R. The menu items remain
clickable; they simply no longer own keys (and Alt+H no longer shadows
Blender unhide). With no current or default binding they also disappear
from the Settings ▸ Shortcuts list automatically.

## 2026-07-16 — Swappable select / place-gizmo mouse buttons

Blender-style "Select With" mouse mapping: by default LMB clicks select and
RMB clicks place the gizmo / 3D cursor; a new option swaps them (2.7x
right-click select). Only the *click* roles swap — drags (orbit / RMB zoom),
the box/circle gadgets, gizmo handles, and paint strokes keep their buttons.

- `GLView::selectWithRightMouse` + `selectMouseButton()` / `cursorPlaceButton()`;
  stored as QSettings `Shortcuts/MouseSelect` = left|right, read at GLView
  construction and pushed per-window by `applyShortcutOverrides()` after each
  settings save.
- Click interpretation is now fully explicit in `mouseReleaseEvent`: the
  edit-mode element pick, the object/block select, and the cursor placement
  each check their button. This also fixes a latent quirk where a plain RMB
  click silently re-picked the selection before placing the gizmo (the select
  block had no button check).
- Cursor placement moved out of `contextMenuEvent` into `mouseReleaseEvent`
  (no more synthesized QContextMenuEvent; the keyboard menu key still opens W).
- Settings ▸ Shortcuts grew a "Select with mouse button" combo at the top, and
  a rebindable, unbound-by-default `viewport.swap_mouse_select` key toggles the
  mapping live with a status-bar confirmation.

## 2026-07-16 — Rebindable shortcuts (Settings ▸ Shortcuts, with search)

Everything currently bound can now be rebound in Settings ▸ Shortcuts:

- **New `ShortcutRegistry`** (`src/shortcutregistry.{h,cpp}`): central table of
  the ~38 built-in 3D-viewport bindings (ids like `viewport.transform.move`),
  each with label/category/default/current; user overrides persist under
  QSettings `Shortcuts/<id>`. `matches(id, key, mods)` does exact
  key+modifier matching. Bindings registered once in glview.cpp
  (`tlRegisterViewportShortcuts`).
- **Viewport key handlers converted**: every hard-coded key check in
  `GLView::keyPressEvent` and the viewport-scoped block of
  `NifSkope::eventFilter` now goes through `matches()` — mode toggles, G/R/S
  (including in-gesture mode switching), selection ops (A/Alt+A, B, C, Ctrl+I,
  Ctrl+L, Ctrl+Alt+Shift+F, Ctrl+=/−), H/Alt+H, X delete, M/P menus,
  Shift+D/S/A/C/V/F, Ctrl+J/P/R/X, Alt+P, 1/2/3 pick modes (Shift still
  extends), paint fills, frame selection, free camera. Fixed by design: the
  modal gesture grammar (X/Y/Z axis locks, numeric entry, Esc/Enter), the
  Delete-key alternate, Escape, and the Blender numpad view block.
- **QAction shortcuts**: `NifSkope::applyShortcutOverrides()` records each
  action's factory default and applies `Shortcuts/action.<objectName>`
  overrides at startup and after every settings save (per window; `options`
  dialog is shared).
- **Settings ▸ Shortcuts pane** (`src/ui/settingsshortcuts.cpp`, registered in
  settingsdialog.cpp): search bar filtering by name, category, id, or key
  ("extrude", "Ctrl+R"); category-grouped tree (viewport categories + one per
  menu); inline QKeySequenceEdit capture (single chord); per-row reset button;
  duplicates within a group highlighted red (cross-group sharing allowed —
  different scopes); Restore Defaults resets every row. The pane populates
  lazily on first show (actions don't exist yet when the dialog is built) and
  hooks the standard pane read/write/Apply flow.

## 2026-07-16 — Viewport menus move into the viewport (floating bottom bar)

Follow-up to the header-menu batch below: the Select/Add/Object (and edit/
paint) menu buttons leave the render toolbar and now live in a floating
Blender-dark rounded bar centered on the 3D viewport's bottom edge. Like the
redo panels, the bar is a frameless `Qt::Tool` window (`WA_ShowWithoutActivating`,
`WA_TranslucentBackground` for real rounded corners) because the native GL
viewport paints over child widgets. It's glued to the viewport by the same
event-filter hooks as the redo panels (Move/Resize), hides on minimize,
returns on restore, and first shows via a deferred call after the main
window's first Show. Buttons: flat text, hover highlight, selection-blue
open state, no menu-indicator arrows (Blender look). Mode swapping and the
shared `GLView::populate*Menu()` builders are unchanged;
`NifSkope::positionViewportMenuBar()` centers it (redo panels keep the
bottom-left corner).

## 2026-07-16 — Viewport RMB menu retired; Blender-style header menus

The 3D viewport no longer has a context menu. A plain right-click (click, not
an RMB zoom drag) now drops the gizmo / 3D cursor on the surface under the
mouse — what Shift+RMB used to do. Shift+RMB is retired (both press handlers
removed); RMB-drag zoom and RMB-cancel of the box/circle select gadgets are
unchanged (box-cancel now also suppresses the gizmo drop on release, like
circle-cancel already did). The keyboard menu key opens the W quick menu.

Everything the RMB menu carried moved to Blender-style viewport header menus —
flat text buttons right of the mode selector on the render toolbar, swapping
with the mode:

- **Object mode:** Select · Add · Object (transform/snap/set origin/duplicate/
  join/parent/show-hide)
- **Edit mode:** Select · Mesh (transform/snap/origin/extrude/duplicate/
  separate/symmetrize/normals/floating decal/show-hide/delete) · Vertex
  (merge/remove doubles/smooth/dissolve) · Edge (loop cut/subdivide/edge
  slide) · Face (extrude/inset/fill-bridge)
- **Paint modes:** Select · Weights/Paint/Segments (fill selection + show/hide)

The buttons rebuild their menus on aboutToShow from new
`GLView::populate*Menu()` functions; the W quick menu shares them (a Transform
section — Move/Rotate/Scale — now heads W in both modes, hidden while
painting), so the entry points cannot drift apart. The block-data spell
submenus (Mesh/Havok/…) left the viewport entirely — they remain on the Block
List / block-tree context menus, which are untouched. The old RMB-menu-only 3D
Cursor items were already covered by the Snap… (Shift+S) menu. Implementation:
`NifSkope::contextMenu`'s graphicsView branch deleted, `contextMenuEvent`
rewritten to place the cursor, `contextMenuShiftModifier` removed (GLView
layout change — qmake6 re-run).

## 2026-07-16 — Skyrim/FO76 rows: FINAL fix (hidden-row state rots between clicks)

The user was right and the stale-instance theory was wrong. Reproduced with a
probe sweep that clicks every BSLightingShaderProperty through the real Block
List path: the FIRST block visited hides correctly (29/29 rows), every
SUBSEQUENT block leaks all 29 — matching the screenshot exactly (their block
32 vs the probe's block 10; earlier probes only ever tested the first block).

Root cause: QTreeView keeps hidden rows as QPersistentModelIndexes, and model
activity during a block switch silently INVALIDATES them (no modelReset — the
reset-override never fired). The hiding pass ran and verified 29/58 hidden at
click time (trace-proven), then the stored set rotted before paint.

Fix: `NifTreeView::doItemsLayout()` override re-derives row hiding for the
current root right before the view rebuilds its layout (re-entry-guarded,
skipped while the model is loading). Whatever invalidates the stored set, the
rows are re-hidden with fresh indexes before anything is drawn. Sweep now
reports 29/29 hidden on every shader block via the real click path.

## 2026-07-16 — Row hiding on expansion (tree mode); Skyrim/FO76 rows re-verified

User re-report: greyed Skyrim + FO76 rows visible on a shader property.
Probe-verified on the CURRENT build against the same file: all version-gated
rows (Skyrim flag variants, GTEFO76 data, Num SF1/SF1) ARE hidden — the
screenshot matches the pre-fix row set, i.e. a stale running NifSkope
instance (the app hands off to a running instance, so opening a NIF can
silently reuse an old process; fully close all windows after an update).
One real gap found and fixed anyway: the root-scoped hiding pass never
covered whole-model TREE mode (invalid root) or rows first revealed by
expanding — an `expanded` hook now re-applies hiding for whatever becomes
visible. Probe now logs row types alongside names.

## 2026-07-16 — W: Blender-style Specials quick menu

`W` over the viewport (user request) opens the 2.7x-style Specials menu — the
one-key hub for the modeling operators (hover-out AutoCloseMenu, like the
other operator popups). Guarded so W stays camera-forward in free-camera /
walk mode and ignored while a text field has focus.

- **Edit mode:** Subdivide, Smooth Vertices…, Merge…, Remove Doubles…
  (= Merge by Distance with the redo-panel default), Dissolve Vertices |
  Extrude Region…, Fill / Bridge…, Inset Faces…, Edge Slide… | Flip Normals,
  Recalculate Normals, Symmetrize… | Hide Selection, Reveal All, Invert
  Selection. Selection-dependent items grey out. (Loop Cut is deliberately
  absent — it needs the cursor on an edge, so it stays on Ctrl+R.)
- **Object mode:** Add Primitive…, Duplicate, Join | Snap…, Set Origin….

## 2026-07-16 — Modeling tools batches 3-5: eight operators at once

Everything remaining from `MODELING_TOOLS_PLAN.md` except Bevel (deferred:
correct tri-mesh corner terminations are a mesh-corrupter risk; better zero
than wrong). Prior work committed first as d5765c4. All BSTriShape-only,
Processing-batched writes, area-weighted normal refresh, Redo Panel v2.

- **Loop Cut (Ctrl+R)** — ring walk from the edge under the cursor across
  tri-pair "quads" (diagonal chosen by opposite-tri validity + most-parallel
  continuation; a/b chain orientation propagated); ladder re-triangulation
  (2(C+1) tris per quad, winding matched to the original face normal); new
  ring verts are true row interpolations (`tlWriteLerpVertex`: UVs, normals,
  top-4 bone weights). Panel: Number of Cuts (1-64). No slide in v1 — select
  the new ring and use Edge Slide.
- **Edge Slide (Shift+V)** — the selection slides along its unselected
  neighbor edges; the panel's Factor (-1..1) IS the modal. Positions via
  ChangeValueCommand transaction + in-transaction normal refresh.
- **Subdivide** (menu) — midpoint split of the selected edges/faces
  (1/2/3-marked-edge tri splits, winding preserved).
- **Inset Faces (I)** — region inset via the extrude plan machinery (dup
  boundary + rim band), duplicates pulled inward perpendicular to the region
  normal. Panel: Thickness / Depth (armed at 0 — scrub to inset).
- **Dissolve Vertices (Ctrl+X)** — each interior vert's fan is removed and
  its 1-ring re-capped (ear clip); boundary/non-manifold verts skipped with a
  count. Also cleans up extrude scaffolds.
- **Symmetrize** (menu) — mirror across a local axis with seam weld: keeps
  one side, snaps near-plane verts onto the plane, mirrored copies with
  flipped winding + mirrored normals/tangents. Panel: Direction (6-way enum —
  first Enum consumer of Redo Panel v2) / Merge Distance. v1: crossing
  triangles are dropped, not bisected; bone weights copy unmirrored.
- **Flip Normals / Recalculate Normals** (menu) — winding reversal of the
  selected faces / area-weighted vertex-normal recompute, both as single
  ChangeValueCommand transactions.
- **Add Primitive (Shift+A, object mode)** — Plane / Cube / Cylinder / UV
  Sphere as a new BSTriShape at the 3D cursor, cloned from the active shape's
  vertex layout + shader/alpha properties (skinned templates rejected), with
  normals/tangents/UVs generated. Panel: Size / Segments.
- New shared machinery: `TlShapeStateCommand` (generic in-place undo for
  arbitrary single-shape rewrites — snapshots the Vertex Data + Triangles
  subtrees as typed values via tlCaptureValues/tlRestoreValues; no model
  reload), `TlExtrudeCommand` generalized to an apply closure (Inset reuses
  it), `tlPushPositionCommands`, `vertexOpTarget` shared validation.
- DEFERRED (explicit): Bevel; in-place undo for Delete/Merge/Duplicate (still
  snapshot-flash on Ctrl+Z); old Merge/Select-Linked panels not yet migrated
  to Redo Panel v2.

## 2026-07-16 — Modeling tools batch 2: Fill (F) + Bridge Edge Loops

`MODELING_TOOLS_PLAN.md` Phase 2, the connection cluster. One smart operator
(`GLView::smartConnect`, `F` in edit mode with the pointer over the viewport —
routed via eventFilter so `F` stays Front View elsewhere; also in the context
menu as "Fill / Bridge…"):

- **Rim extraction** (`tlExtractLoops`): explicit edge picks if present, else
  mesh boundary edges (exactly one adjacent non-degenerate face) with both
  endpoints selected; chained into ordered loops DIRECTED ALONG THE HOLE
  (reverse of the adjacent surface winding, so caps/bands wind correctly).
  Non-manifold rims (3+ rim edges at a vertex) are rejected with a message.
  Scaffold/degenerate triangles are ignored throughout.
- **Fill** — ONE closed loop selected: ear-clip cap in the loop's best-fit
  plane (Newell normal, dominant-axis projection, reflex/containment ear test,
  fan fallback for pathological rims). Adds n−2 triangles, no new verts.
  Redo panel: `Flip Normals`.
- **Bridge Edge Loops** — TWO loops (both closed rings or both open runs):
  band of triangles between them. B is aligned to A by nearest start vertex
  (+`Twist`) and sampled-distance direction choice; equal-count loops band as
  quads, unequal counts zip by normalized arc length. `Number of Cuts` inserts
  interpolated rings (equal counts only — clamped otherwise and by the 65,535
  vert budget): each ring vertex is a true row interpolation via
  `tlWriteLerpVertex` — position, UV, normal/tangent renormalized, **bone
  weights merged/top-4/renormalized**. Redo panel: Cuts / Twist / Flip Normals.
- **In-place undo** via the new shared `TlMeshGrowCommand` (append-only ops:
  undo shrinks the arrays and restores saved rim normals + Data Size — no
  snapshot, no reload flash, panel scrubbing included).
- All writes under Processing (no dataChanged storms), one per-shape
  notification; affected normals recomputed area-weighted.

## 2026-07-16 — Extrude: in-place undo (no more "model turns off" on Ctrl+Z)

Follow-up to the user's undo-flash report: Extrude no longer uses a
whole-model snapshot for undo.

- **TlExtrudeCommand** (glview.cpp): redo applies the extrude plan in place;
  undo shrinks the vertex/triangle arrays back, restores the re-pointed region
  triangles, moved interior-cap positions, refreshed normals (pre-captured in
  the constructor), Data Size and bounds — instant, no `nif->load()` reload,
  no visible flash. Both the E-key path and the redo-panel re-run push this
  command, so **scrubbing Move X/Y/Z in the Extrude panel is now flash-free**
  too. (Delete/Merge/Duplicate still snapshot — same conversion is possible
  per-op later if their undo flash bothers.)
- **Stale-pick sanitation**: `refreshPickedElementPositions` now REMOVES picks
  whose vertex/triangle indices no longer exist (an in-place undo shrinks
  arrays under a live selection) instead of merely skipping their position
  refresh.

## 2026-07-16 — Skyrim rows on FO4 NIFs: the REAL fix (model reset wipes hidden rows)

Third attempt at this bug (user: "Skyrim properties are still visible"), and
this time root-caused with the probe harness rather than patched at a call
site. The predicate (`NifTreeView::isRowHidden`) was always right, and the
hiding pass DID run and apply (verified: 29 of 58 rows hidden on the X01
BSLightingShaderProperty right after selection) — but **a model reset lands
during load completion and `QTreeView::reset()` silently clears ALL
hidden-row state**, un-hiding everything after the fact.

- `NifTreeView::reset()` override: after every model reset, a deferred
  `refreshRowHiding()` re-applies hiding once the root/current are restored.
  Also covers snapshot undo/redo (`nif->load()` → modelReset → reset).
- New public `NifTreeView::refreshRowHiding()`: whole-block re-apply that
  defers itself while the model is loading/processing (the old inline pass
  silently bailed on `state != Default` and stayed stranded, because a later
  select() of the same block skips setRootIndex).
- Safety nets so no path can strand again: `completeLoading` → refresh (tree +
  header), `NifSkope::select()` same-root branch → refresh, `currentChanged` →
  refresh.
- Diagnosis trail preserved: WW_EXTRUDE_TEST probe now also dumps row-hiding
  state (predicate vs actual view) for the first BSLightingShaderProperty, and
  nifview.cpp has env-gated `wwHideTrace` logging to ww_hide.log.
- Bonus probe measurement: the vertex-delete now takes ~0.6 s on the 600-vert
  probe shape (was 10 s before yesterday's Processing fix).

## 2026-07-15 — Vertex picking: no more picking through opaque geometry

Regression from the floating-vert pick fix (user report): the screen-space
nearest-vertex search ignored depth, so back-side vertices could steal a click
through the surface. `nearestScreenVertex` now occlusion-tests each candidate
that would become the winner: one raycast at the vertex's own screen position,
distance compared along the view ray (a vertex ON the hit surface passes the
0.1%+0.01u tolerance, so ordinary surface verts and floating spur verts in
front both still pick; an occluded candidate is skipped so a visible
second-best can win). X-ray mode (Alt+Z) deliberately keeps picking through.

## 2026-07-15 — Topology ops: 10× freeze fix (dataChanged storms) + tolerant normals

User reports: deleting a vertex froze NifSkope for seconds; the "Could not find
Normal subitem" warning reappeared when extruding after a delete.

- **Freeze root-caused and fixed, measured via the WW_EXTRUDE_TEST harness**:
  compacting the packed vertex array emits a dataChanged per leaf write
  (~15 per moved row × thousands of rows), and every live view reacted to each
  one — the UV editor re-read all UVs and rebuilt islands *per write*.
  One deleted vertex on a 600-vert probe shape: **10,058 ms → 1,110 ms** after
  wrapping the mutation in `setState(BaseModel::Processing)` (the proven
  writeLiveUVs pattern) with a single per-shape dataChanged at the end.
  Applied to: deleteGeometry, mergeVertices, duplicateElements,
  tlExtrudeApplyPlan, Rip UV Faces, Smart UV Project. This also eliminates all
  mid-mutation view reactions — the prime suspect for the condition-cache
  poisoning behind the Normal warning (getItem() gates on evalCondition, cached,
  and BSVertexData rows delegate to row 0's same-position child via
  getConditionCacheItem — a mid-op false evaluation would stick).
- **Normal lookups made tolerant**: tlAccumulateAreaNormals /
  tlRecalcNormalsSubset / tlPushNormalCommands now probe rows with the
  NON-reporting `getItem( row, "Normal" )` and skip incomplete rows instead of
  spamming parse warnings.
- Probe harness extended (delete-then-extrude, shape selected, event loop
  pumped between ops): appended rows remain fully structured in every
  model-level sequence tried — the poisoning needs live-view timing that the
  Processing wrap now prevents outright.
- KNOWN (deliberate, documented): Ctrl+Z of a topology op is a whole-model
  snapshot restore — the scene visibly reloads for a moment. Fix would be
  in-place undo commands per op (planned as a follow-up, like UVEditCommand).

## 2026-07-15 — Extrude round 3: pick floating verts, self-edge fix, probe harness

User reports: can't select extruded (spur) vertices; parsing warning
"Vertex Data [2863]: Could not find Normal subitem" when extruding from an
extruded vertex.

- **Floating verts now pickable**: vertex picking raycast the surface first, so
  a spur vert floating in FRONT of the mesh handed the pick to the triangle
  behind it. `nearestScreenVertex()` (extracted from the off-surface fallback)
  now competes with the hit triangle's corner — whichever is closer to the
  cursor on screen wins. Fixes selection of extruded verts everywhere.
- **Self-edge fix**: the scaffold triangle (v, v', v') exposed a degenerate
  (v', v') edge to the induced-edge scan, so extruding the extruded vert built
  garbage walls instead of a spur. Degenerate triangles are now excluded from
  region detection, induced edges, winding lookup, and explicit edge picks —
  extruding the new vert correctly pulls out another spur, and extruding
  {v, v'} together ribbons the scaffold edge into a real quad (Blender flow).
- **Normal-subitem warning investigated with an in-app probe harness**
  (`WW_EXTRUDE_TEST=1 NifSkope.exe <file>` → ww_extrude_test.log, temp code in
  nifskope_ui.cpp createWindow): appended Vertex Data rows are fully structured
  (all 15 children, conditions evaluate, Normal/UV/Vertex accessible) through
  the full extrude sequence, two chained extrudes, and a snapshot undo/redo
  reload, on the user's own X01_Torso.nif. The corruption path was the removed
  commit-time consolidation (snapshot undo interleaved with the move's open
  ChangeValueCommand transaction) from the previous build. KEY LEARNING:
  `getItem(name)` requires `evalCondition(item)` (cached) — if a warning like
  this reappears, suspect condition-cache poisoning, and re-run the probe.

## 2026-07-15 — Extrude fixes: vertex extrude + no reload flash (user feedback)

- **Single-vertex extrude now works** (was rejected). Blender's vertex extrude
  pulls out a bare edge; NIF has no loose edges, so each extruded vert becomes
  a **zero-area scaffold triangle (v, v', v')** — it draws as exactly the new
  edge line in wireframe, a later edge extrude of it stitches real wall quads
  (the scaffold provides the induced edge), and Merge by Distance drops it as
  degenerate. Mixed selections work: verts on extruded edges become walls,
  isolated verts become spurs.
- **"Model disappears and reloads" after extrude FIXED**: the commit-time
  "consolidation" undid + re-ran the extrude/move snapshots, and snapshot undo
  reloads the whole model (visible flash). Removed. Instead the chained move's
  commit now pushes area-weighted **normal-recalc ChangeValueCommands into its
  own transaction** (`tlPushNormalCommands` / shared `tlAccumulateAreaNormals`)
  — normals are correct for the final cap position with no reload, and the
  extrude+move remains two clean undo steps. The redo panel's snapshot re-run
  (which does reload) now only happens when a value is actually adjusted.

## 2026-07-15 — Modeling tools batch 1: Redo Panel v2 + Extrude Region (E)

First batch of `MODELING_TOOLS_PLAN.md` — the fork's first *geometry-creating*
operator, plus the generalized panel system every later operator rides on.

### F0.a — Redo Panel v2 (generalized typed operator parameters)
- `GLView::TlOpParam` { Float / Int / Bool / Enum, label, value, range, step,
  enum names } + `armOperatorPanelEx( title, params, undoSteps, seed )`,
  `reapplyOperatorEx( params )` (undoes the whole gesture — possibly several
  undo entries — restores the seed selection, re-runs via the op-provided
  `lastOpExRerun` callback, re-counts the entries it pushed), signal
  `operatorPanelEx`. Stale-guarded by undo index like the existing panels.
- New floating panel `OperatorExRedoPanel` (nifskope_ui.cpp): 8 recycled rows,
  each Float/Int → DragSpinBox, Bool → QCheckBox, Enum → QComboBox; same
  redoPanelQss styling, collapsible header, freeze-on-stale, mutual-exclusion
  and bottom-left positioning as the other three panels (all loops updated).

### Phase 1 — Extrude Region (`E`, edit mode; context menu "Extrude Region…")
- Blender semantics: region-of-faces extrude duplicates ONLY the boundary
  verts, re-points the region faces onto the duplicates (surface detaches as
  the moving cap; interior verts ride along), stitches outward-facing side
  walls (verified winding: edge a→b in cap winding → tris (a,b,b')+(a,b',a')).
  Edge runs (explicit edge picks, or mesh edges induced by vertex picks)
  extrude to a ribbon; loose verts are rejected (NIF has no loose edges).
- Flow: E → geometry created in one snapshot → cap selected → chained modal
  move (full gizmo modal: axis constraint, snap, numeric). Commit or Esc arms
  the "Extrude Region and Move" panel (Move X/Y/Z world + Flip Normals) and
  immediately **consolidates**: undoes the extrude + move and re-runs both as
  a single snapshot with the offset applied inside the op — one Ctrl+Z per
  extrude, and the new wall/cap normals are recomputed for the final
  positions (`tlRecalcNormalsSubset`, area-weighted, packed ByteVector3;
  layouts without a Normal field no-op).
- Plan/apply split (`tlExtrudePlanBuild` read-only → `tlExtrudeApplyPlan`
  inside `nifSnapshotOp`) because a snapshot op always pushes — invalid
  selections abort before any mutation. 65,535-vertex budget guard.
  BSTriShape (FO4) only; one mesh per extrude (v1). Row duplication carries
  UVs/weights/colors; `tlUpdateBounds` refreshes bounds.
- World→local offset via the scene node transform (`tlWorldToLocalDelta`,
  documented approximation for deformed skinned cages).

## 2026-07-15 — UV editor: full simultaneous multi-mesh editing (last Phase 2 item)

The final UV-editor plan item: selection and editing now span every mesh in the
edit session, Blender multi-object style. Previously `selVerts`/`selEdges`/
`selFaces` were active-shape-only; non-active shapes drew as dimmed dead
wireframes. All in `src/uvtools.cpp`.

**Architecture** — per-shape selection state lives in `UVShapeData` (selVerts/
selEdges/selFaces/viewport3DVerts/hiddenFaces/hiddenVerts/pinnedVerts). The
editor's existing members remain the ACTIVE shape's live working copy (so the
~100 operator references stay untouched); `stashActiveSelection()` /
`adoptActiveSelection(s)` swap them on active-shape switches; for non-active
shapes the stored sets are authoritative in place.

- **Picking** — `pickShapeAt` finds the closest vertex (else edge body, else
  face body) across all shapes; clicking another mesh's UVs makes it the active
  shape (Blender). Plain click is exclusive across all shapes; extend keeps
  others. `L` select-linked also hops to the shape under the cursor.
- **Box select** — applies to every shape (all modes incl. islands + sticky).
- **A / Deselect / Invert / mode switch** — loop all shapes; derived edge/face
  sets rebuilt per shape via the extracted `uvDeriveEdgesFaces`.
- **Transforms** — G/R/S gather `XVert{shape,idx,…}` from every shape's
  selection; live writes notify per shape; commit pushes one `UVEditCommand`
  per shape wrapped in an undo-stack **macro** (one Ctrl+Z per gesture). The
  adjust-panel re-apply (Move/Rotate/Scale) regroups per shape the same way.
  Pivot spans the combined selection; island welds rebuilt per touched shape.
- **Sync** — `syncSelectionFromViewport` buckets `pickedElems` by shape both
  ways; `pushShapeSelectionToViewport(s)` pushes one block (GLView's
  `setElementSelectionExternal` merges per block); frame-selected spans all.
- **Rendering** — every edit-session shape draws its full selection state
  (fills, colored wires, edge highlights, dots); active shape additionally
  shows active-vertex emphasis, pins, and the stretch overlay.
- **Scoped active-only by design** (documented): operators (merge, mirror,
  unwrap, pack, …), pins, hide/reveal — they act on the active mesh; click a
  mesh to make it active first. Extending operators cross-mesh is a possible
  follow-up.

## 2026-07-15 — UV editor: sticky selection (merged seams move as one point)

User request: merged UVs should *behave* connected — moving one must not tear
the seam back open. Implemented Blender's **Sticky Selection (Shared Location)**
in `src/uvtools.cpp`:

- `UVShapeData::coPosVerts` — per-shape map of co-located mesh vertices (split
  seams), built in `loadShapeUVs` by bucketing quantized 3D positions then
  confirming exact equality (same scheme as Stitch). Static per topology.
- **Sticky picking** — in vertex/edge modes, picking or box-selecting a UV also
  selects co-located partners sitting at the same UV spot (±1e-5, transitive via
  `expandSticky`), so they move/rotate/scale together. Face mode intentionally
  unaffected (Blender behaviour). Toggleable "Sticky" button next to Sync
  (persisted `UVEditor/StickySelection`, default on); turn off to pull
  coincident UVs apart individually.
- **Island welding** — island detection (`uvRebuildIslands`, extracted from
  `loadShapeUVs`) now also unions co-located verts whose UVs coincide: a merged
  seam becomes ONE island for island-select/L/pack/average. Islands recompute on
  every reload (undo/ops) and after transform commits, so separating a seam
  splits them again live.
- Structural: closed the anonymous namespace after `UVEditCommand` so the new
  helpers share file scope with `readShapePositions` (was an ambiguous overload).

## 2026-07-15 — Shading menu: Material Contributions / Viewport Effects as text toggles

- The Material Contributions buttons (Diffuse, Normal, Specular, …) no longer
  render as filled blue buttons with icons. They are now plain text entries
  (`Qt::ToolButtonTextOnly`, transparent background) whose **active** state is
  the highlight: **blue fill (#4772b3) + orange text (#ff9d00)**, hover #555555.
  Shared `channelToggleQss` in `nifskope_ui.cpp`.
- Refraction / Particles under Viewport Effects converted from checkmark menu
  items to the same text-toggle widgets (full-width, one per row) so the whole
  dropdown uses one presentation. All behaviour (settings persistence, scene
  flags, Shift-solo on contributions, per-mode enable/disable) unchanged.

## 2026-07-15 — UV editor: redo-panel consistency patch (design, scrubbing, transforms)

User feedback on the first adjust-panel iteration: colors/UI didn't match the 3D
viewport's redo panels, values couldn't be scrubbed by dragging, and transform
gestures (G/R/S) had no panel at all. Full consistency patch in `src/uvtools.cpp`:

- **UVDragSpinBox** — exact clone of the 3D redo panels' DragSpinBox: hold LMB on
  the value and drag left/right to scrub (Shift = fine), plain click to type,
  hover reveals ‹ › step arrows, margin clicks step, scrub highlight overlay.
- **Panel redesign** — same QSS as `redoPanelQss` (#2f2f2f body, #202020 border,
  #cccccc labels), collapsible bold "˅ Title" header (uvSetPanelTitle /
  uvTogglePanelCollapse clones), grid of right-aligned labels + 150px fields,
  bottom-left at 10px like `positionRedoPanel`. Stale gestures now **freeze** the
  panel's inputs (3D behaviour) instead of hiding it; re-arming re-enables.
- **Transform redo panels** — committing a G/R/S gesture arms the panel like the
  3D viewport's transformGesture panel: Move (dU/dV), Rotate (Angle°), Scale
  (Scale U/V, axis-constrained gestures fill the untouched axis with 1). Re-runs
  recompute absolute targets from the gesture's original UVs + pivot
  (`lastOpXVerts`/`lastOpPivot`) after undoing the previous commit.
- **Unwrap (Angle Based)** gained a Margin parameter (island border + spacing,
  default 0.02) and its own adjust panel; `unwrapSelection( float margin )`.
- Operator plumbing generalised to multi-param (`QVector<float>`, kinds 1-8, spec
  table in the dock glue); `commitTransformUndo` now reports whether it pushed.
  Removed the eager `checkOperatorPanelStale` (3D panels check lazily on re-run).

## 2026-07-15 — UV editor: 0-1 tile outline + menu consistency with the 3D viewport

- **0-1 tile outline** — Blender-style soft white border (`0.90/0.92/0.95 @ 0.5α`,
  grid line width) drawn around the unit UV tile under the wireframes, so the
  working space reads even with an image filling it edge-to-edge.
- **Menu consistency** (user report: UV editor menus differed from the 3D
  viewport pop-ups in design, color and features). The UV editor's operator
  pop-ups (Merge / Snap / Unwrap / Mirror-Align / Layout Tools) now match the
  3D viewport's exactly:
  - `UVAutoCloseMenu` — a local clone of glview.cpp's `AutoCloseMenu`: closes on
    hover-out (46 px apron, 60 ms poll), Blender-style, instead of lingering.
  - `addSection()` title headers ("Merge", "Snap", "Unwrap", …) like the 3D ones.
  - Naming: snap items now use "Selection to Cursor" / "Cursor to Selected"
    wording (was "Selection → Cursor"); proper "…" ellipsis everywhere (was
    "..."), and "…" consistently marks operators that pop the adjust panel
    (By Distance…, Smart UV Project…, Pack Islands…, Minimize Stretch…) —
    same convention as the 3D viewport's "By Distance…"/"Select Linked by Angle…".
  - Feature parity: inapplicable items are now shown disabled instead of
    silently doing nothing (snap selection items without a selection, Cursor to
    Active without an active vert, Relax/Stitch/Copy without a selection);
    merge menu gained the 3D menu's separator-before-By-Distance layout.

## 2026-07-15 — UV editor: adjust-last-operation panel (operator redo)

The UV editor's parameterized tools ran with hardcoded values and no way to tune
them (user report: "no popup for merging vertices … to select distance"). Added a
Blender-style **adjust-last-operation panel**: a floating overlay in the canvas'
bottom-left corner that appears after the operator runs and **re-runs it live**
as the value is edited (undo previous result → restore the operator's original
selection → re-run with the new parameter). All in `src/uvtools.cpp`.

- Covered operators: **Merge by Distance** (Distance, default = ½ grid step),
  **Minimize Stretch** (Iterations, default 20), **Pack Islands** (Margin,
  default 0.01), **Smart UV Project** (Angle Limit, default 66°).
- Safety: the gesture is guarded by the undo-stack index — any unrelated stack
  change (a transform, Ctrl+Z, a spell) disarms and hides the panel instead of
  corrupting the stack. A re-run that pushes nothing (e.g. distance too small to
  merge anything) is remembered so the next adjustment doesn't undo a foreign
  command. Rebuilds/mode switches also disarm (seed selection no longer valid).
- Smart UV Project is a whole-model snapshot op: its re-run undoes the snapshot
  (model reload) and synchronously rebuilds the editor before re-projecting.
- Plumbing: `applyUVEditUndoable` now reports whether a command was pushed;
  `mergeSelection` takes an optional distance, `packIslands` an optional margin;
  new `armOperatorPanel` / `reapplyUVOperator` / `cancelOperatorPanel` /
  `checkOperatorPanelStale` + `operatorPanelCb`/`resizedCb` dock callbacks.

## 2026-07-15 — 3D grid decade crossfade + UV editor Phases 3 & 4

Big feature batch. Build green, startup smoke-tested. **Not yet GUI-tested by the
user** — in particular the two *mesh-modifying* ops (Rip/Split, Smart UV Project)
change topology and want in-app + in-game verification before further work stacks
on top. Multi-mesh simultaneous editing (the one remaining Phase 2 item) is **not**
in this batch — it's a core selection-model refactor, deliberately sequenced after
this batch is validated.

### 3D viewport grid — full Blender-style decade crossfade
- The orthographic grid no longer snaps between 1/2/5×10ⁿ "nice" steps (which
  popped at each threshold). It now draws **three power-of-ten levels** whose
  brightness crossfades with the sub-decade zoom position (`levelAlpha = {1-frac,
  1, frac}`, smoothstep-eased). Lines shared between levels (every 10th, every
  100th) are reinforced by alpha-over blending, so the fine/minor/major hierarchy
  emerges continuously with **no popping** across a decade boundary. Axis lines are
  drawn opaque over the faded grid. `src/gl/glscene.cpp` (ortho path).

### UV editor — Phase 3 layout tools (`UVs ▸ Layout Tools…`, context menu)
- **Pack Islands** — shelf-packs the selected (or all) islands into the 0-1 tile.
- **Average Islands Scale** — equalises per-island texel density (uv-area/3D-area).
- **Minimize Stretch (Relax)** — uniform-Laplacian relaxation of the selected face
  region's interior UVs, boundary pinned (20 iterations).
- **Stitch** (`V`) — welds selected UVs that share a 3D position across a seam to
  their average, re-joining separated islands.
- **Select Overlapping UVs** — flags faces whose UV triangles genuinely overlap
  (Sutherland-Hodgman intersection area; edge-adjacent faces skipped), sweep-pruned.
- **Copy / Paste UVs** — by vertex index; paste onto the same or identical-topology
  meshes.
- **Show Stretch Overlay** — per-face area-distortion heatmap (blue = compressed,
  green = even, red = stretched), bucketed & drawn over the island fill.

### UV editor — Phase 4 unwrap / topology
- **LSCM pinning** — `P` pins the selected UVs (shown red, Blender-style), `Alt+P`
  unpins, **Invert Pins**, **Unwrap (Live, Pinned)** solves LSCM holding pins fixed
  at their current UVs (no re-pack). `lscmSolveComponent` gained a caller-supplied
  pin map, falling back to the auto extreme-pair when <2 pins are in a component.
- **Rip / Split** (`Y`) — duplicates the vertices shared between the selected faces
  and the rest of the mesh, freeing the selection into its own island. FO4
  BSTriShape only; packs skin weights inline so the duplicated rows carry weights
  automatically (no skin-partition resync). One whole-model snapshot undo.
- **Smart UV Project** (Unwrap menu) — region-grows charts by inter-face normal
  angle (66°), auto-splits shared verts along chart borders, LSCM-solves each chart
  on the original positions, density-normalises and shelf-packs into 0-1, and writes
  it all (grown vertex array + UVs + remapped triangles) in one snapshot. FO4 only.
- **Export UV Layout to PNG** — renders the wireframe of all loaded shapes (active
  brighter) to a transparent PNG at texture resolution via QPainter; save dialog.
- Topology ops refresh the 3D viewport (`ogl->updateScene()`) and rebuild the editor
  afterward; pins reset on data rebuild; all new geometry code lives in
  `src/uvtools.cpp` (no `glview.h` change, so no full-rebuild/stale-Makefile risk).

## 2026-07-15 — Viewport A key routed by pointer (reliable), focus guard fixed

- **`A` in the 3D viewport now works every time the cursor is over it**, not
  "sometimes". `A` was handled only in `GLView::keyPressEvent`, so it needed
  the viewport to hold keyboard focus — and focus-follows-mouse via
  `QEvent::Enter` alone misses when focus is taken while the pointer is already
  inside the viewport. `A` (select-all toggle) and `Alt+A` (deselect) are now
  routed by pointer-over-viewport in the qApp event filter, exactly like G/R/S,
  so they no longer depend on focus. Text fields still keep the key.
- Also removed a wrong guard (`!ogl->isActive()`) from the Enter focus handler
  — `QWindow::isActive()` tracks the top-level window, not viewport focus, so
  it was suppressing the hover-focus; the handler now focuses on entry
  whenever a text field isn't being edited.

## 2026-07-15 — Focus-follows-mouse for the 3D viewport

- Hovering the **3D viewport** now gives it keyboard focus, so `A` and other
  `keyPressEvent`-based shortcuts fire without a prior click — matching the UV
  editor (which already grabbed focus on `enterEvent`). Implemented in the
  qApp event filter: on `QEvent::Enter` for the embedded GL window it focuses
  the viewport container and calls `ogl->requestActivate()` (the same call the
  free camera uses to take keys), skipping while a text field is being edited.
  Many viewport shortcuts already worked on hover via the filter's
  pointer-over-viewport routing; this covers the focus-dependent ones too.

## 2026-07-15 — Instant UV Undo, themed block-list nav arrows

- **UV Undo/redo is now instant** (Blender-like), no more disappear-then-
  re-render. UV edits were being stored as whole-model snapshots, whose
  Undo did a full `nif->load()` → `modelReset` → editor clear → deferred
  rebuild, so the UVs blanked for a frame. Replaced with a lightweight
  `UVEditCommand` that stores only the changed vertices' old/new UVs and
  patches them in place (same direct `set<HalfVector2>`/re-resolve-by-vertex
  path as the live drag — so it's robust against the stale-index/round-trip
  issues that made the original per-vertex `ChangeValueCommand` collapse the
  layout, without reloading the model). The editor refreshes via the command's
  `dataChanged`; the undo-stack rebuild now fires only after a *structural*
  (model-reloading) Undo, detected by the editor having been cleared.
- **Block List back/forward buttons re-iconed.** They used Qt's
  `SP_ArrowBack`/`SP_ArrowForward` standard icons, which render solid black and
  clashed with the dark toolbar. They now use themed grey chevron glyphs
  (`tlMakeIcon` "chevron_left"/"chevron_right").

## 2026-07-15 — UV editor Phase 2 (operators): merge, mirror/align, hide-faces, projections, bounds

- **Merge (`M`)**: At Center (single average), At Cursor, By Distance (weld
  verts within ~½ the grid step to their group average). UV-space only —
  topology untouched, one Undo step.
- **Mirror / Align (`Ctrl+M`)**: Mirror U/V about the pivot (bbox center /
  median / 2D cursor per the Pivot selector); Align U/V (collapse selection to
  its mean column/row); Straighten to Left / Bottom.
- **Hide / reveal faces (`H` / `Alt+H`)**: UV-local face hiding, honoured by
  drawing and picking in every mode (a vertex is hidden only when all its
  faces are). Reset on rebuild; independent of the 3D edit-mode hidden set.
- **Projections** added to the Unwrap menu (`U`): Cube (dominant position
  axis per vertex), Cylinder (angle + height), Sphere (longitude + latitude),
  alongside the existing Angle-Based Unwrap and Project From View.
- **Constrain to Image Bounds** toggle (bar "Bounds", persisted): clamps every
  UV edit (drags + operators) into the 0-1 tile. **Round to Pixels** context
  action snaps the selection to texel corners.
- Context menu gains a **UVs** submenu (Merge / Mirror-Align / Unwrap-Project /
  Round to Pixels / Hide / Reveal) so the operators are discoverable without
  the shortcuts. All operators respect Object Mode (read-only) and the
  whole-model-snapshot Undo path.
- Proportional editing intentionally **not** implemented (per request).

## 2026-07-15 — Version-mismatched rows actually hidden; brush starts painting

- **Real cause of Skyrim rows showing on FO4 NIFs found + fixed.** The
  `isRowHidden` version check was correct, but `NifTreeView::setRootIndex`
  never re-ran `updateConditionRecurse` for a newly shown block — only the
  current field's subtree was refreshed on `currentChanged`, so sibling fields
  kept stale (visible) row states when switching blocks. `setRootIndex` now
  re-applies row hiding over the whole block, so version-mismatched fields
  (e.g. Skyrim shader-flag variants, `Num SF1`/`SF1`, GTEFO76 data — all with
  verconds false for BSVersion 130) are hidden on Fallout 4 NIFs. Verified in
  nif.xml: those fields' verconds (`#BSVER# < 130`, `#BS_132_139#`,
  `#BS_GTE_F76#`) are all false at BSVersion 130, and `flags()`/`evalCondition`
  already grey them, confirming `evalVersion` returns false for them.
- **Clicking the "Brush" toggle now starts painting.** Previously it only
  toggled brush-vs-select when a paint mode was already active, so clicking it
  in the Vertex Paint / Rigging workspace (before pressing the manager's Start
  Painting) did nothing. It now starts painting from the open manager's Start
  button (Vertex Paint or Weight Paint) when no paint mode is active; if a mesh
  (or bone) isn't selected yet it reverts and shows a status hint.

## 2026-07-15 — UV texture colour fix, edge/fill visibility, brush visibility, version-mismatched rows

- **UV editor texture no longer desaturated.** The underlay colour-conversion
  mode now matches the legacy UV editor exactly by BSVersion: FO4 (bsver 130)
  shows the diffuse **raw** (mode 0) instead of sRGB-compressing it (mode 1,
  which double-encoded and washed it out); the diffuse is sRGB-compressed only
  on Skyrim SE (≥151); normals use BC5 UNORM/SNORM reconstruction. `UVTexSlot`
  now carries the real `colorMode` per slot; custom underlays display raw.
- **UV edges/wires now clearly visible** over both the dark checker and a
  bright texture (unselected edges lightened to a bright cool grey), and the
  faint island fill bumped up slightly.
- **Selected faces fill in every select mode** (Blender behaviour): any face
  whose three UV corners are all selected gets the orange fill, derived from
  the vertex ground truth — so selecting in Vertex/Edge/Face/Island all fill.
- **Paint "Brush" toggle visible in the paint workspaces.** It now appears
  whenever the Vertex Paint or Rigging manager dock is open (not only once a
  paint stroke mode is active), so it's present as soon as you're in the paint
  context; it still sits between the element-select buttons and Deformed.
- **Version-mismatched fields are hidden (Skyrim rows on FO4 NIFs).**
  `NifTreeView::isRowHidden` skipped the version check for fields with a type
  condition, so version-conditioned typed fields (the Skyrim shader-flag
  variants) showed on Fallout 4 NIFs. A field that fails its version condition
  is now always hidden, regardless of the row-hiding mode.

## 2026-07-14 — Weight/Vertex/Segment "Brush" toggle re-iconed

- The paint/select **Brush** toggle button (`ViewportWeightPaintBrushButton`,
  shown in the viewport toolbar only while a paint mode is active) still used
  `:/btn/skinned`; it now uses the greyscale brush glyph so it reads as an
  actual brush. This is the button the earlier "give the brush a proper icon"
  feedback was really about (previously the mode-selector icon and Deformed
  button were changed instead). It appears only in Weight/Vertex/Segment Paint
  modes and can land in the toolbar's ">>" overflow on a narrow window.

## 2026-07-14 — UV island fills, grid-under-image, redesigned greyscale mode icons

- **UV editor: grid hidden when an image is loaded.** With a real texture
  underlay bound, the subdivision grid is suppressed (Blender shows the image,
  not the grid); the checker/empty background still shows the grid. A loaded
  image is also displayed brighter (0.96 vs the checker's 0.75).
- **UV editor: subtle white island fills.** Every visible UV face now gets a
  faint translucent-white fill (Blender's face theme colour) so islands read
  as solid shapes instead of bare wireframe; selected faces keep the brighter
  orange fill on top. Object-mode read-only views fill per-shape in that
  shape's colour. Grid lines further subdued to a cool, uniform subtle grey.
- **Redesigned viewport-mode icons (all greyscale `tlMakeIcon` glyphs):**
  Object = isometric cube, Edit = wireframe triangle with vertex handles,
  Vertex Paint = triangle with per-vertex grey dots, Weight Paint = a proper
  paintbrush (slim handle, ferrule, tapered bristle tuft), Segment Paint = a
  shape split into greyscale segments. One consistent family, accurate to each
  mode.
- **"Deformed" cage button re-iconed.** It used `:/btn/skinned`, which read as
  a brush sitting next to the word "Deformed" and showed in Edit Mode too. It
  now uses a distinct greyscale deformation-lattice glyph (`mode_deform`), so
  nothing brush-like appears outside the weight/vertex-paint context.

## 2026-07-14 — UV undo corruption fix, workspace default, mode-menu greyscale, ortho grid

- **UV Undo no longer collapses the layout.** UV edits (G/R/S gestures, snap,
  unwrap) now Undo as **whole-model snapshots** (`NifSnapshotCommand` /
  `nifSnapshotOp`) instead of per-vertex `ChangeValueCommand`s — completely
  robust against the index-staleness / value round-trip issues that could put
  every UV on a single point. A gesture snapshots the model at start (before
  the live writes) and pushes before/after on commit; snap/unwrap wrap their
  raw writes in `nifSnapshotOp`. Since snapshot Undo reloads the model
  (firing `modelReset`), the editor now rebuilds itself and re-syncs the
  selection on any `undoStack` `indexChanged`. Trade-off: an Undo/redo briefly
  reloads the model rather than patching values in place.
- **Open in the Default workspace.** A newly opened NIF now always starts in
  the Default workspace (was: restored the last session's workspace).
- **Workspace menu marker fixed.** The active-workspace indicator is derived
  from which manager dock is actually visible on every menu open
  (`aboutToShow`), so a dock closed via its own X (or the exclusive-visibility
  logic) can't leave a stale radio dot on the wrong entry; the active entry is
  also bolded for a clearer highlight than the small dot.
- **Material Contributions menu icons greyscaled.** The few colorful resource
  icons (Diffuse, Vertex Color, Specular, Glow, Reflections) are desaturated
  and lifted to the toolbar's grey tone so the channel mixer reads as one
  consistent greyscale system.
- **Ortho grid zoom consistency.** The planar grid's minor subdivisions now
  crossfade by on-screen spacing (`drawGrid` gained a `minorFade`): they fade
  out as they get dense toward a "nice number" re-base and back in afterwards,
  softening the whole-grid pop when zooming. (A full Blender-style decade
  crossfade of the major lines is a larger follow-up if still wanted.)

## 2026-07-14 — UV editor feedback batch 3: Blender look, focus/keys, Ctrl+X freeze fix, brush icon

- **Ctrl+X freeze in Weight/Segment Paint fixed.** `fillRiggingWeightSelection`
  / `fillSegmentPaintSelection` each emit a stroke whose commit serializes the
  whole model into an Undo snapshot; under keyboard autorepeat (or a double
  delivery) those stacked synchronously and froze the UI. Now guarded by a
  `paintFillPending` flag, run deferred via `QTimer::singleShot(0)` so they
  can't re-enter the key event, and the GLView key handlers skip autorepeat
  (`event->isAutoRepeat()`), matching the eventFilter path.
- **UV editor key reliability (A / G / R / S).** Added focus-follows-mouse:
  `enterEvent` grabs keyboard focus when the pointer enters the canvas (unless
  a text field is mid-edit), so the single-key shortcuts fire on hover without
  a prior click. Plain **A** now toggles select-all/deselect-all (second press
  clears), matching the 3D viewport; Alt+A still force-deselects.
- **Weight Paint brush icon** is now a whitish `tlMakeIcon("brush")` glyph
  (diagonal handle, darker ferrule, tapered bristles) in the same family as
  the Object/Edit mode-selector icons, replacing the colored `:/btn/skinned`.
- **UV editor Blender-look pass:**
  - Grid: base **8** subdivisions (was 4), ×8 finer levels that fade in on
    zoom; uniform subtle whitish lines (alpha 0.26/0.20/0.13), no
    bright/bold 0-1 border. With Repeat off, the grid + pixel lines are now
    **confined to the 0-1 tile** (shader `inGridTile`) — outside is plain
    background, like Blender.
  - Edges lightened to a readable medium gray; **vertex dots now show in
    every select mode** (dark unselected dots, orange selected/active) instead
    of only in Vertex mode.
  - 2D cursor is now Blender's **red/white dashed ring + crosshair ticks**
    (QPainter overlay in paintGL), matching the 3D viewport's cursor, instead
    of the small cross+dot.
- Note: the 3D viewport's orthographic grid and 3D cursor were already
  Blender-styled in earlier work; no 3D-grid change this batch pending the
  user pointing at the specific aspect that looks off.

## 2026-07-14 — UV editor feedback batch 2: object mode, tiling, pixels, cursor menu, env filter

- **Object Mode display**: the UV editor no longer forces Edit Mode on open.
  In Object Mode it shows the selected mesh's UVs read-only — the primary
  (active) mesh in white, each secondary-selected mesh in a distinct color
  (6-color palette) so overlapping layouts read apart. Editing (G/R/S, box
  select, unwrap, project, pick) is blocked with a status hint; pan/zoom/frame
  and the snap-menu cursor ops still work. Follows `objectSelectionChanged`.
  Switch to Edit Mode (Tab / mode selector) to edit. This also resolves the
  "sync doesn't work / editor is empty" report: the editor is now populated in
  every mode, and sync-off only hides faces in Edit Mode.
- **Repeat toggle** (bar2): off by default = show only the 0-1 tile with a
  dark outside (Blender default; texture clamped, shader `tileMode=1`); on =
  repeat image + grid across every tile. Legacy pop-up UV editor unaffected
  (shader `tileMode` defaults to 0/repeat when unset).
- **Pixel grid toggle** (bar2, "Pixels"): draws a subtle grid at the underlay
  texture's real pixel boundaries, fading in as pixels grow past a few screen
  px. Resolution is queried from the bound texture
  (`glGetTexLevelParameteriv`). New shader `drawPixelGrid` (0 = disabled, so
  legacy editor unaffected).
- **Pixel snapping**: Snap menu gains "Selection → Pixel" and "Cursor →
  Pixel"; the snap popover gains a **Pixel** Snap Target (Ctrl-drag lands the
  base point on the nearest texel corner, with the snap indicator). All need a
  texture underlay with a known resolution, else a status hint.
- **Right-click "Place 2D Cursor Here"** is now the first context-menu item
  (exact click position), alongside the existing Shift+RMB placement.
- **Env textures removed from the underlay list**: FO4 texture slot 4
  (environment cubemap — renders as garbage in 2D) and slot 5 (env mask) are
  skipped in the slot picker.
- Island fill already derived-from-vertices from batch 1 keeps working in all
  the above.

## 2026-07-14 — UV editor feedback batch: Blender look, sync toggle, island fix, Unwrap, UV sets

- **Blender look**: background is now Blender's neutral #2B2B2B; the
  untextured 0-1 tile renders as mid gray (#393939) so the whitish grid lines
  carry the contrast, matching the 3D viewport's dark-gray-plus-light-lines
  scheme instead of the old white tile.
- **Progressive grids**: the two finer grid levels no longer pop in at fixed
  zoom gates — each fades in over the octave before its gate while zooming in
  (full strength at half the gate), like Blender's UV editor.
- **UV Sync Selection button** (⇄, leftmost on the settings bar, persisted):
  ON keeps the existing both-ways mirroring with the 3D viewport. OFF makes
  the UV selection fully local and — per Blender — shows and hit-tests only
  the faces currently selected in the 3D viewport (the 3D selection keeps
  being tracked as the visibility filter; nothing is pushed back).
- **Island select fixed**: the deferred selection echo from the viewport
  (vertex-typed picks) was wiping the derived edge/face sets, so island
  clicks lost their fill and face membership. syncSelectionFromViewport now
  ignores unchanged-vertex echoes entirely and re-derives edge/face
  membership from the vertex ground truth in island mode; the island fill is
  additionally derived from vertices at draw time, so it can never be lost to
  an echo again.
- **Unwrap** (U key, `Unwrap ▾` bar button): "Unwrap (Angle Based)" runs a
  real LSCM (least-squares conformal map) over the selected faces — split
  into connected components (the existing per-vertex splits act as seams),
  each solved with two pinned extreme vertices via Jacobi-preconditioned CG
  on the normal equations (no assembled matrix, no new dependencies),
  components rescaled to uniform texel density from their 3D area, shelf-
  packed and normalized into the 0-1 tile; one undo transaction. "Project
  From View" projects the selection along the current 3D camera
  (object-space positions; the camera direction is exact, per-shape world
  offsets are not — noted limitation).
- **UV Map selector** (bar2): switches between UV coordinate sets on legacy
  NiTriShape-era meshes ("UV Sets" array), keeping selection since topology
  is shared. FO4 BSTriShape stores exactly one UV channel in its vertex
  format, so the combo is informative-but-disabled there — a second UV map
  cannot be added to FO4 meshes without the game ignoring it.
- GLView::viewTransform() made public for Project From View (access change
  only, no layout change).

## 2026-07-14 — UV Editing workspace, phase 1

- New **UV Editing** workspace (replaces the "UV Manager (Planned)"
  placeholder; right-docked like the other managers; entering it auto-enters
  Edit Mode). New file `src/uvtools.cpp`: `UVEditorView` 2D GL canvas +
  `tlCreateUVManagerDock` factory. The legacy pop-up UV editor is untouched.
- Canvas: texture underlay with slot picker (FO4 lighting/effect shader
  slots), custom image browse (for atlas retargeting), alpha toggle, the
  legacy editor's `uvedit.prog` grid (zoom-gated levels), MMB pan, wheel
  zoom-to-cursor, Home/`.` framing. All edit-mode meshes render; non-active
  shapes draw dimmed; active shape editable.
- Selection: Vertex/Edge/Face/Island modes (`1`–`4` + bar buttons), click /
  Shift-toggle, LMB-drag box select (plain adds, Shift/Ctrl removes — 3D
  convention), `A`/`Alt+A`/`Ctrl+I`, `L` island under cursor, `Ctrl+L` grow
  to islands. Element marking follows the shared palette: active #FF9D00,
  selected #FF7200, unselected near-black wires/points; face fills in
  translucent orange; vertex dots only in vertex mode (Blender behavior).
- **Two-way selection sync** with the 3D viewport: new GLView
  `elementSelectionChanged` signal (coalesced, emitted via the
  `recordSelection()` choke point + selection undo/redo) drives UV-side
  mirroring; UV-side changes push back through new
  `GLView::setElementSelectionExternal()` (records selection undo, rederives
  world positions) and `setElementPickMode()`. Pick-mode changes mirror both
  ways; island mode surfaces in 3D as vertices.
- Transforms: Blender modal `G`/`R`/`S` with `X`/`Y` axis constraint, typed
  numeric input, Shift precision, Esc/RMB cancel, LMB/Enter commit. Live
  write-through during the drag (Processing-state batch + one dataChanged per
  step so the textured 3D preview follows), one merged model-undo transaction
  per gesture on commit — Ctrl+Z is shared with everything else. Half-float
  UVs accumulate in float32 editor-side.
- Snap bar (per the Blender popover): magnet toggle (Ctrl inverts), Snap
  Target Increment/Grid/Vertex (vertex target shows an indicator dot and
  excludes dragged verts), Snap Base Closest/Center/Median/Active, Affect
  Move/Rotate/Scale, rotation increment, grid step; persisted under
  `UVEditor/*`. Pivot selector: BBox Center / Median / 2D Cursor.
- 2D cursor with the 3D cursor's toolkit: Shift+RMB places, U/V spin boxes,
  `Shift+S` snap menu (Selection→Cursor / Keep Offset / Grid;
  Cursor→Selection/Active/Grid/Origin/Tile Center), cursor as pivot,
  crosshair + red dot marker, `Shift+C` resets.
- Data model note honored throughout: NIF UVs are per-vertex (one UV per
  vertex; seams are split verts), so UV points map 1:1 to mesh vertices.
  BSTriShape (FO4) + legacy NiTriShape/NiTriStrips supported; SSE
  skinned-partition data and Starfield BSGeometry are out of scope phase 1.
- GOTCHA for future work in this file: Qt's `slots` macro ate a local
  variable named `slots` (declaration compiled to nothing) — keep Qt keyword
  names out of identifiers in Qt-including TUs.

## 2026-07-14 — UV editing workspace: plan drafted

- `UV_EDITOR_PLAN.md` added: phased plan for a Blender-style UV editing
  workspace (dockable 2D editor, Blender keymap and modal G/R/S, two-way
  selection sync with 3D edit mode, model-stack undo, later stitch/pack/
  projections, deferred seam+LSCM unwrap). Documents the per-vertex-UV
  structural difference from Blender's per-loop UVs. Awaiting user approval;
  no code changes yet.

## 2026-07-14 — Build fix: startup crash from stale incremental object

- The usability follow-up build crashed on launch (access violation in QHash
  `findNode`). No source change was at fault: `Makefile.Release` carried a
  stale dependency list in which `riggingtools.o` did not depend on
  `src/glview.h`, so adding the `sessionDocumentPreviewColors` member to
  `GLView` left that object compiled against the old class layout while the
  rest of the program used the new one. Re-running
  `qmake6 -o Makefile NifSkope.pro` regenerated correct dependencies
  (`src/glview.h` is now listed), the stale object was rebuilt, and startup
  was verified from the console. Rule of thumb: re-run qmake6 after changing
  the include graph or the layout of widely-included classes.

## 2026-07-14 — Loaded NIFs usability follow-ups

- NIF Browser right-click now respects multi-selection: right-clicking a row
  inside the current selection offers **Add N Selected to Loaded NIFs** and
  queues every selected row (same cooperative one-per-turn queue as the Load
  Selected button); right-clicking outside the selection still acts on the row
  under the cursor only. Previously the context menu always enrolled just the
  clicked row, silently ignoring the rest of the selection.
- The primary document now always appears in the Loaded NIFs list — marked the
  Block List way (light-blue row, orange text, arrow icon) — even when it was
  never explicitly enrolled, so the list and the viewport always agree about
  what is being edited. Its context menu works from the automatic row too;
  "Remove from Loaded NIFs" is disabled for it since the primary cannot leave
  its own workspace view.
- The combined secondary-document preview is now fully opaque instead of 38%
  translucent. To keep opaque geometry readable, per-face lambert shading
  against a fixed light is baked into vertex colors once per soup rebuild
  (the neutral gray-blue tone is preserved). The preview now writes depth so
  it occludes and is occluded by the primary correctly, and its polygon offset
  is biased away from the camera so the editable primary always wins
  coincident-surface z-fights against the read-only backdrop.

## 2026-07-14 — Data-only background document layer

- Loaded NIFs enrolled from the NIF Browser are no longer full hidden NifSkope
  windows. Each is now a `BackgroundNifDocument`: a parsed `NifModel` plus its
  source identity (loose path, or configured game + archive path) and its
  workspace-group root — with no QMainWindow, no dock/toolbar/menu construction,
  and no GL viewport. Adding many donors therefore costs one NIF parse each,
  nothing more; parses still run one per event-loop turn through the existing
  queue so input and repainting stay responsive.
- Promotion to primary is now the lazy UI-attach step: **Make Primary / Edit**
  (or double-click) creates a hidden real window, reloads the NIF from its
  original source (loose file or the primary's combined configured archive),
  then runs the ordinary primary switch. Background documents have no editing
  UI and can never be dirty, so the re-parse is lossless. Trade-off: promoting
  now re-parses once, while enrolling donors became much cheaper; a failed
  reload leaves the data-only entry untouched and reports on the status bar.
- The Loaded NIFs pane, its selection wiring, both context menus, isolate /
  show-all / hide-all actions, and the combined viewport triangle-soup preview
  all handle both real windows and data-only documents with the same palette
  and semantics. Removing a data-only entry and closing it are the same
  operation, since nothing else owns it; closing the visible primary still
  closes every member of the workspace group, deleting data-only members
  outright.
- Rigging's donor chooser now enumerates `NifSkope::selectedWorkspaceModels()`
  — model/display-path pairs covering windows and background documents alike —
  instead of window-only `selectedWorkspaceDocuments()`. Donor capture to a
  temporary NIF is unchanged.
- Configured-resource extraction was factored into
  `extractConfiguredNifBytes()`, shared by the window loader and the background
  loader. Background parses use `MSG_TEST` message mode so a batch enroll can
  never raise modal error dialogs; failures drop the entry with a status-bar
  message. Background documents also no longer touch recent-file history or
  emit load signals, and they never call `setCurrentFile()`.

## 2026-07-13 — Rigging workspace and atomic transfer

- Camera projection and saved user-view transforms are now session-only. A
  newly launched NifSkope starts from the configured startup direction in
  perspective, while opening another NIF in the same window preserves the
  current camera instead of snapping back to the startup orientation.
- Added multi-NIF document sessions. Files opened together appear as compact
  tabs while retaining independent models, selections, Undo stacks, dirty
  markers, managers, and editor state. Activating a tab makes it the primary
  editable document; visible secondary documents form a neutral, read-only,
  non-pickable combined viewport preview for complete armor-set assembly.
- Made Loaded NIF ingestion cooperative and much lighter on the active window.
  Selected donors are parsed one per event-loop turn so input and repainting can
  run between files; hidden documents no longer install application-wide input
  filters, rebuild their unused session/browser UI, or rescan configured
  resources after loading. Parsing remains deliberately serialized because the
  current NifModel and renderer are UI-thread objects; promoting a donor lazily
  enables its input handling and NIF Browser only when it becomes primary.
- Document-tab context actions toggle secondary visibility, isolate one
  secondary with the primary, show/hide all, unload preview geometry, or close
  a document with the normal save confirmation. Open secondary documents are
  offered directly by Rigging's donor chooser; their current in-memory state is
  captured to a temporary auto-removed NIF, so unsaved donor edits participate
  without modifying the donor file.
- Renamed **Archive Browser** to **NIF Browser** and made its tree the home of
  an expandable **Loaded NIFs** session category, removing the cramped document
  tabs from the main toolbar and the separate strip above the browser. Loaded
  entries retain primary/secondary state, activation and context controls. The
  same tree combines archived meshes with loose NIF/BTO/BTR files
  under the selected game Data folder, searches both sources together, supports
  extended multi-selection, and loads all selected meshes as independent
  session documents through a dedicated **Load Selected** button. Double-click
  also opens archive entries in a new document instead of replacing a populated
  primary document.
- NIF Browser now builds its **Available NIFs** hierarchy automatically from
  the current game's resource paths configured in Settings. The resource
  manager merges every linked BA2/BSA and loose directory into one virtual
  list, resolves overrides without duplicate paths, and loads either source
  identically. Available files stay above the separate **Loaded NIFs** session
  category; a compact Refresh button rebuilds the list after external changes.
  The confusing Recent Archives/Recent Files controls were removed from the
  browser (their legacy File-menu entries remain available).
- Fixed configured mesh discovery for Skyrim/Fallout-era resource profiles:
  their normal renderer cache intentionally excludes `.nif`, which previously
  left mostly terrain files visible. NIF Browser now creates an independent,
  mesh-only virtual archive from the same ordered Settings paths, accepting
  NIF/BTO/BTR files from BA2/BSA archives, full Data directories, and direct
  loose `meshes` folders while preserving configured override precedence.
- Moved loaded session documents into a dedicated resizable lower pane beneath
  Available NIFs. Primary documents use the established orange `#FFA040`,
  visible secondaries use reddish-orange `#FF602A`, and hidden/unloaded
  secondaries use selection blue `#4772B3`; double-click and the document
  context menu retain primary/visibility/close controls.
- Cleaned the legacy toolbar separator left behind by the removed viewpoint
  actions, keeping Display and the combined Center/Frame control together and
  placing the transform-group separator after them.
- Moved the Refraction and Particles switches into a persistent **Viewport
  Effects** section of the unified shading menu. Both settings now remain open
  while toggled and persist between sessions.
- Rebound the orthographic viewport commands to Blender's numpad layout:
  Numpad 7 Top, Numpad 1 Front, Ctrl+Numpad 3 Left, Numpad 9 Opposite/Flip,
  and Numpad 5 Perspective/Orthographic. Top-row 1/2/3 remain geometry modes.
- Completed viewport-scoped Blender numpad navigation: Numpad 3 Right;
  Ctrl+Numpad 1/3/7 Back/Left/Bottom; Numpad 2/4/6/8 fixed 15-degree orbit;
  Shift+2/4/6/8 view-plane pan; Numpad +/- zoom; Numpad / local-view toggle;
  Numpad decimal Frame Selected; and Home Frame All. Numeric fields retain
  their numpad input, while Numpad 0 remains reserved for active-camera support.
- Removed the now-redundant Viewpoint toolbar dropdown; axis snapping remains
  available through the 3D orientation gizmo and Render menu.
- Orthographic views now use a Blender-style screen-aligned grid that fills the
  viewport, adapts its 1/2/5 subdivision spacing to zoom, stays anchored to
  world zero while panning, distinguishes major and minor lines, colors the
  visible world axes red/green/blue, and renders behind geometry without
  z-fighting. It appears only when the camera rotation is exactly aligned to
  Top/Bottom, Front/Back, or Left/Right; arbitrary User Orthographic angles
  hide it. Perspective view retains the grounded grid.
- Orbiting an orthographic view with MMB or the numpad now follows Blender's
  Auto Perspective behavior, restoring the horizontal perspective ground grid
  while arbitrary User Orthographic views remain gridless until orbited.
- Added disabled **UV Manager**, **Skeleton Manager**, and **Pose Manager**
  entries below the implemented workspaces. They reserve the future workflows
  without creating empty docks or changing persisted workspace indexes; their
  tooltips describe UV editing, skeleton/rest-pose management, and reusable
  load-screen posing with props.
- Combined **Center Viewpoint** and **Frame Selected** into one compact focus
  dropdown between Display Options and the transform controls. Frame Selected
  retains Blender's Numpad-decimal behavior and fits the active selection;
  Center Viewpoint only resets the camera pivot.
- Removed the obsolete material-channel icon grid from the lightbulb popup,
  including its empty placeholder buttons and duplicate Normals entry. The
  popup remains the home of directional, colour, ambient, brightness, tone-map,
  frontal-light, and PBR environment controls. Its remaining file button is now
  explicitly labelled **Choose PBR Environment Cubemap**; it does not override
  the single cubemap authored by Specular/Gloss materials.
- Added an inline **Block List search** for block number, type, displayed value,
  and object name. Space-separated terms combine as AND filters, hierarchy
  parents remain visible for matching descendants, Ctrl+F focuses the field,
  Esc clears it, and the filter survives list/hierarchy switches and file loads.
- Added compact Block List category icons for scene nodes, geometry, skinning,
  materials, textures, animation, collision, particles, lights, cameras, extra
  data, and otherwise generic blocks. Dedicated artwork uses an orange node
  point, wireframe cube with orange selected edges, colored material/texture
  symbols, green animation control, floating particles, lightbulb, camera, and structured-data document;
  Block Details remains undecorated.
- F2 or double-clicking the visible object name now renames uniquely named
  `NiAVObject` scene objects directly inside their Block List row (Enter
  accepts; Esc cancels), without a modal dialog. Double-clicking the block type
  does not rename it. Empty or duplicate names are rejected, and the atomic
  rename path keeps object palettes and controller-sequence node names
  synchronized as one Undo step.
- Enabled **Vertex Paint** as a real viewport mode. It reuses Weight Paint's
  Blender-style camera, vertex/edge/face selection masks, continuous swept LMB
  brush, brush/select toolbar toggle, Tab mode memory, and one Undo snapshot per
  drag. A dedicated **Vertex Paint** workspace and **Vertex Paint Manager** expose
  separate **Color (RGB)** and **Alpha** paint
	channels: RGB preserves alpha and Alpha preserves RGB. Missing packed vertex
	colours initialize to opaque white as one undoable structural operation. The
	RGB brush now docks the complete NifSkope color chooser directly below the paint
	controls in live-edit mode, including its synchronized RGB/HSV fields, hex value,
	standard and custom palettes, screen picker, and drag-to-scrub numeric controls.
- Added a compact **All / Bones / Segments** filter beside the bone search box.
  It filters both receiver and donor lists; Segments includes subsegments and
  donor mesh parent rows remain visible whenever they contain matching rows.
- Fully enabled **Segment Paint** for FO4 `BSSubIndexTriShape`. Segments and
  subsegments are binary face channels with continuous swept LMB painting,
  selection masking, the shared Brush toggle, wheel zoom, face/edge/vertex
  selection tools, and Ctrl+X fill. Each stroke is one Undo snapshot.
- Segment edits rebuild the format's contiguous triangle ranges atomically:
  triangles are regrouped, parent/subsegment ranges and shared data are rebuilt,
  and every face retains one exclusive owner. The manager can add segments and
  subsegments, edit stored subsegment User/Bone IDs with F2 or double-click,
  and delete either kind with affected faces safely reassigned to Segment 0.
- Receiver and donor segment rows participate in multi-selection and drag/drop.
  Binary membership can be transferred from multiple secondary meshes onto the
  active receiver channel using nearest-surface face correspondence. The entire
  transfer is one Undo step and donor meshes remain unchanged.
- The donor bone/segment list, donor action button, and donor prompt are hidden
  when the viewport selection contains only the primary receiver; the receiver
  list expands to the full manager width.
- Segment Paint keeps its evaluated surface automatic and hides the unrelated
  Deformed Cage toolbar control; the Brush selection/painting toggle remains.
- Expanded the viewport mode selector in the requested order: **Object Mode**, **Edit
  Mode**, **Vertex Paint**, **Weight Paint**, and **Segment Paint**.
- **Weight Paint** is now a first-class viewport mode. Selecting it activates the Rigging
  workspace, starts the existing weight-paint tool for the selected (or first available)
  bone, and stays synchronized with the Rigging Manager. Object Mode and Edit Mode exit
  weight painting cleanly.
- Added **Manual weight painting** to the Rigging Manager. Select a skin bone, choose
  Add, Subtract, Replace, or Smooth, then paint directly on the target with a cyan
  screen-space brush. Weight, Strength, and Radius match the familiar Blender workflow;
  the wheel resizes the brush and RMB/Esc exits.
- Each mouse drag is one whole-model Undo entry. Every touched FO4 vertex is reduced to
  at most four deterministic influences and renormalized; Smooth uses one-ring topology,
  bone bounds are recalculated, and standard-only `CustomizationRemapData` is refreshed
  from the packed result when present. Face-sculpt RemapData remains the original
  standard-skeleton snapshot by design.
- Painting is restricted to the active target surface. X-ray controls backface inclusion,
  the selected-bone heatmap refreshes after each stroke, and the Rigging Manager now
  reacquires its target correctly after snapshot Undo/Redo. Native coverage reports
  `weightPaint=clean` and verifies Add/Subtract/Replace normalization plus byte-exact Undo.
- Added a read-only **Donor geometry overlay** to the Rigging Manager. It loads any
  compatible donor shape into a cyan, non-pickable viewport triangle soup aligned through
  the same skeleton-root-relative space guard as transfer, with Filled, Wireframe, Opacity,
  and Clear controls. It never enters the NIF model or Undo stack.
- Added **Show selected-bone weights**: selecting a skin bone paints the target with a
  Blender-style blue→cyan→yellow→red 0..1 heatmap and reports affected vertices plus maximum
  weight. The per-corner colour buffer is viewport-only and coexists with the donor overlay.
- Both Rigging visual buffers disappear while the workspace is hidden, restore on show, and
  clear on target changes, structural edits, atomic transfer, or Undo/Redo. Native-window
  coverage byte-checks the model and now reports `donorOverlay=clean weightHeatmap=clean`.
- Packaged this visualization increment separately as
  `dist/NifSkope-WW-Rigging-Visuals-2026-07-13.zip`, preserving the earlier release archive.
- Added a repository-owned `RiggingIntegration.pro` target and
  `tests/rigging/run.ps1` runner. `run.ps1 -Build` performs an isolated full-source
  build, then runs the native-window suite against versioned target/donor fixtures;
  subsequent runs reuse the executable and complete in about 22 seconds.
- Added a dedicated **Rigging** entry to the Workspaces selector. It activates an
  exclusive right-side Rigging Manager with live target/skin status, bone inventory,
  validation tools, and an expandable advanced workflow.
- Added **Transfer Bones and Weights...** as the primary one-dialog workflow. It
  generates `CustomizationRemapData`, imports the required donor hierarchy, binds
  donor bones, and transfers weights as one atomic Undo entry.
- Its confirmation now summarizes donor and target geometry, used/shared/new bone
  bindings, missing used donor nodes, and whether `CustomizationRemapData` will be
  created or updated (including its expected byte size). Expandable details list the
  affected bones, while incompatible shape spaces are rejected before any mutation.
- The atomic workflow now performs its surface mapping before confirmation and reports
  median, 95th-percentile, and maximum snap distances as percentages of donor span,
  classifying the geometry as Close, Caution, or Poor. The prepared weights are reused
  for the write, avoiding a second mapping pass; cancellation or rejection remains
  mutation-free and Poor matches retain Cancel as the safe default.
- A failed inner step restores the complete pre-transfer model snapshot. Successful
  runs report imported nodes, bound bones, transferred vertices, and remap size.
- Rollback warnings now identify the exact failed workflow step and clearly warn the
  user to close without saving if automatic snapshot restoration itself ever fails.
- Preview and weight-transfer surface mapping now show immediate, responsive progress
  and check for cancellation every 16 target vertices. Cancelling Preview changes
  nothing; cancelling the atomic workflow restores RemapData, imported nodes, and
  bindings through its outer snapshot and creates no Undo entry.
- Workspace buttons invoke the same persistent SpellBook path as the menus; selection
  state is synchronized so actions operate on the active skinned shape.
- Native-window coverage now clicks the primary Rigging workspace button end-to-end,
  verifies the manager refreshes from 10 to 69 bones after its model reset, confirms
  the application-owned Undo stack receives exactly one entry, and restores the
  original model through that same stack before closing the window.
- Reconciled `BONE_WEIGHT_TRANSFER_PLAN.md` with the release-candidate implementation.
  It now distinguishes the completed existing-skin FO4 workflow from the deferred
  unskinned-target backend, mapping controls, independent skeleton reference, donor
  overlay, classic-skin backend, and in-game/manual production-asset validation.
- Hardened geometry ingestion against non-finite positions and out-of-range triangle
  indices before they can reach incident-face indexing. Degenerate faces use a safe
  nearest-corner fallback, and isolated donor vertices fall back to their own normalized
  skin instead of producing an empty transfer result.
- The native harness now rejects every GUI save dialog, chooses Discard for close/save
  prompts, verifies the donor's expected 68-bone identity at startup, and byte-compares
  both versioned fixtures at shutdown. This prevents GUI testing from ever overwriting
  its source assets; malformed-topology rejection is covered as `topologyGuard=clean`.
- Added a reusable `run.ps1` real-asset mode that runs the native atomic spell on external
  read-only NIFs, validates/reloads the generated output, proves byte-exact Undo/Redo, and
  compares weights by bone name with a vanilla reference. Vanilla female-head→BigBeard01
  hairline reproduces the validated ~0.34 mean L1 / ~83.6% dominant agreement (`Caution`),
  while the male-head self-surface case reaches 2.26e-06 / 99.88% (`Close`). No Blender or
  DCC path is involved.
- Real-asset validation now compares post-transfer and post-reload FO4 validator counts with
  the original target's baseline and includes detailed validator text on failure. A 1,523-
  vertex `MaleBody` standard-only self-transfer passes `Close`, 58→58 bones, 18,276 remap
  bytes, 1.70e-07 mean L1, 100% dominant agreement, and validation 0→0.
- Fixed atomic RemapData sequencing for standard-only donors: face-sculpt transfer still
  captures the original standard skin before the weight write, while standard-only transfer
  regenerates the blob afterward from NifModel's exact packed float16 values. The versioned
  suite locks both branches and now reports `standardOnly=clean`.
- Production guards correctly rejected three incompatible body/outfit trials before mutation:
  two `Poor` geometry/rest-pose pairs and a geometrically `Close` OldMaleBody/MaleBody pair
  with an incompatible shared `Chest_skin` rest pose.
- A compatible different-surface standard-skinned outfit case also passes natively:
  female Combat Armor Mid torso → Lite torso reports `Caution`, 32 target vertices, 8→8
  bones, 384 remap bytes, 0.0439682 mean L1, 100% dominant agreement, and validation 0→0.
- Archived the complete fixture/production/rejection matrix in
  `tests/rigging/QA_EVIDENCE_2026-07-13.md`; the only remaining release check is visual and
  FaceGen behavior inside Fallout 4.
- Completed final source/parity review and packaged the audited Release application as
  `dist/NifSkope-WW-Rigging-2026-07-13.zip`, excluding the test-only integration executable
  and including the plan, change log, and dated QA evidence.
- Marked read-only Rigging spells as constant and write spells as undoable, avoiding
  misleading non-undoable warnings while preserving the existing confirmation path.
- Fixed skin-bone enumeration to address the `Bones` array itself instead of its first
  element; this restores correct list, comparison, validation, and transfer preflights.
- Release build and native-window smoke test pass. The 1,689-vertex integration fixture
  finishes with 76 blocks, 70 nodes, 69 bones, 20,268 remap bytes, four advanced Undo
  entries or one combined Undo entry, and survives undo/redo plus save/reload.
- Expanded regression coverage proves donor-dialog cancellation and confirmation
  rejection are mutation-free, verifies a mid-workflow import failure restores the
  target byte-for-byte with zero Undo entries, and checks live workspace enablement
  plus the initial 10-bone inventory through a real NifSkope window.
- Added generated, non-destructive varied fixtures: the production Duplicate spell
  creates a two-shape donor and the picker is verified to select the named second
  shape; a target changed from 1,689 to 1,690 vertices and from 3,230 to 3,229
  triangles completes transfer, validation, one-step Undo setup, and save/reload.
  Every resulting vertex retains normalized weights with in-range bone indices.
- Rebuilt the viewport-shading dropdown as a compact material-inspection popover:
  Flat, Unlit, and Shaded are exclusive modes, while future Game Lighting remains
  visible but disabled. A Specular/Gloss workflow is active and the future PBR
  workflow is shown disabled. The persistent channel mixer groups Color, Surface,
  Lighting, and Alpha controls as compact pressed/unpressed buttons. Specular and
  Gloss are separate renderer controls; PBR-only Roughness/Metallic/AO channels are
  hidden while Specular/Gloss is active. Each display mode remembers its own mask,
  Shift-click solos/restores a contribution, and Reset restores supported channels.
  Legacy duplicate channel buttons were removed from the Render toolbar and lighting
  flyout so the mixer is the single channel-control surface.
- Expanded the Block List into a navigation workspace without changing NIF data:
  category quick filters, working search in both flat and hierarchy views, Ctrl+G
  number/name/type jump, browser-style back/forward history, scene-parent breadcrumb,
  session pins, outgoing/referenced-by link menus, and live block/shape/vertex/triangle
  totals. Hierarchy rows now expose concise type-aware summary tooltips. Block Details
  also has a recursive field-name/value filter (Ctrl+Shift+F) that retains matching
  parents and expands matching branches.

## 2026-07-12 — Rigging: target-derived bone bounds

- Weight transfer now recalculates every `BSSkin::BoneData` bounding sphere from the
  target vertices and newly written weights using the existing `spUpdateBounds` path.
- Weight writes plus bound updates are grouped into one snapshot undo operation.
- Added duplicate-name, 256-bone, and exact four-slot target preflight guards.

## 2026-07-12 — Rigging: shared-pose binding guard

- Existing-node binding now verifies shared bones' skeleton-root-relative rest poses
  as well as their BoneData records before copying any missing binding.
- Restores the prior `holdUpdates` state after structural binding so nested model
  operations are not released prematurely.

## 2026-07-12 — Rigging: minimal donor bone-node import

- Added **Rigging → Import Donor Bone Nodes...** for the missing-hierarchy step.
- Imports only plain `NiNode` ancestors required by donor slots with positive weights;
  copies Name/Flags/Transform and explicitly rebuilds only planned `Children` links.
- Leaves donor controllers, collision, extra data, sibling meshes, and unrelated
  descendants behind; avoids unsafe partial link remapping/branch serialization.
- Preflights duplicate/empty/case-colliding names, cycles, detached or multi-parent
  paths, unsupported node subtypes, invalid transforms, and incompatible existing-node
  rest poses. The import is repeat-safe and wrapped in one snapshot undo operation.

## 2026-07-12 — Rigging: transfer compatibility guards

- Restricted preview/write transfer actions to FO4 BS version 130 and reject donor files
  with a different BS version.
- Transfer now requires donor and target shapes to have matching skeleton-root-relative
  transforms before running raw-position closest-point mapping, preventing silent
  cross-file local-space mismatches.

## 2026-07-12 — Rigging: deep FO4 skin validation

- Expanded **Validate FO4 Skin** with skeleton-root reachability, NiNode type and
  duplicate-name checks, 256-bone enforcement, finite/nonzero BoneData transforms,
  valid bounds, `Num Scales` consistency, and shared skin/BoneData ownership checks.
- Vertex validation now treats every positive representable weight as active and warns
  about repeated active indices.
- Detects multiple or byte-stale `CustomizationRemapData` blocks, not just wrong length;
  byte equality is checked only for standard-skeleton skins because faceBones RemapData
  intentionally differs from the current sculpt-bone weights.
- Corrected scene-parent traversal to scan `NiNode.Children`; `getParentLinks()` is not
  an incoming scene-parent API. Root-relative comparisons now exclude root-local transforms.

## 2026-07-12 — Rigging: harden existing-node binding

- Hardened **Bind Donor Bones (existing nodes)** before hierarchy import: malformed
  skin/BoneData array counts and duplicate bone names now abort before mutation.
- Same-name target nodes must belong to the selected skin's `Skeleton Root` subtree;
  donor and target node rest poses are compared root-relative before binding.
- Wrapped the coupled BSSkin/BoneData resize and record copy in one whole-model
  snapshot undo operation.

## 2026-07-12 — Rigging: bind donor bones to existing nodes

- Added **Rigging → Bind Donor Bones (existing nodes)...**, the first structural
  BSSkin write increment.
- Adds only donor bones actually used by the donor mesh and only when an unambiguous
  same-name `NiNode` already exists in the target; missing hierarchy import remains
  explicitly deferred.
- Requires shared donor/target BoneData transforms to agree within `0.001` before
  copying inverse-bind records, preventing cross-bind-space deformation.
- Extends `BSSkin::Instance.Bones` and `BSSkin::BoneData.Bone List` together, copies
  bounds/transforms, enforces the uint8 256-bone limit, and reports unavailable nodes.

## 2026-07-12 — Rigging: FO4 skin validator

- Added read-only **Rigging → Validate FO4 Skin** before structural bone writes.
- Checks `BSSkin::Instance` and `BSSkin::BoneData` counts/array alignment, valid and
  unique bone links, four-slot vertex skin layout, weight normalization, in-range
  nonzero bone indices, and `CustomizationRemapData == vertexCount × 12` when present.
- Reports a compact summary with detailed problems and never mutates the model.

## 2026-07-12 — Rigging: CustomizationRemapData generation

- Added **Rigging → Generate CustomizationRemapData** for skinned FO4 shapes.
- Encodes the current standard-skeleton skin as the engine's exact 12-byte-per-vertex
  layout: four little-endian float16 weights followed by four uint8 bone indices.
- Preserves half-float encodings through `qfloat16`, zeroes indices for zero-weight
  slots, updates an existing named block, or creates and attaches a new
  `NiBinaryExtraData` named `CustomizationRemapData`.
- The action is intentionally limited to BS version 130 and requires four skin slots.

## 2026-07-12 — Bone & Weight Transfer plan + FO4 faceBones research

- Reverse-engineered and byte-verified FO4 `*_faceBones.nif`
  `CustomizationRemapData` (per-vertex 4×float16 weights + 4×uint8 bone
  indices) and `CustomizationRemapNewBonesData`. Proved RemapData is a copy of
  the mesh's standard-skeleton skinning: an encoder regenerating it from
  `BaseFemaleHead`'s skin reproduces the vanilla blob byte-for-byte.
- Added `BONE_WEIGHT_TRANSFER_PLAN.md`: design for an in-place donor→target
  skinning transfer feature (transfers bone bindings *and* weights, not just
  weights into existing vertex groups like Blender's DataTransfer). Covers the
  geometry core (closest-point-on-triangle + barycentric), the FO4 `BSSkin`
  write path (distinct from the classic `NiSkinInstance`/`NiSkinData` handled
  in `skeleton.cpp`), a vanilla ground-truth validation strategy, phasing, and
  RemapData generation as a follow-on. No code yet — planning only.
- Specced a new **"Rigging" workspace** as the feature's UI home, wired into the
  existing Workspaces selector (`nifskope_ui.cpp` ~L1719–1818) beside Default/
  Animation/Materials/Collision — new `tlCreateRiggingManagerDock` factory plus
  five documented edit points. Scope covers bone list, donor/target bone
  comparison, weight transfer, numeric + brush weight painting, and a shared
  skin-data layer over FO4 `BSSkin` and classic `NiSkin*`.
- Designed **cross-file donor + skeleton reference** as the primary workflow
  (§6.6), reusing the proven `importDonorCollision` pattern
  (`collisiontools.cpp:1356`: standalone `NifModel::loadFromFile`, bsVersion
  guard, branch splice with block remap). Core API takes `(NifModel*, block)`
  pairs so donor/skeleton can come from any file. Also specced a read-only
  **donor viewport overlay** (§9.3) reusing the collision-preview draw path.
- **Phase-1 transfer prototype validated** (Python, scratchpad): closest-point +
  barycentric weight transfer. Self-transfer is near-exact (meanL1 0.0001, 100%
  dominant-bone match); head→hairline reaches ~83% agreement with vanilla
  hand-authored skin (residual is legitimate, not a bug — see §7.1). Confirms
  the algorithm and motivates manual weight paint.
- **Inverse-bind write math validated** (§7.2): `skinToBone = inv(boneWorld) @
  meshBindWorld`, matrices as-stored, world = parent@local — reproduces vanilla
  `BSSkin::BoneData` transforms to ~1e-5 across female/male head, beard, hairline
  (all share bind offset T(0,-0.88,120.84) = HEAD position). Round-trip matches
  float32 to 1.6e-5. **All risky skin math (weights + bone transforms +
  RemapData) now de-risked in Python.** Remaining = block assembly + C++/UI.
- **C++ core ported and verified** against Python via UCRT64 g++: closest-point-
  on-triangle, 4x4 multiply/inverse, inverse-bind (`skincore.cpp`) all pass; the
  end-to-end transfer (`skintransfer.cpp`) reproduces the Python head→hairline
  result **exactly** (meanL1 0.00000, 100% dominant match). Validated Python +
  C++ prototype preserved in-repo at `tools/rigging_prototype/` with README.
  Transfer/skin algorithm is now implementation-ready; only NifModel I/O + UI
  remain (need the Qt build).
- **Rigging spell page built into the app** — new `src/spells/riggingtools.cpp`
  (in `NifSkope.pro`), all compiling clean into `release/NifSkope.exe`:
  1. **List Skin Bones** (read-only) — bones a shape is bound to (NiSkinInstance
     or FO4 BSSkin::Instance).
  2. **Compare Bones with Donor...** (read-only, cross-file) — loads a donor NIF
     (`NifModel::loadFromFile`), diffs bone sets (shared / donor-only / target-
     only). The pre-transfer preview.
  3. **Preview Transfer from Donor...** (read-only) — runs the validated transfer
     core (geometry read via NifModel `get<Vector3>`/`Triangle`, closest-point +
     barycentric + top-4) and reports stats (bones used, influence histogram,
     snap distances). No write.
  4. **Transfer Weights (existing bones)...** (WRITE, undoable) — transfers donor
     weights into the target's existing bone slots; influences to absent bones
     are dropped + renormalized. Only rewrites per-vertex Bone Weights/Indices,
     no structural change. Full bone-adding transfer (new nodes + BoneData) is
     the next step.
  Transfer geometry/skin core is ported from `tools/rigging_prototype/`. Gotcha
  hit + fixed: `slots` is a Qt macro — renamed the local. In-app verify pending.

## 2026-07-12 — Compact unified Shader Flags editor

- Added a 460×360 combined editor for compatible `Shader Flags 1` and
  `Shader Flags 2` rows. It builds its flag list from the active `nif.xml`
  bitflag metadata, so Fallout 4 and Skyrim use their own names and bit
  positions without changing the NIF's two-field storage.
- A live text filter searches flag names, semantic categories and storage
  locations such as `F2 bit 25`, keeping the 64-bit set manageable inside the
  compact window. Clicking anywhere on a row toggles it; the footer shows live
  F1/F2 hex values plus selected and visible counts.
- Applying writes each bit back to its original 32-bit field as one snapshot-
  undoable operation. Cancelling leaves both values untouched.

## 2026-07-12 — Unified mode button and full color studio

- Restyled the Object/Edit Mode selector as the same bordered icon-and-text
  dropdown used by Panels and Workspaces, while keeping Tab and external mode
  changes synchronized with its checked item, label, and icon.
- Rebuilt the shared Color3/Color4 chooser used throughout block details into a
  Paint.NET-style color studio: HSV wheel, synchronized RGB/HSV/hex/alpha
  values, previous/current swatches, preset and persistent custom palettes,
  plus an eyedropper that samples any pixel from any connected screen — even
  outside NifSkope. Color3 fields omit alpha; Color4 fields preserve it.
- RGB, HSV, and alpha numeric fields now use the transform redo panel's
  Blender-style scrub interaction: drag horizontally to adjust, hold Shift for
  fine control, click without moving to type, and use hover-only step arrows.
  The arrows disappear and the field brightens while a scrub is active.
- Gave the color studio a NifSkope-native compact layout and paired every
  numeric RGB/HSV/alpha well with a color-aware horizontal slider. The chooser
  now uses a full HSV disc, tighter previous/current and palette groups,
  section dividers, a copyable hex field, and a restrained manager-style
  footer while retaining screen picking and persistent custom colors.
- Fixed the hidden Color3 alpha widgets appearing as orphan controls in the
  dialog's upper-left corner. Replaced the Windows eyedropper's full virtual-
  desktop overlay with low-latency global cursor/button polling and direct
  desktop-pixel sampling, allowing reliable picks from other applications and
  multiple displays without repainting a monitor-sized transparent window.
- Widened the screen-picker tooltip so its instructions are not clipped, and
  made mouse/keyboard completion edge-triggered as well as state-triggered.
  Quick clicks, right-click cancellation, and Escape can no longer fall wholly
  between polling ticks and leave NifSkope stuck in the modal sampler.
- Removed the sampler's nested application-modal `exec()` path. The parent
  color dialog now remains visible, sampling runs in a non-modal local event
  loop, and focus is restored explicitly after completion, preventing Windows'
  modal warning sound and the hidden-dialog lockout after an external pick.

## 2026-07-12 — New "Vertex Colours" viewport shading mode

- Added a fifth entry to the viewport **shading menu**: *Vertex Colours*. It
  renders the per-vertex **colour and alpha as the albedo** while keeping the
  authored **normal and specular/gloss** maps active — i.e. only the diffuse is
  replaced with the vertex colours; lighting still responds to the surface.
- Reuses the existing diffuse-force path (`Scene::VisVertexColors`, a new
  VisMode): the `BaseMap` is forced to white and `vertexColorOverride` is set to
  0 so the shader's `albedo = baseMap.rgb * C.rgb` collapses to the vertex
  colour, and `alpha = C.a * baseMap.a * alpha` keeps the vertex alpha. Vertex
  colours are shown regardless of the Vertex-Colours display toggle or the
  shader's `SLSF2_Vertex_Colors` flag; a mesh with no vertex colours renders
  white. Applies to the Fallout 4 and Skyrim/OB lighting-shader paths.
- New procedurally-drawn `shade_vertexcolor` toolbar icon (a sphere painted
  with colour patches).

## 2026-07-12 — Label BGSM textures by the material's own slot order

- When listing a BGSM's own textures, the tree was labelling them with the
  `BSShaderTextureSet` slot order — but a BGSM's internal texture array uses a
  different order (`BGSM1_*`/`BGSM20_*` in glproperty.cpp): index 2 is the
  specular/gloss map, not glow. So a BGSM's spec map showed up as "Glow/Skin".
  Material-file textures are now labelled by that native BGSM order (index 2 →
  Spec/Gloss, 3 → Greyscale, 4 → Environment, …), matching what the renderer
  samples. The `BSShaderTextureSet` fallback still uses the texture-set order.
- BGEM textures were already labelled correctly (their file order matches the
  effect shader's inline fields: Source, Greyscale, Env Map, Normal, Env Mask).

## 2026-07-12 — Material file (BGSM/BGEM) is the source of truth for textures

- In Fallout 4 the material is almost always linked, so the Material Manager now
  treats a linked **BGSM/BGEM as authoritative** and lists **its own textures**
  as the unfolded children of the material row (labelled by slot), rather than
  reading the NIF `BSShaderTextureSet`. This overrides the texture set when a
  readable material is present, and also fixes materials that have no texture
  set at all or whose textures aren't hooked into the shader/effect node — e.g.
  `x01_arms.bgsm` in X01_ArmRight.nif, whose preview was blank because it has no
  `BSShaderTextureSet`. Selecting the material previews the material's diffuse;
  each slot row previews its own texture.
- The NIF texture set / effect inline texture fields are used only as a
  **fallback** when there is no readable material file.
- Material-file texture rows are **preview-only** (they are defined by the
  BGSM/BGEM, not a NIF field): they are non-editable and the Browse… picker
  skips them. NIF-backed rows keep their editable Path column.
- Caveat: a referenced `.dds` must still be reachable in the loaded archives or
  as a loose file; if it is missing, the row shows MISSING / the preview shows
  "texture not found" rather than a blank pane.

## 2026-07-12 — Fix "Could not find Index subitem" spam on load

- The Material Manager rebuild walked a `BSShaderTextureSet`'s `Textures`
  array via `rowCount()` without checking the array index was valid. A
  `BSLightingShaderProperty` driven purely by a BGSM file has **no Texture
  Set**, so the index was invalid — and `rowCount()` of an invalid index
  returns the ROOT child count, so the loop iterated every header/block/footer
  row and called `resolveString()` on each, logging *"resolveString: Could not
  find \"Index\" subitem."* once per top-level item (seen loading
  X01_ArmRight.nif). Guarded with `array.isValid()`.
- Hardened the four other texture-array loops that shared the same pattern
  (Find Duplicates signature, Copy/Paste Material, Copy/Paste Texture Set) so
  a missing texture set can no longer trigger the same walk — and, in the
  Paste paths, can no longer write strings into unrelated top-level blocks.

## 2026-07-12 — Hierarchy submenu nested under Block

- The block-list right-click **Hierarchy** submenu (Set Parent / Clear Parent)
  no longer sits pinned at the very top of the context menu. It is now inserted
  **directly beneath the Block category**, and the leading separator that
  isolated it was dropped so it reads as a normal submenu.

## 2026-07-12 — Correct texture-slot labels in the Material Manager

- Fixed the material tree mislabelling texture slots. The old label list was
  mis-ordered and invented slots ("Wrinkles", "Displacement", "Extra"), so on
  Fallout 4 meshes the **specular/gloss map (slot 7) showed as "Wrinkles"**.
  Labels now follow the authoritative `BSShaderTextureSet.Textures` slot map in
  nif.xml: `0 Diffuse · 1 Normal · 2 Glow/Skin · 3 Height · 4 Environment ·
  5 Env Mask · 6 Subsurface · 7 Spec/Gloss · 9 Reflectivity · 10 Lighting`.
- Slot 7 is **version-aware**: it reads *Spec/Gloss* on Fallout 4 / 76
  (BS version ≥ 130, matching the renderer binding slot 7 as `SpecularMap`) and
  *Backlight* on Skyrim, where that slot is the back-lighting map.

## 2026-07-12 — Material Manager sizing & texture-preview interaction

- Removed the now-unused `tlScanMaterialTextures` helper (its only caller was
  dropped when the per-texture slot list went away), clearing the last
  unused-function warning from `meshtools.cpp`.
- Texture-preview channels are now **image-editor / Blender style**: a plain
  **left-click isolates a single channel** (switches the view to just that one,
  shown greyscale); **Shift+click** adds or removes a channel from the current
  selection. Previously every button was an independent toggle.
- Made a selected **alpha a no-op when mixed with colour channels** — e.g.
  `R+A` now shows the red channel instead of washing to white. Alpha still
  reads on its own via the single-channel greyscale path.
- The **UV grid overlay is now adaptive**: the 0..1 space always shows bright
  quarter (major) lines and a boxed border, and **finer subdivisions fade in as
  you zoom in**, like Blender's UV editor (up to 256 divisions at max zoom).
- The **Material Manager dock now opens at a comfortable default width (~690px)**
  the first time it is shown, with the texture preview taking the larger share
  below a compact material list. This is a one-time default (persisted via
  `MatTexManager/InitialWidthSet`), so the user's saved layout and drags win
  afterward.
- Added an **always-on vertical scrollbar** to the *Materials in file* list.

## 2026-07-12 — Material Manager tree & preview refinements

- Made the Material Manager tree **material-first**: the primary column is now
  **Material** (the `.bgsm`/`.bgem` file, or shader type) that unfolds into its
  textures, with the owning **Mesh** name in the adjacent column. Column-aware
  navigation and the right-click *Reveal* action follow suit (Material → shader
  block, Mesh → owning node, Path → the string field).
- Selecting a material now highlights **only that material row** in blue/orange,
  not its texture children — matching how Blender highlights a parent without
  its children.
- Texture preview channels: a single selected channel shows **greyscale**; two
  or more compose **opaquely** so a zero/low alpha (common on diffuse and
  spec/gloss maps) can no longer make the preview go black. A selected alpha
  adds as grey.
- Consolidated find & replace into a single **Replace...** button that opens a
  small Notepad++-style dialog (Find what / Replace with / Match case / Replace
  All), leaving just the live **Filter rows** box on the toolbar.
- Removed the preview's redundant per-texture slot list and type buttons now
  that textures are selectable directly in the tree; the preview pane is a
  single full-width canvas. Selecting a material previews its diffuse; selecting
  a texture row previews that texture.

## 2026-07-12 — Material Manager & texture-preview pass

- Rebuilt the **Material Manager** to be genuinely material-centric rather than
  a flat imitation of the Collision Manager's chevrons. Each material/node is
  now one compact collapsible **parent row** with its textures listed beneath
  it as semantic child rows (Node / Texture / Path / Status columns). Groups
  fold and expand via the chevron, double-click, or the row context menu
  (**Fold / Expand This Node's Rows**), matching the Collision Manager's row
  density and expansion behaviour.
- Constrained the texture-preview **wheel zoom** to a sane range (0.125x–8x)
  instead of the previous unrestricted zoom, and de-duplicated redundant zoom
  updates below a 0.0001 threshold.
- Fixed **multi-channel toggling** in the texture preview: the channel mask is
  now applied as an independent 4-bit R/G/B/A field (`mask & 15`) so channels
  can be enabled in any combination rather than clobbering one another.
- Added **middle-mouse camera pan** to the Texture Preview window, Blender
  style — hold the middle button to drag the view; release restores the cursor.
- Reordered the block context menu so the **Block** submenu sits directly
  beneath **Transform** regardless of static spell-registration order, and
  guarded `SpellBook::sltSpellTriggered` so native application actions hosted in
  the book (e.g. the Block List Hierarchy submenu) no longer fall through the
  spell-casting path.
- Traced and unified the **posed-skeleton edit rendering**: edit-mode overlays
  and the shaded mesh now derive their world transform from the same
  `shapeRenderTrans` (inverse camera × the shape's `viewTrans`), so a skinned /
  posed shape's edit wireframe follows the identical skinned positions used to
  draw its shaded surface.
- Relocated the **Material Manager** from the bottom dock to the right dock
  area so it sits alongside the Collision Manager, and restricted it to the
  left/right areas. It no longer opens as a wide floating window; the Materials
  workspace now activates a vertical right-side inspector.
- Mirrored the Collision Manager's design language onto the Material Manager:
  matching panel margins/spacing (6px), a bold **Materials in file — N
  material(s), M texture(s)** inventory header, a 150px minimum table height,
  alternating row colours, and the same blue/orange selection styling.
- Rebuilt the Material Manager list as a real **`QTreeWidget`** (materials own
  their textures as native children) so it behaves like the Collision Manager
  node list: textures start folded, unfold via double-click or the native `>`
  expand arrow (single-click no longer toggles), the sort indicator shows only
  on the column being sorted by, and the list has its own horizontal
  scrollbar. Path editing is restricted to the Path column via an item
  delegate (F2 / drag-drop / browse); double-clicking a texture opens the
  archive browser, and the right-click **Fold/Expand This Material's Textures**
  drives the native expansion state.
- Nested the **Texture Preview** at the bottom of the Material Manager inside a
  draggable vertical splitter, updating live as rows are selected. A **Detach**
  button (or the toolbar's *Texture Preview…*) pops it out into its own window;
  closing that window re-docks it under the list.

## 2026-07-11 — Scene hierarchy parenting

- Added Blender-style **Set Parent** (`Ctrl+P`) and **Clear Parent** (`Alt+P`)
  menus in object mode, with matching viewport context-menu actions.
- Any `NiNode` subclass can be a parent. Any compatible `NiAVObject` scene
  block can be a child, including `NiNode`, `BSTriShape`,
  `BSSubIndexTriShape`, and other renderable scene-object subclasses.
- Added keep-world and keep-local parenting modes, optional additional-parent
  links, multi-selection with the active node as parent, cycle prevention, and
  snapshot undo. Clear Parent can preserve the child's world transform.
- Non-scene data/property blocks are rejected rather than producing invalid
  `NiNode` child links. Clear Parent Inverse remains visibly unavailable
  because the NIF scene graph has no Blender-style parent-inverse field.
- Added a native **Hierarchy** submenu to the Block List context menu. It
  exposes Set Parent and Clear Parent for the current single or multi-row
  selection; `Ctrl+P` and `Alt+P` now also work while the pointer is over the
  Block List.
- Replaced manager entries in **Panels** with an adjacent **Workspaces** menu:
  Default, Animation, Materials, and Collision. Only the selected manager
  workspace is displayed, and the choice persists between sessions.
- Added **Anim Static** and **Stairhelper** collision-creation presets with
  appropriate layer and fixed/keyframed physics defaults.
- Collision Layer and Material are now compact searchable selectors with
  contained scrolling. The complete FO4 layer table is exposed explicitly,
  including Stair Helper (31), and the physics label is shortened to
  **Material**.
- The Materials workspace now docks along the bottom, preserving viewport
  width and giving the texture preview a wider canvas. Panels and Workspaces
  now use monochrome icons drawn by NifSkope's existing toolbar icon system
  instead of platform-native file/desktop icons.
- Corrected vanilla hknp body decoding: collision filters are read from body
  cinfo `+0x14` (with compatibility for early local builds that used `+0x1C`),
  so `airportroomstairs01.nif` now resolves its ramp body as Stair Helper (31)
  instead of Static. Convex-body material IDs are resolved through the
  embedded `hknpBSMaterialProperties` table, recovering StoneStairs for that
  body instead of displaying the unrelated `0x26067D15` header value.
- Collision Layer and Material selectors now repopulate after a file load and
  use a committing contains-filter completer: typing narrows the popup, and
  clicking a result writes that exact enum value rather than reverting during
  the following model refresh.
- Fixed editable Layer/Material searches writing value zero while the user was
  still typing. They now have dedicated commit paths: incomplete filter text
  performs no model write, and choosing a real result writes only that field
  before rebuilding the manager. Collision Layer therefore no longer snaps
  back to Static after selection.
- Restored cross-file **Copy Branch / Paste Branch** on Windows by returning
  branch and block clipboard MIME names to an ASCII format. Paste remains
  compatible with clipboard data produced by the interim Unicode-separator
  builds, and the Block List once again recognizes branch data instead of
  hiding Paste Branch from its right-click menu.

## 2026-07-10 — Collision Manager feedback pass

- Streamlined the Collision Manager while retaining NifSkope's native dark Qt
  styling: sorting now lives entirely in the clickable column headers; a
  contextual split action exposes Decompile Selected/All or Compile Selected;
  Check Collision and Import Donor are compact actions; refresh and Collision
  -> BSTriShape (renamed Create Editable Mesh Copy) moved to More. Creation now
  distinguishes New collision material from Selected collision material,
  exposes Optimize Source Mesh beside Create Collision, and hides method-specific
  controls when irrelevant. Compiled rows use a concise read-only summary with
  Decompile to Edit Physics instead of a wall of disabled fields, while editable
  rows show the inspector and a collapsed Advanced physics section.
- Normalized Fallout 4 Collision Manager labels to Bethesda's 3ds Max
  exporter vocabulary without changing stored enum values. The base material
  list now uses names such as `ActorCrabArmored`, `Bone`, `WeaponAxe1Hand`
  and `WoodBarrel`; collision layers use `Tree`, `Prop`, `Small Debris`,
  `ShellCasing`, `Character Controller`, `NavMesh Cut`, `spellTrigger`, and
  the other exporter spellings. Internal CK identifiers and exact CRCs remain
  visible in material tooltips.
- Added Bethesda exporter-style **Collision group and advanced physics**
  controls for editable rigid bodies: Keyframed, Linked Group, Collision
  within Group, Wind, packed collision-filter Group, local center of mass,
  inertia-tensor diagonal, allowed penetration, and Deactivator Type. Center,
  inertia, filter flags and filter group are also preserved through the native
  hknp Compile/Decompile round trip. Settings whose compiled byte layout is not
  yet validated (Keyframed, Wind, nonstandard quality/solver/deactivator and
  penetration) remain fully editable but deliberately block Compile instead
  of being silently lost. Phantom/Shape Phantom are shown with an explicit
  disabled explanation until their distinct object/body graphs and hknp
  classes are implemented.
- Restored the complete six-mode creation strip from the visual design:
  **Box** (PCA-oriented fit), **Sphere** (bounding-sphere fit), **Capsule**
  (principal-axis fit), **Hull** (qhull), **Decomp** (CoACD), and **Mesh**
  (`NiTriStripsData` / `bhkNiTriStripsShape` / MOPP chain). Preset, collision
  type, material and Replace now feed the created body; Decimate remains
  beside the conversion controls.
- Collision rows are expandable. Each collision object/body is the parent and
  every compiled or editable child shape appears underneath it with its own
  type, material and state. Expansion survives live refreshes.
- Restored the preview controls for colour mode, solid, X-ray, collision-only
  and labels. They now affect both compiled and decompiled collision, while
  collision-only hides render geometry but keeps the ground grid visible.
- Renamed collision **Decode / Decode All** actions to **Decompile / Decompile
  All**, and renamed the Timeline dock to **Animation Manager**.
- Expanded editable physics controls with motion system, quality, solver
  deactivation, damping and velocity limits; matched Collision Manager row
  selection colours to the Block List; redesigned the Panels dropdown.
- Rebuilt the Physics section around named enum selectors: collision layer,
  Havok material, motion system, quality and solver deactivation now show
  readable names and write directly to the selected editable rigid body/shape.
  Convex Decomposition is now correctly nested as a Convex generation method
  rather than presented as a collision shape type.
- Added persistent sorting by Node, Shape, Collision type, Material, Mass or
  State, with ascending/descending order and clickable-column synchronization.
  Collision rows now have a right-click menu for decompile, decompile all,
  compile status, lint, expand/collapse, Block List selection, material copy,
  creation-default material and refresh.
- Material CRCs now resolve through the active game's Havok material enum after
  decompilation. A `+` material action stores custom name/value pairs and writes
  their numeric value to editable shapes. Decompiled layer zero is normalized
  to STATIC or PROPS from body motion, eliminating misleading UNIDENTIFIED
  collision types in both new and already-decompiled files.
- Corrected collision-material naming against Bethesda's 3ds Max exporter
  labels: CRC 911716378 now displays as `Concrete` instead of the fabricated
  `StoneConcrete`, while tooltips retain `MaterialStoneConcrete` and the exact
  hexadecimal/decimal CRC. The Create field now searches all 157 Fallout 4
  enum entries and accepts exporter labels, internal IDs, legacy shortened
  names, custom names and numeric CRCs. Unlisted values are explicitly shown
  as `Unknown (0x...)` instead of being mistaken for decimal material names.
- Simplified creation buttons to Box, Sphere, Capsule, Convex and Mesh, changed
  them to neutral gray styling, and removed the redundant creation-panel
  collision-type selector; body type is edited in Physics.
- Added a non-destructive live collision preview for Convex, CoACD decomposition,
  accurate Mesh and decimated Mesh creation. A Blender-style floating operator
  panel appears at the viewport's bottom-right with triangle percentage, hull
  precision, decomposition threshold and maximum hull controls. Changes are
  debounced and drawn as an amber world-space overlay; Apply writes the chosen
  collision graph while Cancel/close clears the overlay without touching the
  NIF. The Collision Manager's Decimate action now opens this preview at 50%
  instead of modifying render geometry through the old Simplify dialog.
- Matched the collision preview operator to the transform shortcut panels:
  compact dark layout, neutral action buttons, close/collapse header and the
  same Blender-style number wells. Triangle percentage can be held and dragged
  left/right (Shift-drag for fine control, click to type), with throttled live
  preview updates during the scrub; hull precision and threshold support the
  same interaction.
- Completed the workflow accelerators around the manager: **Import Donor**
  copies an editable or compiled collision branch from another same-version
  NIF; **Mass from Material** calculates closed-mesh volume and applies a
  density-family mass estimate; and **Collision -> BSTriShape** creates a
  visible, editable render proxy under the collision's owning node. Structural
  operations are snapshot-undoable where applicable.
- Expanded **Check Collision** into a guided linter. It now reports dangling
  objects, unidentified layer 0, mixed compiled/editable state, hulls over 64
  vertices, box-like hulls, non-uniformly scaled primitives, STAIRHELPER
  primitives with no slope, and visible geometry with no collision in its
  node hierarchy. Its safe-fix pass removes dangling branches and infers layer
  1/10 from body motion in one undo step.
- Added amber per-row and footer collision budget warnings for meshes over 500
  triangles, packfiles over 128 KiB, or files over 2,000 collision triangles /
  256 KiB. Warnings point directly to the live Decimate workflow.
- **Decompile** and **Decompile All** now each create one whole-model snapshot
  undo step, including the multi-system command.
- Added the native `hknpencode` writer and enabled **Compile** for editable
  collision. It writes an FO4 hk_2014.1.0 compressed-mesh packfile with local,
  global and virtual fixups, material/layer, static or dynamic physics,
  calculated density and AABB inertia; replaces the legacy branch with a
  `bhkPhysicsSystem` + `bhkNPCollisionObject`; and rejects the operation unless
  NifSkope's own decompiler successfully round-trips the generated geometry.
  The first writer is deliberately one section (maximum 255 vertices and 255
  triangles); the manager directs larger meshes through Decimate first.

## 2026-07-10 — Collision Manager first functional slice

- Added a right-side **Collision Manager** dock to the Panels menu. Its live
  browser lists every compiled `bhkNPCollisionObject` and editable
  `bhkCollisionObject` with owning node, shape summary, collision layer,
  material CRC, mass, and state. Selecting a row selects the real NIF block.
- Wired the existing **Decode**, **Decode All**, **Create Convex Shapes**, and
  **Simplify/Decimate** operations into the manager. The physics panel
  live-edits mass, friction, restitution, and collision layer on editable
  rigid bodies; compiled rows remain read-only with a Decode hint.
- Added **Create Collision…** as the user-facing Havok creation spell and as
  a direct object-mode viewport right-click action. Compiled collision gets a
  matching contextual **Decode Collision** action; both remain available from
  the normal block spell menu too.
- Added a first **Check Collision** pass (dangling references, mixed compiled
  and editable state, suspicious >64-vertex hulls) and a footer budget showing
  collision vertices, triangles, and compiled packfile bytes.
- Compiled collision now supports the manager's persisted **Solid**, **X-ray**,
  and **Colour by** settings: translucent fill plus a full-alpha type-coloured
  wire overlay. Selection highlight still overrides the palette.
- This first-slice note is superseded by the completed workflow/encoder entries
  above. The manager was release-built and passed an offscreen startup test
  while loading BoxStaticCollision.nif.

## 2026-07-09 — Motion / mass / layer decoded into bhkRigidBody

- **Decoded rigid bodies now carry real physics instead of defaults.** The
  decode spell fills each created bhkRigidBody's Rigid Body Info from the
  packfile: collision layer, friction, restitution, damping, gravity factor,
  max velocities, and for dynamic bodies mass + center of mass, with the
  authoring enum values the 3ds Max exporter writes (dynamic: Motion System 3
  / Quality 4 / Solver Deactivation 2; static: 5 / 0 / 1).
- Sources, all validated against the controlled Elric pairs
  (Documents/3dsMax/export) and the CK FileConvert XML oracle:
  - PSD+0x10 body_props, **stride 0x50 per body**: friction (trunc-float16)
    +0x12, restitution +0x16
  - PSD+0x20 dyn_motion / PSD+0x30 dyn_inertia (present only on dynamic
    systems): damping/velocity defaults; inverseMass +0x04 (PropCollision
    mass 10 -> 0.1 exact), density +0x08, inertia diagonal +0x20
  - BodyCInfo **+0x1C = collision layer** (matches raw Havok Filter 10/1/31),
    +0x0C = motion index (0x7fffffff = static body)
- Cross-checked against BadDogSkyrim/PyNifly's independent RE (which also
  confirms the Body ID / node-placement rule and documents a working packfile
  *writer*). Two PyNifly errors found and corrected by our controlled pairs:
  their body_props stride 0x110 only fits single-body files (real stride
  0x50), and their collisionResponse@+0x10A reads past the entry.

## 2026-07-09 — Body ID binding: each node places its own body

- **The real placement rule found** (supersedes the earlier "root space"
  entry, which was wrong): each `bhkNPCollisionObject` names its body via the
  **Body ID** field, and that body is placed by **that node's transform**.
  The hknpBodyCinfo position/orientation is only Elric's rest pose — on
  vanilla stair helpers the two differ, and the node transform is the one
  that puts the ramp at ground level (cinfo floated it a story up: the
  stubborn stair-helper offset). On many files node transform == cinfo
  position, which is why cinfo-only ever looked plausible.
- **Preview**: each collision object draws exactly its own body (matched by
  Body ID) in its own node's space — duplication is structurally impossible.
  The decoder tags every shape with its owning body's id and no longer
  composes the cinfo transform into the shapes.
- **Decode spell**: one bhkRigidBody + bhkCollisionObject per body, attached
  to that body's own node (the exact inverse of Elric's compile, matching the
  pre-Elric Max-export layout). Shapes with no owning body decode with body 0,
  same rule as the preview.

## 2026-07-09 — Shared collision system no longer duplicated

- **Root cause of "duplicated + wildly rotated" collision found** (credit:
  user spotted the shared reference): one bhkPhysicsSystem is often
  referenced by several nodes (a platform node plus its ramp nodes all point
  at the same block). The preview drew the whole system once per referencing
  node, each in that node's own space — so N copies at N different
  transforms. Now the system is drawn once, from its first referencing
  collision object.
- **Decode spell matches**: consolidates the shapes onto the first node and
  removes every referencing bhkNPCollisionObject plus the system (previously
  it removed only the first, leaving the others with dangling references).

## 2026-07-09 — Collision decode hardening

- **Body rotation formula corpus-validated**: batch-checked against 300
  vanilla architecture files (21 transformed helper bodies) — the shipped
  convention wins by 4–14x over every alternative; residual differences are
  physically expected ramp-tip overhangs. Validator: batch_validate.py.
- **CMS→CMSD pairing exact**: compressed-mesh geometry is resolved through
  its real pointer (global fixup at +0x60), not file order — vanilla files
  order objects differently than tool exports (was causing skewed duplicate
  collision).
- **Material write fixed**: decoded material CRCs now actually land in the
  created shapes' Material fields (the previous nested write failed with
  "Could not find Material subitem" warnings).

## 2026-07-09 — Collision body transforms (stair helpers, multi-body files)

- **Stair helpers and multi-body collision now land in the right place.**
  Each Havok body's position + orientation (hknpBodyCinfo) is decoded and
  applied — previously separate bodies (stair slope helpers, secondary
  volumes) collapsed to the origin, which also looked like "duplicated,
  slightly offset" collision overlapping the main mesh. Body transforms
  compose with compound instance transforms, and shapes shared by several
  bodies are instanced once per body. Applies to both the preview and the
  Decode Compiled Collision spell (wrapped as transform shapes).
- Validated against vanilla BldgBrick3Story1x2ResEntB.nif (stair helper at
  its correct offset and 30° slope).
- Note: the CK's FileConvert.exe (Downloads/Examples) works as a reference
  XML dumper for pure-Havok blobs but rejects Bethesda-custom classes
  (hknpBSMaterialProperties) — the native decoder handles those fine.

All changes made to this fork on top of fo76utils/nifskope (develop @ f2587869).
Newest entries first. This document is kept up to date with every change batch;
`TO_BE_IMPLEMENTED.md` holds the forward-looking backlog.

---

## 2026-07-08 — Compiled collision: preview + decode to editable blocks

- **Elric-compiled collision (bhkPhysicsSystem) is now decoded natively**
  (src/gl/hknpdecode.*): Havok 2014 packfile parsing with convex polytopes
  (boxes/hulls), compressed triangle meshes, spheres, capsules, and
  compound-shape **instance transforms** (position/rotation/scale per shape,
  byte-validated against pre-Elric bhkConvexTransformShape matrices).
- **Preview**: with Show Collision enabled (display options), compiled
  collision draws as amber wireframe — display only, blocks untouched.
- **Right-click > Havok > "Decode Compiled Collision"** converts a
  bhkPhysicsSystem back to its pre-Elric form: bhkBoxShape /
  bhkConvexVerticesShape / bhkSphereShape / bhkCapsuleShape /
  NiTriStripsData chains, transformed instances wrapped in
  bhkConvexTransformShape / bhkTransformShape with the verbatim matrix,
  multiple shapes in a bhkListShape, all under a fresh bhkRigidBody +
  bhkCollisionObject. "Decode All Compiled Collision" converts every system
  in the file. MOPP is left empty (Update MOPP Code / Elric rebuilds it).
- Capsule radii are recovered within ~0.3% (Elric shrinks them into a core
  hull + margin); sphere centers fold into the wrapper transform.
- Fixed: nav gizmo-compass fading out when collision display is on (leaked
  GL_LINE polygon mode broke the QPainter overlay).
- Not yet decoded: material / mass / motion metadata (new bhkRigidBody gets
  defaults), multi-body ragdoll systems.

## 2026-07-08 — Fixes + snap-to-active + compiled-collision groundwork

- **Edge loop select fixed**: Alt+click was being eaten by NifSkope's
  background color sampler (that's why the viewport background turned
  white/orange/black). In edit mode Alt+LMB is now always a selection click.
- **Snap menu (Shift+S)**, edit mode: new **Selection → Active** (collapse
  the selection onto the last-picked element) and **Cursor → Active**.
- **Circle select got the Deselect panel**: each paint stroke pops the
  gesture redo panel (like box select); Deselect re-applies the same brush
  stroke subtractively.
- **Compiled collision investigated** (bhkNPCollisionObject →
  bhkPhysicsSystem): the Elric-compiled blob is a Havok 2014 packfile;
  convex shapes are fully decoded, triangle meshes partially. Analysis
  tools in `tools/hkdump.py` + `tools/hkparse.py`; findings + display plan
  in TO_BE_IMPLEMENTED.md.

## 2026-07-08 — Blender selection batch

- **Circle select (C)**: brush-paint selection in object and edit mode. LMB
  paints select, MMB paints deselect, mouse wheel resizes the brush, RMB/Esc
  exits. One undo step per paint stroke. (3D-cursor placement moved off the
  C key — see below.)
- **Select More / Less (Ctrl+= / Ctrl+-)**: grow the edit-mode selection one
  connectivity ring, or shrink it by dropping its boundary elements.
- **Edge loop select (Alt+click, Shift+Alt extends)**: walks the most
  collinear continuation edge (per-vertex, skipping edges that share a
  triangle with the current one); boundary edges walk the mesh boundary —
  so it traces borders and straight strips well even on triangulated meshes.
  Turns sharper than ~60° stop the loop.
- **Shift+RMB places the 3D cursor** (the true Blender binding). Plain C no
  longer places the cursor; Shift+C (cursor to picked median) is unchanged.
  The Shift+RMB alternate context-menu pick is retired.
- **Frame Selected (Numpad-. or .)**: centers and zooms the camera on the
  current selection (object or edit mode); with nothing selected it frames
  the whole model. Also in the Viewpoint toolbar dropdown.
- **Collapsible redo panels**: clicking the ˅ title of any redo panel
  (transform / operator / box select) collapses it to just its title bar,
  ˃ expands it again — Blender-style.

## 2026-07-08 — Modal transforms own the mouse (unbounded drag)

- **G/R/S drags no longer stop at the viewport edge.** The modal gesture now
  grabs the mouse for its whole life (Blender): dragging over the block list,
  docks, or outside the window keeps moving the selection, and when the cursor
  hits a screen edge it wraps around to the opposite side so the drag is
  unbounded. The keyboard is grabbed too, so X/Y/Z axis locks, typed values,
  Enter/Esc all reach the gesture even when the block list had focus.

## 2026-07-08 — Box-select redo panel + Blender-look panels

- **Box-select redo panel**: applying a box select pops a small panel at the
  bottom-left with a **Deselect** button — one click re-applies the same
  rectangle subtractively (i.e. "I meant to deselect that"). The rectangle is
  screen-space, so use it before moving the camera.
- **Redo panels restyled like Blender**: bold "˅ Move / Rotate / Scale"
  header, values stacked vertically ("Move X" / Y / Z with right-aligned
  labels), dark rounded value wells, Axis/Orientation rows underneath. Same
  dark styling on the Merge/Select-Linked and Box Select panels.
- **Drag-number fields polished**: the ‹ › step arrows now only appear while
  hovering the field (and hide while typing), and the field lights up while
  you click-drag to scrub its value — matching Blender's number widgets.

## 2026-07-08 — Box select is now additive

- **Box select (B)** no longer replaces the selection: a plain drag only ever
  **adds** what's inside the box, and **Shift- or Ctrl-drag deselects** what's
  inside the box. Use A (deselect all) or click empty space to start fresh.
  Applies to object and edit mode alike.

## 2026-07-08 — Operator redo panels, context menu, camera re-center, selection memory

- **Operator redo panel** (Blender-style, bottom-left of the viewport): after
  **Merge by Distance** or **Select Linked by Angle**, a floating panel shows the
  distance / sharpness value in a drag-number field; scrubbing or typing a new
  value undoes and re-runs the operation live. The old pop-up dialogs are gone
  (the Render-menu "Select Linked by Angle..." entry now uses the panel too).
  If something else touches the undo stack the panel freezes instead of
  corrupting history. Transform and operator panels swap — only one shows at a
  time.
- **Edit-mode redo panel**: the Move/Rotate/Scale panel now also appears after
  vertex/edge/face transforms (G/R/S in edit mode), and editing its values
  re-applies the element transform. (Initial value display for rotate/scale
  wired same day.)
- **Drag-number fields (DragSpinBox)** fixed: click-drag left/right scrubs the
  value (Shift = fine), plain click types, hover shows ‹ › step arrows. The
  original implementation never received mouse events (the spin box's internal
  line edit swallowed them); it now event-filters the line edit.
- **Menu auto-close** fixed: operator pop-ups (Delete/Merge/Separate/Snap/Set
  Origin) close when the pointer moves away. Hover-out is now detected by a
  60 ms cursor-position timer (mouse-move events are never delivered while a
  menu has the pointer grab).
- **Viewport right-click menu** extended, both modes: Select All (A) /
  Deselect All / Invert (Ctrl+I) / Box Select (B), Snap… (Shift+S), Set
  Origin… (Shift+Ctrl+Alt+C), and a 3D Cursor submenu (snap cursor to
  picked/world origin; move verts to cursor in edit mode, snap node to cursor
  in object mode). Select All / Deselect All are also new as explicit actions
  (the A key keeps its Blender toggle behaviour).
- **Camera re-center button** on the render toolbar (bracket-frame icon, next
  to the Viewpoint dropdown): one click re-centers the camera on the model
  (same as Render > Center, Shift+C).
- **Edit-mode selection memory**: leaving edit mode remembers each mesh's
  selected vertices/edges/faces; re-entering edit mode on the same object
  restores them (world positions re-derived, element pick modes re-enabled to
  match). Deselecting everything is remembered too. Cleared when a new file is
  loaded. Caveat: selections are keyed by block number, so operations that
  renumber blocks (e.g. Separate) can shift what a remembered selection refers
  to; out-of-range entries are dropped safely.
- **Version string** renamed to "NifSkope - Wild Wasteland Edition 0.1
  (build rev, date)".

## 2026-07-08 (earlier) — Snap persistence, node transforms from the block list

- Snap settings persist across sessions (target mode, base, affect, align
  rotation, default-on, grid step, rotation step).
- G/R/S work on a node picked in the **block list** (not just a viewport
  pick): the shortcut is routed app-wide while the pointer hovers the
  viewport; the gizmo and a new always-on origin dot (orange) appear for
  geometry-less nodes (NiNode, lights) so they are visible and grabbable.
- Snap marker sits on the snap **target** (Blender), axis-constrained snaps
  move only along the locked axis, element scale respects the axis constraint.
- Merge (M): At Center / At Cursor / By Distance with union-find welding,
  triangle compaction and skin resync.
- Timeline: switching the displayed animation no longer changes the block
  selection.

## 2026-07-07 — Edit-mode operations batch

- **Delete** (X): Blender-style menu — Vertices / Edges / Faces / Only Faces;
  packed BSTriShape vertex compaction, triangle reindex, bounds + skin resync
  (NiSkinData remap, stale NiSkinPartition dropped; FO4 inline skin needs no
  fixup). Snapshot-undoable.
- **Box select** (B) in object + edit mode with X-ray awareness; object mode
  tests actual geometry (not node origins, which sit at the skeleton root for
  skinned meshes). **Invert selection** (Ctrl+I) in both modes.
- **Selection undo** (Ctrl+Z steps back through selections on a dedicated
  stack; a mesh edit resets it so Ctrl+Z undoes the edit).
- **Select Linked** (Ctrl+L; by-angle variant with adjustable sharpness),
  hide/unhide (H / Alt+H), Separate (P) / Join (Ctrl+J) / Duplicate (Shift+D).
- Shading modes: Flat / Solid / Shaded (exclusive) + independent Wire overlay
  and X-ray; display options persist. Set Origin menu (Shift+Ctrl+Alt+C):
  geometry↔origin↔cursor, works on plain NiNodes.
- Particle effect shading: particles get the BSEffectShaderProperty features
  (falloff, greyscale palette, env cube-map) without breaking alpha masks;
  NiPSysColorModifier lifetime gradients.

## 2026-07-06 — Blender transform suite + particle/VFX preview

- Draggable gizmo handles (arrows/rings/boxes), numeric G/R/S input, transform
  orientation (Global/Local/Parent/View) + pivot menus, element pick modes
  (1/2/3), snap to vertex/edge/face with align-rotation, 3D cursor (C /
  Shift+C), transform redo panel (floating, bottom-left).
- FO4 particle preview: CPU NiPSysUpdateCtlr simulation (box/cyl/sphere/mesh
  emitters, gravity/drag/colour/scale modifiers, flipbooks), procedural
  lightning (BSProceduralLightningController) with textured bolt ribbons,
  BSPositionData decoded + Generate spell fixed (raw u16 triangle indices),
  screen-space refraction preview.
- Multi-node object selection with stencil silhouette outlines; free camera
  (Shift+F).

## 2026-07-04 — Animation timeline dock (v1–v3)

- New bottom dock: sequence selector, per-controller keyframe lanes, value
  graph, transport controls, key inspector; drag/snap/insert/delete/copy/
  paste/scale/nudge keys, easing + tangent handles, interpolation switching,
  CSV round-trip, lint with guided name fix.
- Rigging spells: Setup Controllers, Remove From Animation, Duplicate+Scale
  Sequence, Bake B-Spline. Solo view (Alt+Q). Modal Blender gizmo (G/R/S,
  X/Y/Z, Ctrl snap) with Auto-Key.
- Undo model: value edits merge into ChangeValueCommand transactions;
  structural edits snapshot the whole model (NifSnapshotCommand).

---

_Known gaps / backlog: see `TO_BE_IMPLEMENTED.md`._

## 2026-07-13 — NIF Browser resource warning fix

- The configured-resource mesh indexer now ignores dedicated `textures` and
  `materials` directories and silently skips other unreadable/non-mesh paths.
  Expected resource mismatches no longer generate one modal warning per path;
  the skipped count is available in the `Available NIFs` tooltip.
- Added independent **Load Archives** and **Load Loose NIFs** source toggles.
  Either source can be viewed alone, while enabling both merges them into the
  same virtual folder tree using normal configured-resource precedence.
- Opening a browser result no longer enrolls it in the combined workspace.
  **Add to Loaded NIFs** on the file context menu, or drag selected browser
  files into the lower pane, to add them explicitly. New entries start inactive;
  lower-pane multi-selection controls which secondary NIFs are rendered and
  offered to donor tools, while the orange primary remains the editable file.
  The loaded list now uses ordinary Block-List-like grey/selection styling and
  explicit as-needed scroll bars.
- Fixed ordinary NIF Browser double-click/Open creating a successfully loaded
  but hidden document. Foreground opens now create and activate a visible
  NifSkope window; only explicit **Add to Loaded NIFs** background-loads it.
- Loaded NIF selection now uses the exact Block List palette: light-blue and
  primary orange text for the editable document, dark-blue and secondary
  orange text for selected donors, and the normal grey row for inactive NIFs.
- Loaded NIFs are now true background model containers: they are never shown,
  so adding one no longer flashes a black window. Closing the visible primary
  closes its entire loaded-NIF workspace instead of promoting hidden document
  windows one by one.

## 2026-07-11 — Material workspace and collision usability

- Collision Manager content now scrolls inside the dock, so collapsing the
  Advanced section cannot push its footer below the taskbar.
- Mass from Material uses winding-independent hull volume and reports open or
  degenerate collision clearly.
- Material Manager now keeps its resource table focused, while Texture Preview
  is a separate movable, deliberately non-dockable tool window. Material slots
  are labelled by purpose, R/G/B/A can be isolated or combined, the canvas can
  zoom and pan, and the UV-tile overlay persists independently. Owner resources
  use compact Collision-Manager-style folding in the material list.
- Added Material spells and matching manager actions to fill a
  BSShaderTextureSet from BGSM, safely or fully synchronize shader properties,
  compare/lint material setups, copy/paste complete setups, batch-assign to
  selected shapes, select every user, and find/share identical shader blocks.
- Manager status badges identify missing, out-of-sync, overridden and shared
  resources. The Animation Manager overhaul was recorded in the backlog.
- The three viewport shading icons are consolidated into one dropdown with
  Flat, Solid, Shaded, and Normal + Spec/Gloss modes; the diagnostic mode
  removes diffuse colour while retaining authored normal and gloss response.
