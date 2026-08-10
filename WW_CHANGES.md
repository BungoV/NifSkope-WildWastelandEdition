# NifSkope — Wild Wasteland Edition: Change Log

## 2026-08-10i — Row marks are one-click, and a loaded mesh can follow the skeleton

**The marks were invisible.** The weapon mark and the skeleton/face markers shared
one glyph slot and were settable only from the row menu, so the two facts about a
row you most wanted at a glance were the two you had to right-click to find —
while the eye beside them had been one-click since the day it shipped. The strip
is five slots now: the skull/face marker, then **four toggles** — weapon, pose
follow, visible, see-through — each drawn on every row, dim for off and accent for
on (`wwSkinColor` only, no literals), each answering a single click. The row menus
still offer both marks and read and write the same state, as the alternate path.

**A gesture bug fell out of testing it.** The four-toggle gesture suite asserts
three things per control — press does not select, release toggles, moving away
cancels — and the third failed on **all four**, including the eye and the disc
that had shipped that way for weeks. Claiming the press was never enough:
`QTreeView` selects on the MOVE as well, so pressing a glyph and sliding off it
left the row selected and, with a longer slide, started a drag out of the panel.
The panel now owns `mouseMoveEvent` while a toggle press is in flight.

**And the new mark: FOLLOW THE SKELETON'S POSE.** A marked Loaded NIF re-anchors
its skinned geometry to the skeleton's bones **by name, at render time**. No
merge, nothing written into the follower — its bytes are asserted identical before
and after — and unmarking puts it back on its own flat bone copies immediately.
The skeleton is the skull-marked document if there is one, otherwise the primary
when the primary has a real bone hierarchy; a follower with neither renders exactly
as it did unmarked.

The machinery was already there and half-exposed: `Scene::skeletonOverride` is
consumed per bone by name in `Shape::updateBoneTransforms`, and the skull mark
pushed it to EVERY other scene. That behaviour is untouched — `applyPoseFollowers`
takes precedence only while at least one row is marked, so with an empty follower
set not one transform is computed differently from before this existed.

**What proves it is not the pixels.** The harness loads `skeleton.nif` as the
primary and `Frame.nif` as a separate row, marks the frame, and imports a real SAM
pose into the skeleton. It then picks — BEFORE the pose — the follower vertices
that sit within 2 units of a skeleton bone, and after the pose measures the one
whose bone travelled furthest:

| | measured |
|---|---|
| probe bone `LArm_Finger33` moved | **76.166** |
| the follower vertex sitting on it moved | **76.274** |
| its grip on that bone was / is now | **0.54678 / 0.546777** |
| the follower's file, before vs after | **byte-identical** |
| after unmarking, worst vertex from rest | **0.0023** |

Geometry that merely moved would pass "the picture changed" and fail the grip.
Everything is read through the follower's own `Scene` (`Shape::skinVertex`), which
is the route to the screen and the one question the file cannot answer.

**The hard gate, because this touches the scene graph.** An effect document that
is NOT marked must render *identically* while a sibling follows a posed skeleton —
captured either side of the whole pose with animation forced off, and compared
pixel for pixel: **0 differ**. Plus `artobject_attach.sh` 14/14,
`carries_everything.sh` 24/24, `live_effects.sh` 15/15.

Suites: `loaded_nifs.sh` **112/112** (was 95), `weapon_mark.sh` **77/77** (was 62),
`sam_pose_import.sh` PASS, `workspace_skeleton_target.sh` 34/34. Captures:
`release/ww_loadednifs_toggles.png`, `release/ww_weaponmark_follow.png`.

## 2026-08-10h — Weapon parts assemble themselves, on the points the meshes ship

**What the mark could not do yet.** Yesterday's mark parented every weapon-marked
row on the `WEAPON` bone. That is right for the part that IS the gun and wrong for
everything that bolts onto it: a suppressor parented at the bone sits inside the
grip, and the summary could only say so. The entry below called the slot table "a
separate effort". It is not separate any more — it was never a table.

**FO4 ships the assembly graph in the meshes.** Every weapon NIF carries two
`NiExtraData` blocks: `BSConnectPoint::Children` lists the point the mesh plugs
into (`C-Muzzle`), and `BSConnectPoint::Parents` lists the points it offers
(`P-Muzzle`), each with a **node name inside that mesh** and a full local
transform — quaternion w-first, translation, scale. Placement is one line:

```
world(part root) = world(provider node) ∘ (translation, rotation, scale)
```

`nifMergeWeaponPart` applies it, resolving each marked donor against the target
**as it stands**, so parts land on each other in row order.

**The rotation is not optional.** It is identity on 369 of 386 vanilla connect
points, which is exactly why a translation-only assembler looks correct until it
meets a magazine: the 10mm's `P-Mag` is canted **26.52°** (pistol mags are), three
muzzle points are turned a full 180°, and most receivers rotate `P-Casing` 39–100°.
The harness measures the placed magazine at **26.5163°** off the node its point
rides — an implementation that dropped the quaternion scores 0 and fails there
alone.

**Each placed part gets a node of its own, and that is the load-bearing detail.**
A connect point whose `Parent` is EMPTY is expressed in its own mesh's ROOT frame
— `10mmLongBarrel.nif` writes `P-Muzzle` that way — and the merge has always
dropped donor roots as per-file wrappers. With the root gone that frame is not a
node, and a chain cannot be walked past one hop. So the merge now creates one
`NiNode` per placed part, carrying the connect-point transform, and hangs the
part's branches off it. The part's own extra data rides that node, so an
empty-`Parent` point resolves to it. Two hops then work with no special case:
receiver `P-Barrel` (y 18.515) places the long barrel, whose `P-Muzzle` (y 5.594)
places the suppressor at **y 24.108, z 4.850** — dead on the research's figure,
and on the bore.

**The muzzle does not always come from the barrel.** On the hunting rifle none of
the four barrels publishes `P-Muzzle`; the STOCK does, because that mesh is the
whole furniture and forend. Its silencer resolves across to the stock at
**y 30.5426** (research: 30.543). Anything that hard-coded "muzzles come from
barrels" assembles the 10mm perfectly and puts this one nowhere, so it is asserted.

**Still no policing.** Any part may go on any gun; the only requirement is that
something already placed publishes the point asked for. Slot names are matched
case-insensitively (vanilla ships `P-MeleeMod` and `p-MeleeMod`), a mesh's list of
required points means ANY of them (`CombatRifle.nif` declares `C-Receiver` *and*
the misspelled `C-Reciever`), and where two placed parts publish the same point the
last one wins. Nothing resolves → the part still goes on the `WEAPON` bone and the
summary names the point nothing publishes. Two cases stay silent because nothing
is missing: a mesh that declares no point at all is an assembly root, and a
receiver arriving at a target with no connect points has landed on a **rig**.

**A muzzle flash goes at the very end of the barrel.** Flash meshes are the one
part the name match cannot serve: `MiniGunMuzzeFlash.nif` declares **no connect
points at all** — neither `::Children` nor `::Parents` — so there is nothing to
resolve and the fallback would leave the fireball on the `WEAPON` bone, at the
shooter's fist. A marked donor that asks for nothing is instead placed on the
farthest-forward point the assembly ALREADY publishes, down a ladder that is the
barrel chain read backwards: `P-ProjectileNode` (what a muzzle device, a barrel,
or a bare receiver's baked-in barrel publishes — several is normal, farthest from
the gun's origin wins), else `P-Muzzle`, else the `P-Flash*` family, else
`P-Barrel`. Nothing is invented: with nothing published it still lands on the bone
and says so. Distance from the gun's origin rather than a Y coordinate, because
the target may be a posed rig pointing anywhere.

Measured, both shapes the chain takes in the corpus:

| assembly | rung taken | flash lands at (gun frame) |
|---|---|---|
| bare Minigun — no muzzle point, three flash points for three barrel lengths | **`P-FlashFar` from `Minigun001`** | y **135.33**, past the gun's own geometry at 18.56 |
| 10mm receiver + long barrel + suppressor | **`P-ProjectileNode` from `10mmSuppressor`** | y **71.91**, past the suppressor at 24.11 |

The second is a deliberate cross-weapon build — no 10mm flash mesh exists in this
unpack, only the Minigun's, the Broadsider's and the JunkJet's — and it works
because the mark polices nothing.

**What this cannot reach.** Only a marked donor takes the weapon branch; clothing,
skeletons, ArtObjects, loading screens, the CLI `merge` verb and the workspace rig
merge take the byte-identical `nifMergeData` call they always did. The weapon path
adds **one `NiNode`** and nothing else — no controller, particle, effect or shader
block is created, removed, renamed or re-linked by it. That is asserted rather
than claimed: a real animated effect (`MiniGunMuzzeFlash.nif`, 3 shader
controllers + 3 interpolators + 3 float tracks) is merged through the weapon path
and every one of those blocks is counted into the target one for one, with
`NiNode` moving 167 → 168.

**Harness.** `weapon_mark.sh` / `WW_WEAPONMARK_TEST` grew from 37 to **62 checks,
0 failures**, on 20 corpus files and three separate rigs (the 10mm and the hunting
rifle both carry a `WeaponMagazine`; one skeleton for both would hang the rifle's
magazine off the pistol's node). Every placement figure is asserted in the **gun's
own frame**, through the `WEAPON` bone's inverse, so the numbers are the research's
and not this build's. The suppressor band has a **control**: the same file merged
with no barrel in front of it, measured at gun-frame y 0, z 0 — outside the band it
must land in, which is what stops that band from being a number that was always
true. Two assertions from 08-10g were rewritten rather than kept, because they
encoded the behaviour this entry replaces: "all three parts attached to WEAPON" is
now "the base goes on the bone and the furniture goes on the base's own points",
and the missing-slot note no longer says placement is unavailable — it names the
point nothing publishes.

Regression: `sam_pose_import.sh` PASS, `workspace_skeleton_target.sh` 34/34,
`loaded_nifs.sh` 95/95, `artobject_attach.sh` 14/14, `carries_everything.sh` 24/24,
`live_effects.sh` 15/15. Capture: `release/ww_weaponmark_kit.png`.

## 2026-08-10g — Weapons are a row mark, not a programmer's operation

**What was missing.** The workspace could already put a gun in a posed hand —
`WW_SAMPOSE_TEST` does it — but only from code, by calling `nifMergeFile` with
an `attachTo` override. Nothing in the panel said "this row is part of a gun",
so the one thing a viewer actually wants out of a posed rig was unreachable.

**The mark.** A third Loaded NIFs row mark beside the skeleton skull and the
face donor: a small handgun drawn in code from `wwSkinColor("accent")` like the
other two, sharing their one reserved marker slot (skull wins, then face, then
gun), offered as **Use as Weapon Part** in both row menus. Unlike the other two
it is a **SET** — a Fallout 4 gun is a base NIF plus separate OMOD part files,
so several rows carry the gun at once — and it lives in `nifmerge` on the
`NifModel`, held by `QPointer`, because the merge is the only thing that reads
it and a closed document must take its mark with it.

**The merge.** In `mergeIntoLoadedDocument`, a marked donor is spliced with
`attachTo = "WEAPON"` when the target has a `NiNode` of that name at any depth,
resolved through the merge's *own* `namedNodes()` index so it cannot answer
"yes" where the splice would then find nothing. Multiple marked rows go on in
row order; unmarked donors are untouched. A target with no `WEAPON` bone is a
fallback, not a failure — merged at the root, and **said so in the summary**.

**"Connect automatically if the combination is right" = say what you notice.**
Any parts may be combined, cross-weapon included; there is no whitelist, no
notion of a legal combination, and nothing here refuses anything. Two notices,
both structural, both read off the files before the splice:

- **Redundancy** — shape names the donor brings that the target already carries.
  Measured fixture: two barrels on one minigun collide on `BaseRefractionMesh:0`
  (1 of the second barrel's 2 shapes).
- **Slot** — a part declares where it belongs in `BSConnectPoint::Children` as
  `C-<Slot>`, and a file that can hold it offers `P-<Slot>` in
  `BSConnectPoint::Parents`. When that point sits *on* the attach node the part
  self-assembles by mere parenting; when it sits out along the gun or is not
  offered, parenting leaves it at the grip, and the summary says it needs a slot
  node automatic placement does not provide yet. **It is not placed** — the slot
  mapping is a separate job (docs/TO_BE_IMPLEMENTED.md #9).

**Two rules that had to be measured, not assumed.**

1. *"Slot-relative parts hug their own origin"* is **not** a usable signal.
   `10mmGrip.nif` and `10mmSuppressor.nif` both put their shape at (0,0,0) with
   bounding centres inside 0.5 units, and the grip is exactly the part that must
   merge silently. Pivots and bounding centres cannot separate the two kinds;
   the declared connect point can.
2. *"An unoffered slot means the part is misplaced"* warns about the **base
   weapon**, which declares `C-Receiver` like every other part does. The harness
   caught the shipped-looking version announcing that `10MMPistol.nif` "needs a
   Receiver node" while it sat in the hand exactly as intended. A target with no
   connect points at all is a **rig**, not a half-built gun, and the first part
   onto its bone defines the frame — the notice starts only once the target
   carries connect points of its own.

Measured slot offsets from the attach node, which is where the 5-unit slack
comes from: 10mm `P-Grip` 2.4, `P-Mag` ~1.6 (silent, correct); 10mm `P-Scope`
10.4, minigun `P-Barrel` 36.5 (warned, correct).

**Correction to 08-10f.** That entry reads as though `MinigunBarrel.nif`
collides with the minigun base. It does not: measured, the base's five shape
names and the barrel's two are disjoint, and the barrel's four *blocks* arriving
is not a name collision. The real collision is between **two barrels**, on
`BaseRefractionMesh:0`, which is the fixture this harness uses.

**Harness.** New `tests/spells/weapon_mark.sh` + `WW_WEAPONMARK_TEST`, **37
checks, 0 failures**, on ten corpus files. It asserts the mark as *state* on the
model the merge keys on (not a repaint), the set behaviour, independence from
the skull and face marks in both directions, and the row menu item ticked with
its glyph; then merges base+grip+magazine onto a real PA skeleton and measures
all eight arrivals by **world transform walked up the Parent links** — nearest
0.0 and farthest 15.8 from `WEAPON`, closest to the document origin 81.4, against
`WW_SAMPOSE_TEST`'s own 40/80/40 thresholds. Both notices are asserted by their
text, the clean assembly is asserted to draw **neither**, the no-`WEAPON`-bone
fallback is asserted to say so, and one guard assertion holds that an unmarked
donor merges exactly as before. The summary is read back through
`nifLastMergeSummary()` because the box it appears in is modal and a driver has
to dismiss it to let the run continue.

Regression: `sam_pose_import.sh` PASS (all five pose+weapon pairs),
`workspace_skeleton_target.sh` 34/34, `loaded_nifs.sh` 95/95.

## 2026-08-10f — The weapon step generalises past the pistol

Five pose+weapon pairs from the corpus now run through the merge phase:
PA PISTOL3 + 10mm, MINIGUN POSE WOW1 + minigun, SWORD NEW1 + Chinese officer
sword, PLASMA RIFLE POSE1 + plasma rifle, 50CAL1 + hunting rifle — all PASS,
all captures inspected. Two pistol-sized assumptions fell: arrivals are now
counted by shape-count delta (a part NIF may legitimately reuse a shape name
its base also carries), and the one-piece pivot bound is 80 units (a minigun's
muzzle helpers pivot at 47; a root mis-attach still measures 114+). The
stricter accounting earned its keep immediately: it flagged MinigunBarrel.nif
as redundant — the minigun base NIF is already a complete gun, and its parts
REPLACE pieces rather than fill empty slots the way the 10mm's furniture does.

## 2026-08-10e — The attached pistol renders life-size

The 1.75× defended in the previous entry was wrong on screen — a pistol at
forearm length, caught by the user on sight, twice. The pose file's WEAPON
scale is the pose author's scene value, not a rendering law. The import still
writes the bone faithfully, but the harness weapon step now measures the
inherited scale against its own parse of the JSON (1.75001 vs 1.750013,
asserted), then RESETS the WEAPON bone to unit scale and asserts the
accumulated world scale comes back as 1.0 exactly, so the gun renders at its
authored size. Farthest part pivot fell 27.68 → 15.81 units, matching the
removed factor. The capture was inspected against the question "does this
look like a pistol in an oversized fist", not "did pixels move".

## 2026-08-10d — The posed hand holds an assembled gun, and every surprise in
the picture is now a number in the log

**What was missing.** Yesterday's phase 2b proves a SAM pose lands correctly on a
merged rig, bone by bone, world transform by world transform. It proves nothing
about the thing anyone actually poses a rig *for*: the prop in the hand. The
`WEAPON` bone is posed like every other bone and is a bare leaf under
`RArm_Hand`, so the check that mattered — does a real weapon file land in the
grip when attached to it — had never been run, in the harness or anywhere else.

**The step.** `WW_SAMPOSE_WEAPON` is a **semicolon-separated list** of NIFs,
merged in order onto the `WEAPON` node after the pose, through
`nifMergeFile(..., attachTo)` — the *same* entry point the CLI's
`merge --attach NODE --add FILE` uses, so the harness covers the shipped path
rather than a private one. The wrapper defaults to the corpus 10mm and skips any
part the corpus lacks, exactly as it already does for `Frame.nif`.

**A list, because `10MMPistol.nif` is not a gun.** It is the **receiver group**:
bolt, hammer, trigger, bolt release, magazine release and the receiver itself —
six shapes, with the stock barrel and iron sights baked into the receiver mesh.
Its `WeaponMagazine`, `WeaponOptics` and grip nodes are **empty attach points**;
the grip, magazine and sights are separate OMOD part NIFs sitting beside it. The
first version of this harness merged the base file alone and photographed a
pistol with no handle and no magazine, which the capture hid rather than
reported. The default list is now `10MMPistol.nif;10mmGrip.nif;10mmMag01.nif`.

**They self-assemble; nothing is forced.** All three are authored in the same gun
frame with identity roots, which was measured rather than assumed — local
bounding spheres, gun-frame coordinates:

| shape | centre | radius |
|---|---|---|
| `Pistol10mmReceiver:0` | (-0.02, 7.68, 3.05) | 11.50 |
| `10mmGrip:0` | (-0.00, 0.00, 0.00) | 6.29 |
| `Magazine:0` | (-0.03, 0.50, 0.43) | 7.94 |

The grip sits **on** the frame origin, which is the point of the whole exercise:
the origin of a weapon file *is* the grip, and that is what the skeleton's
`WEAPON` bone marks. So merging the parts under `WEAPON` with no transform work
puts them together, and the per-part numbers are +54 blocks / 6 shapes for the
base, +8 / 1 for the grip, +6 / 1 for the magazine.

**On the two `WEAPON`s.** A Fallout 4 weapon's own root node is *also* named
`WEAPON`, which looks like it must produce `WEAPON` nested under `WEAPON`. It
does not. `mergeDonor` builds its `donorTops` from the donor root's **children**
and never imports the root block, which it treats as a per-file wrapper. So each
part's contents simply become the skeleton `WEAPON` node's children, wearing the
local transforms the file authored against its own root. Nothing needed renaming
and no identity transform had to be invented. (One donor name,
`ProjectileNode`, is shared with the skeleton and de-duplicates onto it, rebased
so its world transform does not move: 14 nodes added, 1 reused.)

**The gun is 1.75× life size, and that is faithful.** `sam_pa_pose1.json` gives
the `WEAPON` bone `scale: 1.750013` — Screen Archer Menu recorded the game's
live power-armour weapon-bone scale, and an importer that reproduces it is doing
its job. It is also the kind of fact that has no business first appearing as a
surprise in a screenshot, so it is measured: the world scale **accumulated down
the parent chain** at `WEAPON` reads **1.750013**, asserted equal to the
harness's own parse of the JSON within 1e-4. Every ancestor is unit scale, so
the product is the pose entry itself.

**What is measured, and why it takes three distances.** Not the pixels. World
transforms walked up the `Parent` links:

* the **nearest** weapon shape to the `WEAPON` bone — `10mmGrip:0` at **0.0**;
* the **farthest** — `Pistol10mmRelease:0` at **27.68**, inside the 40 a ~30-unit
  pistol allows. These are shape *pivots*, not mesh extents, so the 1.75 scale
  already in the chain does not push them out. The nearest alone is nearly free:
  receiver, grip and magazine all have identity locals and sit on the grip by
  construction, scoring ~0 even if the rest of the gun flew off;
* the weapon shape **closest to the document origin** — `Pistol10mmHammer:0` at
  **114.71**, which has to exceed 40. This is the one that catches the classic
  failure: a branch that ignored the attach node and landed on the root sits at
  (0,0,0), and "near the `WEAPON` bone" would still hold for it if the rig were
  collapsed there too. Near-the-bone and far-from-the-origin only hold together
  for a gun in a raised hand.

Per-part shape counts are asserted against numbers read out of the corpus files
(6 / 1 / 1), and so is the assembled total, **8**. The `WEAPON` bone lands at
(16.40, 32.43, 111.58), 117.35 from the origin, and each merge is asserted to
report `attached to "WEAPON"`. Geometry count 3 → 11. The capture is
`release/ww_sampose_weapon.png`; `ww_sampose_before` and `ww_sampose_after` are
untouched, so the pose's own pixel delta still means what it meant.

## 2026-08-10c — The SAM harness was photographing a crumple

**The defect, plainly.** Yesterday's phase 2 applied a SAM pose to **Frame.nif**
and passed it. Frame.nif is not a skeleton. It is a skinned mesh that carries a
**flat** copy of the bone names — 55 NiNodes, **0** of them with a non-root
NiNode parent — and a SAM entry is an *absolute parent-space* transform. Applied
there, every bone lands in world space and the frame crumples. The harness's only
check on that phase was "the pose moved pixels", and a crumple moves pixels
beautifully; 25,505 of them, which got written up as a pass. The screenshots said
so at a glance and nobody in the loop was looking at them.

**The refusal.** `AnimSetup::applySamPose` now resolves its target bones before
writing anything and refuses a file whose matching nodes have no parent
hierarchy. The rule is not a new heuristic: `hasWorkspaceBoneHierarchy` — the
test the Loaded NIFs rig merge has used since the skeleton-target work to reject
a flat "skeleton" marker — moved to `AnimSetup::hasBoneHierarchy`, gained an
optional block-set filter so a caller can ask about *the bones it is about to
touch*, and nifskope.cpp's copy is now a one-line forward to it. One test, two
refusals. The message names the problem and the supported workflow (load
`CharacterAssets/skeleton.nif`, add the mesh under Loaded NIFs, mark and merge,
pose the merged document); the dock already surfaces it in a warning box.
Measured on Frame.nif: 54 pose bones matched, **0 applied**, **0 of 58**
transforms written, nothing pushed onto the undo stack.

**Phase 2 rebuilt around an invariant that can fail.** `sam_pose_import.sh` now
runs three processes. Phase 1 (the convention checks on skeleton.nif) is
unchanged. Phase 2a asserts the refusal above. Phase 2b runs the **supported**
path and is what the pictures come from: skeleton.nif primary (147 NiNodes, 144
parented), Frame.nif enrolled through `addWorkspaceDocumentFromFile` and spliced
on with `mergeIntoLoadedDocument` — the same call `mergeWorkspaceDocumentsInto`
makes once its skull policy has chosen a target — taking the document from 177 to
**193 blocks with 3 shapes**, and only then the pose.

What proves it is a pose is the **world transform**, not the pixels. For the
shallowest, a middle and the deepest posed bone, the transform composed by
walking the NIF's Parent links after the import is compared against one the
harness accumulates down the same chain from the JSON, using its own
`Rx(yaw)·Ry(pitch)·Rz(roll)` in doubles — never `Matrix::fromEuler`, never the
importer's arithmetic. Same file, two independent routes, tolerance **1e-3**
(corpus quantization). Measured on `sam_pa_pose1.json`:

| probe | depth | world rot diff | world trans diff | carried by its chain |
|---|---|---|---|---|
| AnimObjectA | 2 | 0 | 0 | 0 |
| PipboyBone | 9 | 1.5e-7 | 8.7e-6 | 113.711 |
| RArm_Finger53 | 12 | 1.8e-7 | 9.8e-6 | 115.247 |

plus all **89** posed bones checked locally the same way (worst 1.2e-7), and the
pixel delta kept as a secondary (**48,941**).

**The invariant was proved to fail.** A negative-control build in which the
expected side ignores the parent chain — the shape of the original defect — was
run against the same fixture: PipboyBone missed by **113.711** units and 1.60 of
rotation, RArm_Finger53 by **115.247** and 1.96, against a 1e-3 tolerance. Five
orders of margin. That control also exposed a weak probe: AnimObjectA scores
zero *either way*, because its ancestors are identity, so a "two routes agree"
check over it proves nothing. The harness now also measures how far each chain
actually **carried** its bone and fails if the deepest probe moved less than one
unit — agreement over a trivial chain is no longer allowed to count.

`tests/merge/workspace_skeleton_target.sh` re-run over the shared hierarchy test:
34/34.

## 2026-08-10b — SAM import photographed on real geometry

The SAM harness now has a second phase that renders the pose instead of only
measuring it. `sam_pose_import.sh` re-runs against the PA **Frame.nif** (a
skinned mesh carrying its own skeleton nodes), captures the framebuffer before
and after the real pose, saves `release/ww_sampose_before.png` /
`_after.png`, and — only in this phase (`WW_SAMPOSE_SHOT`) — **fails if the
pose moves no pixels**. Measured: pose 1 moves **25,505** sampled pixels and
reads as the prone "ARIES BOS PA1"; "POWER ARMOR HERO POSE3" moves 26,473 and
reads as a superhero landing. The captures were inspected.

Two harness fixes fell out of running on real geometry. The synthetic-fixture
bone picker now skips names needing JSON escaping — Frame.nif's root is a
backslashed path, which mangled the hand-written fixture and miscounted the
missing bones. And the real-pose `Back_Armor` tolerance is **2e-3**, not 1e-4:
half the corpus stores angles at two decimals (`%.02f`), worth ~1e-3 of matrix
error at `Back_Armor`'s pitch-90 gimbal point — measured 8.7e-4 on pose 70,
exactly the quantization the format study predicted. A wrong convention misses
by order 1, so the check still bites. The corpus pose used by the value checks
is now a committed fixture (`tests/fixtures/sam_pa_pose1.json`, a real SAM
save from zZovek's set) rather than a path into a temp scratchpad.

## 2026-08-10 — Screen Archer Menu poses import

The Pose Manager now loads a Screen Archer Menu pose (`.json`) as well as the
Outfit Studio `.xml` it already read. **Import SAM pose...** sits under the
existing import/export row and honours the same Blend strength.

The format was measured, not guessed. Each bone entry is `{yaw, pitch, roll, x,
y, z, scale}` with every value a JSON **string** and the angles in **degrees**,
and — unlike an OS pose — the values **replace** the NiNode's local transform
rather than offsetting it from rest. Across 80 real power-armour poses, 5504
(file, bone) pairs carry the skeleton's rest translation verbatim against 6 that
carry zero, which is what settles absolute-vs-delta. The rotation is
`Rx(yaw)·Ry(pitch)·Rz(roll)` — yaw turns about X, pitch about Y, roll about Z,
which is counter-intuitive and is exactly what a plausible guess gets wrong.
That candidate beat 575 others (6 axis assignments × 6 composition orders × 8
sign combinations × transpose) by 25 bones and 2.5 orders of magnitude, median
reconstruction error 7.7e-08, and it is element-for-element SAM's own
`MatrixFromEulerYPR` — and element-for-element NifSkope's `Matrix::fromEuler`,
so the importer converts nothing.

`AnimSetup::applySamPose` parses the whole file before touching the model, so a
malformed pose cannot leave a rig half-posed, and writes every matched bone
inside one `nifSnapshotOp` — one undo step for the whole pose. Bones the file
does not have are counted and skipped: 5 of the 80 sample poses omit the 17
armour-piece bones outright, so a partial match is normal rather than an error.
Blend below 100% interpolates from where each bone is **now** toward the pose
(translation and scale linearly, rotation by slerp), because an absolute pose
has no rest base to blend from. Scale-0 bones — SAM's "hide the weapon" trick —
are reported, so a vanished node is not a mystery.

**Import only.** Writing a SAM pose back out is not built; the dock's export
button is still Outfit Studio XML.

**Verified**: `tests/spells/sam_pose_import.sh` (`WW_SAMPOSE_TEST`) **PASSes,
0 failures**. It writes its own fixture pose and compares the resulting NiNode
against a rotation matrix worked out **by hand** and hard-coded in the test, so
a wrong axis mapping cannot quietly self-agree through `fromEuler` — max element
diff **5.96e-08**, and `fromEuler` matches that same hand-computed matrix to the
same figure. Translation and scale arrive verbatim out of the JSON strings, and
the test records that they **changed** from their pre-import values (rotation
1.0, translation 37.4, scale 1.5), so a do-nothing importer fails rather than
passes. One undo restores all three touched bones; a bone name the NIF does not
have is counted missing instead of being fatal; blend 0.5 lands on the midpoint
to **0.0**. Against the real corpus, `POWER ARMOR POSE (1).json` applies
**89/89 bones, 0 missing** to `powerarmor/CharacterAssets/skeleton.nif` and puts
`Back_Armor` on the orientation and translation recorded in the format study
(rotation diff **8.74e-08**, translation diff **0**).

The release build is green and `WW_OSPOSE_TEST` still passes (round-trip
rotation-matrix diff 2.68e-07). `WW_POSELIB_TEST` fails its final Delete step on
the Bloatfly fixture — **verified pre-existing**: a build with every change here
stashed produces a byte-identical log.

## 2026-08-09s — Refraction follows the normal map locally again

The X-01 torso VFX exposed two refraction defects. Its stock controller ramps
Refraction Strength to 1.0, while the preview interpreted that as 12% of the
whole viewport. The supplied BC5 smoke normal therefore displaced the source by
roughly 194 pixels at its 95th percentile on a 1920-wide view, sampling remote
empty background and producing the reported giant dark silhouette. Refraction
now remains normal-map driven but is capped at eight screen pixels, independent
of resolution. The map bends the framebuffer; it is deliberately not drawn as
colour, which would restore the old green/orange distortion-map artifact.

Loaded NIFs also now share one geometry pass: every document draws opaque
geometry before a single globally sorted transparent/refraction pass. A
refracting primary can therefore capture solid geometry belonging to a Loaded
NIF instead of copying the framebuffer before that document exists. Particles
remain last, procedural lightning remains owned and drained by its Scene, and
the single-document path is unchanged.

The release build is green. At X01_Torso_VFX's authored peak (`autoLoop`, 2.5
s), paired on/off captures prove the normal map is the distortion source and
the dark silhouette is gone. All six non-refraction deterministic render cases
are byte-identical to their pre-fix captures; only `refraction_fixed` changes.
The real workspace skeleton/merge suite remains **34/34**.

## 2026-08-09r — Simpler external-drop menu, explicit edit guard

The Explorer `.nif` drop menu now starts directly with its actions: the unused
top section/separator and the file icon are gone. The disposable-starter action
is simply **Open Here**; its tooltip still explains the adaptive behavior.

Replacement remains limited to an untitled sole starter whose window and undo
stack are both clean **and whose serialized bytes still exactly match the scene
created at startup**. This closes the direct-model mutation case that can change
data without creating an undo command. The harness now performs that exact real
model edit and proves the cube immediately becomes ineligible before restoring
the fixture. The release build is green; `external_nif_drop.sh` passes **17
checks, 0 failures**.

## 2026-08-09q — Operating-system NIF drops are workspace-aware

Dragging `.nif` files from Explorer into any part of a NifSkope window now
reaches that window, including the native OpenGL container and specialist tree
views that previously swallowed the external URL before the old GL handler
could see it. Internal Block List and NIF Browser MIME drags remain on their
existing routes.

Every external drop presents one explicit choice. **Open Here / Add to Loaded
NIFs** replaces only the untouched, untitled starter document; for a multi-file
drop it opens the first file there and enrolls the rest in Loaded NIFs. If the
primary is real, edited, or shares its workspace with anything else, the same
choice preserves it and enrolls every dropped file instead. **Open in New
Window** creates independent windows, and **Cancel** changes nothing. The menu
is queued until Windows releases the native drag loop, so opening it cannot nest
a second modal loop inside the drop gesture.

The release build is green. `external_nif_drop.sh` passes **15 checks, 0
failures** across the native receiver gates, exact file routing, adaptive
starter/multi-file behavior, Loaded-NIF preservation, independent windows and
Cancel. The full `loaded_nifs.sh` regression remains **95 checks, 0 failures**.

## 2026-08-09p — Skeleton-aware merges without requiring a skeleton

Loaded-NIF merging now treats the skull marker as optional intent, not a
universal prerequisite. With no marked skeleton in the merge selection,
clothing, props and other ordinary selections merge exactly as before: the row
that opened the menu is the target. A skeleton marked elsewhere in Loaded NIFs
does not interfere with that unrelated merge.

When the skull-marked model is itself selected, it is now an explicit rig merge:
the marked skeleton becomes the target regardless of which clothing row opened
the menu. Before anything changes, NifSkope verifies that it contains a real
NiNode-to-NiNode hierarchy below the file root. A frame or skinned clothing NIF
whose bone references are all flat root children is refused with an explanation
and the choice to load the game's `CharacterAssets/skeleton.nif` or unmark it for
an ordinary merge. This prevents a Power Armor frame from silently becoming the
target merely because its skull icon was lit.

The supplied `AegisTest.nif` was structurally valid — all 38 shapes, skin counts,
bone-index ranges and segments verified — but its first selected X-01 arm had
become the merge target while the marked PA frame was ignored. The new policy
addresses that exact route without breaking skeleton-free clothing merges.

The release build is green. The new real-corpus
`workspace_skeleton_target.sh` harness passes **34 checks, 0 failures** using two
X-01 arm meshes and the Fallout 4 Power Armor skeleton; it proves the no-marker,
marker-outside-selection, flat-marker refusal and hierarchical-skeleton target
paths. The full `loaded_nifs.sh` regression remains **95 checks, 0 failures**.

## 2026-08-09o — Make Primary reloads the scene, not NifSkope

**Loaded NIFs → Make Primary / Edit** no longer constructs a second complete
`NifSkope` main window, restores its docks and toolbars, shows it, and hides the
window the user was working in. Data-only rows now use the same in-place swap as
the direct edit gesture: their live bytes are parsed into the existing primary
model, so only that model and its viewport Scene are rebuilt. The window,
viewport, left dock, mode stack, searches, Loaded-NIF model and splitter geometry
remain the same objects with the same state.

The outgoing primary still takes the promoted row's place in Loaded NIFs, and
unsaved in-memory bytes still move without touching disk. Skeleton and face-donor
marks now follow the document across the swap rather than disappearing with the
deleted background storage object or silently attaching to the wrong mesh.

The release build is green. `loaded_nifs.sh` passes **95 checks, 0 failures** and
now drives the exact Make Primary implementation while asserting that no main
window is created, the primary `NifModel` and every relevant UI object retain
identity, geometry remains stable, and both rigging roles follow the promoted
mesh.

## 2026-08-09n — Restore a useful left-panel width once

The unified left editor no longer inherits the accidental ~260 px width that
Qt recorded while the four legacy docks were first replaced. Left-column layout
schema 2 performs one width migration to **400 px**, after both `restoreState()`
and `restoreGeometry()` have finished so neither can compress it again. A normal
close records schema 2; every later launch respects the user’s chosen width,
including deliberately narrow ones. Fresh profiles take the same path.

The real schema-1 profile opened at exactly **400 px** in the release build, and
the dock still traverses **164 → 432 px** when explicitly folded and expanded.
The rendered 400 px capture was inspected. The left-editor integration remains
green at **93 checks, 0 failures**. The maximized-state round-trip now expects
schema 2; its embedded PowerShell parses cleanly.

## 2026-08-09m — Seamless full-width editor mode selector

The left editor’s **Blocks · Header · NIFs** selector now uses the same
selection-blue segmented treatment as Collision Creation / Simulation. Its
three modes divide the full dock width equally; no shortcuts or mode behavior
changed. Both controls now share one skin-backed style whose adjoining edges
are square and carry only one border, while rounding is reserved for the two
outside ends. This removes the paired bevels and visible notch at the internal
seam.

The release build is green. `loaded_nifs.sh` passes **93 checks, 0 failures**,
including equal segment geometry and active-fill pixels. `collision_panel.sh`
passes **41 checks, 0 failures**, including adjoining geometry and a pixel at
the selected segment’s formerly rounded internal corner. Both rendered captures
were visually inspected.

## 2026-08-09l — Window-state harness cleanup cannot silently lose settings

`tests/spells/window_state_roundtrip.sh` no longer hides `reg.exe` failures
behind `cmd /c` and then deletes the only settings backup. Cleanup now waits
for direct `reg.exe` processes, checks both exit codes, and retains/reports the
`.reg` snapshot on any failure. This closes the path that left the startup cube
disabled and save confirmations suppressed after a failed diagnostic.

## 2026-08-09k — Loaded-NIF workspace status and identity-safe filtering

The lower NIF workspace now reports what it actually contains. Its existing
header reads **Loaded NIF · 1**, **Loaded NIFs · 2**, or, while searching,
**Loaded NIFs · 1 of 2**. The header tooltip is a compact key for the primary,
skeleton, face-donor, visibility, transparency and unsaved markers already
painted on each row. Row tooltips now add the source path and explicitly call
out unsaved in-memory work without adding another column or widening the pane.

An empty workspace explains that NIFs can be dragged in or added from the
right-click menu. A search with no result names its query in the viewport rather
than leaving an unexplained blank table. Both messages are presentation-only
paint: filtering still uses direct row hiding, so persistent indexes, drag
payloads, selection and exact document/model identity never remap. The visible
splitter handle is 4 px, non-collapsible, described to tooltip/accessibility
readers, and its hint is reapplied after saved-state restoration because Qt can
recreate the live handle during that replay.

The integration harness now proves browser Refresh preserves every exact Loaded
document name/model pointer, a filtered row remains the same persistent row and
background document through match/no-match/clear, shown/total counts are live,
both splitter panes remain reachable, and the populated/no-result states render.
The staged release build is green: `loaded_nifs.sh` is **91 checks, 0 failures**,
the dock harness is **13 checks, 0 failures** with the core **164 → 432 px** range,
and `archive_browse_survives_load.sh` is **4 checks, 0 failures**.

## 2026-08-09j — Searchable standalone Header inspector

Header mode now identifies the live document before presenting its technical
tree: a compact two-line strip shows the source basename and the NIF, User and
Bethesda versions, with the full source and metadata in its tooltip. Unsaved
documents receive a marker, untitled/archive-only documents are distinguished,
and the presentation refreshes across loads and undo-stack clean-state changes.

One full-width **Search header…** row filters field names, displayed values and
types recursively. Matching children keep their ancestor path visible, matching
compounds retain their subtree, and a no-result query names itself in the empty
viewport without replacing the model or Header root. A 100 ms debounce keeps
large Strings tables from being walked once per keystroke. The single overflow
menu copies either a concise Header summary or the source path when a disk path
exists; the useful Type column stays visible and no selector shortcut was added.

Saved Header widths remain intact, but the obsolete section floor is discarded
after restoration so this page folds with the unified left column. The clean
release build is green; `loaded_nifs.sh` is **83 checks, 0 failures** and the dock
harness is **13 checks, 0 failures**, including a measured **164 → 432 px** core
dock range and rendered overview/no-result states.

## 2026-08-09i — Compact Block Details with explicit empty states

Block Details now uses one full-width **Search fields…** row with only two
permanent tools: **★ pinned fields only** and an overflow menu. **Expand All**,
**Collapse All**, and the optional **Type Column** live in that menu instead of
compressing the search box into the left corner. The pin tooltip reports the
resolved pin count for the selected block type, and every icon-only control has
a stable accessible name.

The lower editor now distinguishes three intentional empty states. A Header
index is no longer mistaken for a block and cannot populate Details with header
fields; no selection says to select a block; a field query with no result names
the query; and pinned-only explains when the current type has no pinned fields.
These messages are passive viewport paint, so they insert no fake model rows and
cannot intercept selection, editing, link glyphs, or drag events.

Saved column widths and the Type-column preference remain intact, but the old
section floor is discarded after restore so Block Details folds with the unified
left column. Editing delegates, link navigation/retargeting, sticky expansion,
pin persistence and diff/reference behavior are unchanged. The staged release
build is green and `loaded_nifs.sh` is **74 checks, 0 failures**, with dedicated
selected, no-selection, and no-match renders.

## 2026-08-09h — Readable Block List body and navigable context

The Block List now labels its three columns for what that view actually shows:
**Block**, **Name**, and **Summary**. The shared NIF model remains unchanged, so
Block Details still correctly uses Name/Value terminology. A view-only header
paints the Block List labels in both hierarchy and flat modes, Summary stretches
into spare width, and the obsolete 100 px saved section floor is discarded after
old column widths restore so the panel can still fold narrow.

The selection path is live navigation now. Ancestors are clickable, the current
block uses the skin accent, and the full plain path remains available to tooltip
and accessibility readers. The totals footer uses correct singular grammar and
reports filtered state as, for example, **0/4 blocks shown**. Geometry totals are
cached across selection changes and invalidated by model changes instead of
rescanning the entire NIF on every click.

A search or category with no results no longer leaves a silent blank panel. The
view explains that no blocks match and points directly to **Filters → Reset Search
and Filters**, without inserting proxy rows or changing drag/selection identity.
The clean release build is green; `loaded_nifs.sh` is **66 checks, 0 failures**,
including rendered selected and no-result states.

## 2026-08-09g — Compact Block List tools and real advanced search

The Block List's three rows of competing controls are one compact toolbar now.
Back/forward, search, advanced filters, pinned blocks, reference counts and an
overflow menu remain visible; **Go to Block**, **Expand All** and **Collapse All**
move into that overflow instead of consuming permanent width. Every icon-only
control has a tooltip and accessible name, and the row still folds with the left
column rather than imposing a new width floor.

The **Filters** dropdown is a real advanced search container. It selects the
fields searched (block number, type, name, and displayed value/summary columns),
chooses all-term or any-term matching, retains the existing eight block-category
filters, and resets the complete search state in one action. The rendered menu
uses visible group headings; none of its options are decorative.

The integration harness proves that all-term and any-term searches produce
different results across the starter document, disabling the Type scope removes
a type-only match, Reset restores every default, and the complete menu renders.
`loaded_nifs.sh` is **60 checks, 0 failures**.

## 2026-08-09f — Flat, explicit left-mode selector

The **Blocks · Header · NIFs** selector is visually part of the editor column
instead of looking like another dock-tab group: its background is flat, hover is
subtle, and the active mode is identified by the skin's orange underline. Each
mode has a tooltip, but deliberately no keyboard shortcut—the selector does not
claim application-wide key combinations from editing tools.

## 2026-08-09e — Left selector order: Blocks, Header, NIFs

The three buttons now read **Blocks · Header · NIFs**. Their visual positions
are deliberately separate from the persisted mode values: each tab carries its
stable mode ID, `setLeftColumnMode` resolves that ID back to the current visual
position, and a click resolves the tab data back to the stable stack page. An
existing saved NIF mode therefore still reopens NIFs after its button moves from
second to third.

The integration harness clicks all three reordered buttons and checks the named
page and stable mode after every click. It is **54 checks, 0 failures**, and the
rendered screenshot visibly confirms the requested order.

## 2026-08-09d — Inactive Header page stays invisible

The Blocks screenshot contained a thin vertical string of characters along the
right divider. They were not damaged table columns: they were the inactive
Header page's Type column painting through. The migration code cleared the old
dock contents' hidden flags after those widgets had already entered the new
`QStackedWidget`, overriding the stack's own hidden state.

That visibility reset now happens before the widgets are inserted. Once the
stack owns them, only `setCurrentIndex` controls visibility. The rendered Blocks,
NIFs and Header screenshots are clean on a second visual review. The harness
also requires exactly one page to be visible in every mode, so a correct current
index can no longer hide this class of overlap. `loaded_nifs.sh` is now **53
checks, 0 failures** and the dock topology/folding harness remains **13/13**.

## 2026-08-09c — Three left-editor modes, built as one stable column

The old Block List / Header / NIF Browser dock-tab strip is replaced by one
selector at the very top of the left column: **Blocks**, **NIFs**, **Header**.
Blocks owns a vertical Block List / Block Details splitter; NIFs owns a vertical
NIF Browser / Loaded NIFs splitter; Header fills the column by itself. This is
the requested layout rather than the earlier compromise that left Loaded NIFs
inside the Browser's old dock.

The implementation is structural. Before saved dock state is restored, the
four legacy dock shells surrender their content widgets and are deleted. One
permanent `LeftColumnDock` then owns three fixed `QStackedWidget` pages for the
window's lifetime. Switching modes never reparents a live view, replaces a
model, reloads a NIF, or rebuilds Loaded NIFs. Repeated Blocks → Header → NIFs
cycles preserve the exact Loaded-NIF row count and model pointers, including
unsaved in-memory documents.

Window-state version is now `0x074`. Existing `0x073` state is replayed once to
retain unrelated manager docks and toolbars; the new column then saves its mode
and both splitter states explicitly under `UI/LeftColumn`. A two-process,
maximised second-monitor round trip verifies the migrated graph starts without
the old delayed Qt layout crash and restores NIF mode correctly.

The clean release build is green. `loaded_nifs.sh` is **52 checks, 0 failures**;
the dock topology/folding harness is **13/13**; `collision_panel.sh` is **39/39**;
and `window_state_roundtrip.sh` passes both launches. All three modes are also
rendered to screenshots and visually checked. The pointer-seizing live drag
script was not run.

## 2026-08-09b — Loaded-NIF controls and docks that fold again

Clicking the eye or transparency disc in **Loaded NIFs** no longer selects the
row first. The view now consumes the complete press/release gesture over those
two glyphs, preserving both the current index and the existing selection. This
also prevents an icon click from accidentally becoming a row drag.

Loaded NIFs has a compact **Search loaded NIFs…** field and an as-needed vertical
scrollbar. The filter hides rows directly instead of proxying them, so every
drag payload and context action retains the exact source-model identity. The
test adds 40 temporary rows and proves the scrollbar has a real range rather
than merely checking its policy.

The row menu is grouped into file, rigging, workspace-display, tool/revert and
removal sections. **Use as Skeleton** carries the same skull shown on the marked
row; **Use as Face Donor** carries the face icon. Labels are shorter and
consistent, and a data-only row no longer says both “Remove from Loaded NIFs”
and “Close Document” for the same operation.

The NIF Browser's almost-fixed divider was not a splitter bug: the tabified
List and Tree docks each imposed a **400×240 minimum** on the whole left dock
group. Those dock-level floors are removed. In the live harness the left column
now travels from **164 to 432 px** while the 3D viewport retains its 50 px
minimum. A systematic manager-dock audit also removed UV's redundant 340 px
dock floor and made Collision, Rigging and Vertex Paint horizontally scrollable
when folded. Genuine content minima remain for views that cannot render usefully
below them, notably the 260 px UV view and 200 px timeline graph/lane.

Moving Loaded NIFs into the Block Details tab space was tested as a separate
dock, a nested tab widget, and a stacked designer layout. All three cross-panel
reparent variants produced the same delayed startup SIGSEGV in Qt widget
layout/style handling. The stable lower pane inside the NIF Browser therefore
ships unchanged in ownership, with its new search and scrollbar; relocation is
left open until it can be constructed safely before dock-state restoration.

Release build green. `loaded_nifs.sh` is **48 checks, 0 failures**; the expanded
dock harness is **9/9**; `collision_panel.sh` remains **38/38**; and
`window_state_roundtrip.sh` passes two maximised save/restore cycles on the
second monitor. The pointer-seizing live drag script was not run.

## 2026-08-09a — NIF Browser drag, favorites, compact header, and no silent resets

The NIF Browser and **Loaded NIFs** are a two-way drag surface now. Drag an
available `.nif` leaf into Loaded NIFs to add that exact file; folders cannot
start the gesture. Drag exactly one Loaded NIF back onto the browser and it
opens **Save As…** for that row's live in-memory model. The drop is copy/save
semantics: cancelling or failing the write changes nothing, and a successful
save keeps the row, gives it the chosen path, and clears its unsaved mark.

The browser's two text-heavy rows are one Block-List-style header now: a
stretching search field, **★ favorites only**, one **Sources** menu (Archives,
Loose NIFs, Search filenames only), **Load Selected**, and **Refresh**. All four
tools fit at the dock's tested 400 px width and carry tooltips/accessibility
names. The real Qt item-view subclasses own both native drag directions and
freeze the rows at drag start, so changing selection during a gesture cannot
silently load or save a different NIF.

Any available `.nif` can be starred from its context menu. Favorites are stable
source identities — configured game + virtual path, loose absolute path, or
explicit archive + member — rather than display names or model indices, so a
Refresh/rebuild restores the stars and temporarily unavailable resources can
return. The ★ filter composes with text search as AND and retains only the
folders leading to matching files.

Favorites are user-authored library content, not registry preferences. They are
written atomically as **`<NifSkope Library>/NIF Browser/Favorites.json`**. Changing
Settings → General → NifSkope Library immediately rebinds an open browser to the
new root. The same persistence audit found the only two other reusable authored
payloads still buried in `QSettings`: Collision Manager custom body presets and
custom material-name/CRC aliases. They now live at
**`<Library>/Collision/Presets.json`** and **`CustomMaterials.json`**, with a
one-time import of the old settings values. Selected tools, paths, layouts,
filters and other genuine preferences stay in `QSettings`; Pose files were
already correctly stored under `<Library>/Poses`.

The “Loaded NIFs emptied/reloaded” report exposed several independent ownership
and discard bugs, fixed as one safety boundary:

- every loaded row, tab, preview, duplicate check and positional API is scoped
  to its actual workspace root; two independent windows may load the same path
  without sharing or stealing the same background document;
- rows added/generated after promoting a child window still belong to that
  workspace root and close with it;
- Make Primary/Edit always carries the row's live model in memory, including
  configured archive rows, instead of reloading the archive and dropping edits;
- Remove/Close/X/Delete and Revert warn before discarding an unsaved data-only
  row, with Cancel leaving a multi-selection completely intact;
- Reload now uses the normal Save/Discard/Cancel warning, and choosing Save but
  cancelling/failing Save As cancels Reload/close/open instead of pretending the
  save succeeded;
- cancelling a workspace close no longer leaves sibling windows flagged to
  bypass their next close warning;
- Pose Manager's browser picker no longer destroys an explicitly browsed
  archive/folder tree, configured/archive open now reports actual load failure,
  and a loose-only browsed Data folder no longer indexes an empty archive-name
  list when it labels the tree.

`tests/spells/loaded_nifs.sh` builds a three-file `Data/meshes` fixture and drives
the real browser/load/save/reload paths: compact geometry and icons, both drag
gates, exact payload, folder rejection, cross-workspace ownership, favorite JSON
location/star/filter/rebuild, reverse Save As retention, Cancel on destructive
remove and Reload, and preservation across a real primary reload. **35 checks,
0 failures.** `tests/spells/collision_panel.sh` adds atomic Library round-trips
for both Collision JSON files and remains **38 checks, 0 failures**. Adjacent
regressions remain green: explicit archive browse **4/4** and faceBones
**23 passed, 0 failed**. The pointer-seizing live drag scripts were not run.

## 2026-08-08l — Create faceBones NIF hands you a loaded NIF, not a file

It asked for a path and wrote there, and the Loaded NIFs route had to hand it a
**temporary to build into** and read the written file back. So a finished mesh
could die at the last step on a filesystem, which is what happened:

> Could not write "C:\Users\bungo\AppData\Local\Temp\nifskope-facebones-yhpAji.nif".

The transfer had already run. The mesh existed. There was simply nowhere to put
it, and no second chance at it.

A generated file has no natural home anyway — the mesh is what was asked for, and
where it belongs is a decision for whoever saves it. So the spell now puts the
result **in Loaded NIFs, unsaved**, carrying the name it would take
(`BaseFemaleHead_faceBones.nif`), and you save it wherever you want it from the
row's own **Save As…**. No save dialog on the way in, no temporary, nothing
written to disk unless you ask for it.

Both routes are now the same route: the spell places its own result, and
`generateFaceBonesInto` — the right-click-a-row path — only renames what appeared,
because it knows the row's path and the spell only knows the model's. The one-shot
output-path override and the `WW_TEST_FACEBONES_OUT` env var it existed to serve
are gone with it.

`addWorkspaceDocumentFromMemory` is the new entry point, and it **parses** the
bytes rather than trusting them: a generator hands over what it believes it
produced, and a row that cannot be read back is worse than a refusal because it
looks like a result. That is the same guarantee the temporary file was there to
give, without a filesystem in the way. It lands `unsavedInMemory`, because
`isModified` compares against the bytes loaded from disk and there are none — so
without that flag the close path would call it unmodified and drop the only copy
without asking.

`tests/rigging/facebones.sh` drives the real two-step user action now — cast, then
Save As on the row it made — and checks the row itself before writing it: **B0** it
arrived, **B1** it is unsaved, **B2** saving writes the file, **B3** it stops being
unsaved once written. Everything downstream reads that file exactly as before.
**23 passed, 0 failed** on `BaseFemaleHead.nif`.

## 2026-08-08k — No Search row in the block-list menu

It was the first thing in it, and the menu is not where you look for search:
**Space** opens the palette from anywhere, and so does Ctrl+Shift+P. A row
duplicating a key you already have was costing a separator and the top of the
list — the most valuable place in a menu that long. Copy Branch and Delete are
what belong there.

The palette still keeps a row marked `WW_PALETTE_SEARCH_ROW` out of its own
results; that rule is tested on its own and outlives this particular row.

### The faceBones harness photographs every step, and quotes every dialog whole

It answers each dialog the spell puts up, which is what lets it run unattended —
and was also how a warning could go unread. The log line was one sentence cut at
240 characters; `informativeText` and `detailedText` were never written down, and
nothing recorded whether the icon was a question or a warning. A run could report
14 of 14 with a warning clicked through in the middle of it and leave no trace.

Now: a PNG of each dialog before its button is pressed, the full text, the
severity, and a check that **no warning appeared beyond the one the test provokes
on purpose** (the refusal with no face donor marked — armed around that one cast,
so the check is not right for the wrong reason every run).

Three stage pictures too — the source, the source after the spell, and the
written file opened in the window. The viewport is **composited in**:
`QWidget::grab()` walks the widget tree and GLView is a `QOpenGLWindow` behind a
window container, so its surface is simply absent and the middle of the picture
comes out black. The first run looked exactly like a mesh that had failed to
render.

The full text also surfaced what the truncation had been hiding: transferring a
head's face bones onto a hat scores **Geometry match: Poor** — surface snap
median 15.2%, 95th 33.5%, max 37.6% of donor span. Expected for a hat sitting
above the skull rather than on it, and worth seeing rather than skipping.

### faceBones and RemapData, re-run on real files

`fhat.nif` given face bones from `BaseFemaleHead_faceBones.nif` — 313 vertices,
4 skin bones before and 68 after, 59 of them face-sculpt, 3756 bytes of
CustomizationRemapData. 19 of 19, including checks **Q/R** — every
`NiBinaryExtraData` in both files, named and sized:

```
output NiBinaryExtraData [8]  'CustomizationRemapData'          3756 bytes
donor  NiBinaryExtraData [74] 'CustomizationRemapNewBonesData'   208 bytes
donor  NiBinaryExtraData [75] 'CustomizationRemapData'         20268 bytes
```

The vanilla donor needs the NewBonesData because its remap blob overruns its own
bone list (highest index 68, list of 68); the generated file appends rather than
replaces, so every index resolves and that block is absent. Stated as a pair, so
neither half can pass by being true of any file at all.

And check **P**: the blob names
`0:HEAD, 1:Head_skin, 2:Neck1_skin, 3:Neck_skin` and **no `skin_bone_*` at all**,
while the shape's live skin binds 68 bones of which 59 are sculpt. That is the
whole point of the format — the live skinning is the customization rig, the blob
is how the same vertices map back to the *animation* skeleton — and G and H say
it in byte comparisons where this says it in bone names, which is the language
the question actually gets asked in.

* the open document is byte-identical afterwards, and the sibling file reloads
* RemapData is the **source's standard-skeleton** skinning, not the sculpt
  weights the output itself now carries — and provably not, since the two are
  required to disagree
* wiping the blob and regenerating it from the marked donor gives back all 3756
  bytes with **0 differing**, and without a marked donor it refuses and leaves
  the blob alone
* every remap index in the output resolves against its bone list, so no
  NewBonesData is needed — whereas the vanilla donor's blob does overrun its own

The rest-pose difference on `Chest` (91.28 apart) is the question, answered once,
that used to be a flat refusal.

## 2026-08-08j — The drag card stops changing shape while you aim

Hold the pointer still over a row during a block drag and the card dropped its
verdict line — "Move into Scene Root [0] (Ctrl to link, Shift to keep the local
transform)" — shrank to just what was being carried, and got the line back the
moment your hand twitched.

The card retracts a hint it thinks is stale, and it was calling a hint stale when
no drag event had arrived for 250 ms. **A drag event only arrives when the mouse
moves.** Hold it still and nothing arrives, though the verdict is exactly as true
as when it was rendered — so aiming, which is precisely when the card must be
still, was the one thing that made it flicker.

Stale now means the pointer has **moved away from what the verdict was about**:
events stopped *and* the cursor is somewhere else, which is the case the
retraction was written for. When it does fire it says so in the drag log, with
the elapsed time and how far the pointer went — the ticker is the one part of a
drag no harness can drive, so what it decides has to be readable afterwards from
a real one.

## 2026-08-08i — Block Details fills in again

Selecting a block showed its column headers and nothing under them — 37 rows in
the model, none of them painted.

Blanking the panel when nothing is selected works by handing the tree a filter
with an **empty keep set**, which hides everything there is. That filter was
lifted again only when the tree's model changed, on the assumption that the two
always move together. They do not: `swapModels()` puts the tree back on the real
model during a load without knowing a filter was left on it. So a window that had
no selection and then received a document — the starter cube being exactly that —
came up with the filter still armed, and every block you clicked showed nothing.

Typing anything into the details filter box and clearing it again was the
workaround, which is also the clue: the search box owns the same filter.

It is tracked by its own flag now, so selecting a block lifts **that** filter and
leaves the search box's and the pinned-only toggle's alone.

`starter_reload.sh` grows to 15 checks: rows are painted at all, the search box
narrows them, it keeps narrowing them across a block switch (the keep set is
per block), and clearing it gives them all back. Counted by what the view **lays
out** — a hidden row has an empty `visualRect` — not by `isRowHidden( r, parent )`,
which ignores `r` and answers for the index handed to it. The obvious call asks
"is the block itself hidden" once per row; it agreed with this defect by luck,
which is how a proxy metric earns trust it has not got.

## 2026-08-08h — Reload gives the starter cube back

An untitled window **is** the starter cube — that is what NifSkope opens on — and
reloading one went looking for a file whose name is the empty string. `QFileInfo("")`
resolves to the working directory, the load fails on it, and you were left with a
blank window: Reload was the one command in the program that could destroy the
starter scene without touching anything on disk.

Reload means "throw away my edits and give me the document again", and for a
document that was never saved, the document again is the cube. Same build, same
undo-stack reset, same reframe as startup — a reloaded starter is
indistinguishable from a freshly opened window, including not asking about
unsaved changes when you close it.

With the cube switched off, an untitled document is genuinely empty, and reload
gives back an empty document rather than a failed load of the working directory.

New `tests/spells/starter_reload.sh` — 11 checks over the whole starter path:
that it builds, that it is the scene it should be rather than merely some blocks,
that an untouched window is clean, that it renders, and that reload restores it.
It **edits the document first**, because a reload that did nothing at all would
otherwise pass — the blank window came from the load failing, so the count has to
be asked against one that was deliberately made wrong. Disabling the fix fails
exactly two of the eleven.

## 2026-08-08g — X asks, Delete doesn't

Blender's split, which we had collapsed into one key. **X** is the careful one —
the verts/edges/faces menu in edit mode, `Delete selected objects?` in object
mode — and **Delete** does the obvious thing straight away. Both are one
keystroke, both are one undo step, so this is not careless versus careful; it is
which one you want under your hand.

In edit mode the obvious thing is whatever the pick mode is **for**: in face mode
Delete removes faces. Asking which of vertices/edges/faces you meant when the
mode already says so is exactly the question worth skipping.

The **Block List** splits them the same way, because it is the same gesture on
the same blocks and only differs by which half of the window you are looking at.

Menus still ask, on both surfaces. A menu item that looks like every other menu
item should not be the one that skips the question — the Delete **key** is the
thing you press deliberately.

`Delete Without Asking` is a registered shortcut now rather than a hardcoded
alternate, so it sits on the Shortcuts page next to `Delete Menu` and can be
rebound. Two keys doing deliberately different things is only useful if both are
visible.

## 2026-08-08f — The confirmation is Blender's size, and selected means blue

### The delete popup, measured against the thing it copies

Blender's "Delete selected objects?" is **192x56** with two small buttons under a
line of text. Ours was a dialog wearing a popup's clothes. It is **188x63** now:
the layout margins came down to 6/5, the button row's own margins — a second set
inside the first, and most of the height — to zero, and the buttons to a single
pixel of vertical padding.

The default button was **orange text on blue**. `selTextActive` is orange because
it is the colour of text on a selected Block List row, where orange-on-blue is
how the primary of a multi-selection announces itself; on a button it just reads
as a second warning. It is `textBright` now — white, like Blender's.

**And it lands under the pointer again.** `show()` is not the end of the layout:
the stylesheet is polished and QMessageBox re-runs its own sizing on the way to
the first paint, both after `show()` returns, so the button was measured before
it was the width it would be drawn at. The pointer was landing on Delete's left
**edge** — 46 px out, half a button. The placement now runs again from inside
`exec()`'s event loop, where the geometry is the one that gets painted.

### Selected halves of a switch are the selection colour

The **Collision Creation / Collision Simulation** switch, and the shape buttons in
the Create popups, marked the checked half with the amber plate and orange text —
the same orange this skin spends on invalid material paths and missing textures,
worn by a tab that only means "you are here". Checked is `selBgActive` with
`textBright` on it: the blue the Block List selects a row with.

### What now measures it

`block_dragdrop.sh` opens the real delete confirmation, records its size and
where the default button lands **relative to the pointer as it already is** —
placing the mouse would make the check tidier and take the mouse out of your hand
— and photographs it. It cancels, so nothing is deleted, and it says so rather
than asserting placement when the popup had to clamp to a screen edge.

`collision_panel.sh` samples the **pixels** of the checked half of the switch and
compares them to `selBgActive`. A stylesheet that fails to apply leaves a widget
looking exactly like one nobody styled — which is how this switch lost its fill
once already — and nothing you can ask the button would notice.

## 2026-08-08e — Loaded NIFs becomes a place you can work

Four things, and together they make the panel a workspace rather than a list of
things already open.

**Add NIF to Loaded NIFs…** — load anything from anywhere on disk, multi-select,
without opening it as a document window first. On the row menu and on the empty
panel, because an empty panel is exactly when you need it and there is no row to
right-click.

**Save As…** on a loaded NIF, which had nowhere to belong while these were only
read-only copies of files already on disk.

**Generate faceBones NIF from this** — mark the vanilla head's `_faceBones.nif`
as the face donor once, right-click a base head in the list, and the result
appears beside them. Neither file has to be opened as a document. It lands
**unsaved**, carrying the name it would take: writing generated files next to
their source without being asked is how mod folders fill up with things nobody
chose to create.

**Double-click** opens only what has no window yet. A row that is already a
document is switched to, and a data-only row whose file is already open as a
window switches to that window rather than promoting a second copy of it.

### And the rest-pose refusal is a question now

`Import Donor Bone Nodes` refused the entire import when a node existing in both
files — `Chest`, typically — sat differently relative to the skeleton root. That
is the right default, since the imported bones will follow the target's pose
rather than the donor's, and it was also the only option: a headgear mesh whose
`Chest` sits a fraction from the head's could not be given face bones at all.

It now says **how far apart they are** and offers to anchor to your file's node
and carry on, which is normally what "put the face bones on THIS mesh" means. The
answer is remembered per node name, so 59 face bones sharing four ancestors ask
four questions rather than fifty-nine.

## 2026-08-08d — Decimate is a live operator, and window_state_roundtrip runs again

### Decimate, with the ratio on a redo panel

`Ctrl+Shift+D`, or the object menu beside Join. It halves the selection to start
with and **arms the redo panel**, so the ratio is found by dragging with the
operation re-running underneath, the way Merge by Distance already worked.
Decimation was the one operation that made you commit to a number in a modal
before seeing what it did to the mesh — backwards, for a value whose only honest
definition is "however much still looks like it".

The simplification itself is the **Simplify dialog's own**, driven headlessly:
one decimation in the program rather than two that drift apart.

Two of its persisted settings are deliberately overridden on the live path.
**Min Triangles** and **Max Error** each pin the triangle count no matter what
ratio is asked for — sensible guards for a modal a human types into once, and
nonsense for a field being scrubbed. Measured before that was understood: a
224-triangle sphere asked for half came back with 224, twice.

The check makes its own **UV sphere**, because the fixture's 12-triangle cube has
nothing redundant in it and a simplifier is right to refuse it — which looked
exactly like a dead operator for two more runs. It asserts three things, not one:
the triangles drop, it is a **single** undo step however many shapes were
selected, and a panel is actually armed. Without the last, this is just Simplify
with the dialog removed.

### window_state_roundtrip.sh can run again

It had been stopping at its first check with "geometry magic 0xCB — format
changed", which I filed yesterday as the app storing geometry in QSettings' text
form. **That was wrong.** The stored blob was correct all along; the harness's own
PowerShell was not: `-shl` on a **byte** shifts within the byte's width, so
`0x01 -shl 24` is `0` and every high term of the magic fell off, leaving the last
byte. `[int]` casts fix it.

Two more of its own faults behind that one: it compared the window's LEFT EDGE
against the monitor boundary, and `GetWindowRect` includes the invisible resize
border, so a window genuinely maximised on a monitor starting at 1920 reports
1912 and was called "on the primary" — it uses the centre now, which no frame
inset can move across a boundary. And its cleanup ran `reg import` with `2>&1`,
which in Windows PowerShell wraps a native command's stderr in error records and
leaves `$?` false, so the suite printed `PASS: 2 cycles` and then `FAILED` on the
line after.

## 2026-08-08c — Double-click to edit, and a failed promote stops taking the list with it

**Double-click a row in Loaded NIFs to edit it.** "Make Primary / Edit" existed
only in the row menu, so the obvious gesture did nothing at all. It runs the same
two calls the menu does, resolved the same way, and leaves every other loaded
document as it is — nothing is saved, nothing is reloaded.

**A promote that fails no longer closes the workspace.** The failure path called
`close()` on a window still flagged `sessionCollectionMember`, and closing a
workspace member runs the group close, which closes every other member with it —
one promote that could not load took the whole Loaded NIFs list down. The flag is
cleared before the window goes, and the window is hidden and deleted rather than
closed, since it never became visible and `close()` travels the whole close path
looking for something to confirm.

Together with the reload fix before it, the failure that produced this should now
be unreachable: promotes no longer read from disk, so the case that failed and
triggered the cascade does not arise. The guard stays regardless — a cascade this
expensive should not depend on nothing ever failing.

**Filed, not fixed**: `window_state_roundtrip.sh` cannot run. It expects the raw
Qt geometry blob and the registry holds QSettings' textual `@ByteArray(...)` form,
so it refuses before the app is even launched. Details in
`docs/TO_BE_IMPLEMENTED.md`.

## 2026-08-08b — A marked face donor, and CustomizationRemapData that can be rebuilt

### Mark the donor once

Right-click a row in **Loaded NIFs → Use as Face Donor for faceBones**. Marked
files carry a face glyph in the slot the workspace skeleton's skull uses, drawn
to the same rules. Every rigging step that needs a donor now takes it from the
mark instead of asking each time, reading the in-memory state so unsaved edits in
the donor are the ones used. The picker still handles anything unmarked.

### CustomizationRemapData can be regenerated on a _faceBones.nif

Edit the rigging in a `_faceBones.nif` and the remap data needs rebuilding — but
it *is* the standard-skeleton weights, and those are no longer in that file. So
it refused, and there was no way forward.

It reads them from the **marked face donor** now: mark the base head, run
`Generate CustomizationRemapData` on the sculpt-bound shape, and the blob comes
back. What is checked is what the donor actually holds rather than the role it
was marked for — a matching shape, not itself sculpt-bound, with the **same
vertex count**, because the blob is one record per vertex in order and a
mismatched donor would write one mesh's weights onto another's vertices and
produce a file that loads, passes every structural check, and deforms wrongly in
game. Ambiguity is refused with the reason rather than guessed at.

### The checks, and what they caught

Three, and the first exists to make the third mean something: the output's blob
is **wiped** before regenerating, and the wipe is itself asserted — without that,
a spell that did nothing would pass. Then: refuses with no donor marked; comes
back **byte for byte** with the base head marked.

It failed first time out, with a blob of the right size and the wrong contents —
all zeros, i.e. the wipe untouched. The mark was being discarded the moment it
was set, because the liveness check scanned the open and selected document lists
and threw away any model they did not carry. `NifModel` is a `QObject`, so it is
a `QPointer` now, which answers the question actually being asked — is it still
alive — with no assumption about where the model is registered.

## 2026-08-08 — Properties can be dragged, and the Collision Manager can be dropped on

### A mesh dragged into the Collision Manager now lands on the body you aim at

Two faults stacked, and neither could be seen by a harness.

**The action mask.** `drag->exec( Qt::MoveAction | Qt::LinkAction, … )` did not
include `CopyAction`, and the Collision Manager answers every mesh it would take
with `CopyAction`. Qt refuses an action the drag does not offer — so the no-drop
cursor showed and **no `QDropEvent` was delivered at all**. The panel's entire
handler was unreachable by a mouse. The collision tree's own drag had the mirror
of it (`exec(Move)` while the Block List answers `Copy`), so the shape-back-to-
geometry direction was equally dead. The mask is now a named function,
`wwBlockListDragActions`, so a target's answer can be checked against what the
drag actually permits.

**The tree covers the bodies.** The inventory tree is a child widget that accepts
drops, so while the pointer is over a body row Qt delivers to the TREE, not the
panel behind it — and the tree only understood collision-shape payloads. A mesh
was refused over every body and accepted only over the panel's bare furniture
below them: exactly inverted, with no body highlight because the panel never saw
the drag. The tree now hands payloads it does not understand to the panel at the
panel's own coordinates, so the body targeting and the highlight are the ones
that were already written.

### Properties, texture sets and the other 492 block types can be moved

The Block List drag modelled parenting as the `Children` array, so only the 71
`NiAVObject` types could move. Now a block held by a **typed link** moves too: its
owner's field is cleared when it is dragged out, and written when it is dropped
on something that has a field for it — replacing what was there, which is what a
single-valued field means.

Which field is decided by the FORMAT, not a table: every link cell declares what
it accepts and the model knows what inherits what. The first attempt took the
first field that would accept the block and put a shader property into a
BSTriShape's **`Skin`**, whose declared type is broad enough to take anything and
which comes earlier in the block. Candidates are scored now — exact type, then
"already holding one of these", then a meaningful ancestor, then `NiObject`,
which says nothing — and a single-valued field beats an array at equal score.

### Corrections

`collision_drop.sh` was red for an hour and I chased it through three wrong
answers — the shape-type setting, monitor placement, qmake configuration. It was
a **stale binary**: an earlier link had failed with "Permission denied" while a
NifSkope straggler held the exe, so every hypothesis was tested against a program
that was not the source. This repository already has that written down as the
first thing to rule out. It also now has a check for the thing that made the
symptom so unreadable — whether the panel's create hook is wired at all, since a
null one accepts the drop, says nothing and does nothing.

## 2026-08-07zb — 0.3: merge collision shapes, and an audit of what cannot be dragged

### Merge Shapes into One Mesh

Right-click a collision body → **Merge Shapes into One Mesh**. A box, a sphere
and a mesh in one body are three shapes the engine tests separately; merged, they
are one `bhkNiTriStripsShape`.

It turned out to need no round trip through geometry and no join.
`tlCollAppendEditableMesh` already walks a shape — through lists, through
transform wrappers, turning a box or a capsule into triangles on the way — so
merging is that walk run over every shape in the body into ONE mesh. Geometric
shapes and mesh shapes merge by the same route, because by the time they are
appended there is no difference left between them. The 65,535-vertex budget is
checked before anything is written, and the entry is greyed with the reason when
a body holds only one shape.

No MOPP is generated: that is its own spell, and guessing at it here would be a
second opinion about something that already has an owner.

### The audit: 71 block types of 563 can be dragged

Prompted by not being able to drag a `BSLightingShaderProperty` or a
`BSShaderTextureSet` out of its parent. That is not those two types — it is
structural. `wwReparentBlocks` models parenting as the **`Children` array**, and
only an `NiAVObject` can be in one. Counted against `nif.xml`: **71 movable, 492
not**. Everything else in the format is immovable — every property, every
`NiExtraData`, every controller, `NiSkinInstance`, all the collision blocks, all
the geometry data blocks.

There is a sharper bug inside it: `wwParentsOf` also only scans `Children`, so
dragging a shader property to blank space refuses with **"is already a root — it
has no parent to leave"**, which is false. It has an owner; the owner points at
it through `Shader Property` rather than `Children`.

Not fixed here. Making these movable means teaching the drag **typed links** — a
texture set dropped on a shader property sets that property's `Texture Set` —
which is a feature, not a patch. Filed in `docs/TO_BE_IMPLEMENTED.md`.

### A correction

The merge was reported broken and stashed on the strength of a check that held
the target body as a plain block number across an operation that inserts two
blocks and removes a branch. It was reading a different body afterwards. The
feature was correct; the test was not — and the first fix aimed at it, persistent
indices inside the merge, changed nothing because nothing there was wrong.

## 2026-08-07za — Join shapes from the Block List

`Ctrl+J` in the 3D view has merged compatible `BSTriShape`s into the active
object for a while. There was no way to reach it from the **Block List**, which
is where you are when you are looking at blocks.

Right-click → **Hierarchy → Join Selected Shapes**. The row you right-clicked is
the target: everything else merges INTO it and takes its vertex format, which is
Blender's active-object rule and the one the viewport already follows. Disabled
with a tooltip that says why — fewer than two meshes selected, or the row you
right-clicked is not one of them.

It is the **same call**, told what to join instead of reading the viewport's
selection, so the vertex-format rules, the 65,535-vertex and 256-bone caps and
the rigging-aware bone remap are the ones that were already there rather than a
second opinion about any of them.

Checked in `block_dragdrop.sh`: duplicate the cube, join the two, and require
**both** that the file has one mesh fewer and that the target carries both
meshes' vertices — 24 + 24 → 48. Either alone would pass for a join that deleted
the source without appending it, which is the failure worth catching.

**Not yet**: merging collision shapes in the Collision Manager. The route is
mapped — collision → geometry → this join → collision, which is the loop closed
in 07-07w run round a circle, with the mesh budget checked before the trip back —
but it is not built.

## 2026-08-07z — Clicking a child block selects that block

Click a `BSShaderTextureSet` in the Block List and the whole shape branch lit up
with the `BSTriShape` as the **primary** — three rows highlighted, and the wrong
one of them active. Select the `BSLightingShaderProperty` instead and you got the
same three rows and the same primary.

The viewport can only draw an `NiAVObject`, so a click on a property or a texture
set is walked up to the shape that owns it before it reaches the 3D view. That is
right for the viewport. What was wrong is that the promotion came back round
through the object-selection mirror and repainted the **list** with it, so the
list stopped showing what had been clicked and started showing what the viewport
had been given instead.

The viewport still gets the shape. The list's own highlight now comes from the
list, and the mirror leaves it alone while that is the direction of travel.

Checked in `block_dragdrop.sh`, and proven by A/B: against the old code the check
reads `active 9` — the shape — where the texture set is block 11.

## 2026-08-07y — Half the cost of every structural edit, and an instrument that lied

### Undo used to save the whole file twice. Now it saves it once.

Every array resize, block insertion and removal in the program goes through
`nifSnapshotOp`, which saved the entire NIF before the operation **and again
after** — 88 ms a time on a 512-block file, 160 on 2012.

The second copy is only ever read by a **redo**, and a redo needs an undo in
front of it. At that moment the model is already holding exactly the state redo
has to restore, because the undo stack is LIFO: by the time a command is undone,
everything pushed after it has been undone too. So the redo snapshot is taken
there instead. Nothing is given up; the cost moves off every edit and onto a
keystroke that was going to rewrite the whole model anyway.

Checked by comparing the **saved bytes of the whole model** across undo and redo,
twice round — a redo that restored the right parent and lost a transform would
pass the obvious check and fail this one.

### The perf flag was making the program slow

`WW_PERF_TEST` opened and appended to a log file **inside every frame** through
the grid tracer. That is the flag you reach for when something is slow, so it was
distorting exactly what it was brought in to measure — and "the log fills with
`drawGrid` pairs" got read as a repaint storm. Frames are counted now, not
logged: one or two per step, under a millisecond of painting. The per-frame trace
has its own variable, `WW_GRID_TRACE`.

### And the list-mode hang is a different thing than it was written up as

Not a repaint storm, and not a modal dialog either — a 1.5-second watchdog timer
logs nothing at all across a 60-second hang, and timer events *do* run inside a
nested loop. So it is one event handler that never returns. It is also not the
animation setting, which was the last writeup's answer: 4 runs in 8 still hang
with it forced off. Narrowed to a precise statement, filed, and left there — it
is reachable only from the harness. Details in `docs/TO_BE_IMPLEMENTED.md`.

## 2026-08-07x — The check that was missing, and the bug it found

Yesterday's body-targeted mesh drop shipped without a check of its own: the
fixture's three meshes were all consumed by the drops before it, so there was
nothing left to aim at a body with. It has one now — and it failed on the first
run.

### Dropping a mesh on a body did nothing, and looked fine doing it

Creating collision **consumes the source mesh**, and removing a block renumbers
every block after it. The drop read the target body's number before running
Create and used it afterwards, by which time it named something else — a
`bhkCollisionObject`, as it turned out. So the move was refused, the body Create
had just made was left standing, and the shape never reached the body you aimed
at.

Held as `QPersistentModelIndex` now, both the target and the bodies that were
already there, which is what `castCollisionOverSelection` does a few files over
and for the same reason. Proven by A/B: with plain numbers the new check reads
`1,1,1 -> 1,1,1,1` and fails; with persistent ones, `1,1,2`.

### What the check asks

The **shape of the whole arrangement**, not a block number: three bodies holding
one shape each become three bodies where the one under the pointer holds two. A
drop that made its own body reads as four holding one each; a drop that did
nothing reads as three; a drop that joined the wrong body puts the two in the
wrong place. None of those can pass — and none of them could be told apart by
counting bodies alone.

`collision_drop.sh` is 30 checks. The fixture is four meshes now, and the fourth
lives under a node of its own — see the entry in `docs/TO_BE_IMPLEMENTED.md` for
why that is a finding rather than housekeeping.

## 2026-08-07w — The loop closes: collision back to geometry, and drops that aim

Two halves of the same gesture set.

### A mesh dropped ON a body joins that body

The mesh drop made a body of its own wherever it landed. Now the body under the
pointer **lights up** as you cross it — the same highlight a shape dragged inside
the tree gets, and the same one the Block List gives a block — and dropping there
puts the new shape in that body instead of beside it.

It is Create plus a move: the shapes are identical either way, so the drop only
decides where they hang. The body Create made is taken away with its collision
object once its shape has moved out; the node it hung off is left alone.

**Convex and Mesh keep their own body**, and the status bar says so. Those two
hand off to the live preview, which returns long after the drop does, so there is
nothing to move yet when the gesture ends. Saying it beats quietly doing
something else.

### And a collision shape dragged into the Block List becomes a mesh again

Drag a shape row out of the Collision Manager and drop it on a `NiNode`: it comes
back as a `BSTriShape` under that node. The node lights up on the way, through
the same `NifModel::dropTargetBlock` a block drag uses, so both gestures look
alike while they are happening.

The conversion is not new — it is the Collision Manager's own **Collision to
BSTriShape**, split so it can be told *which* shape and *which* node rather than
asking the tree what is selected. The menu route still passes the selection.

**Neither side knows the other's payload format.** The block payload stays
private to the Block List and the shape payload private to the inventory tree;
each reads the other's through one call. A payload dropped somewhere that does
not understand it is ignored rather than half-understood, which is what both
formats were made private for.

### Measured

`collision_drop.sh` is **22 checks**. The last five are the loop closing, and the
one that matters counts `BSTriShape`s: it must go **up** by one, because the
first drop in that harness consumed a mesh to make the collision — a conversion
back that produced nothing would leave a file that had quietly lost a mesh, and a
check that only asked "did a block appear" would not notice.

## 2026-08-07v — Move a shape from one collision body to another

Until now the only way to get a shape out of one body and into another was to
delete it and build a fresh one somewhere else. Drag the row: the body it would
land in lights up while the pointer is over it, the same way the Block List
lights the row a block would drop into.

`tlMoveCollisionShape` is the operation under it, shared rather than living in
the drag, so nothing can grow a second idea of what moving a shape means. Both
ends handle a list — taken out of a `bhkListShape` the entry goes and a list left
holding one shape stays a list, because that is legal and unwrapping it is a
second edit nobody asked for; put into a body that already has a shape, a list is
made if there is not one, which is exactly what Create does with Replace off.

Refused, and refused **by the drop ACTION rather than by ignoring the event**:
the body it is already in, a row that is not a shape, a target that is not a
rigid body, and a shape that would end up inside itself (the list a body holds is
a shape too). Ignoring a drag event ends the drag over the widget and not one
further move arrives — the trap the Block List's drag died in through four wrong
fixes — so the event stays accepted and the verdict rides in the action.

### Measured

`collision_drop.sh` is **17 checks** (was 7). The new ones: a shape row is
draggable and a body row is not, the inventory takes drops, hovering a body that
would take it offers the move *and lights that row*, the drop moves the shape and
leaves it beside what the body already had, and the body it is already in refuses
it.

`Qt::ItemIsDragEnabled` is set in `setItemRoles`, the one call both the compiled
and editable population paths make, and it is checked on its own — that flag is
what `QAbstractItemView` gates `startDrag` on, so without it the gesture does
nothing whatever else is in place. It is also precisely what a harness delivering
events directly steps over, and how the Block List's drag first shipped: green
checks over a dead feature.

## 2026-08-07u — Drag a mesh onto the Collision Manager

The last unbuilt line of the Block List drag-and-drop spec: *"dropping a
BSTriShape onto the Collision Manager is the same payload routed to the
mesh→collision path"*. It is, and that is all it is — the payload already
travelled and the panel already knew how to make collision out of a selection,
so the drop only aims the existing Create at what was dragged.

- It runs **the same call the shape popup's Create runs**, held in a
  `std::function` the button and the drop share, so the two cannot drift apart.
- At **the shape type the panel is showing**, not one the drop picked: whatever
  that popup says it will make is what a drop makes. Convex and Mesh open their
  preview exactly as they do from the button, so the expensive two keep their
  confirmation step.
- **Several meshes make one body each**, which is what
  `castCollisionOverSelection` already does for a multi-selection.
- The current block is only re-selected if it has to be — the drag came out of
  the Block List, so the current row is normally already one of the blocks being
  dropped, and selecting again would collapse the very multi-selection that makes
  it one body per mesh.

### The predicate is the FILE, not the scene

The obvious first move was to ask the create spell's own `isApplicable` rather
than keep a second opinion about what is legal. It refused every mesh. That
predicate asks the **Scene** whether a block has vertices and triangles, so until
a frame has been drawn with that mesh in it the answer is no — measured with the
view, the scene, its renderer and the node itself all present, on a real game
mesh, and still no.

Which is wrong for a drop quite apart from being untestable: whether a mesh can
be dropped must not depend on whether the viewport has got round to drawing it,
or the gesture refuses on a collapsed viewport and on a file that has only just
opened. It asks the file now — the geometry classes, and vertices and triangles
above zero, read through the same BSTriShape-or-its-Data split `wwBlockSummary`
uses. A `NiNode` is deliberately not taken, though the spell would: dropping one
means every mesh under it, which is a far bigger thing than the gesture looks.

### Measured

`tests/spells/collision_drop.sh`, 7 checks, no game corpus. The one that matters
is check 2, and it is there because of how the block list's own drag first
shipped — 26 green checks over a feature that never ran, because the harness
entered below the one broken step. So: with `setAcceptDrops` turned off, which
makes the whole thing do nothing whatsoever for a real drag, **six of the seven
checks still pass and only check 2 fails**. Measured, not asserted.

`NifSkope::blockListDragPayload` is public now. The payload FORMAT stays private
to the block list — this remains the only way to read it, so a payload dropped
somewhere that does not understand it is still ignored rather than
half-understood, which is what the format was made private for.

## 2026-08-07t — The flat list does not crash. It hangs, and I said otherwise

**Correcting 2026-08-07p**, which stated that the crash filed against list mode
"does not reproduce in 36 runs". Those 36 runs did pass. The claim was still
wrong, because the instrument behind it — *did the log end with `done`* — cannot
tell a crash from a hang, and this is a hang.

It reproduces **7 times in 10**. What that took, and what each step ruled out:

| suspect | measurement | verdict |
|---|---|---|
| stale incremental build | `make clean`, full rebuild | 7 of 10 — no |
| the IPC port, reused per run | unique port per run | 6 of 10 — no |
| my own builds relinking the exe under it | ran with nothing else going | still fails — no |
| a crash | Windows event log, and gdb over the whole run | no APPCRASH, **no fault at all** |
| `GLView/Enable Animations`, inherited | forced off in the harness | 7 of 10 — no |

A passing run takes **4 seconds** and a failing one **63**, which is the script's
own 60-second deadline: it kills the process and reports that it did not finish,
which from outside reads exactly like a crash. Hierarchy mode is 4-5 seconds
every time, across dozens of runs.

Narrowed with the `WW_PERF_TEST` markers, and it moved twice as better traces
went in: `NifSkope::select()` runs to completion, the slot behind
`currentNifIndexChanged` reaches its end, and the block is in the harness's own
bare `QApplication::processEvents()` afterwards — with the perf log filling with
`drawGrid` pairs for as long as it lasts. A queue that never empties.

**Left open, honestly.** `processEvents( AllEvents, 100 )` would make the harness
robust and would hide the question; if the viewport really does spin forever
posting updates then somebody sitting in list mode is burning a core, and that is
the version of this worth knowing. Written up in the backlog with the markers
still in place.

Animations are forced off in `block_rename.sh` regardless — a harness must force
the state it measures, and that is the second time this exact setting has been
inherited into a measurement.

## 2026-08-07s — A block in several places, and which one you are dragging

A NIF block may sit under more than one parent — Ctrl-drop makes exactly that,
and it is a real capability rather than a corruption. Three things were wrong
about it, one of them able to corrupt a file.

### A cycle through the other parent was allowed

`NifModel::getParent` answers with the **lowest-numbered** parent, and the cycle
check walked up through it. Give a child a second parent that sorts before its
first, and dropping the first onto the child climbs the wrong chain, finds no
cycle, and writes one into the file. Measured before the fix, in as many words:
`dropping 6 onto its own child 7: ALLOWED`.

The walk now goes up through **every** parent, from a map built in one pass —
the refusal is asked on every DragMove, so a per-level `wwParentsOf` would
re-read the file's whole link structure once per step of the way up. The seen-set
also makes it terminate on a file that already contains a cycle, which is what
the old fixed guard of 256 was for.

### Dragging one instance moved all of them

A plain move took the block out of **every** parent it sat in. That was
deliberate — and wrong, per bungo: the other placings are other placings, and the
gesture said nothing about them. Only the row knows which one was picked up, so
the row's parent is read at drag start and carried in the payload beside the
block numbers.

A caller with no row — the Collision Manager's Set Parent, and every spell —
still means the block itself and still leaves every parent, which is the only
thing it can sensibly mean.

### And nothing said which one you were on

Every row of a multiply-parented block carried the same number and the same name,
so after a drag "moved it" there was no way to tell which. The **current** row now
reads `13 NiNode   2 of 2`. Only the current one: numbering every duplicated row
would put a mark on rows nobody asked about.

### Measured

`block_dragdrop.sh` is **103 checks** (was 87). The discriminating pair is the
same block dropped on the same target twice, differing only in which parent the
row was dragged out of — one implementation cannot satisfy both. Plus: the cycle
refusal (which fails against the old walk), a block with six children in
scrambled block order moved to the front and back to the end without losing or
duplicating one, and the instance marker appearing on the current row and on
neither the other row nor a block that sits in one place.

## 2026-08-07r — The live drag sweep, and the two ways it lied

`block_drag_live.ps1` had one verified scenario and three written but never run.
It now drives **seven**, over a scene with three roots and nodes nested two deep:
into a shut node, into a row that only the drag's own auto-unfold revealed, into
a second root, a root made a child, out to blank space, onto a mesh row, and the
root onto its own descendant.

**Six of the seven passed on the first honest run. The drag-and-drop feature came
out of this unchanged** — every failure in the two runs before it was the
script's.

### The script could not fail

Every scenario read `if (-not (Verdict '...')) { $fails++ }`, and `Verdict` wrote
its message with `Write-Output` before returning the bool. The parenthesised call
captures the **whole** output as the condition, and a two-element array is truthy
whatever the bool says — so `$fails` could never increment, and the messages
never reached the console either, having been eaten by the condition. The one
test that reaches above the native-drag boundary printed PASS no matter what
happened. It says `Write-Host` now, and returns only the verdict.

### And its fixture had evaporated

It opened `E:\dragfx.nif`, a file built by hand in an earlier session and never
committed. It was gone, and the run died before touching the mouse. The scene is
built by the program now (`WW_DRAGFIXTURE`), because the CLI cannot: both
`Node/Attach Node` and `Node/Attach Parent Node` want a QWidget and die headless.

### Two readings that convicted the program of the script's faults

- **`payload [N]` in the drag log is the block COUNT**, not a block number.
  Reading it as the identity of what was picked up made every scenario report
  grabbing block 1 — which is simply how many blocks a one-block drag carries.
  The identity is in the `=== drag start … first N ===` header.
- **A refused target never receives a drop event at all.** The handler answers a
  refusal with `Qt::IgnoreAction`, and Qt then does not deliver the `QDropEvent`,
  so "no DROP reached the list" is the *correct* outcome for the cycle refusal
  rather than the failure it reads as. The check now reads the last DragMove —
  the pointer must have been resting on the illegal target — and that nothing
  moved. Verified against the recorded log of the run that found it, rather than
  by taking the mouse again.

One expectation was wrong about the feature rather than the log: a drop on a mesh
row was written as a refusal, and **a row that cannot take children is all gap**
— there is nothing to drop inside a mesh, so its whole height reorders. The
refusal exists; no point on that row can reach it.

### Also

The click that selects the source and the press that starts the drag were 450 ms
apart, inside Windows' 500 ms double-click time. They were being delivered as a
double click, which opens the inline rename editor. It is 900 ms and a move away
and back now.

## 2026-08-07q — What a drag on a big file actually costs

`wwParentsOf` walks every block in the file, once per block moved, and the
multi-block sort calls `getParent` per comparison. Both are O(blocks) inside a
loop, and the standing note said to measure before caring. Measured, with
`WW_BLOCKDND_BENCH=<n>` on `block_dragdrop.sh`:

|  | 512 blocks | 2012 blocks |
|---|---|---|
| 1 block, out of a 2000-child parent | 88 ms | 160 ms |
| 1 block, out of a 3-child parent | 152 ms | 138 ms |
| 50 blocks | 360 ms | 892 ms |

**The single-block cost does not track the block count at all**, and moving a
block out of a three-child parent costs the same as out of a two-thousand-child
one. It is `nifSnapshotOp`: one undo step **serialises the whole file twice**,
before and after, and every structural edit in the program pays it.

`wwParentsOf` shows up only in the multi-block case — take the single snapshot
off those runs and it is 5 ms a block at 512 blocks and 15 ms at 2012. So caching
a parent map would buy the rarest gesture on the largest files about a third of a
second, and leave the per-block `Children` rebuild, also O(blocks), where it is.
**Not done, on the numbers.** The snapshot is the bigger fish and is filed as its
own backlog item rather than being bolted onto a drag.

The bench is part of `block_dragdrop.sh` and off unless asked for: it builds
thousands of blocks, which is not what the rest of that harness is for.

## 2026-08-07p — Nothing in the flat Block List could be clicked

Filed as "blocks inserted while the flat list is showing are not addressable".
That was the second-worst reading of it. **No row was addressable** — not the new
one, not the ones that had been there since the file opened — so in that mode a
click, a drop and a right-click all landed on nothing, everywhere, always.

The measurement that filed it asked only about the row it had just inserted,
which cannot tell "this row is broken" from "every row is broken". Asking about
all of them took one run.

### What it was

`QHeaderView` keeps `length` as the total of its sections, maintained by adding
and subtracting rather than re-derived. Hiding a section subtracts its width and
remembers it; **changing the model gives every remembered width back without
adding it to `length`**. Hide them again and each width comes off twice. The
Block List hides 9 of the NifModel's 12 columns, so after one file load:

```
list: after resizeSection   12 sections, 9 hidden, length  628, sections total  628
swapModels: on entry        12 sections, 0 hidden, length  649, sections total 1549
list: after all hiding      12 sections, 9 hidden, length -450, sections total  450
```

`QHeaderView::visualIndexAt` returns -1 for any position past `length`, so with
it negative every point in the view belonged to no column — measured, at x = 2,
60, 125, 260 and 400 — and `QTreeView::indexAt` returns no index at all when it
has no column. Hence: a `visualRect` that draws a row perfectly well and an
`indexAt` on that same rectangle that answers nothing.

Hierarchy mode never sees it: the proxy has 3 columns and hides none of them.

### The fix, in two halves

- **Release the columns before any model change, apply them after** — with
  nothing hidden at the moment of the change there is nothing to hand back
  (`wwReleaseBlockListColumns` / `wwApplyBlockListColumns`).
- **Each mode keeps its own saved header blob.** A blob saved against the
  3-column proxy, restored onto the 12-column NifModel, desyncs the total the
  same way — `restoreState` appends the missing sections and does not add their
  widths. The pre-split `List Header` key is deliberately not read; one session's
  column widths is the whole cost.

`swapModels` also now swaps like for like — `nifEmpty` in list mode, not
`proxyEmpty` — which additionally fixes the list showing the *hierarchy* after
every load in list mode, until something happened to call `setListMode` again.

### Measured

New harness `tests/spells/block_list_modes.sh`, 8 checks per mode. Against the
code before the fix: hierarchy 8 of 8 pass, list **5 of 8 fail** — `length -438`
against a section total of 462, 6 of 6 rows resolving to INVALID, and the
inserted block reading as -1. After: 8 of 8 in both.

`block_rename.sh`'s list half is back in the gate (24 checks per mode, both
green), which is what the backlog said to do when this was fixed.

### The crash filed alongside it does not reproduce

The other half of the entry — "roughly one run in three takes the process down"
— did not happen once in **36 runs**: 12 of `block_rename.sh` in list mode
against the code before this fix, 12 after, and 12 of the new harness's
switching loop, which is 120 model switches with real paints, inserts and
removes between them.

Worth knowing before believing it next time: the session that filed that crash
lost an hour the same night to heap corruption that turned out to be **a stale
incremental build** (its own note, in HANDOFF.md: identical source, 6 of 6
incremental deaths against 0 of 12 clean). Not proven either way. Not
reproducible today, and the mode is back in the gate rather than out of the
program.

### Also measured, not acted on

`insertNiBlock` ends its row insertion **before** the block's fields exist —
`endInsertRows()`, then `insertAncestor` and `insertType`. A new block is
announced with 0 rows and has 24 a moment later, with no second signal. Nothing
observed goes wrong because of it (the model's own row signals agree with its row
counts, 14 of 14 in the harness), but a view that laid that row out on the signal
cached a child count that was already stale. Filed, not fixed.

## 2026-08-07o — Paste followed the pointer in one window only

There is **one hover-probe slot for the whole application**, and every document
window registered its own `this` into it. The last window opened therefore owned
the slot, and its probe declined whenever that window was not the active one — so
in any other document, Ctrl+V quietly went back to pasting into the selection.
Nothing said so; the failure is indistinguishable from the feature not existing.
And when that window closed, the slot kept a lambda holding its dead `this`,
which every subsequent paste in every window called through.

The probe captures nothing now. `NifSkope::blockListHoverResolve` asks each open
document whether the point is over ITS block list, so the answer follows the
pointer rather than the order the windows were opened in, and it belongs to no
window that can be destroyed underneath it.

`isActiveWindow()` went from being a gate to being the tie-break. Which list is
under the pointer is a question about geometry; two lists share a place on screen
only when their windows overlap, and then the one in front — the active one —
owns the pointer. As a gate it was also wrong for a legitimate case: a window
that is not active can still be the one under the pointer.

### Measured

`block_dragdrop.sh` is **87 checks** (was 82). Five are new, and they open a
second document, place the two windows side by side, and ask the probe from a
fabricated point: both windows answer, a point over neither is declined, and the
probe survives one of the windows closing.

Against the pre-fix probe, kept honest by leaving the fabricated-point seam in
place so only the multi-window defect could fail it: **2 of the 5 fail** — "the
probe answers for THIS window" (it answered `no`) and "closing the second window
leaves the probe working". The third, "and for the OTHER window's block list",
**passes on the broken code too**, and should: the last window opened was exactly
the one that worked.

That seam — a fabricated cursor position — is the one step of the path the
harness cannot drive, because the pointer cannot be moved from inside the
process. The registered slot, both windows' real geometry and the index lookup
are all the production path. Named here per the rule that came out of the drag
session: ask what your harness enters below, and say so.

## 2026-08-07n — Unfold only where the block could go, and reach what landed

### The hover opened anything

Auto-expand opened whatever the pointer rested on, including rows that can take
no children at all — so crossing a list of meshes unfolded every mesh on the way
past, showing you the inside of somewhere the block cannot go. It asks the same
verdict the drop uses now, so the two cannot disagree about what is allowed.

### The restore could not reach a landed block

Found reviewing the rest, not reported. `wwRestoreBlockListBranches` descends
only into branches it is re-opening — so it could not **reach** a landed node
whose ancestors were not themselves in the set, and an ancestor that the hover
auto-opened is exactly one that gets folded back and is therefore not in it. Drag
over a node, into its child, drop: the block landed correctly and was invisible,
which is the failure that restore exists to prevent. The ancestor chain of the
landed block counts as open now.

Both fail with their fix disabled.

### Also

- The unparent status message carried no `%1` and was still being `arg()`'d,
  which warns once per drop and returns the string unchanged.
- Checked and fine: an unparented block IS added to the footer's `Roots`, so it
  survives a save rather than becoming unreferenced. That was the one thing in
  this batch that could have lost data.

## 2026-08-07m — Dragging a block OUT, and folding back what the hover opened

### There was no way out

Every drop resolved to some node to go *into*. A block already under the scene
root therefore could not be lifted out of it, nor dropped above it — there is
nothing higher than a root, so the gap beside one fell back to dropping ONTO it,
which is where the block already was. And the blank space meant "the end of the
root's children", which is still inside it.

**No parent is a destination now.** The blank space below the rows, and the gap
beside a top-level row, both mean *out*: the block loses every parent and becomes
a root of its own. That is the same answer paste gives in that same space, which
is what made the inconsistency obvious once both existed.

With `PreserveWorld` the compensation against no parent is the identity, so the
block's world transform is written into its local and it does not move — which is
what "take it out of there" should mean.

`-1` now means two different things to the drop spot — nothing to aim at, and
deliberately nowhere — and only the second is a drop. `wwReparentRefusal` cannot
tell them apart, because from its side they are the same argument, so the caller
distinguishes them.

### The hover-expand was permanent

Auto-expand opens a branch when the pointer rests on it, so a drop can reach
inside something folded. It never closed again: a drag that merely passed over a
node left it open, and crossing a file left the whole file unfolded.

Branches this drag opened are remembered and folded back when it ends — except
the one the block landed in. They are also subtracted from the "what was open"
snapshot the post-drop restore uses, or the restore would have re-opened them by
the other route and made the hover-expand permanent anyway.

## 2026-08-07l — Deselecting only deselected half of it

### The row stayed orange

Clearing the block list's selection cleared Qt's selection and the current index,
and the row went on looking exactly as selected as before while the status bar
underneath it read "No block selected". The row COLOUR is
`NifModel::selHighlight`, mirrored from the **3D view's object selection** — a
different thing entirely from what a QTreeView calls selected. Four things go
now: the Qt selection, the current index, the published block list spells read,
and the object selection.

### Block Details listed the whole file

Mine, from the same change. A `QTreeView` reads an invalid root index as *"show
the whole model"*, so with no block selected Block Details showed every block in
the file at once — which is literally what a root of nothing asks for and nothing
like what it means. It parks on the empty model now, the same one `swapModels()`
uses while a file loads, with its header and footer rows hidden so the panel
reads as blank.

**Only the explicit `setRowHidden` does that, and the first version of this
comment said otherwise.** `refreshRowHiding()` returns early on an invalid root
index, so the details filter was set and never applied: measured, with the filter
alone both rows stayed visible. It is still set, so any future re-derivation
agrees rather than undoing the hide — but it is not what clears the panel.

And a check that it comes BACK: blanking the panel swaps the model and sets a
filter that hides everything, so selecting a block has to undo both, or this
would have traded a cosmetic complaint for a Block Details that never shows
anything again.

### The measurement was broken first

The check read `NifTreeView::isRowHidden( row, parent )`, which **shadows**
`QTreeView`'s with a different meaning — it ignores the row number and answers
for the parent item — so under an invalid root it always came back false and the
check measured nothing. Two rounds of "still 2 visible" were the instrument, not
the code. It reads `visualRect().isEmpty()` now, which is the claim being made
anyway: is the row on screen.

## 2026-08-07k — Paste follows the pointer, and blank space means nothing

### Clicking blank space deselects

A `QTreeView` leaves the previous row selected when you click past the end of the
list, so the Block List had no way to have **no** primary selection — and
therefore no way to say "no parent". It clears both the selection and the current
index now, because those are separate in Qt and leaving a current index behind
means the spell book still has a block to act on while the list shows nothing
selected.

Block List only. In the field views the current row is the thing being edited and
losing it to a stray click would be hostile.

### Paste follows the pointer, not the selection

Ctrl+V parented into whatever happened to be selected, wherever you were
looking — so pasting next to a branch meant selecting that branch first, and
pasting a free copy was not possible at all. Now:

| pointer | paste lands |
|---|---|
| over a row | into that row |
| over the blank space below the rows | with **no parent**, as a second root, to be dragged into place |
| not over the block list at all | exactly as before, on the index the spell was handed |

The pointer is asked for on demand when the spell runs — a probe registered by
the window, rather than anything watching the mouse. It declines for an inactive
window, so a second document cannot answer for the one you are in.

`spPasteBranch::isApplicable` accepts an **invalid** index now: it means "paste
with no parent". That was the one state in which Ctrl+V was simply disabled, so
clearing the selection and pasting did nothing at all.

The check casts the paste with a block as its index — as if it were still
selected — while the probe reports blank space, because that is the case
reported: something is selected, the pointer is over nothing, and the paste must
not go into the selection. Casting with an invalid index instead passed on the
old code too, for the wrong reason, and did until it was corrected.

## 2026-08-07j — Dropping into a node shut the node

A `QTreeView` keeps expansion against **model indices**, and the proxy rebuilds
wholesale whenever links change — so every structural edit closed the tree,
including the node just dropped into. The block landed exactly where it could not
be seen, which is indistinguishable from not landing.

What was open is captured **before** the write, by BLOCK NUMBER (indices do not
survive the rebuild), and re-opened after the proxy has relaid itself out —
together with the node dropped into, so the moved block is visible where it went.
The walk descends only into branches that are open, or that are being re-opened,
so it costs what is on screen rather than what is in the file.

Removing the re-open fails both new checks: the node is shut afterwards and the
moved block has no visible row. That is the only reason to believe them.

## 2026-08-07i — A logger that unfolded the file it was measuring

The drag log dumps every on-screen row's rectangle at the start of each drag, so
a real mouse can be driven at them from outside the process. It called
`expandAll()` first — which the live-drag script needs, and which meant picking
anything up unfolded the user's entire file under their hands.

`expandFirst` is a parameter now, false at drag time and true only for the
script, which runs against a fixture nobody is looking at. The dump is also
bounded to rows that actually intersect the viewport and no longer descends into
closed branches: it runs on every drag, and a large file has thousands of rows.

The check collapses a branch, calls the dump, and asserts the branch is still
collapsed. It fails on the old default, which is the only reason to trust it.

## 2026-08-07h — The drag was dead before it began

**Ignoring a drag event ends the drag over the widget.** Not one `DragMove`
follows it. And a drag always begins **on the row being dragged**, whose two
neighbouring gaps are precisely the positions that refuse as no-ops — so the
first event was a refusal, the refusal killed the event stream, and every
subsequent aim went to a program that had stopped listening.

That is the whole of "I cannot drop between two rows", and it survived **four**
fixes made by reading code, with `block_dragdrop.sh` sitting at 44 green checks
the entire time.

Our own payload is accepted always now, wherever the pointer is. The **action**
carries the verdict — `IgnoreAction` gives the OS no-drop cursor while the event
stays accepted, so moves keep arriving and the next position gets its own answer.
The drop is where a refusal actually refuses.

### The thing that found it

`tests/spells/block_drag_live.ps1` drives the **physical mouse** at the block list
and reads the drag log back. A native drag is the one path no harness can enter —
`QApplication::notify` routes drag and drop through the drag manager, so nothing
synthetic reaches the loop — and everything above that boundary had no coverage
at all. One run: `ENTER … | spot 3 pos -1`, then silence. Afterwards: `ENTER`, 20
moves, `DROP … spot 0 pos 3 → moved 1, refusals: none`.

The harness gained the check that would have caught it: a hover the drop would
refuse must still come back **accepted**, and say so through the action instead.
It takes over the mouse for three seconds and runs on the second monitor.

### And the log is on by default

It was behind `WW_DRAG_LOG=<file>`, and the one run that mattered was launched by
double-clicking the exe, where no shell variable can reach it — so a drag was
performed and nothing at all was recorded. It writes to `ww_drag.log` beside the
executable now, truncated at each drag start so it cannot grow. A diagnostic that
needs the person reporting the bug to launch the program a particular way is a
diagnostic that does not run.

## 2026-08-07g — The end of the list was nowhere

`indexAt()` answers nothing in the empty space below the last row, so a drop
aimed there resolved to no target at all and was refused — and under the last row
is exactly where anyone aims to put something last. That space is the **end of
the root's children** now. The last row is found by walking back up from the drop
point, which also gives the insertion line somewhere to sit.

The check asserts the resolved position equals the child count, and logs that
`indexAt` is invalid at that point, so it is on the record that the old code had
nothing to work with there.

The card is also driven by a **timer** now rather than by drag events. Moved from
the drag handler it followed the cursor only while events kept arriving, and when
they stopped it did not lag — it stopped dead somewhere else on screen, still
asserting a verdict about a row the cursor had left. `QCursor::pos()` is global
and always current. A hint older than 250ms is retracted rather than carried
around.

**`WW_DRAG_LOG=<file>`** appends one line per drag event: the position, the row it
hit and that row's rect, the spot and insert position it resolved to, the payload
size and modifier mode, then what the drop moved and every refusal. Nothing about
a native drag is reachable from a harness — no synthetic event enters the loop,
which is how every one of these bugs walked through 40-odd green checks — so the
program has to say what it saw.

## 2026-08-07f — The card was a window, and one bug wearing three hats

### The card froze, so the verdict froze with it

Three reports, one cause. The card is a real top-level window travelling with the
cursor during a **native** drag loop, and as an ordinary window it takes part in
hit-testing — so the moment it passed under the pointer the block list got a
`DragLeave` and stopped receiving `DragMove`. From there the card sat still, the
insertion line stopped updating, and whatever it last said stayed on screen.

Which is why gaps that are perfectly legal looked refused: the card was showing
**"Already in that position."** from an earlier position of the cursor. The
refusal logic was right the whole time — the ladder of verdicts for every gap is
logged by the harness now, and it reads `0:already, 1:already, 2:ok, 3:ok` for a
block sitting at index 0, exactly as it should.

`Qt::WindowTransparentForInput`, `WA_TransparentForMouseEvents`,
`WA_ShowWithoutActivating`, no focus. And it hides on `DragLeave`, because it is
only moved by `DragMove`: off the list it would hang in mid-screen describing a
target the cursor is nowhere near.

### "The ID number is not being actively updated"

It was never an id. The model renders a `tStringIndex` as `Cube [1]`, where 1 is
the index into the **header string table** — so four meshes all called "Cube"
all showed `[1]`, which reads as an id that is both wrong and frozen.

Real information in Block Details, where you are editing that field. Noise in a
list of blocks, where the Value column is simply the block's name. Resolved
through the model rather than trimmed off the end, so a node genuinely named
"Bone [1]" keeps its name.

The check paints the same cell with two delegates differing in nothing but that
flag and compares the **pixels**. Asserting the flag would only assert that a
bool was set; the question is whether the cell comes out different.

## 2026-08-07e — A row you cannot drop into is all gap, and the line you never saw

### Only the two ends of a list could be aimed at

Reported, and the screenshot said it: a list of four `BSTriShape` children of one
root, and the card reading "BSTriShape cannot take children". **There is nothing
to drop inside a mesh**, so a mesh row's middle third spent itself refusing —
which in a list of meshes leaves reorder slots at the two ends and dead pixels
everywhere in between.

A row that cannot take children is **all gap** now: top half above it, bottom
half below it. Only a `NiNode` keeps the three-band split, because only a
`NiNode` has an inside to aim at. Measured on a 20px row: 12 of 20 sampled
positions offered a gap before, 20 of 20 now.

The primitive still refuses parenting into a non-node. That is not in tension —
the UI simply never asks it any more.

### The line was drawn into a queue

`QDrag::exec()` runs a **native modal loop**, and a posted `update()` is coalesced
and can sit there until the drag ends. The insertion line was being painted after
it stopped being useful, which from the outside is indistinguishable from never
being painted at all — and that is exactly how it was reported, twice.
`repaint()` for everything a drag draws.

The harness could not have caught this: it delivers drag events directly, so it
was never inside the modal loop that swallows the paint.

### The card

Directly above the pointer now, not below-right — below-right covers the rows
under the cursor, which is precisely what you are aiming at when the gesture is
"between these two". It carries the block's **own icon** (asked of the list's
model, so the card and the row cannot disagree), its **type** muted, and its
**name** bright, which is Blender's layout.

`block_dragdrop.sh` is 42 checks. Two are new and both were shown to fail on the
old behaviour: every pixel of a non-node row offering a gap, and the card
carrying icon, type and name.

## 2026-08-07d — An insertion line you can aim at, and Blender's drag card

### The gap was five pixels wide

Reordering worked from the first version, but the band that meant "between these
two rows" was a quarter of the row height capped at 6px — about five pixels on a
normal row. The gesture existed and was effectively unreachable. It is a **third**
of the row now, top and bottom, which is Blender's split: the middle parents, the
edges reorder.

### The line

Drawn at the gap, indented to where the row's own text starts so it reads as
"between these two children" rather than as a rule across the dock, with a round
cap on the left end — a bare hairline gets lost against the row grid. Qt's own
drop indicator is not an option: it is set inside
`QAbstractItemView::dragMoveEvent`, which asks `canDropMimeData()` first, and
`NifModel` does not implement it.

The check counts **accent-coloured pixels in a grab of the viewport** along the
line's row, because `wwDropLineY` only says the view was told to draw one. 846
pixels; the difference between a marker the user can see and a variable that got
set.

### The drag card

Blender's, and for the same reason: the modifier hint and the name of what is
being moved belong in one place, next to the cursor, and the hint has to follow
Ctrl and Shift while the drag is in the air. A `QToolTip` cannot — it re-shows
and fades on every reposition, which reads as shimmering, and it takes no
styling. The `QDrag` pixmap cannot either: it is snapshotted at `exec()`.

So it is a frameless label that follows the cursor, in the skin's card colours,
with the hint muted on top and the dragged name bold underneath — the same
pattern the delegate's reference-drag ghost already uses. The drag pixmap is
gone with it: the card carries the name, and a second ghost beside it was the
same information drawn twice.

The card's second line comes from the **payload**, not from where the drag
started, so it always describes what is actually being carried.

## 2026-08-07c — Renaming no longer drags the list sideways

`renameBlockListIndex` called `scrollTo()` to put the row on screen, and
`scrollTo()` ensures visibility on **both** axes. The name lives in the
right-hand column, so starting a rename scrolled the list across far enough to
push the block **type** off the left edge — the one thing you need to still see
while you are renaming something.

Vertical only now: the row still has to be on screen, but the horizontal
position is put back where the user had it, once after the scroll and again
after `show()` (giving a child widget focus inside a scroll area is its own
reason to scroll).

The check widens the Name column first so the list genuinely has somewhere to
scroll — with both columns fitting there is nothing to get wrong and it would
pass on the broken code. Removing the fix fails it; leaving the widening out
does not.

## 2026-08-07b — The drag never started, and two things in the way of the name

All three reported against the batch below, within minutes of it landing.

### The drag never started

`QAbstractItemView` refuses to enter `DraggingState` unless the **model** reports
`Qt::ItemIsDragEnabled` for the pressed index. That flag appeared nowhere in this
codebase, so `startDrag()` was never called and nothing happened when you dragged
a row — with every piece below `startDrag` working perfectly. `NifModel::flags`
now adds it for block rows, which the proxy forwards, so both list modes get it.

**The harness passed 26 of 26 on that.** It drove the drop handlers directly,
which was the right way to reach code Qt's drag routing hides — and it meant the
one step that was broken sat entirely outside what was measured. It now checks
the flag on both models, `dragEnabled` on the view, and puts a real press and
move through the viewport with the drag hook swapped for a counting one, because
the production hook ends in `QDrag::exec()` and would block forever with nobody
at the mouse. Removing the flag again fails three of those checks; that is how
they were shown to bite rather than assumed to.

### Double-click edited a number

Qt's default edit triggers (`DoubleClicked | SelectedClicked`) opened the
**delegate's** editor on the same double-click that starts the inline rename, so
two editors landed on one cell and the delegate's was on top. A block row's Value
cell buddies to that block's `Name`, which in a Bethesda file is a
`tStringIndex` — an integer — so what you got was a spin over the raw string
index. The block list is a block browser, not a field editor, and it has its own
rename: `NoEditTriggers` now, and a check that every line edit in the viewport
belongs to the rename editor.

### The txt icon down the whole Value column

`spEditStringIndex` is `instant()` and applicable to every `tStringIndex`, and
every block row's Value cell buddies to its `Name` — so it matched every single
row, drew its txt icon in all of them, and a click cast a string-index dialog
nobody asked for. The Block List's delegate suppresses instant-spell icons now;
Block Details, where the icon marks the few fields that have one, is untouched.
The two views already get their own delegate instance, so this needed no new
plumbing.

### Reorder by dragging

New, asked for at the same time. Dropping in the **gap** between two rows moves
the block to that position in the parent's `Children` array instead of
re-parenting onto a row — dragging a block up or down among its siblings. An
insertion line is drawn at the gap (Qt's own drop indicator is set only inside
`QAbstractItemView::dragMoveEvent`, which asks `canDropMimeData()` first and so
never appears here), the row fill is *not* shown, because the gap is what is
being pointed at, and a drop that lands where the block already is is refused
rather than pushing an empty undo step.

Hierarchy mode only, and deliberately: the flat list is the file's block order,
not anyone's children, so a gap there names no position in any array.

`wwReparentBlocks` takes a position now. It is resolved against the array as the
user saw it — removing a block that sat above the insertion point shifts it back
by one — and several blocks land consecutively in the order they were dragged.

`block_dragdrop.sh` is 38 checks; `block_rename.sh` 19 per mode.

## 2026-08-07 — Drag a block onto a node, and the rename that was already there

### Drag to move

Rows in the Block List drag onto a `NiNode` to re-parent. Blender's Outliner
tooltip is the whole specification — "Move inside collection (Ctrl to link,
Shift to parent)" — and the one place it does not map is that Blender has two
separate things where a NIF has one. A collection is organisational and never
changes an object's world transform; parenting is transform-level. A NIF has
only `NiNode` children, so the two collapse into one operation and the
distinction is re-cast as **what happens to the transform**:

| gesture | meaning |
|---|---|
| plain drop | re-parent **preserving world position** — nothing appears to move |
| **Shift** | re-parent keeping the **local** transform, so the block snaps into the new parent's space |
| **Ctrl** | **link** — add the child link and leave the old one, so the block has two parents |

A multi-selection drags as one payload, the ghost reads "3 objects", the target
row lights up in the amber plate, and the hint at the cursor names the other two
modifiers. Refusals say which: a block on itself, on its own descendant, on
something that is not a `NiNode`, or where the link already exists.

The operation and every refusal live in `wwReparentBlocks` in `blocks.cpp`,
hoisted out of the Collision Manager's **Set Parent** rather than written twice.
That settles the question its own comment left open — Set Parent stays
`KeepLocal`, which is right for attaching collision to a bone, and the block
list's plain drop is the world-preserving one. Every world transform is read
before anything is written, so dragging a parent and its own child in the same
selection comes out right.

### Three things the drag cost, worth writing down

**A drag event cannot be delivered with `QApplication::sendEvent`.**
`QApplication::notify` routes drag and drop through the drag manager, so a
synthetic one reaches neither the widget's `event()` nor any event filter —
measured at zero, sent to the view and to the viewport both. The first
implementation put the drop half in an event filter on the viewport and it
silently did nothing; the handlers are `NifTreeView` overrides now, which is the
path a real drag takes anyway, and `wwDeliverDragEvent` gives the harness an
entry point that begins where Qt's routing ends.

**The heap corruption was a stale incremental build**, exactly as the handoff
warns. Three widely-included headers had gained members; a dozen incremental
builds later the rename harness was dying half way through with a freed-block
write. `make clean` after re-running qmake: 25 of 25. That landmine is real, and
it cost an hour of reading perfectly correct code.

**`make clean` then breaks the build.** qmake writes the `icon_res.o` rule with
an absolute target path and lists the object with a relative one, so a full
rebuild stops at "No rule to make target". `windres -i res/icon.rc -o
GeneratedFiles/.obj/icon_res.o --include-dir=./res` once, then build.

### The rename was not missing

The backlog filed inline rename as not started, with a note to try it first. It
shipped in `d5765c4`: F2 and double-click, an editor in place, Enter commits,
Escape cancels, and `wwPropagateNodeName` carries the name into
`NiDefaultAVObjectPalette` entries and controller sequences. Nothing measured
it, which is how it came to be filed as absent.

It had a real gap. `renameBlockListIndex` returned unless the index came from
the proxy, so **in flat list mode F2 and double-click did nothing at all** —
silently, and on the mode a type filter switches you into automatically. The two
models do not even agree on which column the name is in: the proxy has three and
uses 1, the flat list is `NifModel`'s own and uses `ValueCol`. Both are handled
now.

### Harnesses

`block_dragdrop.sh` — 26 checks. The discriminating pair is plain-vs-Shift on
the *same* two blocks: plain drop must leave the block where it was in the
world, Shift must move it by exactly the new parent's offset. One implementation
cannot satisfy both. A third check catches "it did not move" being satisfied by
nothing having happened, and the target's offset is set by the harness, because
with an identity parent the pair measures nothing.

`block_rename.sh` — 15 checks per mode, gating hierarchy. The one that matters
is the palette entry: renaming the node alone passes every other check and
silently stops the animation binding to it.

Both build their fixture from the starter document, so neither needs a game
corpus. The drag one drives real drag events at the view; the rename one fires
the F2 `QShortcut`'s own signal and the view's `doubleClicked` — and takes the
shortcut **parented to the list**, because the Rigging dock installs an F2 of
its own and taking whichever came last drove the wrong widget.

### Filed, not fixed: the flat list mode

Two things found while covering it, neither caused by this change and both
reproduced with the drag-and-drop wiring disabled:

- **Blocks inserted while the flat list is showing are not addressable.**
  `visualRect` draws a row at y=140 and `indexAt` at that point returns nothing,
  so the view's item list is stale against the model. Hierarchy mode is fine.
- **The flat list intermittently takes the process down** — roughly one run in
  three, and 4 in 5 when switching into it. Running the flat list first dies
  before any rename happens, so it is neither rename's nor drag-and-drop's.

Rename in flat list mode is verified, 15 of 15 repeatedly, but not gated:
`MODES="hierarchy list" bash tests/spells/block_rename.sh` runs it.

## 2026-08-05r — The second body that did nothing, and creating from the row menu

### "Nothing happens" was accurate

`tlCreateCollisionBody` returns the body **existing or new** — a `NiAVObject`
holds exactly one Collision Object, so a second one on the same node cannot be
made. It handed back the body already there, the selection moved to a row that
was already selected, and the button appeared to do nothing whatsoever. The
return being *valid* is why the not-usable message added earlier never covered
it: that one only fires when the node is unusable, and this node was fine.

Asked before the call now, because after it the answer is always yes. Both ways
out are named, since neither is guessable from a refusal: shapes go on the body
you already have, and a separate body needs a node of its own.

### Create from the row menu

**Create Collision Body…** and **Create Collision Shape…** are in the Collision
Manager's right-click menu, at the top, routed through the existing buttons
rather than a second copy of the create logic — they already position the popup
and carry the enable rules, and a duplicate is how the popup and the panel came
to disagree about the preset in the first place.

**The menu no longer bails out on empty space.** It returned early when
`itemAt()` missed, which made it unreachable in an empty file — the one state
where creating is the only thing you can do. Everything needing a row is
disabled rather than absent, so the menu reads the same either way. `Expand
Shapes` was dereferencing the row unconditionally while it was there, which that
early return had been hiding.

### Keep the source mesh

A checkbox in the shape popup, **off** by default, which is what creating
collision from a mesh has always done — it consumes it, and the standing advice
was "duplicate it first if you want it kept". That is a footgun a checkbox
removes.

Gated inside `collisionConsumeSource` (havok.cpp), the one function every create
path funnels through, rather than at the call sites: a per-caller flag is how
you end up with two that disagree.

Measured both ways rather than eyeballed, using a check that already existed.
`collision_per_shape` asserts "the meshes that became collision are gone". With
the setting off it passes; with it on that exact check **fails**, which is the
proof the toggle does something — a setting that quietly did nothing would leave
it passing in both runs.

### Parent column, re-parenting, and Mesh to Collision

The list has a **Parent** column: column 0 is the node that owns the collision
object, and this is what that node hangs under. Two bodies on identically-named
nodes were told apart by nothing else, and it is the thing you have to know
before moving one. Filled in `setItemRoles`, the one call the compiled and
editable row builders both make, so those two cannot drift.

Eight columns did not fit 560px and the scroll bar came back, so the dock
minimum is re-measured at 640 rather than nudged.

**Set Parent from Block List** moves the body's node under the block selected
over there. It refuses four things and says which each time — nothing selected,
a parent that is not a NiNode, the node itself, and any descendant of it, that
last one because it would cut the branch out of the file and leave a cycle. The
old parent's `Children` array is rebuilt rather than blanked, since dropping a
link leaves `Num Children` still claiming an entry that points at nothing: the
dangling child link `collisionConsumeSource` already documents.

The **local transform is left alone**, so a body moves in world space when the
new parent sits somewhere else. Right for attaching collision to a bone, wrong
for tidying a hierarchy. Preserving world position is a separate decision and is
deliberately not made here.

**Mesh to Collision…** ticks shape mode 4 and opens the shape popup, so it goes
through `idToggled` exactly as clicking the Mesh button would — writing the
setting directly would leave the popup showing the previous shape.

### A check that was counting when it should have been naming

Adding the keep-mesh checkbox failed `the shape popup holds only what a shape
holds`, which asserted "exactly one checkbox". The rule it defends is real —
Material is the only shape property in that popup — but the count was standing
in for it, and would have been satisfied just as well by a body property
arriving as a checkbox the moment Replace was renamed or moved. It names both
expected boxes now, and logs them, which is stricter than what it replaced.

## 2026-08-05q — The startup view now actually applies

The previous entry shipped the startup view inert and said it worked. It did
not: NIFs still opened dead-on Front, reported immediately.

**Changing a default does nothing once the value is on disk.** The Render
settings page writes *every* field, so anyone who has opened it once already has
`Settings/Render/General/Camera/Startup Direction = 1` stored, and the default
in the code is never consulted again. The value was sitting in the registry the
whole time.

It cost three wrong guesses — a hard-coded `ViewFront` in a physics harness,
`setOrientation( viewState() )` on load, and `restoreUi` ticking the Front
action — each read carefully and each innocent. What settled it in one run was
recording the fact instead of inferring it: `WW_CAMERA_LOG=<file>` appends every
reorientation with its rotation and state, and the first line said
`ctor Rot=-90,0,180 view=5`. Straight from settings, before anything else ran.
Kept, because the next camera question will want it.

Fixed by moving the stored value once, only when it is the old default, behind a
marker so it is a one-shot — choose Front deliberately afterwards and it stays.
Verified end to end: `ctor Rot=-63.5593, 0, 133.308 view=8`, the stored setting
reads 6, and the capture is the three-quarter view.

Also: `restoreUi` holds a **second copy** of the same logic against the same key
with its own default of 1, so on a fresh profile it ticked Front in the View
menu while the camera was elsewhere. Defaults match now. It knows only the three
axis directions; User Perspective ticks nothing, which is honest, because the
camera is then not on an axis.

Two corrections to the previous entry while I am here. `setOrientation( ViewUser )`
does **not** disturb the existing saved user view — that path goes through
`saveUserView`/`loadUserView` and `setRotation`, not through here. And
`setOrientation( ogl->viewState(), true )` after a load, the "reframe on the new
contents", cannot ever reframe: it passes the current state, hits the
`state == view` early return, and never recenters. Left alone, noted here.

## 2026-08-05p — Saved presets, a dock that fits, and Blender's opening view

Five things, all asked for.

### The Collision Manager stops folding

The list has seven columns — Node, Bone, Shape, Layer, Material, Mass, State —
and nothing claimed any width, so the dock took whatever the main-window layout
offered. At about 290px that is four columns and a scroll bar, with Material and
Mass off the end and no sign that they exist.

A minimum, not a resize on show: `restoreState()` replays a saved dock width and
would put the narrow one straight back. A minimum outranks the replay.

**On the scroll area as well as the panel, and that is the whole trick.** A
`QScrollArea` with `widgetResizable` does not adopt its child's minimum —
scrolling is its answer to not having the room. The first attempt set it on the
panel alone and the dock still squeezed to 80px, which screenshotted identically
to having changed nothing.

### Custom collision presets

`+` saves what the fields are showing, `−` removes the selected one, and a
double click renames it in place. They live in the settings under
`CollisionManager/Presets/<name>`, one group each, holding exactly the keys
`tlCollisionPresetDefaults()` returns for a built-in — so a saved preset is
applied by the same three lines that apply a built-in, and no second code path
can disagree about what a preset means. The key set is taken from that function
rather than typed out, so a field added to the body later is picked up instead
of being silently missing.

Keyed by NAME, because a rename has to survive. Built-ins keep their integer ids
and customs carry a `QString`, and every read tests which it got — including
`saveCreationSettings`, where `toInt()` on a name is 0, which is Static, and
would have quietly rewritten the built-in fallback every time a custom was
picked.

**Why the popup now stays open on a single click.** `itemClicked` fires on mouse
RELEASE, so a popup that closes there is gone before the second press of a
double click can arrive — the rename gesture was not awkward, it was
unreachable. The alternative was delaying every pick by the double-click
interval to find out whether a second click was coming. Staying open costs one
Escape and no lag, so: click selects and applies, double click renames, Escape
or a click outside dismisses. Opt-in, via `setKeepOpen`; layer and material are
pick-and-go and keep closing.

### Create Collision Body says why it did nothing

It already parented under the block-list selection and already messaged on an
empty one. What it did not do was speak up when the selection existed but could
not hold a body: `tlCollisionAttachNode` returned an invalid index, the loop
skipped the block, and the button appeared to do nothing whatsoever — nothing in
the panel, the viewport or the block list changed, so the only available reading
was that the feature was broken. Named by block number rather than counted,
because "1 block was skipped" does not say which one to fix.

### Create Collision Shape accepts a body from the block list

The gate read this panel's own list and nothing else, so selecting a
bhkRigidBody in the block list left the button greyed with a tooltip telling you
to select a body — which is what you had just done. The creation path never had
the problem; it casts against `currentSource()`, the block list index. Only the
enable test was wrong, and the two now agree. A body is reached from whichever
of the three you clicked: the body, its collision object, or the node.

### NIFs open on Blender's view

Startup Direction gains a seventh entry, User Perspective, and it is the
default. The six before it are axis-aligned, and an axis-aligned start hides the
thing you opened the file to see — Front is a flat square until you orbit. Kept
as an addition rather than repointing Front, which would have silently moved
everyone who chose Front deliberately.

Neither of Blender's numbers transfers as written, and the fork's own
`viewRotations` table is what says so:

- **X** — Top is 0 in both, but this program's Front is -90 where Blender's is
  +90, so the tilt off Top is negated: `-63.5593`.
- **Z** — Blender measures its `46.6919°` from its own front, which is Z 0. Here
  the ring is Back 0, Right 90, Front 180, Left 270. Carrying `46.6919` across
  unaltered lands between Back and Right and opens every model showing its back.
  The value is `133.3081`.

`ViewUser` is a real destination now rather than the label for "somebody
orbited", and `WW_RENDER_VIEW` accepts it. It could not before: the bound was
`> ViewWalk`, so asking for index 8 was silently rewritten to Front and produced
a capture that looked like a passing test of a view it had never used. That is
exactly the "check satisfied by something other than the fix" trap in HANDOFF,
and it caught the first attempt at this — the capture looked fine and proved
nothing.

The world-axis gizmo does not mirror Blender's exactly; matching it would
require the other 180°, which opens models backwards. Framing matches, which is
what was asked for.

### Covered by

`collision_panel.sh`, 35 checks, green — including that nothing is left parented
to the create group without a layout cell, which is the check the new +/− row
had to satisfy. Dock width and the startup view were each verified by capture
(`WW_UI_SHOT_DOCK=CollisionManagerDock`, and `WW_RENDER_VIEW=8` on a head mesh
where front and back are not confusable).

**Not covered:** saving, renaming and removing a preset have no harness yet.
The storage is exercised only by the create popup building itself. Worth one
before the feature is relied on.

## 2026-08-05o — The window-state crash, found

It was never the saved layout. `restoreUi()` restores state before geometry now,
and NifSkope survives its own clean close.

### What was actually wrong

Upstream's `restoreUi()` ran `restoreGeometry()` first, then
`if ( isMaximized() ) QApplication::processEvents();`, then `restoreState()`.
Restoring a layout saved while **maximised**, into a window `restoreGeometry()`
had just flagged maximised but that had not been shown yet, faulted
`0xC0000005` inside `QLayout::addChildWidget`.

Held one variable at a time, four runs:

| saved blob | restored geometry | result |
|---|---|---|
| maximised | maximised | **crash** |
| maximised | normal | fine |
| normal | maximised | fine |
| normal | normal | fine |

Only the both-maximised cell dies. The decisive one is row 2: the *same* blob,
byte for byte, with only the geometry swapped underneath it, restores perfectly.
The two blobs are both 2090 bytes and differ in seven rows of dock sizes — same
items, same structure, nothing corrupt. So "clearing `UI/Window State` fixes it"
was true and completely misleading, and it framed the blob as the culprit for
three sessions running.

Fix: `restoreState()` first, `restoreGeometry()` second, so the state is always
replayed against a plain freshly-constructed window — the two rows above that
are provably safe — and the geometry is applied afterwards, which is the same
resize path as maximising by hand. Qt's documentation suggests geometry first;
this is a deliberate divergence and the table is the reason. The
`processEvents()` is dropped rather than moved: `createWindow()` calls
`restoreUi()` before `show()`, so there was no window for the WM to maximise and
nothing for the pump to settle.

### Why it took three sessions

`saveUi()` writes whatever state the window is in, so poisoning the next launch
needs a maximised window **at close**. Before `648cfa2` that meant a user
maximising by hand — intermittent, and twice written up as unreproducible. Once
NifSkope opened maximised, every clean close armed the next launch: open, close,
open, dead. That is what finally made it catchable, and 648cfa2's own entry,
which called the cause unexplained, can now be closed.

### How it was pinned down

The `.dmp` files WER kept were gone and nothing here has symbols, so the address
came from `gdb -batch -ex run -ex bt` and the names from the DLL's own export
table — nearest export at or below each fault RVA, via `objdump -p`. Frames #0
and #1 landed in `QLayout::addChildWidget`, which is what pointed at layout
replay rather than at the bytes.

Also worth keeping: QSettings does not store these as raw binary. It writes the
UTF-16 string `@ByteArray(<raw>)`, so **payload byte *i* is at blob offset
`22 + 2i`** and every hex dump of one looks like `XX00 XX00`. That is how the
geometry got decoded — magic `0x01D9D0CB`, major 3, then `frame(x1,y1,x2,y2)`
and `normal(x1,y1,x2,y2)` as big-endian `qint32` — and it is what lets a test
seed a window onto a chosen monitor.

### Covered by

`tests/spells/window_state_roundtrip.sh` — open, close while maximised, open
again. Cycle 2 is the assertion; it faults on the old ordering and passes on the
new one. It is the one harness that cannot use `_harness.sh`: `saveUi()` bails
out on any `WW_*` variable, `WW_WINDOW_AT` is one, so the flag that places the
window off the primary monitor also disables the write path under test. It seeds
`UI/Window Geometry` onto the second monitor instead, asserts the window landed
there before going on, and restores the settings key on exit.

Not fixed, and not new: one dock field jitters about ±25px between runs. Two
independent closes on the *old* ordering differ in it by the same amount and in
the same three rows, so it predates this change.

## 2026-08-05n — Opens maximised, and a flaky check made honest

**NifSkope launches maximised.** This window is a viewport, a block list, a
details tree and two or three docks; at whatever size a default geometry picks
it is cramped, and the first thing anyone does is maximise it.
`restoreGeometry` still runs, so un-maximising gives back the size last used
rather than a default one.

Not applied on the `WW_WINDOW_AT` path — that exists to put a harness window on
a second monitor without disturbing whoever is working on the first, and
maximising would undo the placement it just made.

### A check that was a coin flip

`quick_favourites` failed once and passed on the re-run, on the same binary.
"A search result can be pinned from the palette" sampled
`QApplication::activePopupWidget()` **once**, 150ms after asking for the context
menu, and passed or failed depending on whether the menu had opened yet.

That is worse than having no check: a suite that fails intermittently teaches
you to re-run it rather than to look at it. It polls for the popup now, up to a
three-second deadline — and the wait is armed *before* the menu opens, because a
`QMenu::exec` runs its own event loop and nothing queued after the request runs
until the menu closes. Three consecutive green runs.

### On the window-state crash from the last entry

I could not reproduce the poisoning afterwards: a harness run followed by a
plain run is clean, and the second monitor the harness places on does exist, so
neither is the mechanism. What is certain is that clearing `UI/Window State`
fixed a crash that survived every code change, twice. The cause of the bad value
is unexplained and recorded as such rather than dressed up.

If NifSkope ever refuses to start, that value is the first thing to clear.

## 2026-08-05m — Body first, then shapes

Four asks, and the last one is a restructure that follows the block layout
rather than fighting it.

### Create Collision Body, then Create Collision Shape

Only **Material** is genuinely per-shape. `bhkSphereRepShape` carries Material
and Radius; the box, sphere, capsule and convex blocks add their geometry and
nothing else. Every physics value —

> Havok Filter (layer), Inertia Tensor, Center, Mass, Linear/Angular Damping,
> Friction, Restitution, Max Linear/Angular Velocity, Motion System, Deactivator
> Type, Solver Deactivation, Penetration Depth, Quality Type…

— is in `bhkRigidBodyCInfo`, one block up, shared by every shape hung off it.

So one button did two jobs and had to pretend the body settings belonged to the
shape being made. With **Replace** off, where the new shape joins an existing
body's `bhkListShape`, they were a straight lie: the values either did nothing
or silently overwrote that body's own.

Now:

- **Create Collision Body…** — the object, the body, and all the physics. No
  shape. It appears as a row in the list. Several meshes selected still means
  one body each, on a node each.
- **Create Collision Shape…** — the shape type, its Material, and Replace. It
  joins the body selected in the list.

The shape button is **disabled until there is a body**, and the reason is on a
wrapper widget rather than the button: a disabled widget receives no mouse
events, so Qt never delivers it a `ToolTip` event and `setToolTip` on it shows
nothing — silently, in exactly the case the explanation exists for. Three dead
ends get three answers: *Create a collision body first*, *Select a collision
body in the list above*, and *The selected body is compiled — decompile it to
add shapes*.

The body popup also gains **Deactivator type** and **Allowed penetration**,
which were editor-only.

### And three smaller ones

- **Compact drop-downs.** Between four rows and twelve, sized to what is in
  them. They were briefly as tall as the screen allowed — an overreaction to a
  list that looked short and was short because it was the wrong game's table.
- **"Prop (dynamic)" → "Prop"**.
- **Hover highlight** in the drop-downs, distinct from the current row. Needs
  `setMouseTracking` on the list and its viewport, or `::item:hover` never
  matches and the row only lights up while dragging — which is not hovering.

### The crash that was not in this code

Every build of the two-button work segfaulted at startup inside
`NifSkope::restoreUi`. I bisected it for a long time — stubbed the state
updater, pulled the wrapper out of the layout, instrumented `buildUi` to
completion, stashed to a clean baseline and back — and every result pointed at
new widget code.

It was a **poisoned `UI/Window State` in QSettings**. One early crash saved a
dock layout that `restoreState` then replayed on every subsequent launch, of any
build. Clearing that one value made the same binary that had "always crashed"
pass 35 checks unchanged.

Worth recording as a shape of bug: *a crash that survives reverting the change
that appears to cause it is not caused by that change*. The baseline appearing
to run clean was luck about when the bad value was written, and I trusted it.

### Measured

`collision_panel.sh`, 35 checks:

```
  ok   there is a Create Collision Body button
  ok   ...and a Create Collision Shape button
  ok   Create Collision Shape is disabled without a body
  ok   ...and something enabled carries the reason
  ok   ...which is not on the disabled button, where it would never show
  ok   the body popup holds the physics          (6 combos, 8 numbers)
  ok   the shape popup holds only what a shape holds   (1 combo, 0 numbers)
  ok   both popups are labelled for what they make
```

The pair either side of the tooltip is the one worth keeping: the text has to be
on something **enabled**, and *absent* from the disabled button. A check that
only asked "is there a tooltip" would pass on the version that shows nothing.

`collision_undo` (12), `collision_compiled_edit` (7), `collision_per_shape`
(2+8+8).

## 2026-08-05l — One material table, and it is Fallout 4's

bungo: *"This nifskope is for Fallout 4, the default is Fallout 4, do not show
me collision from Oblivion."*

The last entry made **Fallout 4 the fallback** and left the ladder above it. That
was the wrong shape of fix and this is the right one: there is no ladder. The
collision material and layer tables are Fallout 4's, unconditionally.

`materialEnumType()` and `layerEnumType()` are one line each now. A Skyrim or
Oblivion mesh opened here shows Fallout 4 names against its values, which is the
trade this fork already makes everywhere else — the handoff is explicit that the
other games are inherited and unmaintained.

### There were two copies of that ladder

`collisionCreateMaterial()` in `havok.cpp` had its own, which is how the panel
and the code that writes the material could disagree about which game they were
in. Both are gone.

The same function gated custom-material hashing on `getBSVersion() >= 130`, so
typing a name on a versionless document silently produced no material at all.
Fallout 4 permits custom physical-material editor IDs and this is a Fallout 4
program, so the CRC32 runs whatever the header says.

### The default cube was already Fallout 4

`nifCreateStarterScene` calls `createNew( 0x14020007, 12, 130 )` and the harness
confirms the new document really is BS version 130. So the cube was never the
problem — the panel's list had been built while no document existed, when the
ladder still had an answer for that, and nothing brought it back. With one table
there is no moment at which the wrong one can be chosen.

### Measured

```
material rows: create 158, editor 158
Fallout-4-only materials present: 3 of 3
  ok   and they are Fallout 4's, by name
starter cube: BS version 130, material rows 158
  ok   the new document is a Fallout 4 file
  ok   ...and its material list is Fallout 4's too
```

The last two run against the document that provoked this, built into the model
rather than loaded — the previous checks only ever saw a stock Fallout 4 mesh,
which is the one case that was never broken.

**They run LAST, and that is not tidiness.** Placed in the middle they replaced
the model every later check depends on, and the create test came back with no
rigid body: `mass -1, layer 0`. Two failures that read as the product and were
the harness eating its own fixture.

31 checks. `collision_undo` (12), `collision_compiled_edit` (7),
`collision_per_shape` (2+8+8).

## 2026-08-05k — An unknown BS version means Fallout 4, not Oblivion

The last entry fixed *when* the material list is built. This one fixes *what it
decides* — and it is the same bug, one level down.

bungo's tooltip said it outright: **`OB_HAV_MAT_STONE`**. Every name in the
drop-down was Oblivion's, on a document with a BS version of 0. Refreshing the
list on load could never help, because the list was correct for what the code
was asking for.

`materialEnumType()` and `layerEnumType()` walk the BS version down and fall off
the bottom into Oblivion. A BS version of 0 is what you get before a file is
open **and from a document this program has just made** — a new mesh with a
Cube, which is exactly what collision gets authored onto. So the panel offered
Oblivion's 32 materials where Fallout 4 has 157, and nothing on screen said so:
the names are plausible words. Stone, Metal, Water, Snow, Wood. Only the
tooltip's `OB_HAV_MAT_` prefix gave it away.

Unknown now means **Fallout 4**. This fork is Fallout 4 only — the handoff says
so, and the other games' paths are inherited and unmaintained — so the fallback
is the one game it exists for rather than the oldest row in the table. A genuine
Oblivion mesh loses its own names here, which is a trade this fork already makes
everywhere else.

### Measured

The count check from the last entry would not have caught this on a file with no
BS version, because it compares two lists that are wrong together. So the check
now names materials that exist in **no other game's table**:

```
material rows: create 158, editor 158
  ok   it lists this game's materials, not the last game's
Fallout-4-only materials present: 3 of 3
  ok   and they are Fallout 4's, by name
```

`MaterialActorMetalArmoredPower`, `MaterialMetalBarrelTrashCanOffice`,
`MaterialCeramicCoffeeMug`. A list containing those three can only be Fallout
4's; a count can be any long list, and a comparison against a sibling combo can
be two stale lists agreeing with each other.

**Stated, not proven:** the harness loads a real Fallout 4 file, so it exercises
the version-130 path. The version-0 fallback is a one-line default and is not
covered — the harness has no way to open the versionless document that provoked
this.

29 checks. `collision_undo` (12), `collision_compiled_edit` (7),
`collision_per_shape` (2+8+8).

### Also: the drop-down is as tall as the screen allows

157 materials behind a fixed 320px popup showed fifteen at a time, and a thin
scroll bar at the edge is the only thing that said there were more. The list now
takes what it needs up to 80% of the screen, and the search box says how many
there are to choose from.

## 2026-08-05j — The material list was Oblivion's

bungo: *"Material is missing all the material types in Fo4."* It was showing
**32** materials. Fallout 4 has **157**.

`materialEnumType()` reads `getBSVersion()` to decide which game's table to
offer, and `getBSVersion()` is **0 before a file is opened** — which returns
`OblivionHavokMaterial`. The create-side list was filled once, inline in
`buildUi`, and the panel is constructed before anything is loaded. So it got
Oblivion's list and kept it for the rest of the session, on every file.

The same shape as the layer combo two entries ago, in the one list that was
still being built at construction. It is a member now, refilled by
`populatePhysicsEnums()` — which already runs on `modelReset` for exactly this
reason — and the stored selection is applied afterwards by `loadCreateFields()`.

The create list keys its items by material **name** where the editor's keys by
numeric value, because `CollisionManager/Create/Material` stores a name: a
custom material named in a BGSM has to keep working when nif.xml has never heard
of it. The two conventions are opposite on purpose and the code now says so.

### Measured

The old check asked for **more than 20 rows**, and 32 is more than 20. It passed
on the bug for two builds. It compares against the editor's combo now — the one
that has always been refilled on load — and demands a three-figure list:

```
material rows: create 159, editor 158
  ok   it lists this game's materials, not the last game's
material drop-down: search 1, add-custom 1, rows 159
```

A threshold picked to be safely below the right answer is a threshold the wrong
answer can also clear. Checking one list against another that is known to be
refreshed cannot be satisfied by a stale one.

28 checks. `collision_undo` (12), `collision_compiled_edit` (7),
`collision_per_shape` (2+8+8).

## 2026-08-05i — The material field was still a text box

bungo: *"Material type still only type in, no dropdown with vanilla fo4
materials or search bar, or custom material name."*

`setEditable( true )` at the combo's **creation** site, from the version before
last. I removed the copy further down and never the original. An editable
`QComboBox` is a line edit with a list behind it: clicking it puts a cursor in
the text and opens nothing, so making it a `WwSearchCombo` changed nothing —
`showPopup()` is not what a click on an editable combo calls. Everything added
last time was there and unreachable.

Gone, along with the completer that belonged to the line edit. A stored material
name that matches no row is now added as a real row rather than set as edit text
there is no longer a box for.

### Convex method and Triangles move to the preview

Both belong to shapes that open the live preview on **Create**, and the preview
already carries both — method, a scrubbable Triangles field, hull precision and
the decomposition parameters, all redrawing the actual hull as they change.
Setting a percentage in the popup meant choosing a number blind, pressing
Create, and then being shown the same number again next to the geometry it
produces. The copy attached to the picture is the one worth keeping.

The preview opens at the ratio and method it was last left on rather than
resetting to a full-density hull every time, and its method combo now takes the
same field chrome as the scrub fields it sits between. Box, Sphere and Capsule
have no parameters of their own, so Create still makes those outright.

### Measured

The check that should have caught this asked whether the field was **editable**
— and passed for a whole build on the leftover `setEditable( true )`, which was
itself the bug. It opens the drop-down and looks inside now:

```
  ok   the material field is not a text box
material rows: 33
  ok   it lists the vanilla materials
material drop-down: search 1, add-custom 1, rows 33
  ok   the drop-down has a search box
  ok   ...an add-a-custom-material row
  ok   ...and the materials under it
```

That is three checks where there was one, and none of them can be satisfied by
the state that fooled the last one.

One more self-inflicted miss: opening the material drop-down puts an "add a
custom material" `QPushButton` inside the popup's own child tree, which the
"Optimize Source Mesh is not a separate button" check counted. It skips buttons
under a `QComboBox` now — a button in a drop-down is not a button on the panel.

28 checks. `collision_undo` (12), `collision_compiled_edit` (7),
`collision_per_shape` (2+8+8).

## 2026-08-05h — The number fields were the wrong species, and four more

bungo spotted it from a screenshot and asked me to guess: **mass, friction,
restitution, both dampings and both max velocities were plain
`QDoubleSpinBox`es.** Qt stepper arrows, no drag — six inches above the
*identical seven fields* in the body editor, which do get `wwMakeScrubField`
(`collisiontools.cpp:2795`). The same quantity was two different species of
control depending on which half of the panel you were in. There is one number
field in this fork and I did not use it.

That is the landmine the handoff names by name: the scrub gesture comes from one
place, and bypassing it is how the five private copies that got deleted came to
exist.

### And the "Invalid (0)" it exposed

Motion, Quality and Solver deactivation all read *Invalid (0)*. Zero is the
`INVALID` member of all three tables — `MO_SYS_INVALID`, `MO_QUAL_INVALID`,
`SOLVER_DEACTIVATION_INVALID` — so it is never something anyone picked. It is
what the previous build wrote when it read the create combos before they had any
rows in them, and it is **sitting in the settings of anyone who ran it**. A
stored zero for these three is now treated as unset and the preset's value used,
so the residue heals itself rather than needing the user to know about it.

### Searchable drop-downs

Collision layer is 57 rows and the material list is longer; picking from either
by scrolling is the slow way to do something you already know the name of.
Layer, Preset and Material now open a drop-down with a **search box at the top**.

Qt's own answer is an editable combo with a completer, which was tried here and
removed for good reasons — no drop-down arrow under this stylesheet, a clear
button on a field with no empty state, and grey placeholder text where the
current value should be whenever an edit matched nothing. A search box *inside*
the drop-down leaves the closed field alone.

Material gets a third element between the two: **search box, then "＋ Add a
custom material…", then the vanilla list.** Naming a new one is a thing you do
while looking at the list and finding it is not in there. Leaving the value blank
hashes the name the way the Bethesda exporter does, so a material named here and
the same name in a BGSM come out identical.

### One column, full names

Two columns fitted more on screen and made every row a guess about which label
owned which field — and it forced the abbreviations, so "Max ang. vel." sat
beside "Solver deact." and neither was a phrase anyone says. One field per row,
whole words: *Motion system*, *Quality type*, *Solver deactivation*, *Linear
damping*, *Maximum angular velocity*. The shape buttons stay a row, and now
carry a **Collision Shape** heading.

### Measured

Five new checks in `collision_panel.sh`, 24 total:

```
number fields left as plain spin boxes: 0
  ok   the numbers are scrub fields, like every other number
create enums sitting on INVALID: 0
  ok   motion is a real motion type, not Invalid (0)
  ok   the shape row is labelled
settings grid columns: 2
  ok   the settings are one per row, not two columns
combos with a search box: 3
  ok   layer, preset and material are searchable
```

`wwMakeScrubField` stamps `wwScrubbed` on what it converts, which is what makes
"is this the right kind of field" countable rather than only visible — the
original mistake was invisible to every assertion in the file and took a human
looking at a screenshot.

`WwSearchCombo` carries a `wwSearchable` property for the same reason: this file
has no moc pass, so the class declares no `Q_OBJECT` and `metaObject()` still
reports `QComboBox`. The first version of that check asked `inherits()` and
counted zero on a working build.

`collision_undo` (12), `collision_compiled_edit` (7), `collision_per_shape`
(2+8+8).

## 2026-08-05g — The create popup authors the whole body

Three asks: type a material as well as pick one, put the rest of the collision
settings in the popup, and fold *Optimize Source Mesh* into mesh creation.

### The body settings are on the create side now

The preset used to be the **only** way to reach any of them — a ladder of
if-elses in `applyCollisionBodySettings`, with "Custom" meaning *return early
and leave whatever the block template had*. So authoring a body with a chosen
mass or layer meant creating it wrong and correcting it in the editor below.

The popup now carries the same eleven fields that editor shows — layer, mass,
motion, friction, quality, restitution, solver deactivation, both dampings, both
max velocities — and every one is written as its own setting. The preset is a
button that **fills them in**, not a twelfth value that overrides them; picking
one loads the fields and then stores what the fields say, in that order, so
there is no arrangement where the file disagrees with the panel.

`tlCollisionPresetDefaults()` is the single definition of what each preset
stands for, keyed by the settings each value lives under, shared by the panel
that shows them and the code that writes them.

### Material: pick one or type one

It is editable again, with a placeholder that says so. FO4 accepts any material
name — `collisionCreateMaterial` hashes an unknown one the way the Bethesda
exporter does, lowercase CRC32, and remembers it — so the field has to take
text, not only a row. It could not while it lived in a `QMenu`.

### Optimize Source Mesh is gone, and is not missed

It opened the same live preview **Mesh** already opens, at 50% instead of 100%:
one operation wearing two names, with the second sitting beside Create implying
it did something to the source mesh. It is the triangle percentage the collision
is built at, so it is a **Triangles %** field beside Convex method, shown for the
two shapes built from triangles, and the preview opens at whatever it says.

### The bug this found

Layer 0. The popup's combos were being filled by `populatePhysicsEnums()` called
during construction — where it **returns early**, because it guards on the
physics editor below, which does not exist yet. Every create combo was empty,
`currentData()` came back invalid, and new collision was authored at layer 0:
*Unidentified*, the one value the layer repair spell exists to find. The fill now
runs after the enums are really populated, and again whenever they are refilled.

### Measured

```
  ok   the create layer list has real layers in it
created body: mass 7.5, layer 10
  ok   the mass typed in the popup is the mass the body gets
  ok   ...and the layer picked in the popup is the body's layer
```

7.5 is deliberately a mass no preset produces — Prop gives 10, everything else
gives 0 — so a body arriving at either is one that ignored the field.

**Both values are set through the FIELDS, never by writing the setting.** The
first version of the layer check compared the body against
`CollisionManager/Create/Layer` and passed while both were 0: the broken build
had already written that 0 into `QSettings`, so the harness read the bug back
and agreed with it. A check that trusts stored state cannot see a bug that
poisoned it.

19 checks, run twice back to back. `collision_undo` (12),
`collision_compiled_edit` (7), `collision_per_shape` (2+8+8).

## 2026-08-05f — Create Collision opens a popup, not a dropdown

bungo: *"Rename the button to 'Create Collision'. Do not make it a dropdown
menu, but a popup one with all the options."* Which is the original request read
correctly — "have the relevant settings in that menu" meant **all of them, at
once**, not five submenus.

The split button is gone. One plain **Create Collision…** button opens a popup
panel holding the whole decision:

```
[ Box ] [ Sphere ] [ Capsule ] [ Convex ] [ Mesh ]
Convex method  [ Single Hull (qhull) ▾ ]     (Convex only)
Preset         [ Prop (dynamic)      ▾ ]
Material       [ MaterialMetalSolid  ▾ ]
[x] Replace existing shape
[ Optimize Source Mesh… ]                    [ Create ]
```

Nested submenus buried what the permanent group did well and the only thing it
did well: you could not see the preset and the material at the same time, and
reading either cost two hovers. A panel shows them together — and none of it is
on screen unless it was asked for, which was the point of the declutter.

It also gives the **material picker back its search**. In a `QMenu` it had to be
thirty flat rows, because a combo hosted in a menu only gets the keys the menu
chooses to forward and click-to-focus through a `QWidgetAction` is the flakiest
interaction in Qt. In a plain popup frame it is an ordinary widget again.

The popup opens under the button, clamped to the screen and flipping above it
when there is no room below — not centred on the display, which was the first
idea. The panel is docked at one edge and the thing being operated on is in the
viewport beside it, so the middle of the screen is the one place the eyes are
not. Same reasoning as the search menu opening where the right-click was.

### Measured

`collision_panel.sh` follows the button:

```
button reads: 'Create Collision…'
  ok   the button is the one generic Create Collision
  ok   ...and is not a dropdown
  ok   clicking the button opens the popup
popup shapes: Box | Sphere | Capsule | Convex | Mesh
  ok   all five shapes are in the popup
popup settings: 3 combo(s), 1 tick(s)
  ok   and the settings are visible beside them, not nested
picking Box wrote Shape=0
  ok   picking a shape writes the key the spells read
```

Two things the harness caught on itself, both worth recording:

**It counted the popup's own buttons as panel clutter.** `findChildren` walks
into the popup, because the popup is parented to the panel that owns it — so
*Optimize Source Mesh*, which is supposed to be in there, read as a leftover.
The scan skips anything the popup is an ancestor of.

**It passed once and then failed on the next run.** The shape is restored from
`QSettings` when the panel is built, so the second time round Box was already
the checked button — and clicking the checked member of an exclusive group emits
nothing and writes nothing. The harness now clicks Mesh first, so the Box click
is always a real change. Run twice back to back, both green: that is the
condition it failed.

## 2026-08-05e — Three things the declutter broke

bungo, with a screenshot: "We've got an issue." All three are the same kind of
mistake — deleting a **row** without deleting what was standing in it.

**A save button drawn over the group's title.** `savePreset` was parented to the
creation group and lived in the preset row. The row went; the widget did not.
Qt does not complain about a parented widget with no layout cell — it draws it
at 0,0, which here was on top of the group's own title. It is deleted now:
every menu choice writes through immediately, so it had nothing left to do.

**The primary button had no button around it.** It was styled with
`wwBoxedButtonQss`, which is for *toolbar* buttons — transparent background,
transparent border — so the panel's main action rendered as bare floating text.
And a `MenuButtonPopup` needs `::menu-button` styled explicitly: without it the
arrow half is drawn by the base style over a stylesheet-painted body, which is
the detached sliver that was hanging off the right-hand edge.

**The Creation / Simulation switch lost its selected state.** It borrowed
`createGroup->styleSheet()`, which existed for the five shape buttons that used
to be in that group. They went into a menu, the group stopped carrying a
stylesheet, and the switch quietly lost the orange fill that says which half you
are on — the one thing a segmented control has to do. It has its own copy now.

The group box also lost its title: it read "Collision Creation" one row under a
switch already labelled "Collision Creation", and with the group down to a
single button the frame was most of what was left of it.

### Measured

A new check in `collision_panel.sh`, for the class of bug all three are:

```
CONTROL: with one planted stray, counted 1
  ok   CONTROL: an unplaced widget is detected
visible children of the group with no layout cell: 0
  ok   nothing is left parented to the group but unplaced
```

Every visible child of the group must occupy a cell in the group's layout. The
control plants a stray and confirms it is counted first — "0 unplaced" is also
what a check that cannot count reports, and this whole session has now produced
three vacuous green checks that a control caught.

That check would have failed on the build in the screenshot. The other two are
appearance, which no assertion here reaches; they are stated rather than proven.

`collision_undo` (12) and `collision_per_shape` (2+8+8) re-run.

## 2026-08-05d — Zooming and rotating are rebindable

Asked for by a friend of bungo's, and a fair thing to be missing: `convertKeyCode`
was a switch on the raw key, so the arrows and PageUp/PageDown were the last
hard-coded bindings in the viewport. Forty-odd operators were already rebindable
from **Options ▸ Shortcuts** — the two things someone arriving from another
program most wants to change were the only ones it could not offer.

Twelve entries now, in two new categories:

| | |
|---|---|
| **3D Viewport – Navigation** | Rotate View Up / Down / Left / Right, Zoom In, Zoom Out |
| **3D Viewport – Fly / Walk** | Move Forward / Back / Left / Right / Up / Down |

Defaults unchanged: arrows, PageUp/PageDown, WASD + Q/E. The fly keys stay
inert outside free camera and walk view, so the letters remain free for the
Blender-style transforms everywhere else.

The modal transform's own keys (M, J, K, I, O, Shift) stay hard-coded on
purpose — they are read while a drag is running rather than looked up as
bindings, and rebinding them from a settings page would move them out from under
the code that reads them.

### The bug this introduced, and the check that holds it

These bindings latch a bit in `kbdState` on key press and clear it on release.
The moment one can be rebound to a **combination**, the release can arrive with
the modifier already let go — release Shift before Z and an exact match never
fires, the bit is never cleared, and **the view keeps moving on its own**. So a
release matches on the key alone, and the harness releases the modifier first
and then measures whether the camera is still travelling.

### Measured

`tests/spells/nav_keys.sh` (`WW_NAVKEYS_TEST`):

```
navigation bindings registered: 12 of 12
  ok   rotate, zoom and the fly keys are all rebindable
  ok   ...and zoom still defaults to where it was
CONTROL: PageUp on the default binding moved 147.996
  ok   CONTROL: the default zoom key moves the camera
after rebind, PageUp moved 0
  ok   the key it was bound to no longer zooms
the rebound Shift+Z moved 69.5806
  ok   the key it was rebound to zooms
after release with the modifier let go first, drifted 0
  ok   letting go stops the camera, modifier released first
```

**The control is the whole reason this is trustworthy.** The first two attempts
reported "the old key no longer zooms" and "letting go stops the camera" as
passes while the camera was not moving at all: the pump spun `processEvents` for
microseconds and `advanceGears` steps on real elapsed time, and then the
measurement read `cameraDistance()` when zoom changes `Zoom`, not `Dist`. Two
green checks, both vacuous, on a feature that had never been exercised. Proving
the default binding moves the camera *before* rebinding anything is what turned
them into measurements.

## 2026-08-05c — The Collision Manager loses twenty controls

bungo: "How could we simplify this menu? Or declutter it?" Three moves, none of
which removes a capability.

### Collision Creation: eleven controls become one button

Five shape buttons, a convex-method combo, a preset combo and its save button,
an expander hiding a material picker and a Replace tick, an Optimize button and
a Create button — all permanently on screen, in a docked panel that is mostly
the list above it. Of those, the method line applies to **one** of the five
shapes, Optimize to two, and the rest are defaults you set once and forget.

Now one split button: click **Create <Shape> Collision** to make one, or open
its menu to pick a different shape or reach the defaults. The button names the
shape it will make, so the menu is never needed just to find out what clicking
does. Picking a shape from the menu makes it, the way an Add menu behaves.

Two things went with it:

- **The save button.** Every menu choice writes through immediately. The state
  where the panel showed one preset and the next Create used another because
  nobody pressed save is gone — a menu you ticked is a decision, and asking for
  it to be confirmed by a second control was the panel not believing the first.
- **The source hint.** "Select a BSTriShape or NiNode in the viewport/block
  list" was fixed text that never changed: an instruction, permanently, on a
  panel you have by then already used.

Material is menu rows rather than the combo in a `QWidgetAction`. A hosted
widget in a `QMenu` only gets the keys the menu chooses to forward, and
click-to-focus through one is the flakiest interaction in Qt — the same reason
the command palette is a dialog and not a menu with a line edit in it. The combo
survives as the *model* the menu is built from, so the material list with its
CRC values and tooltips is still loaded in one place.

### The display row moves to Overlays

*Show collision*, *Colour by*, *Solid*, *X-ray*, *Only*, *Labels* are viewport
state — not one of them changes the file — so they belong with the rest of "what
the viewport draws on top of the model". They are now **Overlays ▸ Collision
Display**.

*Show collision* was outright a second face for `ui->aShowCollision`, which sits
in that same Overlays menu and always has: the dock carried a duplicate of a
toggle two clicks away, and could not keep it in sync. Same `QSettings` keys
throughout, so the drawing code reads exactly what it read before.

### Row operations move to the row

*Decompile* / *Compile* / *Import Donor* all act on the row selected in the list
directly above them, and that list has had a right-click menu offering the same
operations the whole time — so the buttons were a second way to do what pointing
at the thing already does, taking a permanent row to do it. *Check Collision* is
file-wide and stays a button, under **More**, with the other file-wide entries.

Both routes run one implementation: the split button's menu and the row menu are
built from the same `QAction`s.

### Measured

`tests/spells/collision_panel.sh` (`WW_COLLPANEL_TEST`). A refactor that only
*moves* things has one failure a build cannot catch — a control lands somewhere
that no longer writes what the old one wrote, and the settings the create spells
read quietly stop changing. Nothing on screen says so: the menu ticks, the
button looks armed, and the next Create makes the shape you picked last week. So
the checks come in pairs, each removal against where it went:

```
create menu: Box | Sphere | Capsule | Convex | Mesh | Convex Method | Preset |
             Material for New Collision | Replace Existing Shape | Optimize…
  ok   all five shapes are in the one menu
  ok   so are the settings that used to be rows
  ok   the button says which shape it will make      ('Create Box Collision')
  ok   picking a shape writes the key the spells read            (Shape=0)
  ok   the row-operation buttons are off the panel
  ok   the display row is off the panel
  ok   the display controls are in the Overlays menu             (5 entries)
  ok   a moved display control still writes its key            (ColourBy=2)
  ok   the inventory tree still offers a row menu
```

"Gone" on its own would pass for a control that was simply deleted, which is the
failure this exists to catch.

This harness flushes its log per line, unlike the others. It drives menu entries
that run operations, and an operation that puts up a modal with nobody at the
keyboard hangs until the wrapper's timeout kills it — at which point a buffered
`QTextStream` has written nothing and a zero-byte log says only "something went
wrong somewhere". It cost two rounds of exactly that to find the fixture had
compiled collision on its root, which `attachCollisionShape` refuses on a modal.

`collision_undo` (12), `collision_compiled_edit` (7) and `collision_per_shape`
(2 + 8 + 8) re-run: they drive the panel this rebuilt.

## 2026-08-05b — Quick Favourites, and Space opens the search menu

Blender's, and it earns its place the same way there: the things one person
reaches for twenty times an hour are not the things the next person does, and no
menu layout can be right for both. A pinned list is the user saying which those
are.

- **Space** in the viewport opens the search menu.
- **Right-click any menu entry** → *Add to Quick Favourites*. Also on a row in
  the search menu, which is where you find the thing whose name you did not know
  and therefore where you decide it is worth keeping.
- **Q** opens Quick Favourites.
- Both keys are named actions, so **Options ▸ Shortcuts** lists them and either
  can be rebound — the pane already collects every named action of the window
  that carries a sequence.

Animation playback moves off Space to **Shift+Space**, keeping a Space-shaped
binding rather than being scattered to an unrelated key.

### What a favourite is

An ID, never a pointer. Menus are built per right-click and per selection, so
the `QAction` a favourite names does not exist between one press of Q and the
next; it is resolved fresh each time.

```
spell:Page/Name      resolved through SpellBook::lookup
action:objectName    found on the main window by name
```

Anything with neither is **not offerable**, and the right-click says so rather
than doing nothing — an action built inline for one menu cannot be found again,
and a favourite that silently does nothing is worse than one never offered.

A favourite that does not apply to the current selection is **left out**, not
greyed. The list is a shortlist assembled by hand, and padding it with rows that
cannot run defeats the point of having made it short. The palette greys instead,
because there the greyed row is the answer to "why is this not in the menu".

### The search menu now searches the program, not the spell registry

Opened from the viewport, "the function for this mode" means Edit Mode's
operators, the paint tools, the transform gizmos — none of which are spells, all
of which are menu actions. It collects the window's menu bar as well as the
book, deduplicated by action. Availability comes for free: a mode's actions are
disabled outside it, and the palette already sorts what applies above what does
not and says which is which.

### Two single keys, and why neither is window-wide

A bare Space or Q on a `WindowShortcut` is matched **before** the key reaches
the focus widget, so binding either one window-wide would eat the space bar and
the letter Q in every line edit in the program — the material search, the block
filter, a rename. Both are scoped to the viewport, which is also how Blender's
per-editor keymaps behave.

That is not sufficient on its own. Two viewport modes already own those keys:
the physics sim pauses on Space, and the walk camera descends on Q. A QAction
shortcut still wins over a widget's `keyPressEvent`, so both would have gone
quiet with nothing on screen to say why. `GLView::viewportClaimsKey` reports
when a mode owns the key and the window reserves it during `ShortcutOverride`,
which hands it to the mode as an ordinary keystroke — `ShortcutOverride` only,
since consuming the `KeyPress` too would swallow the keystroke being protected.

### Measured

`tests/spells/quick_favourites.sh` (`WW_QUICKFAV_TEST`), which forces its own
store and puts back what it found — favourites live in `QSettings`, so a run
that inherited a real list would measure that list:

```
Space=Space Q=Q play=Shift+Space
  ok   Search is on Space
  ok   Quick Favourites is on Q
  ok   playback moved off Space to Shift+Space
  ok   ...and neither single key is window-wide, which would eat it everywhere
ids: spell='spell:/Edit String Offset' action='action:aQuickFavourites' anonymous=''
  ok   a spell is stored by page and name
  ok   a named action is stored by object name
  ok   an unnameable action is not offerable
  ok   pinning stores it
Q on the collision object -> Decompile Compiled Collision
  ok   Q lists a pinned spell that applies
Q on a block it does not apply to -> No favourite applies to this selection
  ok   a favourite that does not apply is left out, not greyed
  ok   an empty menu says how to fill it
  ok   right-clicking a menu entry offers to pin it
  ok   the search menu finds menu actions, not only spells
  ok   a search result can be pinned from the palette
16 checks, 0 failures
```

The pair either side of "applies" is the one worth keeping: the same pin, two
selections, listed on one and absent on the other. Asserting the keys *with
their context* is the other — "Space opens search" passing on its own would not
have caught a binding that also eats every space bar in the program.

## 2026-08-05a — Converting a mesh consumes it; two pickers; the search popup

Four things bungo asked for, and two bugs that turned up underneath them.

### Converting a mesh to collision consumes the mesh

It used to leave both, so every conversion shipped a rendered copy of the shape
sitting inside its own collision hull — on a proxy built for the purpose, pure
weight in the file. Wanting it kept is the rarer case and has an obvious answer:
duplicate the shape and convert the copy.

Only when the source **is** a mesh. A `NiNode` source converts everything
beneath it, and emptying a whole branch is a much larger action than the one
asked for; the node is also not itself a mesh. Combined with per-shape bodies
this is the shape a collision-only file wants: three shapes in, three `NiNode`s
out, each named for the mesh it replaced, standing where it stood, each carrying
its own body and nothing else.

### Two bugs found while measuring it

**`spRemoveBranch` reads the block-list selection.** It calls
`spellSelectionRoots` like any branch spell, so with three shapes selected the
first consume removed all three — at the block *numbers* published before any of
this ran, which by then pointed at whatever had moved into those rows. The run
came out with two bodies instead of three, one wrapper node missing entirely
with its body inside it, and a dangling child link on the root.

This was already latent in the per-shape change: `attachCollisionShape` drops a
replaced shape the same way. `castCollisionOverSelection` now reads the
selection once and takes it down for the run, restoring it on the way out.

**Removing a block does not shorten the arrays that pointed at it.** The link is
blanked, so the node the mesh hung under kept claiming a child it no longer had.
Consuming a source now compacts its parent's `Children`.

Both are held by checks: the body count is what the first one broke, and
"the consumed mesh leaves no dangling child link" is the second.

### Collision layer and Material of this body are plain pickers now

They were editable combo boxes, for type-to-search over ~57 layers and ~30
materials. It cost more than it bought:

- an editable `QComboBox` draws no drop-down arrow under this stylesheet, so the
  two controls at the top of the form did not look like the four below them and
  did not look clickable;
- the clear button put an **✕** on a field with no empty state — a body always
  has a layer;
- an unmatched edit left grey placeholder text where the body's material should
  be, which reads as "no material" on a body that has one.

Non-editable still type-aheads: Qt jumps to an item on its first letters.

The **+** that named a custom material goes with it. It was a second way to do
something the Create group already does — type a name into *Material for new
collision* and `collisionCreateMaterial` hashes it the way the Bethesda exporter
does (lowercase CRC32) and remembers it, after which it is in this list like any
other. One route, on the side that creates things.

### The search popup behaves like the menu it replaces

Opened from a right-click and dismissed like anything opened from a right-click:
**Escape**, a click outside, a right-click elsewhere. It had none of those. It
was an application-modal dialog, and modality *discards* clicks outside before
anything can act on them — so it is a `Qt::Popup` now, which holds the mouse
grab and delivers those presses to the palette, where `PaletteDismiss` judges
them on their global position.

Escape closed it only when the field was empty. The old reasoning — a half-typed
search is not a thing to lose to one keystroke — is real but wrong here: a popup
that eats the first Escape reads as a popup that will not close. Retyping four
characters is cheaper than that doubt.

It also opens **where the click was**, not in the middle of the window, clamped
so a right-click near the bottom edge does not put 420px of palette off the end
of the screen. And it no longer lists **Search…**, the row that opens it — that
row is an action on the book like any other, so it collected itself.

### Measured

`tests/spells/spell_search.sh`, four new checks on top of the ten it had:

```
rows labelled 'Search…': unmarked 1, marked 0
  ok   CONTROL: an unmarked Search row is listed
  ok   marking it keeps it out of its own list
  ok   Escape closes it even with a query typed
  ok   a click outside closes it
asked for 120,120 -> opened at 120,120
  ok   it opens at the point it was given
```

The dismissal checks synthesise the real gesture and record whether *it* closed
the palette; the fallback `reject()` exists only so a failure is a failed check
rather than a harness stuck in a modal loop. The Search-row check counts an
exact label against a control run of the same action without the marker — the
first version asked whether any row *started with* "Search", which another entry
also does, and it read as a failure when the skip was working.

`tests/spells/collision_per_shape.sh` gains the consume checks, and
`collision_undo.sh` and `collision_compiled_edit.sh` were re-run because the two
pickers they drive are the ones that changed:

```
  ok   the meshes that became collision are gone
  ok   each node is named for the shape it replaced
  ok   ...and stands where that shape stood
  ok   the consumed mesh leaves no dangling child link
12 checks, 0 failures      collision_undo
 7 checks, 0 failures      collision_compiled_edit
```

## 2026-08-05 — Three shapes selected, three collision bodies

bungo selected three meshes, pressed **Create Collision**, and got one body. The
report was "I want to turn them all into separate collision objects, not one and
the same one."

A spell is handed **one** index. Create is reached from a context menu and from
the Collision Manager's button, and both hand it the current block — so the
other two selected shapes were not merged into that body, they were simply
never looked at. Nothing said so: the panel reported one body afterwards, and
one body is what the file had. This is the same defect `spellSelectionRoots` was
written for when Remove Branch and Duplicate Branch silently ignored a
multi-selection; the create spells just never consulted it.

### Where a second body can go

A `NiAVObject` holds exactly **one** `Collision Object`, so three shapes under
one root cannot carry three bodies there — a second create replaces the first,
or absorbs it into a `bhkListShape`, which is one body wearing three shapes.
Each body needs a target of its own.

A `BSTriShape` has the field, so it looks like the obvious target. It is not
what the game does. Measured over 489 sampled stock FO4 meshes carrying compiled
collision, resolving every collision object's `Target`:

```
625 bodies   ->  625 NiNode
                   0 BSTriShape
```

No exceptions, and 40 of those files carry more than one body. What they do
instead is wrap: `NCA1x1StairsCorner01` hangs its second body on a `NiNode`
`'Point003'` holding the shape, and `BunIntElevatorOut01` gives each moving door
leaf the same treatment so the animation has a node to drive. Both put the
transform on the node and leave the shape at identity — node `'Door'` carries
the offset, shape `'Door:12'` sits at zero.

So `collisionAttachNode()` follows that: a shape sharing its parent with others
gets a `NiNode` inserted between it and that parent, named after it, carrying
its transform and its flags, with the shape reset to identity beneath. Flags are
copied rather than invented, which matches the static case exactly (`Point003`
and its shape are both `14`); the `0x80` the elevator's nodes carry comes with
being animated, which is the author's to add.

**A shape already alone under its parent is not wrapped** — that parent already
is its own node. So selecting three shapes that each live under their own node
produces three bodies and no new blocks, and a single selection behaves exactly
as it always has.

### Two things that would have made it wrong

**Gathering after moving.** A `Node`'s id is assigned when the Scene builds it
and does not follow the renumbering an insert causes. Creating the first body
inserts blocks, so gathering the third shape's mesh afterwards would read it
through a stale id and quietly return the wrong triangles. Every source is
gathered before that source is touched, and `collisionSourceSpace()` answers
with `node->id()` — the id of the Node it is about to read — rather than
computing a block number that may already be stale. The selection is held as
persistent indices for the same reason.

**Applying the transform twice.** Handing the new node the shape's transform
without clearing the shape's own doubles every wrapped shape's offset from the
root. Invisible on a file where the shapes sit at zero, which is most of them.

### Measured

`tests/spells/collision_per_shape.sh` (`WW_COLLPERSHAPE_TEST`), on
`ArcadeShootGalleryBase01.nif` — a stock mesh whose root has three `BSTriShape`
children and no collision of its own. It runs three times. The **control** is the
same binary with the block-list selection never published, which is the path the
old code took on every run:

```
--- control: no selection published
collision objects created: 1
  ok   CONTROL: with no selection published, one shape makes one body
--- three shapes selected, menu spell (Box)
collision objects created: 3
  ok   three selected shapes make three collision objects
  ok   each body targets a different node
  ok   every body targets a NiNode, never a shape
  ok   each new node holds exactly the shape it was made from
  ok   wrapping left every shape where it was in the world
--- three shapes selected, preview commit (Mesh)
collision objects created: 3
  ok   ... the same six
```

Both entry points, because they are different ones: Box goes in as a menu spell,
while the Manager's **Mesh** and **Convex** buttons open the preview and Apply
calls `tlCommitCollisionPreview`. Mesh is the mode this was reported from.

One body against three, same binary and same fixture, is what makes the count a
measurement rather than an assertion — and it cannot be satisfied by a
`bhkListShape` either, which is what turning "Replace" off would have produced.
Check 6 is the doubled transform. Check 4 is the corpus result held as an
invariant.

The fixture has to be one with **no** existing collision. `attachCollisionShape`
refuses to add editable shapes to a node that already carries compiled
collision, and it refuses on a modal — the first fixture tried had a compiled
body on the root, and the control, which targets the root, hung on that dialog
instead of measuring anything. The multi-shape run never hit it, because every
body was going onto a node that had just been created. Worth knowing: on a mesh
that already has compiled collision, per-shape create works where whole-mesh
create stops and asks you to decompile first.

Applies to all five shape kinds (Box, Sphere, Capsule, Convex, Mesh) and to the
context-menu entries as well as the Collision Manager button, because the loop
sits in the spells rather than in the panel. The Manager's preview follows the
same rule, so what it draws is the set of bodies Apply is about to write, not
one of them.

## 2026-08-04a — The four rigging steps come back to the menu

bungo reported spells missing, naming *Generate CustomizationRemapData*. It was
not missing — `3294710` (the menu taxonomy batch) gave it and its three siblings
`menuHidden()`, so they existed, cast fine, and appeared nowhere you would look
for them.

The reasoning at the time was sound and the fix was not. A flat context menu
lets you run step 4 before step 1, and the Rigging Manager's numbered buttons
order and gate them properly. But the problem was that a flat list makes the
**order invisible**, and removing the entries does not make the order visible —
it makes the feature invisible.

`label()` already exists for exactly this: it is the menu text while `name()`
stays the id. So they are back, numbered:

```
1. Generate CustomizationRemapData
2. Import Donor Bone Nodes...
3. Bind Donor Bones (existing nodes)...
4. Transfer Weights (existing bones)...
```

Ids are untouched, so `castSpell( "Rigging/Generate CustomizationRemapData" )`,
the CLI and `_tools/animate_securitycamera.sh` all keep working. No
`menuHidden()` remains anywhere in `src/spells/`.

### Audited the whole registry while here

Comparing `REGISTER_SPELL` against `upstream-base` for anything else lost:

- **`spCreateCVS`** — deregistered in `d5765c4`, but **not lost**. The class is
  the engine behind Create Collision and Create Accurate Mesh Collision
  (`havok.cpp` 1172–1255 call `spCreateCVS::createConvexShapes` directly). Its
  own menu entry was superseded, not its code.
- **`spFixSkeleton` / `spScanSkeleton`** ("Fix Bip01" / "Scan Bip01") — genuinely
  deleted, in `3294710`, with the reasoning recorded in `skeleton.cpp`: both
  gated to NIF 4.0.0.2 with a root block named `Bip01` (Morrowind), both bound to
  `:/res/skel.dat`; the authoring half opened that Qt resource **WriteOnly**,
  which cannot succeed, so its `REGISTER_SPELL` had been commented out upstream
  for as long as the file existed. That left Fix Bip01 consuming a 6 KB blob
  nothing in a shipping build could regenerate, for a game this fork does not
  target. Still a deletion rather than a deprecation, and recoverable from
  `upstream-base` if it is ever wanted.

Nothing else differs.

### "Create a decal from a mesh" does not exist here

Searched the current tree and `upstream-base`: the only decal spells are
**Add Decal 0–3 Map**, which attach decal *texture slots* to a
`BSShaderTextureSet`, and all four are present and unchanged. No spell has ever
generated decal geometry from a mesh in this codebase or its parent.

## 2026-08-03f — CustomizationRemapNewBonesData, decoded and answered

`Rigging ▸ Sync Remap New Bones` closes the gap the faceBones work left open.

### The format

Measured across all 399 vanilla faceBones files:

- record size is exactly **208 bytes**
- record count == (highest index the remap blob uses) − (bones in the list) + 1
- the block is present **iff** that count ≥ 1 — exactly **173 of 395**, sizes
  137×208, 18×416, 5×624, 2×832, 11×1872 (= 208 × 9)

| offset | what |
|---|---|
| `0x00` | NUL-terminated bone name — **not always `Neck`**; BigBeard01 says `HEAD` |
| `0x18`–`0x7f` | leaked memory, not data |
| `0x80` | bounding sphere, 4× float32 |
| `0x90` | 4×4 row-major affine — rows unit length, determinant 1.0000 |

Only **65 of 208 bytes are constant** across 295 records. The rest holds values
like `0x77851a05` and `0x000f3748` — Windows DLL and heap addresses — and files
exported in one batch share identical garbage: both base heads carry
`48 37 0f 00`, every BigBeard `30 03 27 00`. So roughly half of each record is
uninitialised memory from Bethesda's exporter.

**That is why nothing is synthesised.** A fabricated record would be a guess
dressed as data. Records are copied from a donor instead, which is at least
genuinely what the game shipped, and the dialog says plainly that the transforms
are the donor's.

### The more useful answer: our files do not need one

The block exists because Bethesda's faceBones bone list **replaces** the standard
bones with sculpt bones, leaving the blob pointing past the end. Create faceBones
NIF **appends** and never drops a bone, so its indices always resolve.

That was a plausible-sounding argument, so it is now measured. Harness checks
J and K, on the file the spell actually writes:

```
our output:    highest remap index  9, bone list 69   -> resolves
vanilla donor: highest remap index 68, bone list 68   -> overruns by exactly 1
```

K matters more than J. Without it, J would pass on any file whatsoever and prove
nothing about the pipeline; requiring the donor to *fail* the same test is what
gives it force. And the donor's overrun of exactly 1 predicts a 208-byte block,
which is exactly what it ships.

So the success dialog no longer warns vaguely that NifSkope "does not yet
generate" the block. It reports the two numbers and states whether one is needed
— and for a mesh built by this pipeline, it is not.

`Sync Remap New Bones` handles the other directions too: a stale block on a mesh
that no longer needs one can be cleared, a correct one is confirmed with the
bone names it describes, and a missing one is copied from a donor of the user's
choosing. It refuses a donor without enough records rather than truncating,
noting that only 173 of 395 vanilla faceBones meshes carry the block at all.

Harness now 11 checks, all green.

## 2026-08-03e — Check Face Rig, and two statistics that did not work

`Rigging ▸ Check Face Rig` looks for the one failure a weight transfer can
produce that every existing check passes: **a vertex holding the wrong bone
index**. Its weight record stays perfectly well formed — weights sum to 1, no
repeated index, no empty slot — so `Validate FO4 Skin` sees nothing. What breaks
is geometry: a face bone drives a small patch of skin, so a mis-indexed vertex
sits far from that patch and tears across the face when the bone moves.

### Two wrong statistics, both caught by measuring

**First attempt — "further than 4x this bone's own median".** Measured over 19
vanilla faceBones meshes it flagged **six of them**, Hair17 alone with 16
strays, while missing 1 and 3 deliberately mis-indexed vertices entirely. Hair
and hat rigs legitimately drive scattered vertices, so any fixed multiple is
tuned to whichever mesh happened to be open when it was picked. A check that
fires on a third of healthy files trains you to ignore it.

**Second attempt — "median influence 3x the donor's".** Correct instinct (compare
against the donor, since the same bone on two heads governs the same patch, and
a naturally sprawling hairline bone sprawls on both), wrong statistic. It was
built, compiled, and then measured:

| mis-indexed vertices | median-3x | beyond-donor-max |
|---:|---:|---:|
| 0 | 0 | 0 |
| 1 | 0 | 0 |
| 3 | 0 | 1 |
| 10 | 0 | 8 |
| 30 | 0 | 27 |
| 100 | **0** | 78 |

**Zero at every level.** The median is robust to outliers, and a handful of
outliers is the entire thing being looked for. It would have shipped as a check
that never fires — which looks identical to a clean bill of health.

### What shipped

For each sculpt bone shared with the donor, count target vertices reaching
further from the bone than *any* of the donor's do, +25% for honest proportion
differences between two faces. No magic multiplier: the threshold comes from the
donor.

Residual false-positive rate is stated in the dialog rather than hidden — a
healthy **male** head measured against the **female** donor yields about seven,
and an unrelated beard against the same donor yields **zero** across 46 shared
bones. So the number is a magnitude to read, not a boolean. Without a donor the
spell says outright that it is a profile and not a verdict.

### Where it lives

Beside `Validate FO4 Skin` on the Rigging page for now. It belongs in a **Face**
workspace with the expression preview and `.tri` morph sliders, and moves there
when that dock is built — shipping half a workspace to give it a nicer home
would have meant verifying neither.

### While measuring: FO4 faces use three systems, not two

`.tri` files are `FRTRI003`. `BaseFemaleHead.tri` holds **50 morphs** —
`Pucker`, `LSmile`, `RFrown`, `RJaw`, `UprLipFunnel`, `StickyLips` — the talking
and expression shapes. `BaseFemaleHeadChargen.tri` holds a separate **41** —
`EyesFeature1..10`, `LipFeature1..8`, `Overbite`, `Underbite`.

So sculpt sliders are bone-driven (`skin_bone_*` + CustomizationRemapData),
while **talking and emotes are morph-driven and touch no bones at all**. A
talking preview would therefore look perfect on a faceBones mesh rigged
completely wrong, which is why the bone check above drives bones instead.
Morph vertex counts match the *normal* head (1689), not the faceBones one.

## 2026-08-03d — Create faceBones NIF

`Rigging ▸ Create faceBones NIF...` builds a `_faceBones` sibling from a mesh
rigged to the standard skeleton. Pick the donor — `BaseFemaleHead_faceBones.nif`,
`BaseMaleHead_faceBones.nif`, or a creature's — and it writes
`<name>_faceBones.nif` next to the original.

### It is a wrapper, deliberately

The transformation already existed and was already tested: Transfer Bones and
Weights with a faceBones donor generates the RemapData snapshot **first** when
the donor carries sculpt bones, then imports and binds the `skin_bone_*` nodes
and transfers weights, rolling back as a unit on failure. Reimplementing any of
that would have been the wrong kind of new code.

What it could not do is produce a *separate file*. It edits in place, so making
a faceBones variant meant transfer → Save As → Undo to get the original back.
This runs the same spell against a serialized copy and writes the result out.

The copy is not just tidiness. **It makes a wrong donor free.** Pick a
standard-rigged mesh by mistake and the transfer runs, gets judged, and is
discarded — no Undo, no reload, and no file on disk. So the spell checks the
*result* rather than the user's intent: no `skin_bone_*` bones in the output
means it is not a faceBones mesh, and it refuses to write one under that name.

### The check that needed a harness

`tests/rigging/facebones.sh`, `WW_FACEBONES_TEST`, 9 checks, all green.

Eight of them are ordinary. The ninth is the reason the file exists:

> **G: RemapData equals the SOURCE's standard skinning**
> **H: ...and is NOT the output's own sculpt skinning**

Generate the snapshot *after* the transfer instead of before and you get a blob
of exactly the right length, structurally valid, that passes every other check
here and looks perfect in every view NifSkope has — and is exactly wrong, because
it records the sculpt weights instead of the animation skeleton's, which is the
one thing RemapData exists to carry. Ordering is the whole correctness argument,
and ordering is what survives a refactor by luck.

H is there because G alone can pass vacuously: if the transfer silently did
nothing, the output's skinning would equal the source's and both comparisons
would agree. Requiring them to **disagree** is what gives G its teeth.

Measured on `BaseFemaleHead.nif` → 10 bones becomes 69, 59 of them sculpt bones,
**RemapData 20268 bytes** — the same size as the blob Bethesda ships in
`BaseFemaleHead_faceBones.nif`, which `tools/rigging_prototype/encode_test.py`
independently reproduces byte-for-byte.

### Two test seams, for the same reason as the first one

`WW_TEST_FACEBONES_OUT` joins the existing `WW_TEST_DONOR`. Both bypass **native**
file dialogs, which no `QTimer` can drive — the driver can accept a `QInputDialog`
and click through a `QMessageBox`, but `getSaveFileName` is the OS's window.

### The harness hung, and it was the documented trap

First run: 9/9 in the log, then exit 124 from `timeout`. The scoped `QTimer`
driving the modals dies with the lambda, and the save-on-quit prompt appears
*after* it — so `quit()` opened a dialog with nobody left to answer. Handed over
to an app-owned answerer that outlives the scope and deliberately never touches
the log stream, since that and its `QFile` are already gone by then. Clean exit
in 43 s.

### Known gap, stated in the UI rather than buried here

Vanilla faceBones assets carry **two** extra-data blocks:

```
NiBinaryExtraData -> 'CustomizationRemapData'
NiBinaryExtraData -> 'CustomizationRemapNewBonesData'
```

Confirmed present on both `BaseFemaleHead_faceBones.nif` and
`BaseMaleHead_faceBones.nif`. NifSkope writes the first and has **no handling of
the second anywhere in `src/`**.

It exists because Bethesda's faceBones bone list *omits* `Neck` while the blob
still references it, at index 68, past the end of the list — `NewBonesData`
supplies those dropped bones and their transforms. This pipeline is append-only
and never drops a bone, so every index it writes stays inside the final bone
list, which suggests it may not need the block to be self-consistent. But
"self-consistent" and "what the engine expects" are different claims and only
the second one matters. The success dialog says so and tells the user to
validate in game.

## 2026-08-03c — 0.2, and the repository goes public

First public push, as `bungov/NifSkope-WildWastelandEdition`. Nothing about the
program's behaviour changed; this is the packaging.

### Two version numbers, and why

`WW_VER = 0.2` now lives in `NifSkope.pro` and reaches the code as
`WW_EDITION_VERSION`. It is deliberately **not** `build/VERSION`, which stays at
upstream's `2.0.dev11`.

The reason is `applicationName`. It is `"NifSkope " + rawToMajMin(NIFSKOPE_VERSION)`
— `"NifSkope 2.0"` — and that string is the **QSettings key**. Renaming it to
carry the edition would silently move every existing user's settings to a path
nothing reads. `NifSkope::migrateSettings` also compares against
`NIFSKOPE_VERSION` to decide what to carry forward, so that number has to keep
tracking upstream rather than us.

So the edition lives on `applicationDisplayName` and the About title, and the
two numbers are shown together where a bug report would need them:
*About NifSkope - Wild Wasteland Edition 0.2 (on NifSkope 2.0.dev11, revision …)*.
Cutting 0.3 is one line in `NifSkope.pro`.

Verified in the linked binary, not just the source: `Wild Wasteland Edition 0.2`
appears once as UTF-16LE (the `QStringLiteral`), `0.1` not at all.

### README is generated — which the build had to teach me twice

`QMAKE_PRE_LINK` builds `README.md` from `build/README.md.in` by substituting
`@VERSION@`. Editing `README.md` works right up until the next link, then
reverts. The rewrite went into the `.in` file, and the rule gained a second
token, `@WWVERSION@`.

The `READMES` list in the same file then broke the build — it copies docs next
to the exe, and `README_GLTF.md` had moved to `docs/`. Worth recording because
the failure came *after* a successful compile and link: `make` reported the exe
as up to date on the retry and the post-link step never re-ran, so the fix
needed a forced relink to prove itself. A green incremental `make` is not proof
that the post-link steps pass.

### The description says Fallout 4 and nothing else

Upstream's README opens with seven games. This one names one. That is a scope
statement, not a claim of removal — the inherited format support for other games
is still compiled in, and `WW_FEATURES.md` says so in as many words rather than
leaving someone to discover it. No work here targets them and nothing here is
tested against them.

### Two new documents

- **`WW_FEATURES.md`** — everything the fork adds against `upstream-base`
  (fo76utils `develop` @ `f2587869`), in fourteen sections. The counts are
  measured, not estimated: 382 commits, `src/` +112,219 / −7,902 over 145 files,
  57 new source files, spells 158 → 204, 53 rebindable shortcuts, 60 harnesses.
  It ends with a "Not in this edition" section, because a feature list that only
  lists wins is a sales page.
- **`HANDOFF.md`** — replaces `CURRENT_STATUS.md`, which was twelve days stale
  and described a working agreement that had since been superseded twice. Build
  commands, layout, the landmines (CRLF files, stale incremental builds, the
  `QMenu` icon/check column, event-filter ordering, `QToolButton::sizeHint`).

### Repository scrub

- Four inherited GitHub Actions workflows removed, plus `.travis.yml` and
  `appveyor.yml`. They are fo76utils' release pipeline: they fetch CoACD and
  NifMopp from *their* releases and push tags. On another account they either
  fail or publish releases nobody asked for.
- `.gitmodules` removed. It declared five submodules — qhull, gli, meshoptimizer,
  kfmxml, nifxml — but **no gitlinks are tracked**; all five are vendored as
  real files. It was inert, and it told contributors to clone with
  `--recurse-submodules` for nothing.
- `out.txt`, `err.txt`, `build_rigging.log`, `tools/link.log` and a committed
  `tools/test_hkdecode.exe` untracked and gitignored.
- 22 plan/audit/reference documents moved to `docs/`. Source comments cite them
  by bare filename, which still resolves — the names are unique.

### A downloadable build, and why `release/` is not it

`scripts/package.sh` stages a distributable tree into `dist/` and refuses to
finish if anything scratch reaches it.

The naive version of this task is "zip `release/`". That directory was **120 MB,
of which about 100 MB had no business in a download**: 60-odd `ww_*.log` harness
outputs, 30-odd screenshots, `ls_*.png` loading-screen renders, scratch `.bin`
files, `RiggingIntegration.exe`, and `NifSkope_backup_pre_details_20260720.exe`
— a five-week-old build that a user could easily have run by mistake. The
packaged tree is 116 files, 94 MB staged, **37 MB zipped**.

The file list is derived from the copy rules in `NifSkope.pro` rather than from
whatever happens to be sitting in `release/`, so it stays right as long as new
runtime assets are added in both places. The script says so at the top, because
that coupling is the thing that will rot.

Two things checked before publishing, neither assumed:

- **The baked revision.** `NIFSKOPE_REVISION` arrives as a `-D` on the compile
  line, and make compares timestamps, not command lines — so after committing,
  the exe still carried the *previous* hash. `main.cpp` and `about_dialog.cpp`
  have to be touched or the About box lies about which build it is.
- **Self-containment.** Ran the packaged CLI with `PATH` cut down to
  `C:\Windows\system32;C:\Windows`, from the staged directory. It parsed an
  80-block NIF and exited 0, which proves the exe, every DLL and `nif.xml`
  resolve out of the folder and not out of the MSYS2 toolchain that built them.
  (A first attempt appeared to fail with exit −1; that was PowerShell tearing
  down the pipeline at `Select-Object -First 4`, not the program. Worth
  re-running before believing an exit code that comes through a truncated pipe.)

Built at `-j2`. This machine has hard-shut-down under sustained all-core load.

### The `.gitattributes` that was doing nothing

It said `*.cpp eol=auto`. **`eol=auto` is not a value git recognises** — the
valid forms are `text`, `text eol=lf`, `text eol=crlf`, `-text` and
`text=auto`. So the file had no effect at all, and the five deliberately-CRLF
files (`glview.cpp`, `nifskope.cpp`, `gl/controllers.cpp`, `spells/havok.cpp`,
`WW_CHANGES.md`) survived on nothing but `core.autocrlf=false` being set in this
one clone.

That is fine for a private repository with one clone. It is not fine for a
public one: Git for Windows defaults `core.autocrlf` to **true**, so the first
contributor to clone would have had those files rewritten on checkout and their
first commit would have carried a 33,000-line diff that changes no code — the
exact failure this project has hit repeatedly from the other direction.

Replaced with `* -text`. Checked before committing that it renormalises nothing:
the only changed blob is `.gitattributes` itself, and all five files are still
stored CRLF.


bungo asked for the Move field's behaviour — hover arrows, press and drag the
number — on every type-in field. The gesture already existed **five times**:

| where | class |
|---|---|
| `nifskope_ui.cpp:183` | `DragSpinBox` — the original, and the best |
| `uvtools.cpp:96` | `UVDragSpinBox` |
| `spells/collisiontools.cpp:345` | `CollisionDragSpinBox` |
| `ui/widgets/colorwheel.cpp:105` | `ColorDragSpinBox` |
| `ui/widgets/valueedit.cpp:72` | `WwScrubFilter` |

**The cause was scope, not design.** `DragSpinBox` sat in an anonymous namespace
inside a `.cpp`, physically unreachable from anywhere else, so every new panel
wrote its own — and they drifted. `src/wwskin.h` records the same disease and the
same cure; this is that cure applied to the number field.

All five are deleted. There is one: `src/ui/widgets/wwnumberfield.{h,cpp}`.

### The check came first, deliberately

`tests/spells/scrub_uniform.sh` was committed one commit *before* the fix, so its
failures are recorded against the unfixed binary rather than asserted afterwards.
What the old build actually did:

| | before | after |
|---|---|---|
| copies of the gesture | **5 files** carry the scrub cursor | 1 |
| the same `VectorEdit`, two ways | direct **0**, via ValueEdit **0.5** | 5 and 5 |
| `<float_max>` + a 3 px drag | **`inf`** | `<float_max>` |
| Move field step *(control)* | 50 px → 5.0 | unchanged |

The `inf` was the real find, and it was not a consistency nit. `FloatEdit` spells
±`FLT_MAX` as a word; Block Details scaled per-pixel step by magnitude, so on that
value it was `3.4e38 × 0.005` = **1.7e36 per pixel**. Three pixels of accidental
drag destroyed the field. Measured, not predicted.

The B row is the control and the one number that must not move: Move X/Y/Z never
calls `setSingleStep`, so it inherits Qt's 1.0 and scrubs at 0.1 units/px.
`WwNumberField` deliberately does not call it either.

### Two shells, one mechanism

Neither holds gesture or painting code; both just construct a `WwScrub` and a
`WwScrubChrome`.

- **`WwNumberField`** — a subclass, because eight viewport key guards test
  `inherits("QAbstractSpinBox")` to decide whether a keystroke belongs to the 3D
  view or a text field, because Designer promotion needs a class name, and
  because `SettingsPane` persistence `qobject_cast`s on `QDoubleSpinBox`.
- **`wwMakeScrubField`** — a retro-fit, because `FloatEdit` is a `QLineEdit` and
  whole forms come out of `setupUi`.

The chrome is a child widget rather than a `paintEvent` override, because a
`QObject` filter cannot paint over what it watches — which is exactly why Block
Details had no arrows at all, on the largest field population in the program.

### Coverage

~370 value fields. About 55 scrubbed before; the rest now do, except ~60 excluded
**by design**, each with its reason at the call site: file paths, version strings,
hex and CRC entry, bitmasks, enum ordinals, indices into other structures, and
integers with four-billion spans where no per-pixel step can both traverse the
range and express a value.

The highest-leverage single edit was moving the attach into `VectorEdit` /
`ColorEdit` / `RotationEdit`'s **own** constructors: 36 fields across Transform
Edit, Light, Material and Matrix4 were the identical widget, live in Block Details
and dead everywhere else, decided only by who called `new`.

Steps matter as much as attachment. The field scrubs by `singleStep`, so anything
left at Qt's default would move mass by a kilo per ten pixels. The collision body
fields got steps chosen for what they measure — which also fixed their arrow and
keyboard steps, until now inconsistent with the drag.

### Four things I broke and fixed

1. **The number sat on top of the arrows.** The original held its line edit out of
   the two gutters in `resizeEvent`; the port dropped it. One omission, two
   symptoms — the value painted over the glyphs *and* covered the click zones, so
   the arrows were decorative. The inset now lives in the chrome and corrects the
   editor's **own** Resize, because a filter runs before the target's handler.
2. **A locked field did not look locked.** Setting `color` in a stylesheet
   overrides the palette's Disabled role, so read-only numbers painted bright next
   to greyed-out combos.
3. **Move stopped scrubbing.** I added a passthrough so a *focused* field's presses
   reached the line edit for drag-select — but a plain click focuses the field, so
   from the first click onward it could never be scrubbed again. Traded the
   primary interaction for a secondary one, on the field the feature is named
   after. Always-arm is back; clicking an already-focused field places the caret.
4. **Selected numbers were blue.** The selection colour was `bgBtnDown` (`#355f86`)
   and a plain click selects the whole number, so every field the pointer touched
   came up as a blue block.

### Blender parity

Read the manual's Input Fields page directly. It corrected one thing: Blender's end
arrows step **once per click**, no auto-repeat — so ours already matched and the
"gap" named earlier was not one. Four real ones:

- **Esc and RMB cancel a drag.** We had no cancel at all; once you started pulling,
  the start value was gone and Ctrl+Z was the only way back.
- **Ctrl snaps to whole steps** — the documented companion to Shift's precision.
  Only Shift had ever been implemented.
- **Typed expressions** — `1024/3`, `2^10`, `sqrt(2)`, `pi`, `rad(90)`. A
  hand-written recursive-descent parser, not QJSEngine: that would drag the QML
  stack into a widget and would evaluate anything at all, in a field whose contents
  can come from a file. This grammar has no identifiers beyond its named constants,
  no assignment, and no reach outside itself.
- **One drag sets a whole X/Y/Z row.** Recruitment is by sibling and only of fields
  that already carry the gesture, so a drag wandering off a form cannot grab
  unrelated widgets. Each recruit moves from its own start value.

Also wired: `wwRestyleScrubFields` was written for theme switching and **never
called by anything**, so every field kept whichever theme it was born under — and
they are all built before `loadTheme` runs.

### Selectors, and LOD

The Collision Manager's combos were Qt defaults in a grid of restyled fields — own
frame, own background, native arrow. `wwMatchFieldStyle` gives them the field's
tokens, read from the same `wwSkinColor` names so the two cannot drift apart.

Its tree columns were also wrong: Material was `Stretch`, which only gets the space
the others leave over, so in a docked panel it sat clipped at `U...` — and because
`Stretch` and `ResizeToContents` both disable dragging, it was clipped **and**
frozen. Every column sizes to its text now.

**LOD is a dropdown**, beside Animation and Collision. The slider was the wrong
control twice: it hid its own value behind a handle position, and it vanished
entirely on a file with no LOD meshes — a control that is *absent* reads as a
missing feature rather than as "not applicable here". Always present, greyed with a
tooltip saying why, label naming the level. Level 3 is listed and disabled outside
Starfield, because `Scene::updateLodLevel` clamps to 2 everywhere else — so on a
Fallout 4 file a level-3 row would have looked like a choice and silently done what
level 2 does.

### And the material that was never unknown

bungo asked why a stock body's material read `Unknown (0x26067D15)`. Measured over
150 FO4 files with compiled collision: **body material is `0` on all 157 bodies**,
so the shape's own ID is what reaches the UI; of 250 shape materials over 27
distinct IDs, **only 72% resolve** against nif.xml. The most common material in the
whole sample — `0xFCB37EA0`, 52 uses — is itself unnamed.

So the decoder reads the right field and nif.xml's list has holes. Calling that
"Unknown" made a healthy file look broken. It reads `Unnamed` now, and the tooltip
says the collision data is fine and points at the **+** button that names it.

### Verified

`scrub_uniform` 27 checks, `top_bar` 43, `collision_undo` 12,
`collision_compiled_edit` 7, `menu_taxonomy` 26.

Checks are chosen for the code path a change reaches, not swept: a blanket run of
all fourteen harnesses opens a dozen NifSkope windows and returns no information
about most of them.

## 2026-08-03a — Bug sweep: 22 confirmed defects, patched

A 38-agent sweep across the whole program raised 28 candidates; 22 survived
adversarial refutation and are fixed here. Six were killed by the skeptics and
are not in this list. Where a fix could be measured from the CLI it was
**verified by reverting the one file and re-running** — the numbers below are
what the old code actually produced, not a prediction. `tests/spells/bug_sweep.sh`
(10 checks, no GUI) keeps them from coming back.

### Corruption

| where | what was wrong |
|---|---|
| `basemodel.cpp` `saveToFile` | any non-zero `write()` counted as a complete write, so a short write (disk full, flaky share) reported success and `completeSave` marked the document clean — over a target that `open(WriteOnly)` had already truncated. Now a `QSaveFile` with `setDirectWriteFallback(true)`: full-length write or nothing, and the original survives a failure. The fallback matters here — MO2's VFS can refuse the sibling temp file, and a save that used to work must not start failing. |
| `glview.cpp` `tlCopyItemValues` | a freshly grown `BSVertexData` row leaves its `#ARG#`-conditional `Bone Weights`/`Bone Indices` 0-length, so the copy hit a length mismatch, gave up, and wrote the array item's own empty value. Join had a local workaround; the other **eight** append sites (Extrude, Inset, Duplicate, Split, Rip, Bevel, Symmetrize, and the lerp/barycentric writers behind Knife / Loop Cut / Subdivide / Bridge) did not, so every vertex they created on a skinned FO4 mesh came out weight-0. Fixed once, in the shared copy, which is where Join's own comment said the problem lived. |
| `glview.cpp` segments | `Num Triangles` was maintained by every topology op; `Num Primitives` and the per-slot `Segment` ranges by two of them. A delete left slots indexing past the end of the triangle buffer; a grow left the new faces in no dismemberment slot. Delete now rebuilds the ranges **exactly** from its keep mask (`separateBuildSegments`); everything else re-tiles through a new `tlSyncSegments` called from the three undo commands' `redo()`, with a matching `TlSegmentSnapshot` so undo puts the old table back. |
| `glview.cpp` Loop Cut | the 65,535-vertex recovery floored to `max(1, …)`, so when the budget could not fund one cut per ring edge it proceeded anyway and wrapped both the `ushort` `Num Vertices` and every new corner index. It now refuses, like `tlApplyEdgeCut` and every sibling op already did. |
| `mesh.cpp` Optimize Indices | **measured:** on `MaleBody.nif` block 59 the permutation left one dismemberment slot holding **0 of its 438** original faces and another 20 of 435 — with every count still agreeing, so nothing flagged the file. Segmented shapes now optimize each range in place: boundaries survive, the optimization still runs. |
| `mesh.cpp` Remove Unused Vertices | `Tangents`/`Bitangents` are `length="Num Vertices"` but were never compacted, so the arrays outlived the count and `saveItem` wrote them at full length behind a shorter header — desynchronising the stream on the next read. |
| `blocks.cpp` Paste / Duplicate Branch | `holdUpdates(true)` leaked on the load-failure path. Nothing else clears it, so `updateHeader`/`updateFooter` became permanent no-ops and every later save in that session wrote its blocks behind a stale header. |
| `skeletontools.cpp` delete bone | the "bone is in use" guard checked the selected bone; the operation removes the whole subtree. A weight-free parent over weighted children — `LArm_UpperArm` above `LArm_UpperArm_skin` — walked straight through. The guard now covers the same set the delete touches, and names the offending descendant. |

### Wrong result

| where | what was wrong |
|---|---|
| `obj.cpp` import | over a **selected** BSTriShape — the workflow the dialog advertises — `iData` was bound only for a newly created shape, so the bounds update silently no-op'd and the file kept the previous mesh's Bounding Sphere. |
| `obj.cpp` face parser | a negative texcoord index adjusted `v` instead of `t`: it corrupted the vertex index and left the texcoord negative. A copy-paste slip between two correct neighbours. |
| `collisiontools.cpp` Apply Safe Fixes | read `Motion System` off `filter.parent()`, which on every Skyrim-or-later file is the rigid body — no such child. The read always yielded 0, so "infer Static or Props from motion" could only ever infer Static. |
| `collisiontools.cpp` / `havok.cpp` layer edits | **measured:** `bhkRigidBody` stores the HavokFilter twice and only the `Rigid Body Info` copy was ever written. Casting the layer spell on a body with block-level `Layer 7` / RBInfo `0` left `7/1` — and `glnode.cpp` colours collision by the 7. Now `7/0 → 1/1`, through a shared `bhkSetFilterField` used by all six write sites. |
| `glscene.cpp` bounds | `bounds()` counts only visible nodes, but the cache is invalidated solely by `transform()`, which early-outs while the camera is idle. Hide / Isolate / Alt+H kept the pre-hide extent. New `Scene::invalidateBounds()`, called from every visibility change. |
| `glscene.cpp` `clear()` | `hiddenNodes`, `hiddenTris`, `soloNode` and `restPoseBlock` are keyed by **block number** and nothing reset them, so loading a different NIF re-applied the previous document's hide set to whatever blocks carried those numbers. |
| `nifskope_ui.cpp` / `nifskope.cpp` | a promoted background window never ran `restoreUi()` but did `saveUi()` on close, writing its unrestored default layout over the user's — and its viewport header came up empty. It now restores on promotion, and `saveUi()` is gated on a `uiRestored` flag so a window that never restored can never persist. |
| `nifmodel.cpp` `moveAllNiBlocks` | announced the insert range one row low (the blocks land before the footer, not at `getBlockCount()`), so every persistent index the views held was updated against a layout the model never had. |
| `blocks.cpp` Paste Branch | `getBlockByName` returns -1 for "not found", so 0 is a hit — `else if ( block > 0 )` rejected a valid mapping and aborted the whole paste. The sibling in Duplicate Branch already used `>= 0`. |
| `controllers.cpp` particles | `fadeIn`/`fadeOut` belong to `BSPSysSimpleColorModifier` but were gated on `!hasColorGradient` and never reset, so a system without that modifier applied the class defaults (0.1 / 0.9) — and `emitParticle` evaluates the colour once, at `u = 0`, where fade-in is exactly zero. Every particle was born and stayed fully transparent. |
| `freezeanim.cpp` `--strip` | inserted into the `doomed` `QSet` while iterating it; `std::as_const` suppresses the detach, not the re-bucketing. Collect first, merge after. |

### Checks that could not fail

| where | what was wrong |
|---|---|
| `tangentspace.cpp` | **measured:** "Add Tangent Spaces and Update" tested applicability of a *NiTriShape-filtered* (invalid) index instead of the real block, so it processed nothing on any BSTriShape file — every Skyrim SE and Fallout 4 mesh. Output was **byte-identical** to input (`2ca1eb073323` in, `2ca1eb073323` out). It now changes the file. This spell survived the Unfuck-dialog removal earlier the same day: it was verified *present*, never verified *working*. |
| `mesh.cpp` `getTriShapeData` | resolved the `NiTriShape` Data link into `iData` and then returned an unconditional empty index, discarding it — so Flip Faces, Optimize Indices and Prune Triangles reported themselves inapplicable on every NiTriShape block (Oblivion / FO3 / Skyrim LE). The two halves were in the wrong order. |
| `nifskope_ui.cpp` `WW_TEXCOLOR_TEST` | the broken-path red check read column 0, where `NifModel::data` never returns red. It could not fail whatever the path said. It reads `ValueCol` now, matching the sibling check eleven lines above. |

### Verified

`tests/spells/bug_sweep.sh` — 10 checks, CLI only, no window. Three fixes were
confirmed by reverting their file, rebuilding, and watching the check go red
first: `mesh.cpp` segments, `tangentspace.cpp` no-op, the collision filter copies.

All twelve existing harnesses still pass: `collision_undo` 12, `collision_compile`
12, `collision_compiled_edit` 7, `cycle_type` 12, `particle_cap` 5,
`subtex_flipbook` 4, `top_bar` 36, `menu_taxonomy` 26, `spell_search` 9,
`destructive_confirm` 8, `relative_paths` 6, `unfuck_panel` 0 failures.

## 2026-08-02j — The top bar, rebuilt on Blender's

The row was doing two jobs. Blender keeps an application topbar (File, Edit,
workspace tabs) apart from the 3D viewport's **own header** — mode, the menus
that mode governs, the transform widgets, overlays and shading. Here both were
fused into one strip, so a global menu and a transient viewport tool sat at the
same visual level competing for the same eye.

```
app        File · View · Spells · Options · Help │ Workspaces
                                                 │ Animation · Collision
viewport   Object Mode │ Select Add Object │ Global ⌖ 🧲 ⋮⋮ │ vert edge face
           Brush │ Overlays ⧉ wire Shading
```

The viewport header is a real row above the 3D view, inside the central widget.
The mode and render toolbars are **moved** into it rather than rebuilt — which
is the only reason this was tractable, because `syncViewportMenus` drives the
whole per-mode show/hide table off the `QAction`s that `addWidget` returned, and
rebuilding would have invalidated every one.

**Done after `restoreState`, not at construction.** `restoreState` replays a
layout saved by an older build and drags toolbars back into the toolbar area.
That is not a hypothesis: the toolbar *reorder* attempted earlier the same day
failed silently for exactly that reason — the constructor was right and the
restore overwrote it a moment later, with no error and nothing in the diff.

### What moved, and Blender's reason

| | |
|---|---|
| `Select / Add / Object` | from the far right to directly after the mode selector. They are mode-scoped verbs, and their position teaches you they follow the mode; they were at the opposite end of the row from the mode that governs them. |
| Display options → **Overlays** | Blender's name for this exact dropdown, whose contents are already nearly Blender's list. Moved right, beside Shading. |
| Isolate / Hide / Restore | off their unlabelled struck-through eye and into the **Object** menu, where Blender keeps them (H, Alt+H). They already retexted themselves "Objects"/"Geometry" by mode — already behaving like an Object-menu entry, just not in the menu. |
| Center / Frame Selected | into the **View** menu. And `Render` was renamed `View`, because that is what it holds: Top, Front, Left, Flip, Perspective, Walk, Load/Save View. Nothing in it renders. |
| Undo / Redo | off the bar entirely, into Options. Blender has no undo buttons in any header; these held the most valuable spot on the row for a pair nobody clicks twice. |
| Workspaces | to the menus. Blender keeps workspace tabs in the app topbar — a workspace switches the whole layout, so it is not a viewport control. |
| **Panels** | deleted as a button; its seven dock toggles, Toolbars and the two block-view display submenus all folded into **View**, which is where Blender keeps panel toggles. |
| Lighting Options | the bulb was the only control on the row with **no label and no tooltip** — the `.ui` sets both to empty strings. Its sliders moved into Viewport Shading; the widget was *moved*, not rebuilt, so its `Settings/Render/Lighting/*` keys could not drift. |

`Animation` and `Collision` have no Blender counterpart and stay on the app row.

### Spacing

One helper, `wwGroupBreak`: symmetric 7 px either side of the rule. A bare
`addSeparator` draws a hairline hard against the buttons on both sides, so eight
of them made a picket fence in which every boundary looked equally important.
The rule has to be earned — the trailing `Animation`/`Collision` run has none
inside it, because those are one group and ruling between them said they were
two.

### Five defects, each caught by a different method

**Photographed.** A double rule between Object and Global, introduced by the
spacing pass itself: `wwGroupBreak` wraps each rule in spacer widgets, and the
duplicate-separator cleanup detects adjacency only when *nothing* sits between —
so it silently stopped working and two meeting groups each kept their boundary.
The first fix then over-corrected, stripping both sides and leaving no rule at
all. Leading and trailing are properties of the **row**, not of each toolbar.

**Photographed.** A stretch item after the two header toolbars claimed all the
spare width and left them on their minimum — which for a `QToolBar` is "as small
as you like, I have a chevron". Add, Object and the entire display group
vanished off the ends, silently, with every button still present as far as any
code could tell.

**Measured on pixels.** The Render menu still carried full-colour resource PNGs.
Verified by walking every icon in **both** states and failing if any
non-transparent pixel has R, G, B more than 2 apart: 24 states, none coloured.
With the pass disabled, 15 of 24 — and `Load View(on)` came back fully saturated
while its off state was already grey, which is the case a naive fix misses.

**Reasoned from the API.** A harness check that could not fail: it recovered each
spell with `SpellBook::lookup( action->text() )`, and `lookup` takes a
`"Page/Name"` id — a bare name makes it search for a spell whose `page()` is
empty, and every spell in that menu has one. Always null, so the assertion was
green against an empty set. The `SpellPtr` rides on the action now.

**Found by the recon, before it bit.** Two loops walk `children()` to find
toolbars — the one that fills `View ▸ Toolbars` and the one that sets 16×16
icons. A reparented toolbar is not a direct child, so both would have skipped
the viewport header in silence. The first matters: that submenu is the only way
to bring a hidden toolbar back, because `QMainWindow`'s own toggle popup is
unreachable here (`Qt::NoContextMenu`, nothing overrides `createPopupMenu`).
Losing the entry would have made hiding the header permanent.

### Icons

`NifSkope --icon-sheet FILE.png [filter]` renders every `tlMakeIcon` glyph to a
labelled contact sheet — each at judging size and again at 16 px, the size it is
actually used at — and exits before any window or GL context exists, so it runs
under `-platform offscreen` and takes no focus. The set is drawn with QPainter,
so there was previously no way to look at an icon short of hunting for its
button.

That sheet found four defects at once: `mode_pose` and `mode_physics` did not
exist and were being handed the Object Mode cube, so three of seven menu entries
were the same picture; the mode *button*'s icon ladder was missing those two
rungs, which was invisible precisely because of the first bug; and
`mode_weightpaint` was byte-identical to the plain `brush` glyph.

Six glyphs were then redrawn on Blender's, judged as a set. Two lessons worth
keeping: an orbit — filled circle inside an ellipse — is an **eye** at 16 px, and
adding depth so the ring passes in front and behind made it a better orbit and
still an eye, because at 16 px all that survives is blob-inside-lens. And on the
bone chain, an outline *brighter* than its own fill averages to mid-grey, so
"one of three is lit" collapsed into three equal shapes: **size** carries a
selection through blurring, two dark greys do not.

### Harnesses run off the primary monitor

Running the suites opened NifSkope over the top of whatever was being worked on
and took the keyboard with it. `WW_WINDOW_AT` already existed for exactly this
and nothing used it: it moves the window **before** `show()` and skips
`raise()`. Both halves matter — moving after `show()` flashes it on the primary
monitor and then jumps, and `raise()` takes focus even once the window is out of
the way. `tests/spells/_harness.sh` holds the geometry; the ten windowed
harnesses source it.

`tests/spells/top_bar.sh` is 36 checks. The row-shape assertion reduces each row
to a string — `x|x|xxx`, `x|xxx|xxxx|xxxx|xxxx` — and fails on `||`, on a leading
rule and on a trailing rule, which is the double-bar defect *and* its
over-correction.

## 2026-08-02i — The preview reads numbers the files were already carrying

Four things the file said and the preview ignored. Each one is a field that has
been sitting in every stock mesh since 2015.

**Cycle Type.** `NiControllerSequence` says what it does at its end — LOOP,
REVERSE or CLAMP — and nothing read it. One session-wide Loop checkbox decided
for every sequence in every file, it starts unchecked, and `saveUi`/`restoreUi`
have the line that would persist it commented out. So the default preview played
every clip exactly once, whatever it was authored to do.

| stock FO4 mesh tree | 1,124 files, 3,337 sequences |
|---|---|
| CYCLE_CLAMP | 2,664 |
| CYCLE_LOOP | 673 |
| CYCLE_REVERSE | 0 |
| no cycle type at all | 0 |

Selecting a sequence now sets the Loop toggle from the file and faces playback
forwards; ticking Loop by hand still wins for as long as you stay on that
sequence. CYCLE_REVERSE ping-pongs, which is new behaviour — at the end it turns
round rather than wrapping — and its direction lives in its own `animDir`, not
in the sign of `animSpeed`, which belongs to the Speed and Reverse controls and
is rewritten wholesale the next time either is touched.

`Bloatfly.nif` is the whole argument in one file: `CharFXOn` is CLAMP and
`CharFXOnLoop` is LOOP, so no single setting of one checkbox is right for both.
It also ships `CharFXOffLoop` as CYCLE_CLAMP — the name is not the truth, the
field is, which is why the harness picks its sequences by cycle type and never
by name.

**The particle cap: nobody ever got the number they asked for.**
`PSysSimController` asked the `NiParticleSystem` for `Num Vertices`. No version
of that block has ever had one — the count is a `NiGeometryData` row and lives on
the DATA block, renamed `BS Max Vertices` for `NiPSysData` on Bethesda 20.2. The
read returned 0 on every file in every game and the 512 fallback beneath it
always won. Over 1,345 stock FO4 `NiPSysData` blocks: **not one is 512.** 1,287
authorise fewer, 58 more, smallest 3, largest 38,464. A plume authored for 17
particles previewed with up to 512 — not a busier version of the same effect, a
different one.

**BSPSysSubTexModifier was not implemented.** Every particle picked one random
cell of the atlas at birth and held it until it died, so the 302 stock FO4 meshes
carrying the modifier showed a frozen frame of an animation. Particles now start
at `Start Frame` (+ fudge), advance at `Frame Count` frames a second (+ fudge),
and wrap from `End Frame` back to `Loop Start Frame`.

Frame Count as frames-per-*second* is an inference — nif.xml documents none of
these fields beyond their names. The evidence is that its default is 30.0 and
the authored values cluster on 30, 35, 40, 45, 50, 60, 100, 120: frame rates,
not counts of anything in the file. Nothing is tied to the atlas size, because
the files are not: `End Frame` is left at its 63 default on sheets with 4, 8 and
16 cells, so the frame index is taken modulo the real cell count rather than
trusted.

**Refraction Strength** was parsed into a `case … break;`. The property carries
it and the renderer uses it; only the controller dropped it. 71 lighting-float
controllers in stock FO4 weapons and effects animate variable 0.

**Drag and gravity were single-slot.** Both kept only the LAST modifier on a
system and threw the rest away, and drag ignored its `Drag Axis` entirely. FO4
authors drag as three modifiers per system — measured over 140 stock effect
meshes, 944 drag modifiers, **every one** named `(X-Axis)`, `(Y-Axis)` or
`(Z-Axis)` and arriving in triples. Keeping only the Z one and applying it to
all three axes is harmless while the three percentages agree and wrong the
moment they do not: `AttachFXMist01.nif` asks for 0.07 / 0.07 / 0.02 and got
0.02 on everything, `BubblesSurface01.nif` asks for 0.06 / 0.06 / 0.03 and got
0.03. Damping is now applied along each modifier's own axis, which reproduces
the old isotropic result exactly when the percentages match.

Gravity accumulates the same way, and `Force Type` is read: `FORCE_SPHERICAL`
pushes along the line from the gravity object to the particle instead of along a
fixed axis. Its SIGN is an inference — every gravity modifier sampled has a
positive strength, so the tree never shows the other direction — settled on what
they are used for: `ExplosionBottleCapMine.nif` drives its debris with a
spherical strength of 675, and inward at 675 is an implosion. `Decay` and
`Turbulence` are still unread; there is no defensible curve for them in nif.xml
or in the corpus, and guessing one would be worse than leaving them out.

### Three harnesses, each proved to fail without its fix

| | checks | disproof |
|---|---|---|
| `tests/spells/cycle_type.sh` | 12 | 3 fail: the LOOP clip does not loop, is not still running past its end, and the REVERSE one never turns round |
| `tests/spells/particle_cap.sh` | 5 | 1 fails: a system authored for 12 peaks at 15 |
| `tests/spells/subtex_flipbook.sh` | 4 | 1 fails: 0 samples with most of the population moving, against 83 |

Each disproof neutralised only the new decision and rebuilt, rather than
trusting that the check would have failed.

The discriminating measurement is the point in all three. For cycle type, checks
3 and 4 fail in OPPOSITE directions on the old code, so a new default cannot
pass them — only reading the field can. For the cap, "live ≤ cap" passes on the
broken code whenever the emitter is quiet, so the check is that the population
sits exactly ON the cap; `CryoJet01` and most of the effects tree never saturate
in three seconds and were rejected as fixtures for that reason. For the
flipbook, cells changed on the broken code too — particles die and are reborn
with a new random cell — so the check is that MOST OF THE POPULATION changes
cell between two samples 20 ms apart while the population size holds steady,
compared as a sorted multiset because a death shifts every later slot.

### Three smaller ones

**Every Havok round trip grew the collision.** `Decompile Compiled Collision`
scaled by `havokConst * 10 = 70.0` while every other Havok conversion in the
program — `gl/gltools.cpp`'s `bhkScale`, `physics/physicspreview.h`, the CLI —
uses `1/1.42875 × 100 = 69.99125`. One codebase, two answers 0.013% apart, so
decompile → compile → decompile came back 1 part in 8,000 larger, every time,
cumulatively. `tests/spells/collision_compile.sh` grew a 12th check for it: the
first vertex's magnitude, before 85.857643 and after 85.857597 (5×10⁻⁷
relative, float noise through two conversions). On the old constant it comes
back 85.879066 against 85.868378 — 1.25×10⁻⁴, which is what the check is set to
catch at 250× either side of its tolerance.

**BSPSysInheritVelocityModifier** (53 stock FO4 meshes) went unread, so a spray
fired from something moving hung in the air where it was made instead of
trailing behind. The emitter object's velocity is not in the file, so it is
differenced from the node's world position between simulation steps; a backward
scrub now forgets the previous position, because differencing across a jump is
not a velocity.

**`NiLightRadiusController`** was the one of the three light controllers missing
from Animation Setup, while `spells/blocks.cpp` has always been willing to
attach it. It still cannot be *frozen* like the dimmer can: freeze bakes a
controller by writing its value into the field it drives, and nif.xml gives
`NiPointLight` no radius row — that number lives in the LIGH form, outside the
mesh.

### Two things measured and deliberately NOT changed

**The `x_` sequence prefix does not mean what the reference says it means.** The
claim was that `x_` marks a sequence driving shader or emitter properties. In
`SentryBotFaceLight.nif`, `partA` and `x_partAhead` drive the *same three*
`BSEffectShaderProperty` controllers — the prefix separates nothing there. It is
real (292 of 3,337 sequences) and it does correlate with looping (46% CYCLE_LOOP
against 18% for the rest), but no file in the tree contains both `x_Foo` and
`Foo`, so it is not even a variant-marker. Modelling it would encode a guess.

**`BGSM1_GLOW = 5` and `BGSM1_ENVMASK = 5` are not a typo.** A BGSM has no
environment-mask texture — the mask is the smooth/spec map's alpha — so slot 5
is the glow map by name. But `Materials/Weapons/Machete/machete.bgsm` ships
`MacheteBlade_m.dds` in it with `bGlowmap` set and environment mapping on, and
`_m` is FO4's environment-MASK suffix. Of 643 stock BGSMs, 140 have environment
mapping and nothing in slot 5, 7 have a texture and no environment mapping, and
exactly **2** have both. Two files cannot decide which sampler Bethesda meant,
and either choice is wrong for one of them, so both readings stay — now with the
measurement written next to them.

## 2026-08-02h — Collision round-trips perfectly, and the encoder is reachable

**839 of 839 packfiles in the 445-file corpus now reproduce byte-identically.**
Where the session started: 810 exact, 17 refused, 12 differing.

| | exact | refused | differing |
|---|---|---|---|
| start | 810 | 17 | 12 |
| `hknpConvexShape` | 827 | 0 | 12 |
| inverse mass | 833 | 0 | 6 |
| fixup reserve | **839** | **0** | **0** |

**The inverse mass.** The file stores an inverse; the decoder made `1/invMass`
and the encoder wrote `1/mass`. In float32 that is not the identity — three
values in the corpus come back one ULP out. The stored value is now carried and
written back whenever mass has not been edited, tested by the decode's own
expression so it is exact by construction.

**The reserved fixup slots** looked like one per null constraint-motor pointer.
They are not: corpus-wide that rule is wrong 46 times in 64, and two shipped
packfiles with identical classes, objects, fixup counts *and* null-pointer
patterns have different table lengths — the one with **more** nulls reserving
**less** table. It is a fossil of the source `.hkt`, so the only correct answer
is to carry the length. Grow-only. 23,448 of the game's 23,454 packfiles already
sit at the computed minimum, so it is a no-op for everything but the six.

**`cinfo +0x12` is not a material index** — it is the body's slot in the shape
list, on 178 of 178 multi-body systems. Read as a material index into
`bodyMaterials`, which *concatenates* every shape's own table, a slot number
from one shape indexed into another shape's materials: measured on
`BldgIntUpprStairsL01.nif`, a decompiled convex shape came out with
`0x7000682E` where its own header says `0x26067D15`.

**Compile Collision is a spell**, so the Collision group is no longer one-way —
but the real reason is that a private dock member cannot be tested, and that is
exactly how it came to write layer Static/0/0 into every packfile it produced.
The hoist had to bring three things with it: a Fallout 4 gate (the dock never
needed one, because its tree only lists what the file contains), `destructive()`
with a warning naming what goes, and a selection-safe removal — `spRemoveBranch`
consults the Block List multi-selection, so from the Block List this would have
deleted every selected branch inside a snapshot labelled "Compile collision".

**BSXFlags bit 1** is now set whenever collision is created or compiled. Nothing
ever wrote it, so a mesh could leave this editor with collision the engine
ignores. Measured: 22,496 of 22,496 collision-bearing stock meshes set it, no
exceptions. Deliberately never *cleared* — 71 stock meshes ship the bit with no
collision at all, so a spurious bit is tolerated while clearing one loses intent.

### Two things testing found that reading had not

Writing the BSXFlags block with `set<QString>` named it **"1"**: from 20.2.0.7
the Name is an index into the header's string table, and a plain string write
puts the index digits in as the name. It needs `assignString`.

And the harness itself: `get -f "Name"` returns that raw index, so the check had
to read the resolved name from the block listing instead. The check was run
against the previous build to confirm it fails there — 8 checks, 1 failure —
before being accepted as evidence.

## 2026-08-02g — Collision: the encoder gets a user, and 17 files stop being invisible

An exhaustive round-trip audit over **445 stock FO4 meshes / 839 compiled
systems** (every rare shape and constraint class in the tree, not a sample) found
the format is no longer the bottleneck: **810 packfiles reassembled
byte-identical**, 12 differed for two fully-diagnosed reasons, and 17 refused
with one cause. Two things were the bottleneck, and both are now fixed.

### `hknpConvexShape` decoded to nothing

The base of the convex family is a vertex cloud with no faces, and its vertex
payload starts at `+0x30 + 0x10` — which **is** `+0x40`, exactly where a
polytope's plane descriptor sits. So `decodeConvexLike` read the first vertex's
floats as the plane/face/index counts, got `nf` between 5,662 and 52,483, and
tripped the sanity guard — which returns one line *before* `shape.verts = raw`.
Every one decoded to 0 verts and 0 tris: invisible in the viewport, invisible to
any geometry check, and not reported anywhere. `encodeShapeObject` then had no
branch for a convex shape with no faces, so all 17 systems refused to assemble.

This is the same aliasing the file already documents for spheres, never applied
to the base class.

Matched by **class name**, not by the descriptor: the tempting data-driven test
("payload offset `0x10` means no polytope arrays") is byte-identical on
`hknpSphereShape`, and a sphere whose four vertices differ is *meant* to reach
the polytope tail. Keying on bytes would change 114 objects to fix 17. The
sanity guard itself is untouched — it protects 6,075 polytopes.

Measured on all 17 rather than assumed from one: **eleven have 8 vertices, not
4**, storing each corner of a quad twice, so the vertices go back verbatim —
de-duplicating would write 128 bytes where the file holds 192 and shift every
offset after it.

**Before: 839 packfiles, 810 byte-exact, 17 refused. After: 839, 827, 0.**
810 + 17 = 827, and the 12 known-differing are still 12.

### The encoder had no user

`hknpEncodeSystem` reproduces those 810 packfiles — bodies, compounds,
primitives, constraints, ragdoll skeletons — and was reachable **only from the
CLI's own round-trip self-test**. The one production write went through
`hknpEncodeCompressedMesh`, which flattens a system to one static body with one
triangle mesh. So changing a compiled body's friction meant Decompile → edit →
Compile, and losing everything else on the way.

A compiled body is now edited **in place**: decode, change one modelled field,
encode again. Nothing is decompiled, so every opaque region goes back as it came.

Only what is genuinely *stored* is editable, and establishing that was most of
the work — the compiled display is largely substitution (Motion System and
Quality Type come from `hasMotion`, Penetration Depth is the literal 0.15,
Keyframed and Wind are always false). Friction and restitution are stored; the
filter is stored only when `hasStoredFilter`, because a layer of 0 is real and
the decode substitutes 1 or 10 so the row reads usefully. Offering that
substitution as editable would write the guess into the file.

The guard before any edit is a byte comparison — the untouched decode is
re-encoded and checked against disk, and one of the twelve known differing
systems is refused rather than silently rewritten.

`tests/spells/collision_compiled_edit.sh`, 7 checks. The last is the point:
**one undo restores the byte-identical packfile.** An edit path that rewrites
more than it was asked to passes the other six.

### And the reason it all stayed hidden

`decodeShapeSlot` keeps any `*Shape` class that has raw bytes and answers
"decoded", so a shape contributing no geometry was indistinguishable from one
that worked. There is now a `geometrylessShapes` report — deliberately separate
from `unknownShapes`, which doubles as the switch disabling positional body
attribution. It fires on 14 systems corpus-wide, all compressed meshes decoding
to zero triangles, and `hknpConvexShape` is correctly absent.

The Decompile and Compile warnings also stated a reason that is no longer true —
the constraints, motor and skeleton "are not decoded". They decode and re-encode
byte for byte (742/742 ragdoll constraints, 461/461 limited hinges, 75/75
skeletons). What is missing is any NIF representation to carry them back.

## 2026-08-02f — The Block List menu gets a shape, and a harness finds two real bugs

**A taxonomy separate from `page()`.** 46 top-level entries on a block row are
now 13 submenus, five flat verbs and a trailing group. This could not be done by
editing `page()`: that string is also the CLI namespace (`-s "Page/Name"`), the
id `SpellBook::lookup` parses, the Unfuck dialog's membership test, the switch
deciding whether a cast suppresses model signals, and the prefix of a dozen
QSettings keys holding the user's last-used texture path and Simplify settings.
So `page()` is frozen as the internal id and **`Spell::group()`** is what the
menu is built from — 106 spells declare one. **`Spell::label()`** is the same
argument for leaf text: four spells are named "Choose" and three "Update", told
apart only by living on different pages, and merging pages puts them side by
side.

`checkActions` no longer scans 200 spells per action looking for a name+page
match. It uses the `QAction -> Spell` map that already existed — faster, but
mainly *unambiguous*: two spells sharing a name on one page used to overwrite
each other's enabled state.

**Three deletions, each an entry that could not do what it said.** Create Convex
Hull / Convex Decomposition Collision were one spell with a checkbox pre-ticked,
and each wrote `Enable CoACD` into **persistent** settings — picking one silently
reconfigured Create Convex Shapes and the Collision Manager for every later file.
Fix Bip01 read `:/res/skel.dat`; Scan Bip01 was the tool that wrote it and opened
that Qt resource `WriteOnly`, which cannot succeed, and its registration had been
commented out for as long as the file has been in the repo. And two whole-file
spells that ignored their index parameter — `Enforce Node Name Authority` did not
even name it — appeared on every block row and then rewrote the entire file.

**The verb row.** Copy / Paste / Duplicate Branch, Delete and Rename now lead the
menu. The three branch spells are *hoisted*, not copied: the same `QAction` moves
out of the Block submenu and keeps its spell, shortcut and enable checking.

**Select & View**, so the menu can build the multi-selection three of its own
entries consume. Select Branch and Select Same Type are new code — neither
existed anywhere in the tree; the only same-type machinery was a filter, which
hides rows rather than selecting them. Reading the implementations corrected the
plan three times: there is no `restoreAllHidden` (it is `restoreAllVisibility`,
and the difference is real — `unhideAll` leaves a solo'd node hidden);
`hideSelected` does not read `objSelection` despite the name; and Join silently
no-ops unless a BSTriShape is active with two or more selected, so that condition
is now on the action's enabled state.

**`Edit Material File…`** and **`Select All Geometry Using This Material`**.
Right-clicking a shape offered six ways to check, compare and copy a material and
no way to open it. Both already existed as rows in the Material Manager's own
tree menu — the capability was there, the block you would want it on was not.

### What the harnesses found that reading could not

`tests/spells/menu_taxonomy.sh` (20 checks) builds a real `SpellBook` and reads
the menu back. It caught two bugs in its own subject: `/` is the group path
separator, so **"Import / Export" built a submenu called "Import " holding one
called " Export"** — and every other check still passed, because a title nobody
is looking for is a title nobody notices. And Qt eats `&` as a mnemonic marker,
so the two ordering passes compared different forms of the same title and the one
group with an `&` was ordered by the first pass and abandoned by the second.

`tests/spells/collision_undo.sh` (12 checks) was previously abandoned as needing
"a fixture from a game that authors editable rigid bodies — Skyrim or Oblivion",
since FO4 ships only compiled collision. But the body is already in the file,
compiled: **Decompile Compiled Collision is a pure data transform and runs
headless**, so one CLI cast builds the fixture from a stock FO4 mesh. No second
game required.

It then found two defects:

**`Havok Filter` is a mixin.** `nifxml.cpp` flattens it into its parent, so no
row of that name exists and `getIndex( info, "Havok Filter" )` returns an invalid
index on every Skyrim, FO4, FO76 and Starfield file — and invalid parents fail
*silently*, with `get` returning `T()` and `set` returning false, nothing logged.
It survives only in `bhkRigidBodyCInfo550_660` (Oblivion, FO3), which is why it
works on the two oldest games. Eleven sites were wrong: **Compile Collision
hard-coded layer Static/0/0 into every packfile it wrote**, Decompile decoded the
layer, flags and group and then discarded all three, the Layer combo could not
write at all, Create Collision ignored the layer the user picked, and
`Set Collision Layer from Motion` was never applicable to anything.

**`applyPhysics` read its widgets from inside the snapshot.** `nifSnapshotOp`
serialises the whole model before running its operation, and that is enough to
put the panel's editors back to what is still in the file — so a lambda asking
`mass->value()` got the *old* mass and wrote it straight back. All twenty fields
did. The edit produced a well-formed undo step containing no change, which is
worse than not working, because the history says something happened.
`applyLayerSelection` never had the bug: it takes its value out of the combo
before opening the snapshot.

The harness got two things wrong first, both worth recording: picking a combo row
by the *model's* layer can land on the row already showing, because the panel
substitutes a guess when the layer is Unidentified — the write never fires and a
working editor looks broken. And undo depth is `undoStack->index()`, not
`count()`: a push after an undo truncates the redo tail before adding.

### The rest of the plan

**Import / Export rows.** Both exporters were already block-scoped and lived
only in File ▸ Export — `exportObj` prints a message box whose entire job is to
tell you which block it guessed, `exportGltf` refuses without a selection. Their
version gates are read off the `ImportExportOption` table rather than reinvented,
so a menu entry cannot outlive the format support behind it.

**`Open in <Manager>`** — one row, titled from the clicked block's type. Three
fixed rows would have been the obvious shape and the wrong one: two would always
be inapplicable, in a menu whose whole problem was length. It `show()`s the dock
before `select()`ing, because the collision dock's handler returns early while
hidden.

**The command palette** (`Ctrl+Shift+P`, or the `Search…` row). Deliberately not
a line edit in the menu: a popup `QMenu` holds a keyboard grab, `keyPressEvent`
spends every printable key on first-letter jump, a filtered menu cannot expand a
submenu inline, and the popup cannot re-centre. It walks a live SpellBook's
`QAction` tree, so the hand-added native actions are searchable for free. The
group is part of the search key, which is what makes "transform copy" narrow to
one row when four spells are called Copy. **Inapplicable entries are shown,
greyed** — that is the whole advantage over filtering, since Batch, Sanitize,
Optimize and Error Checking vanish from a block row by design. And the top hit is
auto-highlighted **unless it is destructive**: two keystrokes and a reflexive
Return must never reach Crop To Branch.

**Batch 8, the two items marked "confirm first".** The four numbered Rigging
steps leave the menu — the dock numbers and gates them, the flat submenu let you
run step 4 before step 1. New `Spell::menuHidden()` rather than an `isApplicable`
gate, because the dock casts them by id with a valid index; the registry is
untouched, so `castSpell` and the CLI still work.

And the two rename propagation paths turned out **not to be two
implementations** — the plan deferred merging them because "they are two
different propagation bodies", and they were identical line for line. That makes
the merge a no-op, which is precisely why it was worth doing: two identical
copies is one copy that has not drifted yet.

### Not done, and why

`Compile Collision` and `Check Collision` stay in the dock. Check would be a
third copy of a walk that already exists twice, and it is whole-file — putting a
whole-file action on a block row is what this batch removed twice over. Compile
is genuinely block-scoped and genuinely missing, but `compileSelectedCollision`
is a hundred-line private member driven by tree-item state, popping three modals,
deleting the source branch — and it was just found to be writing layer
Static/0/0 into every packfile it produced. It needs its own harness first.
`Open in Collision Manager` now puts it one click away.

## 2026-08-02e — The workspace consistency audit is finished

All twelve items are implemented. The last three were the ones about *looking*
the same, which is where the drift had gone unnoticed longest.

**One selection palette.** Four colours — `#4a7ab0` / `#2b425f` background,
`#FF9D00` / `#FF7200` text — were hardcoded in **six files** with zero drift
between them, which is what made them worth naming rather than reconciling. Two
more views had already drifted, and one badly: **Skeleton took its secondary
selection from `wwSkinColor("danger")`**, the app's error colour, so a
multi-selection of bones rendered in the same red that means "missing texture" in
Materials and "key out of range" in the Timeline. Pose's blue had slipped to
`#2b3b5c` under a comment claiming it copied the Block List.

The patch had to respect a distinction that is easy to miss: Qt's `:!active`
means *this view lacks window focus*, while four of the sites use the same two
colours for *selected, but not the active object of a multi-selection*. A
stylesheet cannot express the second, so those sites read the variables directly
and say why. Conflating the two is part of how the values got copied by hand.

*The test is not a grep* — "the literals are gone" would pass on a conversion
that resolved to the wrong colour. It asserts the emitted sheet carries all four
resolved values, that a live view took it, and that **no selection colour equals
the error colour**.

**One heading, one boxed button.** Four heading idioms; scrolling a single
Collision column passed through three of them. Pose had already factored its
version into a helper whose comment read *"matching the other manager docks"* —
from inside an anonymous namespace no other dock could reach, which is exactly
how four idioms happen.

`wwBoxedButtonQss` was `static`, and `style.qss` scopes its rules to
`QToolBar QToolButton`, so five dock buttons inherited nothing. The one
workaround written in its absence made the case by itself: `skelBoxedButtonQss`
claimed to reproduce the look and did the opposite — a filled plate, a visible
border and an amber checked state, reinstating precisely the *"fifteen bordered
boxes competing for attention"* the original's rationale says were removed.

Two corrections from the adversarial pass were load-bearing: Pose's button needed
**InstantPopup**, not MenuButtonPopup, because it has no `clicked()` connection
at all — splitting it would have left the main segment dead — and its file had no
`<QToolButton>` include, so the change as first written would not have compiled.

## 2026-08-02d — The Animation dock stops being a second playback engine

**Play did not animate the viewport.** The dock ran a private 16 ms timer that
advanced its own playhead and emitted `timeChanged` → `GLView::setSceneTime` —
and `setSceneTime` never touches `scene->animate`, while controller evaluation is
skipped when that is false. So with *View ▸ Animations* off, Play scrubbed the
playhead across a **frozen viewport**. `playPauseRequested` was declared, and
connected, with a comment saying it fixed exactly this, and **nothing in the tree
emitted it** — confirmed by grep before touching anything: two hits, a
declaration and a connect.

None of it needed reimplementing, which is the point. GLView's loop already
handles speed, reverse (as a negative speed), Loop, Switch-sequence and the
stop-at-the-end case, and `sceneTimeChanged` was *already* wired to the dock's
`setTime`. So the timer is deleted, `transportToggle` emits the signal that was
waiting for it, and direction becomes the sign of `animSpeed` — which is what the
wrap logic reads, so Loop behaves identically in both directions. The buttons now
follow the application instead of their own clicks.

*The test watches `scene->animate`, not the playhead* — the playhead moved on the
broken code too, and that was the whole illusion.

**Materials bulk edits: one undo step, and a count that is true.** Replace All
and Retarget called `setText` per row; each fired the itemChanged handler, which
snapshots the whole model twice. Two hundred paths meant ~400 full
serialisations, 200 undo entries, and no way to undo as a unit. The count lied
too: material-file rows have no field in this NIF to write, so their text changed,
the write bailed, the next rebuild reverted it — and they were counted anyway.

**Retarget Folder silently did nothing on the files it was for.** It normalises
its prefix (lowercase, backslashes) and then matched it against the *raw* stored
path — so a NIF storing `textures/effects/x.dds` never matched `textures\`. The
separator convention it exists to fix was exactly what stopped it matching.

**Pose lost the bone highlight on every viewport pick**, because `refresh()`
clears the bone list and only the pose list was being stashed and restored.

**The Block List can delete and rename.** Deleting was reachable only from the
viewport, so the one widget that can build a multi-selection of blocks had no way
to delete it.

Two proposed changes were **checked and not made**, because an adversarial pass
showed each would regress something: hoisting Pose's row colouring into a shared
helper wipes the grey that marks a pinned bone, and the boxed-button conversion
in Pose would leave a dead button segment. Both need their own change.

## 2026-08-02c — Two reported bugs, one Unfuck, and a lot of measuring

**Typing `11.25` into a transform gave `1125`.** The gizmo's numeric entry was
never at fault — it handles `.` correctly and was written for keypad and
non-English layouts besides. It simply never received the key. **Three** separate
handlers upstream claim a bare period, and all three fire during a modal
transform: the app-wide filter's `frame_selection` branch (bound to a bare `.`,
whose two guards — pointer over the viewport, focus not a text widget — are
*necessarily* true during a mouse-driven G/R/S), the same filter's numpad branch,
and `physicsKeyPress` inside GLView. Digits are bound to nothing, which is exactly
why it read as "the decimal key is broken" rather than "keys are broken". The
third site was found by an adversarial pass over the first diagnosis; without it
`.` is still lost whenever the physics preview runs.

**Loading a nif destroyed a browsed archive tree, and worse.** Browse mode sets
`currentArchivePath`, which makes `configuredIndexLive` false forever, so the
cache fast path could never be taken and every load did a full teardown. That
teardown also clears `currentArchiveNames`, and `openArchiveFileString` returns
immediately when it is empty — **so the first load out of a browsed archive made
every other file in it unopenable.** Nothing in the UI shows that. The
configured "Available NIFs" tree was never affected, which is why the existing
browser harness passed honestly and a second one was needed rather than a
sharper look at the first.

**There is one Unfuck now.** ~350 lines of modal dialog are gone and the Spells
entry opens the workspace. Three things moved across first: the curated run
order, single-snapshot undo across a whole run, and four Batch/Optimize repairs
the panel's `Sanitize || sanity()` rule could not see — Update All Bounds among
them, which is the fix for meshes that vanish as the camera moves.

**The panel can now see materials and collision**, via two new `checker()` spells
that report through `logMessage` instead of a modal. The severity calibration
took five rounds of measurement and every one contradicted what I expected:
vanilla FO4 effect meshes have no materials at all; blank shaders on
`EditorMarker` geometry have nothing to configure; lighting shaders are *not*
always material-driven; and NIF-vs-BGSM disagreement is the normal state of
Bethesda's own assets. The rule that survived is **absence is a defect,
disagreement is normal** — and the number that matters: **eight stock files, zero
errors or warnings, every finding a white note.** It started at seven warnings on
the first file tried.

**Three bugs in the path checker**, found while cataloguing. `P_EMPTY` was
declared without a value in a flags enum, so `invalid & P_EMPTY` was `& 0` — that
message had never been reachable, for any caller, since it was written. The loop
used `return` where `continue` was meant, so the first empty slot in a texture
array abandoned every slot after it. And `if/else if` between the no-extension
and absolute tests hid the more serious of the two.

**Absolute paths get a repair.** An adversarial review returned six corrections
against the first sketch and three became the design: the archive code's own
anchor rule prefix-matches (`tex` matches `textures`) so it is not reused;
material paths resolve under `materials`, not `textures`; and there is
deliberately **no verification pass**, because `findResourceFile` lowercases,
forward-slashes and coerces the extension, and appends the open NIF's own folder
to the search path — so it happily verifies a path that only resolves on this
machine, the exact problem being fixed.

**Block List batches 2 and 3.** Remove Branch and Duplicate Branch now honour a
multi-selection the way Copy already did — selecting five nodes and pressing
Ctrl+Delete used to remove one. Submenu order is declared rather than inherited
from link order in `NifSkope.pro`.

**Workspace consistency.** Every drawn icon was invisible in the Light theme
(17 hardcoded glyph colours); eight muted hint labels used `palette(mid)`, which
`loadTheme()` never sets, so they followed no theme at all; Rigging's collapsed
"Advanced steps" box still showed two of its children; and three docks arrived
floating or already open.

**Also**: `Spell::hint()` had zero overrides tree-wide and nothing read it, while
the same sentences sat in a private table — moved onto the spells, so the
right-click menu shows them too; the rotation-key fix from `2ff7457` is finally
proven at runtime, on a fixture that had to be *built* because no stock FO4 asset
has quaternion rotation keys.

*Four harnesses passed for the wrong reason during this run and were caught:* a
0-check run reporting PASS, a fix button that resolved nothing, a cursor that
never reached the viewport (`QCursor::setPos` is a no-op from a non-foreground
process), and a dock assertion that would have let a real defect through. The
pattern is always a precondition quietly not holding.

## 2026-08-02b — The confirmation now asks the right spells, in their own words

**The prompt was inverted.** `SpellBook::cast` fired one generic question — *"This
action cannot currently be undone. Do you want to continue?"* — for nearly every
write, because `undoable()` is overridden `true` by six spells in the entire tree.
And it carried a **Do not ask me again** checkbox. So the first time anyone ticked
that box to get past a routine edit, Crop To Branch, Remove Branch, Remove, Flatten
Branch and Apply Transformation all went silent too, permanently, in the registry.
Loudest where it mattered least; absent where it mattered most.

`Spell::destructive()` replaces it. The handful of spells that destroy something
ask; nothing else asks at all. The question names the loss —

> Delete 267 of the 268 blocks in this file, keeping only \[145] NiNode
> 'BlastRadiusNode'?

— the go-ahead button is labelled with the operation rather than "OK", Cancel is
the default so Return and Esc both back out, and **there is no off switch**.

Marked destructive: Crop To Branch, Remove Branch, Remove, Flatten Branch (both),
Apply Transformation. Each writes its own text. Remove says the children stay
behind orphaned, which is the part that surprises people. Flatten Branch says the
nesting cannot be rebuilt — it deletes nothing, so it reads as harmless and is the
one most likely to be run on the wrong node. Crop To Branch counts through a
`QSet`, because `getBranch` appends without deduplicating and the raw list
overstates the kept set on any file that shares a block between two parents.

`Convert` is deliberately **not** marked, against the audit's recommendation: its
target type is chosen inside `cast()`, so a confirmation before the chooser cannot
name what is lost and a second one after it would be two modals for one click. The
chooser is the confirmation.

Apply Transformation had its own animated/skinned warning inside `cast()`. That
text moved into the destructive question so the menu path shows one dialog, and
the inner one is now gated on `Spell::confirmedByBook()` — it still fires for the
direct callers, which is how Combine Shapes reaches it.

**Verified against the code it replaces.** `WW_DESTRUCTIVE_TEST` casts real spells
and answers the real modal from inside the nested event loop: Cancel leaves the
file alone, the counts in the text match the file, the old suppression key does
not disarm it, a non-destructive spell (Move Up) raises **no** dialog and still
runs, and the accept path loses *exactly* the number quoted. 8 checks green.
Reverting `SpellBook::cast` to the old prompt fails 5 of the 8 — including, worth
naming, `blocks 268 -> 1` with the suppression key set and no dialog at all.

The poller answers any modal `QDialog`, not just `QMessageBox`, on purpose: the old
prompt was a `CheckableMessageBox`, and a `QMessageBox`-only poller would have left
it unanswered, so the old code would have *hung* rather than failed and "runs
without a prompt" would have read as passing until the run timed out.

**Block List free wins, from the context-menu audit.** `Convert` was
`return index.isValid()` — it appeared on every field row in Block Details, where
`cast()` reads a field name as a block type and offers an empty chooser; gated on
`isNiBlock` now. The trailing separator in the Block List menu is inserted only
once something follows it. `setToolTipsVisible(true)`, so the four hand-written
tooltips in that menu stop being dead text. The `kfmtree` → `contextMenu` connect
is gone; the slot returns immediately for any sender that is not tree, list or
header.

Two spells renamed to what they do: `A -> B` → **Recompute B Frame from A** (it
recomputes the B-side pivot and axes from the A side through both bodies' world
transforms), and `Fix Geometry Data Names` → **Zero Geometry Group ID** (it sets
Group ID to 0 and touches no name at all — the old label matched the file's own doc
comment, and both were wrong).

Full suite after: 54 checks across six harnesses, 0 failures.

## 2026-08-02a — Unfuck grouped by issue class, with a per-row fix

Grouping findings by the spell that reported them was the first design and it was
wrong, in a way that had already produced a visible error. **One spell emits
several unrelated problems with different answers.** `spErrorNoneRefs` is the
proof — it reports both

    'Properties' link array contains 2 None Refs.     <- a hole in an array
    'Skeleton Root' link is None.                     <- a missing single Ref

The first is repaired exactly by Collapse Link Arrays, which runs `numCollapser`
over `Properties` and `Extra Data List`, the same two arrays that checker inspects.
The second is a lone Ref no collapse can touch and nothing in the spell library
repairs. Filed under one heading, either the fixable half loses its fix or the
unfixable half gains a button that does nothing to it.

So the panel is now keyed on an `IssueClass` catalogue of six problem *kinds*
matched by regex, each carrying its own title, whole-file spell, per-row spell and
caveat. Findings are collected from every check first, then grouped, worst severity
first, with an "(uncatalogued)" fallback so a checker added later shows up
ungrouped and without a fix rather than not at all.

**The per-row fix exists, for one class.** The message carries both halves it needs
— block from `[19]`, array from `'Properties'` — so the array's own index is
rebuilt and `spCollapseArray` cast on that array alone. Its `isApplicable`
re-validates, so a wrong reconstruction refuses rather than collapsing something
else.

**A bug review would not have caught.** `SpellBook::lookup` takes a `"Page/Name"`
id and, given a bare name, matches only spells whose `page()` is empty. Every
lookup in the panel passed a bare name, so every one returned null — including the
**Repairs button, which had been silently doing nothing** while reporting "does not
apply to this file". It looked entirely correct and was already committed. What
exposed it was asserting that a fix *resolves its finding* rather than that a
button exists; the first version of that check passed on a no-op. Replaced with a
`spellNamed()` resolver that searches by name across pages.

## 2026-08-01e — One row at the top, and three audit fixes

**The menu bar and the tool bar share a row.** Three stacked bands at the top of
the window (title, menus, tools) are two. The menu bar is reparented into the
first toolbar as an ordinary widget, so the top toolbar area lays it out on the
same line as the rest — those toolbars already shared a row with each other, and
this just adds one more member to it. `setMenuWidget` then takes an empty
placeholder so the main window stops reserving a band for it.

Order matters and is commented in place: the bar goes into the toolbar *before*
the main window is told to stop reserving its row, because doing it the other way
hands QMainWindow a widget it still owns and is about to replace.

*Known limit:* on a window narrow enough that the menus plus every toolbar group
no longer fit, the row spills into Qt's toolbar extension button rather than
wrapping. Nothing is lost, but it is a click away.

**Three faults from the workspace audit, each verified before being touched** —
two of six claims in the earlier Unfuck audit had been subtly overstated, so none
was taken on trust.

The **Skeleton Manager was stealing the selection from behind a closed dock**.
Its handler had no visibility guard, so in every workspace selecting a collision
object was redirected to the bone that owns it — a hidden dock fighting the
Collision Manager over one selection — and a full `skeletonAnalyse()` walked the
file on every block-list click anywhere in the app. `WW_WSFIX_TEST` checks both
directions, because a guard that is really a removal would pass the first half
and quietly kill the feature: hidden, selecting collision block 7 stays on 7;
shown, it still follows to target node 6. The bug was reproduced on the old code
first.

**Collision's live editors wrote with no undo entry** — `applyPhysics` (~20
fields), `applyLayerSelection` and `applyMaterialSelection` all used bare
`nif->set<>`, which pushes nothing, while Compile and Import Donor in the same
panel snapshot correctly. So undo appeared to work and then reverted whichever of
*those* ran last. All three go through `nifSnapshotOp` now.

**Rotation keys could not be inserted at all.** `insertKeyAtTime` skipped
quaternion channels, so I or double-click on a rotation lane produced nothing and
said nothing, on the most-keyed channel there is. The exclusion existed because
`Controller::interpolate` has no `Quat` specialisation; this adds one, SLERPing
between the bracketing keys off the list the function already reads.

Verification status, plainly: the Skeleton fix is proven both ways by harness;
the collision fix is checked statically (no bare `nif->set` survives outside the
snapshot lambdas) but not exercised at runtime; the rotation fix compiles and the
sampler is straightforward, but nothing has yet inserted a rotation key and read
it back. The last two want harnesses.

## 2026-08-01d — The Unfuck dialog, audited and rebuilt

An audit of what that dialog was actually offering found two things that made it
dangerous rather than merely plain, and I had shipped both.

**It armed two spells that should never be armed.** Rows were ticked on by group
membership, not by the spell's own judgement. `Reorder Blocks` returns
`sanity() == false` with an upstream comment saying exactly why — *"Prevent this
from running during auto-sanitize... can really only cause issues with rendering
and textureset overrides via the CK"* — and it was pre-ticked. So was
`Fill Blank NiControllerSequence Types`, which stops mid-batch on a modal
`QInputDialog` **inside the undo snapshot**. Every row now takes its default from
its own `sanity()`, so a spell added later inherits the right answer for free.

**Two rows could never light up.** `Sort Keys` wants an array row and
`Check Material` wants a shader block; this dialog always asks with an invalid
index, because that is what "whole file" means. They sat there permanently grey
under a tooltip blaming the file. Probing the loaded model for an index that
would satisfy them was tried first and cannot answer the question — a miss means
either "wants a selection" or "this file has none of those" — so the two are
named in the source instead, which is honest about it being a fact of the spell.

Also: `Check Links` reads and logs and changes nothing, yet sat under "these
change the file". It carries `constant()` now and files under Checks.

**Every row says what it does**, in one line read off the implementation rather
than the label, because several of these names mislead — `Fix Geometry Data
Names` never touches a name, it zeroes Group ID; `Reorder Link Arrays` also
silently drops dead children. A greyed row now gives the real reason ("Fallout 3
/ New Vegas only") instead of the old "Nothing for this to do in this file",
which was simply false: these are format gates, not content probes.

**The Checks run when the dialog opens.** They are read-only by definition, so
this is free of risk, and `setMessageMode(MSG_TEST)` redirects their output into
a list rather than a popup per finding. Each shows "clean" or "3 found" in the
danger colour with the detail beneath, and a check that found something ticks
itself. That is what turns a blind checklist into a report card. Verified both
ways: clean on stock files, and "1 found" on a file with a deliberately broken
texture path.

**A run really is one undo step now.** It was not: `spEnforceNameAuthority`
calls `nifSnapshotOp` itself, so wrapping the batch in another pushed two
commands and the second Ctrl+Z walked the file *forward* into a half-repaired
state. `runUnfuck` detaches the undo stack for the duration — every nested
snapshot becomes a no-op, since `nifSnapshotOp` only pushes when there is a stack
— and pushes exactly one command at the end. It also compares the before and
after buffers and pushes **nothing** when nothing changed, so a checks-only run
no longer marks the document modified and asks to save on close. Header and
footer updates moved inside the measured region, where they belong.

**Four repairs added**, all opt-in: Update All Bounds (the fix for meshes that
vanish when the camera moves), Update All Tangent Spaces, Make All Skin
Partitions, Remove Unused Strings. Deliberately not added: the batch optimisers
and normal generators, which overwrite authoring intent on meshes that were fine,
and anything that writes other files on disk. Run order is now explicit rather
than alphabetical, because string-table compaction has to follow the passes that
add strings.

## 2026-08-01c — Unwrap a piece of an island; Unfuck becomes a dialog

**Unwrap Selection In Place.** bungo: *"select uv areas that are already part of
a UV island, and those will get unwrapped, but they'll still be connected to the
rest of the original UV island, which will remain the same."*

Neither existing operator did that. `unwrapSelection` solves the selection and
then **packs** every component into 0–1, so the patch lands somewhere else and
the seam it shared with the island is torn. `unwrapWithPins` stays in place, but
only around pins placed by hand, one vertex at a time.

The insight is that the pins are not a user decision here — they are a property
of the selection. A vertex used by a selected face *and* by an unselected one is
on the seam between the part being re-solved and the part that must not move, so
that set is exactly what has to stay put. Pin those at the UVs they already have
and the solver stitches the new patch back into the untouched remainder. No
density rescale and no packing: both are ways of choosing a size and a place, and
the pins have already chosen both.

One case needed thought. A component with fewer than two seam pins is touching
nothing that has to hold still — the selection took a whole island, or a piece
joined to the rest by a single vertex — and `lscmSolveComponent`'s own fallback
would anchor it at (0,0)–(1,0), flinging it across the atlas: the exact thing
this operator exists to avoid. Those are anchored to their **own** current UVs
instead, so they are re-solved where they already lie. The status line says how
many pieces that happened to.

**Unfuck is a dialog now, and it stopped shouting.** bungo hit a modal warning —
*"'Textures' has a filepath without a file extension"* — the first time he ran
it. That was `spErrorInvalidPaths`, which is a **checker**: it reports and
changes nothing, so bundling it into "fix everything" was wrong. Fixes and checks
are separate groups now and checks start switched **off**.

The menu of checkable entries was also the wrong shape: a menu closes on every
click, so arranging a run meant reopening it once per fix, and there was no way
to back out. It is a proper container — two groups, a checkbox each, **Unfuck** /
**Cancel** — and Cancel really cancels, since the choices are only written back
on OK. Spells with nothing to do in the current file are shown disabled rather
than dropped, so the list keeps its shape and the greying is itself the report.

**The Spells menu is back, with only what is unique to it.** The line turned out
to be sharp rather than a judgement call: right-clicking a block opens a full
SpellBook *at that index*, so every block spell is already one click away there.
What right-click can never reach is the spells that answer `isApplicable` only
for an **invalid** index — the whole-file ones — and those are exactly what the
old menubar copy hid the moment you selected anything. So the menu is rebuilt on
open from the whole-file spells and never filtered against the selection:
5 pages, 21 spells, with **Unfuck…** at the top. `WW_UNFUCK_TEST` opens it *with
a block selected* and checks both directions — something is listed, and nothing
listed wants an index.

**One separator, not two.** Removing Screenshot stranded its group's rule against
the next one and drew a double bar. Runs of separators are collapsed on every
toolbar now, and the ones that end up at either end are dropped, so no future
removal has to remember to tidy up after itself.

## 2026-08-01b — Bolts that move again, and a tidier top bar

**The Tesla bolts were frozen, and that was my fault.** bungo: *"the lightning
bolts that have start and end nodes in the tesla vfx nifs do not actually move
now in 3d viewport preview, only their visibility gets toggled."*

Rebuilding the generator on the engine's rules (07-31/08-01a) tied regeneration
to the Generation and Mutation bool curves, which is right — but only half the
engine's behaviour. The other half is `Animate Arc Offset`: with it set,
`Lightning::Process` runs on EVERY update and re-offsets the branches it already
has (`0x1cf5c04` returns without touching the geometry when the flag is clear).
I knew about that path and skipped it.

It bites exactly the assets bungo named. Those bolts are driven by their own
interpolators, not by a sequence, so the key lists this code reads are empty and
the structure ordinal never advances: each bolt generated once and stayed there.
What still blinked is a separate `NiVisController` on the `_Start` node — hence
"only their visibility gets toggled".

`regenerate()` takes two seeds now. `tick` seeds the branch structure and only
moves when the file says so; `offsetTick` seeds the displacement alone and
advances while `Animate Arc Offset` is set, so the bolt crawls in place with its
branches intact — the engine's own split. The 30 Hz quantum is a preview choice,
labelled as one: the engine has no rate, it just runs per frame, and a frame
count cannot be scrubbed. `lightning_shape.sh` grows two checks that have to hold
together — two times must bake *different* geometry, and the same time twice must
bake *identical* geometry.

**Toolbar icons are monochrome.** The crossed eye, the bulb and the node pair
were the last colour PNGs on a row of a dozen code-drawn grey buttons.
Desaturating the artwork was tried first and does not survive the trip: the red
strike is DARKER than the grey eye it crosses, so luminance buries the one mark
that carries the meaning, and forcing it brighter fattens its anti-aliased edges
into a slab. They are `tlMakeIcon` glyphs now, monochrome by construction and
taking the row's own colour. The original PNGs are untouched.

**Screenshot / Save View is off the top bar** — hidden, not deleted, so the
Render menu entry and `GLView::saveImage` still work.

**Panels moved to the right of Workspaces.**

**"Unfuck" replaces the Spells menu.** Everything in that menu was already one
right-click away, since the Block List and Block Details both open a full
SpellBook. What it was *not* giving anyone is the file-fixing spells, and the
reason is worth recording: they answer `isApplicable` with `!index.isValid()`
because they act on the whole file, and `SpellBook::checkActions` **hides**
whatever does not apply to the current selection — so the moment you clicked any
block, all of them vanished. They were not missing, they were unreachable.

The new menu lists all 14 of them as checkable entries with their state
remembered, greys (rather than hides) the ones with nothing to do and says so in
the tooltip, and runs the checked set inside one snapshot so a bad run is one
Ctrl+Z. `WW_UNFUCK_TEST` opens the menu *with a block selected* — the state that
used to empty it — and asserts the entries are reachable: 14 listed, 10 live.

**Block Details gained a row.** The filter field is shorter and the Expand and
Collapse buttons moved up beside the star, so the frame that used to hold them
under the filter is empty and hidden and the tree starts that much higher.

## 2026-08-01a — Five of the six, fixed

bungo: *"Time to fix 1 2 3 4 and 5."* Area 6 is a fact about the game, not about
this code, so it stays a note in `LOADING_SCREEN_BAKE_PLAN.md`. Details of each
rule and its RVA are in `WW_PDB_COMPARISON.md`; what follows is what changed and
what it was measured against.

**1 — Sequence binding.** `ControllerManager::setSequence` resolves each
Controlled Block through the manager's `NiDefaultAVObjectPalette` now, and falls
back to the old name search only for names the palette does not carry. The
fallback is a deliberate divergence: the engine drops the row, but half-written
palettes are a thing mod files really have.

Honest scope — across 600 shipped assets the palette and the name search **never**
disagree, and the one file with duplicate node names does not disagree either. So
this fixes no vanilla file. It matters for files NifSkope itself produces, where
merging is what makes two nodes share a name. That made the test the whole
problem: `tests/anim/sequence_binding.sh` builds the disagreement by rewiring one
palette `Ptr` in a vanilla file, and `SeqBind::stats().differs` is 0 for the old
code by construction. Watched it read 0 on a reverted build before believing it.

**2 — Procedural lightning.** Four rules replaced. Subdivisions is a recursion
depth (2^s segments — the X-01 bolts author 7 and were drawing 32 segments);
branch counts come from `rand[max(A-B,0), A+B+1)` with A and B halving each
generation, so depth falls out of the parameters instead of being hardcoded at 2;
length is `Length*0.5^gen + rand(-1,1)*LengthVar*0.25^gen`; and the displacement
is one random direction per span under a tent, halving per level and never
normalised, not per-vertex midpoint noise. Branches have no direction at all —
they all run along +Y and the forking is the displacement — so the per-bolt
frames went with them. The 1/24 s cadence was invented: Generation and Mutation
are bool curves and a change in either is what re-rolls the bolt.

`tests/anim/lightning_shape.sh` asserts the shape in numbers off `WW_BOLT_DEBUG`,
because two of these had been tried and reverted for looking wrong — against a
bolt drawn between the wrong two points, which is a fault no screenshot separates
from a fault in the generator. On the X-01 torso: trunk 128 segments at length
32, gen 1 at 64/16, gen 2 at 32/8, counts inside 1..3, depth stopping at 2.

**3 — Particle modifiers, and a correction.** The comparison ranked this gap by
reading the engine's class list and got it wrong. Counting the classes in FO4's
692 effect meshes instead: `NiPSysGrowFadeModifier` appears in **0** of them, the
six field modifiers in **0**, `NiPSysMeshUpdateModifier` in **0**, and
`NiPSysSpawnModifier` in 347 but with `Num Spawn Generations = 0` in 66 of the 67
sampled. AgeDeath and Position were not missing either — they were hardcoded.

What was actually missing is what appears in **288** files: the whole
`NiPSysModifierCtlr` family, plus a modifier's own `Active` field, neither of
which was read. Every modifier ran, permanently, whatever the file said. Both are
honoured now, along with the emitter parameter curves (Speed 51 files, Initial
Radius 21, Life Span 6, Declination 6) including the manager case where those
controllers hold a blend-interpolator stub and the real keys are in the sequences.
`tests/anim/particle_modifiers.sh` pins the one thing that is cleanly binary:
switching an emitter's `Active` off must bake **no** particles, where the old code
baked the same 62 vertices either way. §3 of that test says plainly what is not
pinned and why, rather than leaving a check that cannot fail.

**4 — `AttachT`.** Split at the first `&` and dispatch on the tag, as
`ProcessAttachTechniques` does, instead of matching the `NamedNode&` prefix. That
turned up a real parsing bug: FO4 ships arguments with a `|n` suffix
(`NamedNode&C-ArmsTypeA1|0`) that match no node, so the merge refused those
donors outright. Exact match first, then without the suffix.

Also decoded, and it closes the parked leg-arc question:
`BGSNamedNodeAttach::AttachPolicy::Process` is a plain re-parent with **no
transform**, falling back to the root when the name does not resolve. The X-01
arcs run inside the calf because the asset puts `BoltGeo_01` there — an authoring
change, not a merge bug.

**5 — Controller flags.** Bit 4 is PlayBackwards, bit 6 is ComputeScaledTime —
the "unknown function" TODO — so `ctrlTime` returns the raw time when bit 6 is
clear and mirrors about the interval when bit 4 is set. Sampled 160 controllers
across four asset trees: bit 6 set in all of them, bit 4 in none, so this changes
nothing shipped. Bits 0, 5, 7 and 8 are documented rather than implemented; all
four only govern when the engine bothers to call a controller.

## 2026-07-31o — Six guesses checked against the engine

bungo asked for a comparison check on the six areas where the FO4 PDB could
settle something NifSkope currently guesses. No code changed; the findings are
in **`WW_PDB_COMPARISON.md`**, one section per area, every claim cited by RVA in
1.10.155 so it can be re-read rather than re-argued.

1. **Sequence binding differs, and it is the bug class we hit.**
   `NiControllerSequence::StoreTargets` resolves each Controlled Block through
   `NiDefaultAVObjectPalette::GetAVObject` — a CRC32 hash map — and that palette
   is the one **in the file**: `NiControllerManager::LoadBinary` resolves it
   straight into `+0xC0` and nothing rebuilds it at load. NifSkope uses
   `Node::findChild`, a pre-order scene search, and ignores the palette entirely
   for playback. On duplicate names the two disagree by construction: when the
   engine *does* rebuild a palette it overwrites, so the **last** node in
   pre-order wins, where `findChild` keeps the **first**.

2. **Lightning: two guesses right, four rules wrong.** Amplitude decay 0.5 is
   correct, and `Child Width Mult` compounding per generation is correct. The
   1/24 s cadence does not exist — `Lightning::Process` holds three float
   constants and none of them is a cadence; the rate is authored in interpolator
   2 (Mutation), which NifSkope does not read. Subdivisions **is** a recursion
   depth and branches **do** use the authored `Length`, both of which §12 had
   recorded as "disproven by rendering" — they were tested before 07-31i fixed
   the span rule, so against a bolt drawn between the wrong two points. That note
   is marked superseded.

3. **Particles differ by scope, not by rule.** 16 of the engine's 56 `NiPSys`
   types appear anywhere in `src/`. Absent: AgeDeath, GrowFade, Position and
   Spawn — the four that decide whether a particle dies, fades, moves or spawns —
   plus every collider, all six field modifiers, and the whole emitter-parameter
   controller family whose fields NifSkope already parses.

4. **`AttachT` is modelled correctly.** `ProcessAttachTechniques` reads exactly
   the `NiStringsExtraData` we assume. Three more technique tags exist
   (`HavokGeometry`, `MultiTechnique`, `BGSParticleArrayAttach`), and an attach is
   dropped unless the object has children or a `NiControllerManager`. It does
   **not** settle the leg-arc placement: that transform lives behind
   `AttachPolicy::vftable+8`, one step further in.

5. **The controller flag bits are settled.** All three `glcontroller.cpp` TODOs
   are answerable from named accessors; bit 6 — "unknown function" — is
   **ComputeScaledTime**, which decides whether a controller is evaluated at raw
   or scaled time. `BSXFlags` yields only bit 11, confirming `nif.xml`'s `bLights`.

6. **Loading screens never animate.** `LoadingMenu` activates no sequence and
   ticks no controller in `InitModel`, `AdvanceMovie`, `Render`, `RotateModel` or
   `SetForegroundModel`; the per-frame work is spin, pan and zoom from INI
   settings. The corpus agrees — of 173 vanilla `LoadScreenArt` NIFs, 0 have a
   particle system and 1 has a controller manager, and neither `autoPlay`,
   `autoLoop` nor that file's `CharFXOn` appears as a literal in the exe. The
   live-effects loading screen should not be expected to move.
   `LOADING_SCREEN_BAKE_PLAN.md` is annotated accordingly: freezing is not a
   convenience there, it is the only thing that shows. One detail the merge kept
   by inheritance — `InitModel` frames the model on a node named
   `LoadingMenuZoomTarget`, which 65 of the 173 carry and ours still has.

## 2026-07-31n — "(no sequence)" is a choice in the Sequence picker

bungo: *"allow me to select to play animations that are not assigned to
sequences."* The Sequence dropdown listed `Scene::animGroups` and nothing else,
so a file's standalone controllers — the ones no `NiControllerSequence` names —
could not be asked for. **(no sequence)** is the first entry now, always.

### Why it needed more than an empty name

A sequence does not "play". Selecting one **binds** its interpolators onto the
controllers its Controlled Blocks name, and those bindings outlive the selection:
asking for a sequence that does not exist leaves the last one's bindings exactly
where they were, so an empty name changed nothing at all.

`GLView::clearSceneSequence()` clears the group and then rebuilds the scene from
the model, which runs `Controller::update` on every controller and puts each one
back on the interpolator **its own block** points at — the file as authored. For
every NiPSys effect under `Meshes/Effects` that is the only animation there is.

Two smaller consequences. The picker is enabled whenever the file has anything to
animate, rather than only when it has named sequences — a file with none now
offers the choice instead of a greyed-out "No named sequence" label. And the
entry carries an **int** marker rather than an empty string, because a sequence
is allowed to have an empty name (the "(unnamed)" rows), so an empty string
cannot mean "none" without also meaning one of those.

### Proof

`WW_LIVEFX_TEST` 11 → 14 checks: with no sequence selected the scene reports an
empty group and the X-01 rig still generates **10 arcs and 6 sprite clouds** —
the same as with `autoPlay` bound. That check exists because the failure mode
worth guarding is not "it errors" but "it quietly freezes everything", which
looks like a working feature until someone uses it.

## 2026-07-31m — Transfer Normals off the Block List selection

bungo: *"So secondaries can be transferred to primary?"* — meaning the Block
List's own selection, not one NIF to another. It could not: a spell is handed one
block and nothing else, so the dialog had to ask which mesh to take from.

Right-click in the Block List with several geometry blocks selected now offers
**Transfer Normals from N Selected…**. The **secondaries are the source**, the
**primary — the clicked row — is what gets written**, which is the direction
asked for and the one the highlight already communicates. Only Mapping and Mix
are asked for; the selection has already said the rest.

### Several sources are one surface

Not "run it once per source and keep the last". Every mapping here asks which
source vertex or face is NEAREST, and over a set of meshes that is the answer
over their union — so the sources are concatenated and asked once. Five armour
pieces then behave as the one surface they visually are. Topology is the
exception and is refused outright: index N of a concatenation means nothing.

### The overflow that would have been silent

Combining meshes meant the working copy could no longer index faces with
`Triangle`'s **quint16**. Five 20k-vertex pieces is 100k vertices, and every
index past 65535 would have wrapped into another piece's geometry — a wrong
answer with no error anywhere. The working mesh carries `int` triples now;
16 bits stays where it belongs, in the file.

`--from` repeats on the CLI for the same reason, which is also how the combining
gets tested at all.

### Proof

`tests/mesh/transfer_normals.sh` 9 → 11 checks, green: two sources onto a third
writes all 2046 normals, and topology across two sources is refused with a
reason.

## 2026-07-31l — Transfer Normals, with Blender's mapping list

**Mesh ▸ Transfer Normals…** copies one mesh's normals onto another. Blender's
Data Transfer modifier is the reference — bungo asked for it by that screenshot —
so the mapping list is its Face Corner list, in its order and by its names:

| Mapping | What it does |
| --- | --- |
| Topology | index for index; refuses meshes of different vertex counts |
| Nearest Corner and Best Matching Normal | the corner in that place whose normal already agrees |
| Nearest Corner and Best Matching Face Normal | the same, judged by face normal |
| Nearest Corner of Nearest Face | closest face, then its nearest corner |
| Nearest Face Interpolated | closest face, barycentric blend (the default) |
| Projected Face Interpolated | cast along this vertex's own normal |

Plus Blender's **Mix Factor**: 1.0 replaces, lower blends with what is there.

Two divergences, both forced by the format rather than chosen. A NIF stores **one
normal per vertex**, not one per face corner, so every mapping lands on the
vertex — "corner" here means the corner's vertex. And both meshes are read
through their **world transforms**, so a donor posed differently from the target
still lines up; Blender leaves that to the object transforms.

### The dialog is not where the algorithm lives

`src/spells/normaltransfer.{h,cpp}` holds the mapping as a pure function over two
meshes, and both front ends call it: the spell, and a new CLI verb

```
nifskope-cli transfer-normals <file> --from N --to M [--mapping 0..5] [--mix F] -o OUT
```

The verb exists because **a modal dialog cannot be driven headlessly**, and an
algorithm nobody can run in a test is one nobody should trust. It also reports
the thing worth reporting — how far each normal actually turned, in degrees —
because "2046 normals written" is equally true of a transfer that changed
nothing.

### Proof

`tests/mesh/transfer_normals.sh`, 9 checks green, built on one idea: **a mesh
transferred onto itself has a known answer.**

- Topology onto itself: **0.0015° average, 0.028° worst** — the identity, with
  only 8-bit normal quantisation between it and exactly zero.
- Best Matching Normal onto itself: **0.146° average, 6.98° worst**.
- Nearest Corner / Nearest Face / Projected: ~1.29° average, and **130° at the
  worst corner** — which is correct, not broken. A seam is several vertices in
  one place carrying different normals, and a nearest-anything lookup cannot know
  which side you meant. That is precisely why Blender offers the "best matching"
  variants, and a version of them that scored zero here would be quietly doing
  something else.
- Mix 0 changes nothing; a cross-mesh transfer writes all 2046; topology refuses
  196 verts onto 2046 and says why.

Best Matching Normal read **127° worst** before the seam-aware pass went in: the
nearest-vertex search returns whichever coincident vertex it saw first, so
judging "best matching" among only that one's faces judges one side of the seam.
It now considers every corner at that position.

## 2026-07-31k — Ctrl+N and the Normals menu, and the write that never wrote

**Ctrl+N** recalculates the selected faces' winding outward, **Shift+Ctrl+N**
inward, and **Alt+N** opens the Normals menu — Blender's bindings, doing
Blender's job, with the redo panel carrying the same single `Inside` checkbox
Blender's does.

### Recalculate is not what was already there

`recalcSelectedNormals` (W menu) re-derives vertex normals from the winding
already in the file — Blender calls that **Reset Vectors**, and it is now the
menu entry of that name. Ctrl+N decides the **winding itself**, in two steps
because they answer different questions:

1. **Consistency is local and exact.** Two triangles sharing an edge agree
   exactly when they traverse that edge in opposite directions, so a flood fill
   across shared edges settles an island with no geometry involved.
2. **Which way is out is global**, and on an open surface not strictly decidable.
   A closed island's signed volume settles it — positive means counter-clockwise
   seen from outside. A flat patch has no volume to read, so it falls back to
   asking whether the island's area-weighted normal points away from the mesh's
   middle. Blender ray-casts; this agrees on everything closed and is honest
   about the rest instead of picking at random.

### The menu

Flip · Recalculate Outside · Recalculate Inside · Set from Faces · Point to
Target · Point Away from Target · Merge · Average ▸ (Face Area / Corner Angle) ·
Copy Vector · Paste Vector · Smooth Vectors · Reset Vectors.

Deliberate divergences from Blender, since they are the reference: **Face
Strength** (Select By / Set) is an attribute of the Weighted Normal modifier and
has nowhere to live in a NIF. **Split** is Merge's inverse and needs vertex
duplication, which Rip (V) already does with the selection semantics people
expect. **Rotate** is modal in Blender and is Point to Target here — the same
destination with a click instead of a gesture. And **Set from Faces** averages
where Blender splits the corner, because a NIF stores one normal per vertex:
Rip first if you want the hard edge.

### What the test found: the write that reported success and wrote nothing

The suite vandalises a mesh — turns over every third triangle — and requires the
signed volume to come back. First run: the operator reported "96 turned over" and
**the volume did not move**. The control it prompted, running the SHIPPED Flip
through the same path, did not move either.

`getIndex( parent, row )` hands back a **column 0** index. `NifModel::setData`
switches on the column: on the name column it renames the item, **returns true**,
and leaves the value alone. So a `ChangeValueCommand` built that way pushes,
undoes and redoes perfectly while writing nothing.

Two shipped operators were doing exactly that: **`flipSelectedFaces`** (Flip
Normals in the W menu) and **`tlPushNormalCommands`**, which is every post-edit
normal recompute in the viewport — extrude, move, inset. Both fixed, along with
the two new sites that copied the idiom from them. Four sites; the other eleven
`ChangeValueCommand` constructions in the tree already used `ValueCol` or the
`tlVertexValueIndex` helper.

That trap is now a check of its own, asserting that a name-column write reports
success and does nothing, so the next person to reach for `getIndex` is told
rather than left guessing.

### Proof

`WW_NORMALS_TEST`, 11 checks green on `X01_Helmet.nif`: volume 502.09 → 170.29
vandalised → **502.09** after Outside, **−502.09** after Inside, a second Outside
turning over 0 faces, the triangle multiset unchanged throughout, and **126 of
126** sampled vertex normals agreeing with their face's winding. The suite also
logs the status line, which is what turned "nothing changed" from a mystery into
"the operator declined the selection" in one run.

## 2026-07-31j — animation is one button now

Five widgets in a row — play, sequence, loop, settings, scrub — plus **View ▸
Animations** in the menu bar. Six places to look for one subject, four of them
icon-only, and the master switch nowhere near the transport it governs.

One labelled **Animation** button on the View toolbar now, unfolding a panel, in
the same shape as the Collision button beside it: transport (play, loop,
reverse), a Time scrub, Sequence and Speed pickers, the **Animations** master
switch, **Cycle through sequences**, and the Timeline dock. `aAnimate` is gone
from the View menu — it lives in the panel.

Nothing keeps its own state. The panel drives the existing `aAnimate` /
`aAnimPlay` / `aAnimLoop` / `aAnimSwitch` actions and `Scene::animGroups`, so it,
the Timeline dock's transport and Space in the viewport cannot disagree, and it
re-reads all of it on `aboutToShow` because the keyboard changes the same state
behind its back.

### What the screenshot could not tell me

The panel photographs fine on an animated file and fine on a static one — the
difference between enabled and disabled is a few percent of grey, which is
exactly the kind of thing a picture invites you to guess at. So `WW_SHOT_UI`
writes a state file next to the image, and it caught the flaw: on a file with
nothing to animate, **the whole panel went dead** — because disabling the button
disables the popup with it.

Two of those controls are not about the current file. **Animations** is a stored
preference (`GLView/Enable Animations`) and the Timeline dock is a dock; a suite
went from green to "captured 0 arcs" once because that preference had been left
off, and a panel you cannot open to look at it would make that worse. The button
stays enabled and only the per-file controls grey out. Measured after: button
enabled, Play / Loop / Reverse / scrub / Sequence / Speed / Cycle all disabled,
**Animations** and **Timeline dock** live.

### New harness

`WW_SHOT_UI=<out.png>` photographs the CHROME rather than the scene —
`grabFramebuffer` returns the GL viewport alone, so it cannot show a toolbar or a
panel. `QWidget::grab` renders any widget's own tree whether or not it is on
screen or focused, which is what makes it usable while someone is working on the
machine. Writes `<out>` (the popup), `<out>.toolbar.png` (the strip) and
`<out>.state.txt` (enabled-ness, combo contents, check states).
`WW_SHOT_UI_BTN=<objectName>` points it at a different button.

## 2026-07-31i — the bolt rule, read out of the game instead of guessed

bungo asked whether the leaked 1.10.155 PDB would help with the arcs. It did, and
the first thing it produced was a negative: **the engine never looks up a node
name anywhere in the procedural-lightning path.**

### What NifSkope was doing

`ProcLightningController` walked up from the controller's target for an ancestor
whose name ends in `_Start`, chopped that off, appended `End`, searched the whole
scene for the first node of that name, and stretched the bolt between the two. Its
own comment called that "rig convention (edison_pa / shieldtesla)" — an honest
label for a guess.

### What the engine does

`BSProceduralLightningController::Update` → `UpdateGenerationParams` /
`UpdateProcessParams` / `AddTasklet` / `GetPropertyHolder` →
`BSProceduralGeometry::Lightning::CreateInstance` / `Process`. Not one of them
touches a `BSFixedString` or walks the scene. The parameters are the NIF's own
fields — `GenerationParams::LoadBinary` reads three `u16` (Subdivisions, Branches,
BranchVar), `ProcessParams::LoadBinary` five floats and three bools (Length,
LengthVar, Width, ChildWidthMult, ArcOffset, the three fades) — and the controller
keeps its evaluated copy at `+0x178`, which is exactly what it hands `Process`.

`Process` starts at `NiPoint3::ZERO` and writes, per segment, four vertices:

    X = jag.x ± width      Y = segment * (length / segments)      Z = jag.z ± width

So the bolt runs **from the target's own origin, along its local +Y, for Length**,
jagging laterally in X and Z. The counts agree: `GetBranchSubdivisions(n) = n-1`,
`GetBranchVerts(s) = 4·2ˢ + 4`, `GetBranchTris(s) = 4·2ˢ` — 2ˢ segments, four
verts a ring, i.e. two crossed strips rather than anything camera-facing.

### Checked before believing it

On both `X01_Torso_Tesla_VFX` bolts the target's local +Y points along the
`_Start`→`_End` line to within a degree; the only disagreement is that the engine
runs the full 32 units where the node pair spans 25.3. **That is why the guess
looked right on the torso** — the author aligned the node pair with the axis — and
why it put the legs somewhere else, since nothing ties those nodes to the axis.

The torso arcs now cross the whole back plate instead of stopping short of it.

### What this does not fix

The leg arcs still run down inside the calf. The rule changed their direction and
length, not their **origin**, which is the `BoltGeo_01` node — and the asset puts
that on the bone axis, about 5 units in front of the coil. Nothing in the engine's
path moves it: the bolt starts where its target node is. So either the leg VFX is
authored that way and looks the same in game, or the difference is in something
that has not been measured yet — not in how the bolt is generated.

### Not adopted

The crossed four-vert cross-section. Ours is a mitred, camera-facing ribbon with
arc-length UVs and texture-aspect tiling, and swapping it is a visual change with
no placement benefit; it is decoded and written down here rather than done
half-considered.

Suites: 9 boltbake (determinism survives — the seed is the target's name, not the
block number), 15 live effects, 8 effect bake, 57 freeze, 24 carries-everything,
21 bake.

## 2026-07-31h — six files' animations were all driving one limb's nodes

bungo: *"the two bolts on the torso are not rotated correctly, the start and end
point, they are meant to go across the rear of the torso plate, not away from
it."* Correct, and it was the merge, not the convert — the same file measured
before conversion had it too.

### What the endpoints were doing

The bolt spans were being read from the right nodes: `LightningBolt_01_Start` and
`_End` resolved by name, both present, both unique. What was wrong was **where
those nodes were at t = 2.5**. Instrumented:

    ribbon BoltGeo_01  span  A (12.49, -29.93, 138.46)  B (15.47, -26.81, 163.37)

`B` should be `(-12.70, -30.33, 136.32)` — level with `A`, across the plate. It
was at head height because something else was animating that node.

### One name, six files, first match wins

NifSkope binds a sequence's controlled block with
`target->findChild( nodename )` — the **first** node of that name in the subtree.
The merge qualifies colliding effect names (`R_Pauldron_BoltGeo_01`), and it
rewrites the rows that name them; but a row was matched to its node **through its
controller's Target**, and the rows that move NODES all point at the file's one
shared `NiMultiTargetTransformController`, whose Target is the ROOT. So those
rows resolved to the root, found it unrenamed, and were left naming a node that
had just been renamed out from under them.

Six files, six rows saying `LightningBolt_01`, all binding to the first file's
node. Measured on the merged rig: **one node addressed by 10 rows and two more by
6, where no donor addresses any node more than 4 times.**

The row's NAME is what identifies its node, so the name is consulted first now,
and the controller is only the tie-breaker for a donor that wore one name twice.
After: at most 4 rows per node, and the torso spans read
`A (12.49, -29.93, 138.46) B (-12.70, -30.33, 136.32)` — level, across the back.

### Why it looked like only the torso was broken

Five limbs' endpoints simply stopped being animated, and an un-animated node sits
in its bind pose, which is exactly where it belongs. Only the sixth — the one
every row piled onto — visibly moved. A bug that hides in five places out of six
and shows in one is why the check for it counts **rows per node** rather than
looking for something out of place.

### Looking at it

`WW_SHOT_TEST=<out.png>` with `WW_SHOT_VIEW=front|back|left|right|top`,
`WW_SHOT_TIME`, `WW_SHOT_AT=x,y,z` and `WW_SHOT_DIST`. The arcs on this armour
are on its BACK, which is the one side a default view never shows, and no number
in the suite says whether the thing looks right.

Two things it had to learn, both of which produced a confident wrong answer
first: `setOrientation` recenters and repaints, and a repaint STEPS the scene, so
moving the camera after the stepping loop walks the animation past the instant
asked for — the first shots came out with no arcs in them at all. And framing
decides legibility: a bolt is 4 units wide, so on a 160-unit figure it is a
hairline, while the same effect in its own file — where the camera frames 40
units — looks ten times thicker. Two shots of identical geometry can disagree
about whether anything is there.

Regressions: 24 carries-everything (up from 21, with the rows-per-node check),
15 live effects, 10 artobject, 9 merge sweep, 21 bake, 8 effect bake, 57 freeze,
9 boltbake.

## 2026-07-31g — the leg particles were 21 units out, and the check could not see it

bungo, on the file shipped an hour earlier: *"the leg particles are off position and
broken. The bolt start and end position is not accurate either."* Both were real.
Both were invisible to a suite that had just gone green on 13 checks.

### The check was the first thing wrong

`live_effects.sh` measured the effects **in aggregate**: overall Z range, widest
|X|. A leg effect emitting from the wrong place still lands inside the figure's
bounding box, so the suite passed. It now captures **both** files with the same
harness and compares **per named effect** — triangle count, centroid and bounds —
which is the claim actually being made: *this converts without moving anything*.

The aggregate numbers stay as a cheap net. They are not evidence.

### The leg particles

Four of the six limbs were pixel-identical before and after the convert; the two
legs moved by (−1.65, −13.55, +16.04). That vector is exactly the change in their
pulse mesh's node translation — the offset the flatten introduces when it moves a
shape's vertices into world space and puts the centroid on the node.

The four that were right have their pulse mesh **inside** the effect branch. The
two that were wrong do not: the leg VFX files carry no `NamedAttach` nodes, so
their contents hang off the calf bone as siblings, and the mesh was flattened
while the particle system that emits from it was kept.

`NiPSysMeshEmitter` names that mesh with a **Ptr**, and a Ptr is not a Ref — the
branch walk never reaches it. So the collection has a second phase now: anything a
live block points at, which is otherwise flattenable geometry, becomes a branch of
its own. The leg pulse meshes are kept and placed on the calf stub, and all six
limbs are identical before and after.

This is the third distinct way the same pointer has failed. Deleting the mesh left
the emitter with a dangling pointer and the sprites 90 units out (07-31f);
flattening it moved them 21; only keeping it is right.

### The bolts

Their spans were never wrong — every `_Start`/`_End` node came through the convert
at an identical world position, and the arcs demonstrably reached them. What
changed was the **shape**: 40, 48 and 56 triangles where the source had 48, 40 and
48.

`ProcLightningController` seeded its RNG from **the controller's block number**.
That is a fact about the file's layout, so every merge, every convert, every
deleted block silently reshaped every bolt in the file — and nothing about the
result looked wrong, which is why it survived this long. It seeds from the target
node's **name** now: stable across renumbering, already unique per limb because
the merge qualifies effect names, and hashed with an explicit seed so a Qt release
cannot salt it.

### Proof

`tests/loadingscreen/live_effects.sh` 13 → 15 checks, green, and the two new ones
are the ones that matter: 16 effects on the rig, 16 on the screen, and **every one
generates identical geometry in an identical place**. Byte-for-byte on the dump,
across two separate processes — which also re-proves the generator's determinism,
since the two files share no block numbers.

Regressions: 9 boltbake, 8 effect bake, 21 bake, 57 freeze, 21 carries-everything,
10 artobject, 9 merge sweep.

## 2026-07-31f — a loading screen whose effects are still running

The merge already carried the animation across: merging `X01_Torso_Tesla_VFX.nif`
onto the skeleton imports 131 of its 132 blocks — particle system, every PSys
modifier, the controller manager, both sequences, the object palette, all
re-pointed. **The loading-screen convert is what killed it.** It deletes every
NiNode and then sweeps whatever is no longer reachable as a geometry child of the
root, so the particle systems and every node the sequences drive went with the
skeleton. `--keep-particles` kept the modifiers and left the `NiParticleSystem`
itself to the sweep: a file that loads, and does nothing.

`loading-screen --keep-effects` keeps those branches whole instead.

### Where a branch goes once the skeleton is gone

An ArtObject branch is authored relative to the bone it hangs from, and the
convert deletes that bone. So the bone is **kept as a stub**: same name, its full
world transform written into its local one, its children cut down to the branch.
Nothing inside the branch is touched at all — which is the point, because the
alternative is arithmetic on every node under it, and arithmetic can be subtly
wrong in a way that "it still renders" will not reveal.

The branch root is found by climbing from a seed — a particle system, a
procedural-lightning controller, or a node carrying an `AttachT` — up to the
highest ancestor that is neither the root, nor a bone, nor an ancestor of
anything rigged. An `AttachT` seen on the way up wins over where the climb
stopped: it is the file naming its own attachment point. On the X-01 that gives
13 branches on 8 stubs — `NamedAttachTank_Armor` on `Tank_Armor`,
`NamedAttachHEAD` on `HEAD`, and for the legs, whose file carries no
`NamedAttach` nodes at all, the effect's own top-level nodes on
`RLeg_Calf_Armor2`.

### One animation graph per file

Six merged ArtObjects brought six `NiControllerManager`s, six object palettes,
six multi-target controllers and twelve sequences sharing two names — and they
did not even sit on the root: each landed on the node its file attached to, so
five of them were invisible to anything looking where a manager belongs.

**0 of the 34,983 meshes in the Fallout 4 corpus have two managers.** So the
merge folds them into one on the root (`src/animgraph.cpp`): sequences that share
a name are fused rather than renamed, because whatever plays `autoPlay` has to
drive all six limbs and can only ask for one sequence. Palettes merge by name,
multi-target extra targets merge by pointer, and every controller chain the fold
emptied is re-spliced — deleting a block from a chain would otherwise cut it
there and orphan everything after it.

Result on the full rig: 1 manager, 1 palette, 1 multi-target controller, 2
sequences — `autoPlay` with 66 controlled blocks and `autoLoop` with 82, which is
exactly 148, the sum of the six files'.

### Names, because a palette is addressed by name

Six files bring six nodes called `LightningBolt_01` and six shapes called
`BoltGeo_01`; two of them duplicate names inside themselves. That was survivable
while the merge only placed geometry — each copy hung under its own limb and the
pointers were pointers. It stops being survivable the moment one palette has to
hold them: one name, one entry, one node, and five effects driving the helmet's.

A colliding import is now qualified with its first pre-existing ancestor —
`RLeg_Calf_Armor2_LightningBolt_01` — the same rule `qualifiedEffectName` uses
when the bake writes shapes. References follow by pointer: the palette by its
`AV Object`, each controlled block through its controller's target, falling back
to the old name only when the row carries no controller. The full-rig merge now
reports **no duplicate bone names**, where it used to warn about fourteen.

`X01_Torso_Tesla_VFX` ships a palette with two rows called `BoltGeo_02`, one of
them pointing at the node actually called `BoltGeo_01`. That is Bethesda's, it is
carried verbatim, and it is not ours to "fix": the palette is what the runtime
resolves through, so renaming the row would change which node animates. The test
measures against the donors' own duplicates rather than against zero.

### The file-level extra data stopped travelling

A donor root's `AttachT`, `BSXFlags` and `BSBehaviorGraphExtraData` describe the
FILE, not anything in it, and extra data is linked into whatever node the branch
attaches to — so the helmet effect's `NamedNode&HEAD` ended up **on the HEAD
bone**, saying the skeleton's head is an ArtObject wanting attachment to itself.
Twelve merged pieces brought twelve `BSXFlags`; the loading-screen converter has
been deleting eleven of them ever since, which was this mess being cleaned up
downstream. They are skipped now when the target already has its own.

### Two bugs, both found by measuring rather than reading

**The convert deleted three leg shapes.** The branch climb landed on
`LLeg_Calf_Armor2` for one seed and on its children for others, and the stub
rewrite then cut the bone's children down to the seeds — dropping the leg pulse
and lightning meshes, which are live but are not seeds. They vanished, and with
them the `Emitter Meshes` pointer of two particle emitters. A dangling emitter
mesh does not stop an emitter; it **relocates** it. The sprites came out at
|X| = 90 on a figure 30 wide. Now a stub keeps the children that are live, read
off the node rather than from the seed list, and nested branch roots are dropped
in favour of the outer one. |X| back to 46.9.

**The climb swallowed the skeleton.** With only "is it a bone" as the stop
condition — where "bone" means "some skin's Bones array names it" — a helmet seed
climbed `NamedAttachHEAD`, `HEAD`, `Neck` and would have taken the whole head,
because no skin binds to the nodes in between. It stops at anything rigged now,
and 411 kept blocks became 342.

### Proof

New `tests/loadingscreen/live_effects.sh`, 13 checks green, and it is in two
halves because the two failures are unrelated:

- **It runs.** The GUI harness `WW_LIVEFX_TEST` loads the converted screen, steps
  to t = 2.5 and asks the renderer what it produced: **10 arcs and 6 sprite
  clouds**, world Z 11.9 – 164.4, widest |X| 46.9. No block count can make this
  claim — a particle system's geometry is not in the file, so every count can be
  perfect on a file that draws nothing.
- **It runs where it was.** `world`, a new CLI verb, prints each NiAVObject's
  world transform so two files can be diffed by name: **75 effect nodes compared,
  0 moved.** Shapes are deliberately not compared — a flattened one moves by
  design — and the missing-shape and dangling-emitter checks cover what that
  leaves.

Regressions: 21 carries-everything (up from 12, with a controlled-block total and
an AttachT split), 10 artobject, 9 merge sweep, 21 bake, 8 effect bake, 57 freeze.
The carries-everything self-proof (`WW_BREAK=1`) fails the new check, which is
what says it has teeth.

### What this is not

**Not validated in game.** The evidence for the approach is 18 of the 173 vanilla
loading screens animating node transforms and one, `CreatureBloatfly.nif`,
carrying a full controller manager and sequence — so the menu does step them.
**No vanilla loading screen contains a particle system or procedural lightning.**
The bake (`tools/make_x01_loadscreen.sh`, unchanged) remains the path with
precedent; `--live` builds the same file with the effects running instead, and
the two are identical up to the merge, so they can be compared directly.

## 2026-07-31e — Loaded NIFs behaves like the Block List

### Colour means selected, not visible

The list already used the Block List's exact highlight values — primary `#FF9D00`
on light blue, secondary `#FF7200` on dark blue — but keyed on **visibility**. So a
visible row looked permanently selected and there was no way to see what actually
was selected. Visibility has its own control now; it does not get to colour a row.

Colour follows selection, read straight off `State_Selected` and the view's current
index rather than plumbed through the model. The primary document keeps its arrow
and loses its background: the arrow already says which one it is.

### Right-click acts on the selection

Selecting several rows used to offer **only** the merge items, so hiding six limbs
at once was impossible — you did them one at a time through a menu that silently
applied to whichever row you happened to click. A multi-row right-click now offers
Show All, Hide All, Make All See-Through, Make All Solid, Merge…, and Remove, each
applying to the whole selection.

`selectedWorkspaceTargets()` resolves a selection that can mix real document windows
with data-only background documents into one list of flag pointers, which is what
lets a single loop serve every bulk action. The clicked row is included even when
unselected, so right-clicking a row you have not selected still acts on that row
rather than on nothing.

### X removes, as in the Block List

`X` or `Delete` in the list removes the selected documents, handled in `eventFilter`
next to the Block List's identical shortcut. A window is un-enrolled; a data-only
document is deleted, and if it was the marked skeleton the mark is cleared with it.

### The skull moved into the strip

It was the item icon, left of the name, where it competed with the primary's arrow.
It is now the first glyph of the right-edge strip, ahead of the eye — **skull, eye,
see-through**. Its slot is reserved even when nothing is marked so the two real
toggles stay aligned down the list, and the slot is dead to the mouse, so a stray
click cannot silently unmark a skeleton. The primary row can show the skull without
the toggles, since it is always drawn and has no visibility to offer.

### The Untitled row

An empty unsaved primary got a row saying "Untitled" that could not be removed —
Remove is disabled for the primary — and that is the state NifSkope starts in, so
the list opened with a row nobody asked for and nothing could clear. It is skipped
while the primary has no file and no unsaved changes. Content brings it straight
back, so a scratch document being built up does not vanish.

Keyed on the undo stack rather than a block count, because a starter document may
legitimately carry blocks and the count would not have told the two apart.

### Facing for baked effects

The snapshot facing was hardcoded to −Y. The Freeze dialog now offers front (−Y),
behind (+Y), its left, its right, or **whatever the viewport is showing now**, the
last resolved at bake time from `GLView::viewForwardAxis()` — the same axis
`drawPreview` passes when the effect is live, so "bake what I am looking at" and the
preview cannot disagree. The result text names the facing used.

### Proof

`WW_WORKSPACE_TEST` 18 → 20 checks, green, including two rows selected at once and
"removing the selection removes every one of them" — 2 documents, 2 removed, 0 left.
A per-row implementation would have left the unclicked ones behind, which is what
that check exists to catch.

Verified by looking as well: `ww_workspace_list.png` shows an unselected primary with
its arrow, the active selection member in light blue with `#FF9D00`, and the second
selected row in dark blue with `#FF7200`; `ww_groupskel_list.png` shows skull → eye →
see-through in order, on a row that is visible but *not* highlighted.

Regressions: 11 group skeleton + 12 carries-everything + 21 bake + 6 artobject, green.

**Not GUI-verified:** the Facing dropdown itself. It is a modal dialog with no
harness, so only the code path is exercised.

## 2026-07-31d — secondary refresh: 54 ms → 7 ms per edit

Fixing the stale-preview bug in 07-31c made every edit to a secondary document drop
and rebuild its whole Scene. That was correct but blunt, and it landed on exactly
the workflow the new feature invites: **posing a marked skeleton is a
continuous-edit workflow**, and the refresh sits on that path.

Measured on the FO4 power-armour skeleton as a secondary: **54 ms per edit**, about
18 fps while dragging.

### Two kinds of change, two costs

`Scene::update` refreshes the nodes a Scene already has. It cannot create nodes for
blocks that did not exist when `make()` ran, and calling it on a model that has just
had blocks spliced in **crashed the process** — so a structural change still has to
drop the Scene and rebuild.

A value change moves no blocks, which is the case the primary has always handled
with `Scene::update` on every edit. Those take the cheap path now. Block count is
the discriminator: it catches adding and removing, which is every structural edit
these previews see. A reorder preserving the count would slip through, so it is not
offered as a general-purpose test.

**Result: 7 ms per edit, 7.7× faster**, with all 11 group-skeleton checks still
green — 2863 of 2863 vertices following, exact restoration on unmark.

### The measurement was wrong first

The first number was **123 ms per edit** — and meaningless, because the harness's own
`repaint()` sleeps 120 ms to let things quiesce, so the timing loop was measuring
that sleep. Re-timed around a real edit cycle (change the model, let the coalesced
flush run, paint) with no sleep in it, and with one warm cycle discarded.

The threshold is **25 ms**, deliberately not generous: rebuild-always measured 54 and
the in-place refresh measures 7, so a loose bound would pass both and guard nothing.
This one fails if the split is ever lost.

## 2026-07-31c — mark one NIF as the skeleton and the rest snap to it

Right-click any loaded NIF → **Use as Skeleton for Loaded NIFs**. Every other
document then evaluates its bones against that file **by name**, so a skinned
armour piece follows it instead of sitting at bind pose. The marked row gets a
skull in the browser, because a setting that changes how every *other* row is drawn
has to be visible without opening a menu.

**Strictly opt-in**, as asked: with nothing marked, not one transform is computed
differently from before the feature existed. A file that merely happens to contain
a skeleton does nothing at all. The whole lookup — name resolution included — is
behind one `isEmpty()` check.

### Why by name

Bones are addressed by **block number** inside each file. That is why an armour
piece opened on its own sits at bind pose: its bone links point at its own flat
`NiNode`s, with no hierarchy above them to pose. Block numbers mean nothing across
files, so snapping has to go by name.

`Scene` carries a `QHash<QString, Transform>` of the marked skeleton's pose,
root-relative so a marked file with an offset root does not shift everything that
snaps to it. Deliberately a **snapshot, not a pointer** to the skeleton's Scene: a
Scene reaching into another Scene is the shape of the bug that once emptied whole
frames, and a value copy cannot dangle when a document closes mid-frame. A cheap
summed fingerprint decides when to re-push, so marking a skeleton does not force a
full propagation of every secondary on every frame.

### The bug underneath: secondary previews were never updated at all

The feature appeared to work and moved nothing — 0 of 2863 vertices followed a
skeleton posed by 146 nodes, with the override present (147 entries) and every one
of the shape's four bones matched. `Chest` sat at Z 106.332 before and after.

Cause: **a workspace Scene had no connection to its own model.** The primary is kept
current by `GLView::dataChanged` → `Scene::update`; a secondary was built once by
`Scene::make` and never told anything again, so editing a secondary left its preview
showing the bytes it was loaded from. Pre-existing, and invisible until something
depended on a secondary's live state.

Fixed by connecting each secondary model's `dataChanged`, coalescing to one flush
per event-loop turn. Two things that had to be got right:

- **Not from the draw path.** Rebuilding a Scene inside `paintGL` killed the process
  outright — the merge harness went from 18 green checks to a **0-byte log**, because
  the crash beat `QTextStream`'s flush.
- **Rebuild, not patch.** `Scene::update` with an invalid index refreshes the nodes
  it already has; it does not create nodes for blocks that did not exist when
  `make()` ran. After a merge splices in 158 blocks that leaves the preview
  structurally wrong, and calling it on a spliced model crashed too. The Scene is
  dropped and re-made instead.

### Proof

`WW_GROUPSKEL_TEST`, 10 checks green. It measures the **evaluated skin** —
`Shape::skinVertex` for all 2863 vertices of the primary's largest shape — which is
what the viewport actually draws; block counts and transforms would not answer the
question.

Marking a skeleton still in its bind pose moves almost nothing, because the armour's
bones and a bind-pose skeleton agree — so that is *logged, not asserted*. Asserting
movement there would be asserting that two identical poses differ. The skeleton is
posed instead, and then:

| | result |
| --- | --- |
| marked, bind pose | 0 moved; 147 override entries; all 4 shape bones matched |
| marked, posed +30 Z | **2863 of 2863 followed**; `Chest` 106.3 → 166.3 |
| unmarked | **0** vertices differ from the start — exact restoration |
| unmarked, posed again | **0** moved — a loaded skeleton does nothing unmarked |

Regressions: 18 workspace + 12 carries-everything + 21 bake + 6 artobject + 9 merge
sweep, all green.

## 2026-07-31b — Merge Into, and proof the merge carries everything

### Merge Into

The multi-row menu only offered **Merge into a new NIF…**, which writes a file and
leaves every loaded document alone. Added **Merge N Selected Into "<name>"**: the
others are spliced into the loaded document itself, no save dialog, and the result
stays in the workspace where you can look at it.

The target is now **the row you right-clicked**. It used to be whichever selected
row happened to come first, mentioned only in a tooltip — so which file absorbed
the others depended on selection order you cannot see. Clicking one of several
selected rows is an unambiguous way to say which, so the clicked row is moved to
the front for both merge paths.

`nifMergeData` already wraps each donor in its own `nifSnapshotOp`, so undo steps
back one donor at a time rather than all-or-nothing.

### Does the merge carry everything?

Asked directly, so it was measured rather than asserted:
`tests/merge/carries_everything.sh` takes a block-type histogram of the target and
every donor, merges, and requires the arithmetic to hold.

It does. Across three cases — two Tesla effect files, armour plus its hardware
layer, and the full 15-donor rig — **every single non-`NiNode` type adds up
exactly**: `NiControllerManager`, `NiControllerSequence`, `NiTextKeyExtraData`,
`NiDefaultAVObjectPalette`, every interpolator/data pair, all nine `NiPSys*`
modifier types, `NiParticleSystem`, `NiPSysData`, `BSProceduralLightningController`,
both shader-property controller types. No losses and no duplicates.

`NiNode` is the one type allowed to shrink, and does: by 2, 14 and 105 across the
three cases. That is the bone dedupe that lets merged armour share one skeleton
instead of carrying six copies, not loss.

Counts alone are not enough for sequences — one that survived but got renamed is
broken, because sequences are addressed by name — so every donor's sequence
**names** are checked to still be present.

**Proven to bite.** `WW_BREAK=1` throws the merge away and measures the bare target
instead, which is precisely "the donors did not make it": 43, 9 and 48 block types
reported lost and 4 and 12 sequence names gone. A green run means something.

### Proof for Merge Into

`WW_WORKSPACE_TEST` 13 → 18 checks with `WW_WORKSPACE_MERGE=1`. The in-place path
is driven for real, modal summary included (a timer dismisses it), and measured:
target **132 → 290** blocks, donor **159 → 159**. What that rules out is a merge
that emptied the donor or edited the wrong one of the two, both of which look
identical from the dialog. Plus the guards — merging a document that is not there,
and merging a document into itself, both correctly refuse.

## 2026-07-31a — Loaded NIFs: selection stops hiding things, and the eye

### The bug: selection *was* visibility

Selecting a row in Loaded NIFs used to write `sessionPreviewVisible`, and
`rebuildLoadedNifsBrowserGroup` selected rows back from it. The two were the same
state under two names, so clicking a row to aim a menu at it **hid every other
document**, and there was no way to say "operate on this one" without also
changing what the viewport was showing.

They answer different questions. Selection is "which rows is the next command
about" — what the multi-row merge menu reads. Visibility is "what is drawn", and it
belongs to the row's own toggle and nothing else. Both directions of the coupling
are gone; `wireLoadedNifsSelection` is deliberately empty, with the reason written
where the handler used to be.

A freshly added document is visible, as it always intended to be — the flag was
already `true` at construction, and the selection handler was overwriting it on the
next selection change.

### The toggles are independent now, and one is an eye

They used to be two mutually exclusive **mode** buttons (solid / ghost, where
clicking the lit one meant off). That cannot represent "hidden" as a state of its
own — it shows as neither button lit, which reads as broken — and turning a
document off forgot whether it had been ghosted.

Visible and see-through are two separate facts, so each gets a toggle:

| Glyph | Off | On |
| --- | --- | --- |
| Eye | closed lid, muted | open almond with a pupil, accent |
| Half-disc | outline only, muted | left half filled, accent |

The eye follows Blender's outliner and starts open. The second glyph has **no
Blender counterpart** — Blender's outliner offers selectability and render-disable,
neither of which means anything for a NIF workspace — so it stays the conventional
half-filled opacity disc. Stated as a deliberate divergence.

See-through on a hidden row is a preference that takes effect when the row comes
back, so toggling it does not force the row visible; the glyph draws at 45% to say
it is set but currently inert.

Colours come from `wwSkinColor( "accent" )` and `"textMuted"`, replacing two
hardcoded literals.

### Double-click no longer promotes

Double-clicking a row opened that document as primary and re-homed the workspace
around it — a large, disruptive action hanging off a gesture people make by
accident while picking rows, and it was breaking things. The handler is gone;
**Make Primary / Edit** is already the first item in both right-click menus, which
is where something that consequential belongs.

### Proof

`WW_WORKSPACE_TEST` extended from 8 to 13 checks, all green. Beyond the existing
ones it now asserts that every new document starts visible, and — the check that
would have caught the bug — that selecting the first row, the last row, every row,
and no rows each leave every document's display mode **identical**. A new
`workspaceDisplayMode()` getter mirrors the existing setter so this is readable
without reaching into the delegate.

Verified by looking, too: `ww_workspace_list.png` shows the three states side by
side — open orange eye with an empty disc (solid), open eye with a half-filled disc
(see-through), and a closed lid with a dimmed disc (hidden) — with the selected row
deliberately *not* the only visible one, the state that used to be unreachable.

## 2026-07-30r — the loading screen has its arcs, and the pipeline is a script

`tools/make_x01_loadscreen.sh` builds the deliverable end to end, and the shipped
`X01TeslaLoadScreen.nif` now carries the Tesla effects instead of dropping them:
62 shapes, **18 of them baked effects**, 0 emitter blocks left.

### The order is the whole point of the script

Doing this by hand gets it wrong in two ways that both produce a file that loads:

**Merge with the effects still LIVE.** The established pipeline freezes each effect
file and then merges, because a frozen file is what a finished screen wants — that
is what `tests/merge/artobject_attach.sh` does. But freezing strips the controller
graph, and the arcs and sprites are generated *by* that graph. Freeze first and
there is nothing left to snapshot.

**Snapshot on the assembled rig, not per file.** A baked shape is written in the
space it was captured in. Capture a helmet effect inside its own file and the
coordinates are helmet-local; the merge would then have to rebase them, and it only
knows how to rebase `NamedAttach` branches, which a baked shape is not. On the
assembled rig everything is already in actor space, so no rebase is needed and none
happens.

### Names have to distinguish, not just label

An assembled X-01 carries **four nodes called `BoltGeo_01` and six called
`LightningArcs_VFX`** — the effect files are authored from a shared template, the
same name reuse that once made the merge fuse the two arms.

`qualifiedEffectName` walks up the parent chain and prepends the first
distinguishing ancestor, falling back to a numeric suffix only when the hierarchy
cannot tell them apart. On a merged rig that ancestor is the limb, so the shapes
come out as `RLeg_Calf_Armor2_LightningArcs_VFX` and `NamedAttachHEAD_LightningArcs_VFX`
rather than six copies of one name. `writeShape` additionally refuses any name a
block in the file already holds, and `Result::shapeNames` reports what each capture
was actually written as.

### The 115-unit error that was not there

The first full-rig run failed two checks and reported a **115.44-unit** round-trip
error. It was the harness: it matched baked shapes to captures **by name**, and with
four `BoltGeo_01` in the file it was measuring a helmet arc against a leg arc. The
`Data Size` failure was the same cause — it took `hasCol` from the mismatched
capture.

Matching by index through `Result::shapeNames` took the error to **2.1 × 10⁻⁶
units** with all ten checks green. Diagnosed by measurement, not by argument, and
the fix went into the product as well as the test, because indistinguishable shape
names are a real defect in a file someone has to edit.

A uniqueness check now runs in the harness every time, and
`tests/loadingscreen/effect_bake.sh` gained a second stage on **both arm effects
merged onto one skeleton** — the actual collision case, since one file's effects
have distinct names and cannot catch this.

That new check was wrong on its first attempt too, and in an instructive way: it
counted every `BSTriShape` and failed on shapes the bake never touched. The arm
files carry their own duplicate `Bolt_01`/`Bolt_02`/`Bolt_03` (0 vertices each — the
empty shapes the controllers targeted) and *both* name their pulse mesh
`X01_ArmRight_Tesla_Pulse`, which is a slip in the source assets. Asserting that is
a claim about the input. It diffs the pre-bake and post-bake shape lists now, so
"which are baked" is established rather than guessed.

### Where everything landed

Fan Z 128.0 (chest), helmet effect 156.4, arm pulses ±26.3 symmetric, leg pulses
48.7, bolt arcs 132–144 at the chest and arms and 87–91 at the legs, sprite clouds
47–157. Nothing beyond |X| = 60. Full Z range 0.0 – 156.9, feet to helmet.
`LoadingMenuZoomTarget` present. `PAFrame01` at Z 79.9.

Still carried, still bungo's call: `Frame.nif` brings `basesuit_reduced` and
`BaseMaleHands_reduced` — the occupant — which vanilla's X-01 screen does not have.

Suites: 8 effect_bake + 10 effectbake + 9 boltbake + 57 freeze + 21 bake + 6
artobject + 9 merge sweep, all green.

## 2026-07-30q — the other half: effects written into the NIF, and the choice

The capture side landed in 07-30p with an explicit gap: *"NOT built: emitting the
arcs into the NIF as BSTriShape + a copy of the shader property."* That gap is
closed, particles are captured too, and the choice the user asked for is in the
Freeze dialog.

### The choice

**Freeze Animation** already asked for a sequence and a time per file. It now also
asks what to do with the effects, when the file has any:

| Effects | What happens |
| --- | --- |
| Snapshot as static geometry at this time | capture the instant, write it as a `BSTriShape`, remove the emitters |
| Keep the emitters running | left alone — live particles and live shader animation |
| Remove them | dropped without a bake |

Snapshot is disabled, with the reason shown, when the document has no live scene:
a bake reads the **rendered scene**, not the file, so there is genuinely nothing to
read. A background document visible in the workspace does have one — `paintGL`
steps every workspace scene with the primary's time — so "freeze each limb at its
own instant" works for effects too, not just for values.

Order inside the dialog is capture → write → freeze, and it has to be. The capture
reads the live scene, so it must precede the freeze that strips the controllers
generating it; the write must precede it too, for a duller reason — the captures
hold `QModelIndex`es to the shader properties they need copied, and removing blocks
moves those.

### Particles

`Particles::bakeSprites( viewAxis, tris, uvs, cols )` builds what
`particles.geom` builds on the GPU, corner for corner: one quad per live particle,
with its size, flipbook cell, sprite rotation and colour. Only the first `active`
particles — the tail of the array is dead storage the draw call never reaches, and
baking it would scatter stale sprites around the model.

The flipbook cell computation was **lifted out of `drawShapes` into
`spriteUVCell()`** and both paths call it now, for the same reason `buildRibbon`
was lifted out of `drawPreview`: two copies would drift, and the bake would quietly
show a different cell than the preview.

Unlike the arcs this is **not reproducible from the time alone.** Sprite positions
integrate frame to frame, so the scene must be *stepped* to the instant, never
jumped there. Both the dialog and the harness step at 1/30 s, and going backwards
restarts from zero.

### Emission

`src/bakegeom.{h,cpp}`. `writeShape` creates one `BSTriShape` under the root:
world capture → root-local (the root's own transform comes off first) → re-centred
on its centroid, translation carrying the position, exactly as the loading-screen
converter does for every other shape.

The vertex layout is the one the OBJ importer has been shipping — full-precision
position, half UV, byte normal/tangent, byte colour; 28 bytes, or 32 with colours.
Full precision is deliberate: half floats step ~0.008 units at Z ≈ 111 and these
are thin ribbons. Vertex colours are not optional either — they carry the head and
tail fade, and `bakeRibbon` was dropping them, which flattens every bolt to a
uniform bar. It takes a `cols` out-parameter now.

The shader and alpha properties are **copied**, not shared: the originals hang off
the emitter, and the emitter is removed in the same operation.

`writeShape` refuses to claim a shape it did not fill. It counts the positions and
UVs that actually took the value and, if any did not, **removes the block it just
created** and reports the count. That is the precise failure that shipped in the
converter three commits ago: `set<Vector3>` on a half-precision "Vertex" doing
nothing and returning a `false` nobody read.

### Two real bugs, both found by testing rather than reading

**A green suite that depended on the registry.** `WW_BOLTBAKE_TEST` was 9/9 green
when written and returned *"baked 0 arcs"* when re-run, with no code change in
between. Cause: a GUI session had left `GLView/Enable Animations` at **false**, and
with animation off no controller updates — so there are no bolts and no live
sprites, and the capture correctly finds nothing. A harness that inherits the
user's settings is not measuring the code. Both harnesses force it on now, which
needed a new `GLView::setAnimationEnabled( bool )`: `updateAnimationState` is a
QAction slot that reads `sender()->data()` and does **nothing at all** when called
directly, which is why the first fix appeared to change nothing.

**A harness that wedged the process.** `WW_EFFECTBAKE_TEST` modifies the model, and
quitting with a modified document raises a save prompt that blocks — a `-Wait` run
hangs until something kills it, and `CloseMainWindow` cannot dismiss a modal. It
clears the undo stack and the modified flag before quitting now.

### Proof

`WW_EFFECTBAKE_TEST`, 10 checks green on the torso effect at t = 2.5: 3 effects
captured (`BoltGeo_01` 40 tri, `BoltGeo_02` 48 tri, `LightningArcs_VFX` 12 tri of
sprites), 3 shapes written, 300 vertices, 16 emitter blocks removed, none left
behind, saved, re-read, sizes and `Data Size` consistent, shader properties kept.

The check that matters is the last: **save, re-read from disk into a fresh model,
and verify `translation + vertex` reproduces the captured world position.** Worst
error **5.4 × 10⁻⁵ units**. That one invariant catches all three ways FO4 geometry
writing looks successful and is wrong — a silent half-precision write, a `Data
Size` that disagrees with the desc, and arrays that were never sized.

Proven to bite, not assumed to. Writing `Translation` as zero instead of the
centroid — the same class of bug as the converter's — takes the error to **38.4
units** and fails that check *while the other nine stay green*. Sizes, desc, save,
re-read and properties can all be perfect while the geometry is nowhere near where
it was captured.

`tests/loadingscreen/effect_bake.sh`, 6 checks, carries it to the end: the baked
file has no emitters left, its shapes survive the loading-screen conversion, and
they still have triangles in them afterwards.

Regression suites: 21 bake + 6 artobject + 9 merge sweep + 9 boltbake, all green.

### Still not done

The facing is hardcoded to −Y, the front of the figure. That is the right default
for a loading screen and the result text says so, but a snapshotted billboard is
wrong from every other angle and there is no control for choosing it.

Baking cannot be done from the CLI. It reads the rendered scene, and `-no-gui` has
none.

## 2026-07-30p — the Tesla arcs can be captured (half of it built)

0 of the 173 vanilla loading screens contain a `BSProceduralLightningController`,
so the X-01 Tesla arcs — the visual signature of that armour — were being dropped
by the loading-screen convert. They can be kept, and the capture side is now
built and proven.

### Why it is possible at all

`ProcLightningController::regenerate()` is **seeded from quantised time**, on
purpose: an earlier version seeded from a mutation counter, which made the bolt
depend on how many times regenerate happened to run before a frame was observed.
Keyed on time, the same instant always produces the same bolt.

Confirmed before relying on it: two separate process runs rendering
`X01_Torso_Tesla_VFX` at t = 2.5 produced **pixel-identical** frames (`C00F0ED6…`).

The geometry is also camera-independent except in one respect. `regenerate()`
produces polylines in a normalised frame; `boltPoint()` puts them in world space
from the Start/End nodes. Only the ribbon's **width expansion** billboards against
the camera.

### The refactor, proven by pixel hash

`drawPreview()`'s geometry construction became
`buildRibbon( viewAxis, tris, cols, uvs, tint )` — the view axis is a parameter
now, because a still cannot turn to face anyone and a bake has to pin it.
`drawPreview()` passes the scene camera and is otherwise unchanged.

That moved ~180 lines whose own comments describe subtle fixes ("this is the
bolts-do-not-connect break"), so it was verified the only way worth trusting: the
same file at the same time hashes **identically** before and after.

### Capture

`GLView::bakeLightningArcs( viewAxis )` returns one `BakedArc` per arc —
world-space triangles, UVs, tint, and the `BSEffectShaderProperty` it draws with.
Walked off `Scene::nodes`, not `Scene::pendingBolts`: that queue is filled during
`drawShapes` and cleared at the end of it, so it is only non-empty mid-frame.

`Node::lightning()` is a new public accessor, added inside an **existing** public
run — a fresh `public:` in that header would flip the access of everything below
it, which has bitten it before.

New harness `WW_BOLTBAKE_TEST`, 9 checks green on the torso effect at t = 2.5:
2 arcs, 88 triangles, world bounds (−17.6, −13.1, 28.2)..(13.2, 4.7, 41.1), every
arc with whole triangles, one UV per vertex, a named shader property, real extent,
and **identical geometry when baked twice**. That last check matters because "the
generator is deterministic" and "the bake reproduces it" are different claims.

### What is NOT built

Emitting the captured arcs into the NIF as `BSTriShape` + a copy of the effect
shader property. That half has to construct FO4 vertex arrays, where the
desc / `Data Size` / deferred-array landmines live (see 2026-07-18b), so it gets
its own pass rather than being rushed onto the end of this one.

**Particles stay undoable this way.** Their positions integrate frame to frame —
they cannot be evaluated at an arbitrary *t* the way a keyed controller can — so
there is no equivalent of the determinism that makes the arcs bakeable.

## 2026-07-30o — building a real loading screen found two merge bugs

Ran the finished pipeline for its actual purpose — freeze each X-01 Tesla effect
at its own instant, merge skeleton + armour + Tesla hardware + effects, apply
`PAStandGeneric`, convert — and the first output was a scattered cloud of
fragments. Three things were wrong, one of them mine to begin with.

New test: `tests/merge/artobject_attach.sh` (5 checks, green).
Deliverable: `mods/X01Tesla/meshes/LoadScreenArt/X01TeslaLoadScreen.nif`.

### The `_Tesla` files are a hardware layer, not a variant (my mistake)

`X01_Torso_Tesla.nif` has **2 shapes** (coil + glow); `X01_Torso.nif` has **6**
(plate, decals, fan cover, cap). They stack in game. Merging only the `_Tesla`
files gave a figure made entirely of latches and coils with no armour. Nothing to
fix in the code — the input set was wrong, and the render said so immediately.

### `NamedAttach<NodeName>`: effects place themselves

An ArtObject's top-level children are named for their destination, and the merge
now reads it. The X-01 set, off the files:

    torso   NamedAttachTank_Armor, NamedAttachRoot
    arm     NamedAttachL_Pauldron, NamedAttachRoot, NamedAttachLArm_ForeArm_Armor
    helmet  NamedAttachHEAD

This matters because **one file carries effects for several nodes** — the arm has
a pauldron arc and a forearm arc — which `AttachT` cannot express, since it names
one place for the whole file. All six Tesla effects now land correctly with no
`--attach` at all; before, three of them fell to the root.

### Bug 1: name-dedupe fused the two arms together

`X01_ArmLeft_Tesla_VFX` and `X01_ArmRight_Tesla_VFX` have 20 nodes each and
**15 of the names are identical** — `LightningBolt_01`, `BoltGeo_01`,
`LightningArcs_VFX`... They are copies of one template. De-duplicating NiNodes by
name — the thing that lets merged armour share a skeleton — mapped the right
arm's effect nodes onto the left arm's, and its geometry landed at X = −117 and
−156 with the arms at ±25.

Fix: **an ArtObject's internal nodes stay private.** Only a genuine bone still
fuses. Scoped to effect files on purpose — a first attempt gated dedupe on
bone-reachability for *every* donor and broke 4 of the 9 merge-sweep cases,
because FO4 armour stores its bones FLAT: `Chest` is present in an arm file
without the arm being skinned to it, so a reachability rule duplicates it.

`NifMergeResult::privateNames` reports what was deliberately duplicated.

### Bug 2: those branches are in ACTOR space

`NamedAttachR_Pauldron` sits at (20.96, −7.33, 126.14) — a shoulder position on
the actor, not an offset from the shoulder. Hanging it under its bone applies the
bone's world transform again, and because arm bones carry a large rotation the
piece is *flung*, not merely doubled.

Fix: rebase, `newLocal = nodeWorld⁻¹ · local`, so the branch keeps the world
position the file gave it.

**And not universally** — that overcorrected first. A file whose `AttachT` already
names a node is authored node-local (`X01_LegLeft_Tesla_VFX`'s tops are
(−0.61, −15.44, 17.71) and similar, offsets from the calf). Rebasing those drove
the helmet effect from Z = 156 down to **Z = 5, at the ankles**. So `AttachT`
naming a node is the signal for authoring space, and only files that name nothing
get rebased. The helmet has *both* conventions, which is what exposed it.

### Converter: compact what the removals hollow out

`removeNiBlock` rewrites a link to a deleted block as −1 but leaves the array
entry, so a root that carried the skeleton's extra data came out with
`Num Extra Data List = 31` and 30 of them −1. Now compacted. Also one `BSXFlags`
instead of one per merged file (five, here) — every vanilla screen that has one
has exactly one.

### Two render misreads, again

The output has a grey dome floating above it. I called it a displaced helmet, then
a bounding artefact, and measured neither claim first — no shape in the file
reaches above Z = 166. It is **NifSkope's own light-position marker**: rendering
the same file from the left view leaves the dome at the same *screen* position
while the model turns 90°, and the vanilla X-01 screen shows none because its
different bounds radius puts the marker out of frame.

Likewise the head reads as "helmet missing" at this zoom. It is not: the helmet
node sits at Z = 142.4 with radius 17.35, spanning Z 126–161 centred on X ≈ 0 —
a correctly placed, correctly sized X-01 helmet.

Third and fourth time this week that eyeballing a render produced a wrong
conclusion. The rule earned: **measure the geometry before naming what is wrong
with a picture.**

## 2026-07-30n — workspace documents render for real (07-30k was wrong)

**Correction to 07-30k.** That entry said the per-document `Scene` path "still
draws nothing" after `borrowRenderer` and left it unwired. That was wrong. It
draws, it always drew once the two-Renderer bug was fixed, and it is now the
default for every SOLID workspace document.

The mistake was in the test, not the code. The workspace documents used to judge
it were the X-01 Tesla **VFX** files: a few small effect ribbons whose root sits
at the origin, drawn between the feet of a full-height primary. Nothing visibly
appeared, and "nothing visibly appeared" got written down as "nothing drew".
Re-run with `rig.nif` — the whole assembled armour — as the secondary against a
single helmet as primary, and the entire rig renders behind it with its own
materials, textures and shaders. The probe told the same story before the picture
did: `roots=1 nodes=164 shapes=17 haveRenderer=1 boundsR=69.1` for the rig, versus
`boundsR=-0.25` for the VFX file that had looked like a failure.

This is the second time in two days that an off-frame or too-small subject got
read as a regression. The lesson is the same one as the pistol screenshots:
**when a render test shows nothing, first prove the subject was in frame and big
enough to see.**

The lead recorded in 07-30k — "most likely the per-frame shader and light setup
paintGL does around the primary's draw, which a second scene never receives" —
was also wrong, and usefully so. Those uniforms live on the shared
`NifSkopeOpenGLContext` (`scene->renderer`), **not** on the `Scene`. View
transform, lighting, tone mapping, scene options: all uploaded once for the
primary and already in place for anything else drawn in the same frame. A
secondary needs nothing but the same `viewTrans` and `time`.

So `refreshSessionPreview` now routes the two buttons to two genuinely different
things:

- **Solid** → `GLView::setWorkspaceRenderModels`, one `Scene` per document, built
  the first frame it appears and kept until it leaves the list (`make()` compiles
  geometry and is far too expensive per frame). Options and vis mode mirror the
  primary each frame so a secondary cannot ignore a lighting or wireframe change.
- **Ghost** → still the flat translucent soup, which is what "roughly where does
  this sit" wants and which avoids threading a global alpha through every shader.

Verified with no environment flags set: primary `rig.nif`, `X01_Torso_Tesla.nif`
solid and `X01_Helmet.nif` ghosted — the Tesla fan assembly appears on the chest
with its own material, and the helmet reads as translucent over the head.

## 2026-07-30m — bake a posed rig into loading-screen art

The last step of the pipeline. In the Loaded NIFs list, 2+ rows selected:
**Merge and Convert to Loading Screen…** — merge as before, then evaluate the
skins away, drop the skeleton, and re-centre every shape on its own origin. Also
`nifcli loading-screen <file> [--no-zoom-target] [--keep-particles] -o OUT`.

New: `src/loadingscreen.{h,cpp}`, `tests/loadingscreen/bake_check.sh`.

Whatever pose the bones are in when it runs is the pose that gets baked. That is
the whole point of the ordering: assemble, pose, freeze each effect at its own
moment, **look at it**, then convert.

### The format is not a guess, and one file is not a corpus

`meshes/LoadScreenArt` is 173 files. Every rule below is read off them, not
inferred from `Armor03PowerArmor8X01.nif` alone:

- Plain `BSTriShape`, no `BSSkin::Instance`, no `BoneData` — **the skin is
  evaluated away**.
- No `NiNode`s but the root and an optional `LoadingMenuZoomTarget` (65 of 173) —
  **the skeleton is dropped**.
- Non-zero translation, **identity rotation**, vertices near a local origin. Not
  a style choice: FO4 vertices are half floats, and the step at Z ≈ 111 is
  ~0.0078 units — enough faceting to see on a helmet. **Per shape, translation =
  the world centroid, vertices = world − centroid.**
- Controllers are *not* required to be stripped (18 of 173 move, 13 animate a
  shader), so freezing stays a separate, optional step.
- `BSXFlags` (38 files) and `BSBehaviorGraphExtraData` (9) are kept.
  `BSBoneLODExtraData` and `BSBound` are not — they describe a skeleton there no
  longer is.

### The evaluation is the renderer's

Same arithmetic as `Shape::skinVertex` — the weighted matrix from
`vertexSkinMatrix`, normalised by the weight sum. The renderer works in the
shape's local space and multiplies its world transform back afterwards, which
cancels, so the vertex wanted here is just

    world = ( Σ wᵢ · boneWorldᵢ · boneBindᵢ ) / Σ wᵢ · v

**Normals, tangents and bitangents are rotated by the same matrix.** Skipping
that is what makes a bake look correct until you find a curved surface: the
positions move and the lighting does not follow. The bitangent is stored split
across three fields of two different types, so it has to be reassembled to be
rotated at all.

### Two things that only show up downstream

- **Deleting the skeleton orphans a great deal.** Its ragdoll collision, its
  `BSSkin::Instance`/`BoneData`, every controller that drove a bone — all left
  with nothing pointing at them, and NifSkope's `rootLinks` cannot tell those
  from a real root, because it computes "root" as "nothing refers to it". So
  reachability is walked from the block actually kept, over **Ref links only**: a
  Ptr is a weak back-reference (`Skeleton Root`, a controller's `Target`) and
  following those keeps exactly the debris the sweep exists to remove. 39 blocks
  out instead of 68.
- **An empty shape and the controller that fills it live or die together.** X-01
  Tesla's `Bolt_01`/`Bolt_02` are `BSTriShape` with `Num Vertices` 0 and
  `Vertex Desc` 0 — pure placeholders a `BSProceduralLightningController`
  generates into at runtime. Dropping the controller and keeping the shape leaves
  geometry that draws nothing. `--keep-particles` keeps both.

Particles and procedural lightning are dropped by default and **reported**, with
the count: 0 of 173 vanilla screens contain either. Keeping the look means
snapshotting their generated geometry into an effect-shader mesh, which is not
built.

### The test is about where things are

`tests/loadingscreen/bake_check.sh` — 17 checks, green. A skin bake fails in ways
a block list cannot see, so:

- **Vertex Desc and Data Size move together.** Dropping the Skinned attribute must
  shrink the stride by exactly 12 bytes per vertex (4 half weights + 4 byte
  indices) and `Data Size` must equal `stride*verts + 6*tris`. Measured on
  `X01_Torso:0`: 32 → 20 bytes/vertex, 115820 → 81464. Get this wrong and the
  file reads back as garbage without necessarily saying so.
- **Left/right symmetry.** Arms and legs are separate shapes through separate bone
  chains: baked centroids come back at X = −26.439 / +26.472 and −16.366 /
  +16.355. Mirrored to 0.03 units is not something a wrong evaluation produces.
- **A pose is baked, and only where it should be.** Moving `LArm_UpperArm` alone
  moves the left arm from (−26.44, −4.06, 115.47) to (−18.96, −3.11, 95.44) and
  leaves the right arm bit-identical.
- **Vertices are local**, largest sampled 49.7 — vanilla stores body vertices
  around 30 with the node at Z = 111.

End to end, with three VFX files each frozen at a different instant: 17 skinned
shapes evaluated, 7 rigid folded, 167 nodes and 338 blocks removed, 59 left.

Not verified in the viewport — headless only, per the standing rule about not
stealing focus. In-game validation is bungo's.

## 2026-07-30l — freeze a sequence to a still

Per loaded document, in the Loaded NIFs list: **Freeze Animation…** picks a
sequence and an instant, writes what every controlled block evaluates to at that
instant into the field it drives, and removes the controller graph. Also
`nifcli freeze --sequence NAME --time T [--keep-graph]`, and bare
`nifcli freeze <file>` to list the sequences with their ranges.

The dialog seeds its time from `GLView::sceneTime()`, so the workflow is scrub
the timeline until it looks right, then freeze this file there. It runs through
`nifSnapshotOp`, so a freeze is one Ctrl+Z and a loaded document can still be
reverted byte-for-byte.

New: `src/freezeanim.{h,cpp}`, `tests/freeze/freeze_sweep.sh`.

### The rule: what freezes is what the viewport shows

Every evaluation mirrors the matching class in `gl/controllers.cpp` — same
interpolation, same `ctrlTime` mapping, same Controlled Variable switch — and
writes the result into the NIF field the runtime member was loaded from. A freeze
that disagreed with the viewport would be worse than none, because the entire
point is picking a moment by looking at it.

Baked: node transforms (`NiTransformController`, `NiKeyframeController`,
`NiMultiTargetTransformController`), `BS{Effect,Lighting}ShaderProperty`
`Float`/`Color` controllers by Controlled Variable, `NiLightDimmerController` →
`Dimmer`, `NiVisController` → the node's hidden flag,
`BSNiAlphaPropertyTestRefController` → `Threshold`, `NiAlphaController` →
material `Alpha`.

### Two idioms for "which block does this controller drive", both in one file

`X01_Torso_Tesla_VFX` has both. `autoPlay` names the controller instance actually
hanging off the target (light 117 → controller 1). `autoLoop` carries its **own**
instances (controller 13) attached to nothing at all. `NiTimeController::Target`
is no help — FO4 leaves it at -1 on the shader property controllers (block 26).

So: walk every block's `Controller`/`Next Controller` chain first, and fall back
to the row's own `Node Name` + `Property Type`. Where `Property Type` is blank,
the controller type implies which property it must be. That fallback is what
takes the torso from 9 rows resolved to 11.

(NifSkope's renderer only does the first half, via `findController`, which is why
`autoLoop`'s dimmers and orphan effect controllers never animate in the viewport.
Noted, not fixed here.)

### Two silent lies, found by checking bytes instead of the success count

The first version reported "9 baked" while writing **nothing** for four of them:

- nif.xml declares `TexCoord` as a struct of `u` and `v`, but `NifValue` maps the
  whole type to `tVector2`. Setting a child row named `"u"` did nothing, and the
  write helper returned `true` regardless. Now read-modify-write the whole
  `Vector2`, and every write helper returns what the write actually did.
- **Three of the shader properties take every parameter from a `.bgem`.** When a
  material file is set, `setMaterial` builds a `Material` and `updateParams` reads
  every parameter off *that* — the NIF's own `UV Offset` row is never looked at.
  Baking into it writes a number nothing reads. These are now **skipped**, naming
  the material: the frozen value has nowhere to live in this file, and saying so
  beats a number that does nothing.

Measured proof the float path is right: property 111 (no material), Controlled
Variable 22 = V_Offset, keys (0, 0.0) → (4.933, −1.0) linear. Frozen at t = 0 /
1.25 / 2.5 gives `UV Offset.v` = 0.000000 / −0.253395 / −0.506791, against
hand-computed 0 / −0.253396 / −0.506791.

### What a still may not keep

Stripping removes the sequence machinery **and every controller that drives a
value** — leaving one behind would animate the baked field away the moment the
file plays. Controllers that drive a *simulation* stay: a particle system's state
is a cloud of generated vertices and a procedural arc is a generated ribbon,
neither is a field, so they cannot be baked and are not competing with anything
written. Turning those into a still means snapshotting their geometry, which is
the loading-screen convert's job.

Any controller removed without being baked is **reported**, not swallowed.

### The test checks bytes, not the report

`tests/freeze/freeze_sweep.sh` — 57 checks over 14 file/sequence pairs, all
green. Invariants: every controlled block is either baked or accounted for out
loud (`baked + skipped == Num Controlled Blocks`); the frozen file still loads
with no manager/sequence/palette left; `NiParticleSystem` and
`BSProceduralLightningController` counts are unchanged.

Invariant 1 is deliberately **global**, not per-sequence. The first draft asserted
"two times ⇒ two different files" per case and failed on
`X01_Helmet_Tesla_VFX/autoPlay`, which drives a static `NiTransformInterpolator`
and two flat curves (0.0 → 0.0, 0.35 → 0.35) — identical bytes is the *correct*
answer there. That was the harness encoding a wrong expectation, not a bug. What
must never happen is *every* case being constant, which is what a dead write path
looks like: 13 time-varying, 1 constant.

## 2026-07-30k — two Renderers on one context empty the whole frame

Attempting real per-document rendering (a `Scene` per loaded NIF instead of the
flat triangle soup) turned the viewport **black** — not the new documents, the
*whole* frame: primary model, grid, everything.

Cause: `Scene::setOpenGLContext` does `renderer = new Renderer( context )`, so
every Scene builds its **own** Renderer. A Renderer caches which shader program
is bound; two of them on one GL context each believe they own that state, neither
cache matches the driver, and nothing draws. This is the same class of bug as the
07-2x startup-grid crash — *never let two things with a shortcut cache share one
GL context* — and the symptom is silence, not a crash.

Fixed with `Scene::borrowRenderer( Renderer * )`: workspace scenes take the
primary's renderer and their destructor does not delete it.

**That fixed the black frame but the per-document Scenes still draw nothing**, so
they are **not wired in**. `refreshSessionPreview` still routes both solid and
ghost documents through the soup, because a document that renders as a flat grey
shape is strictly better than one that renders as nothing.

`GLView::setWorkspaceRenderModels` and the Scene cache are in place and unused;
what remains is finding why `make()` + `transform()` + `draw()` on a borrowed
renderer produces no geometry. Likely the per-frame shader/light setup `paintGL`
does around the primary's draw, which a second scene never receives.

Verified restored against the known-good picture: solid document opaque grey over
the primary, ghosted document translucent — pixel-for-pixel the state before the
attempt. (Two intermediate pistol screenshots looked like regressions and were
not; the armour was simply off-frame. Checked apples-to-apples before concluding.)

## 2026-07-30i — merge from Loaded NIFs, and a pose that stays on

### Select, right-click, merge

Two or more rows selected in the NIF Browser's **Loaded NIFs** list get their own
context menu: **Merge into a new NIF…**, then a Save As for the output. The
**first selected row is the target**, so a rig is assembled skeleton-first and
the skeleton dictates position for everything after it — a bone that exists in
both is the skeleton's, and an armour piece's flat copy de-duplicates away.

Nothing loaded is modified. The merge runs on a fresh model built from the
target's own bytes, so a merge that turns out wrong costs a file on disk rather
than the documents in the workspace. The summary reports, per piece, shapes
added, bones shared, and where an effect attached — including "the ROOT, its
AttachT names no node", which is the case that silently goes wrong.

### A pose stays loaded until you pick Default

A pose is written into the bone transforms, so it survives on its own for bones
that already exist. But a piece merged in **afterwards** brings bones that were
never posed, and the rig comes apart at exactly the seam you just added.

The Pose Manager tracks the **active** pose and re-applies it over the whole rig
after every merge. The library list gained a **Default (rest pose)** row above the
files — the only thing that turns a pose off — and the active pose is marked with
a dot and the selection orange, so "which pose is on" is answerable by looking.

## 2026-07-30h — the merge honours AttachT, so ArtObjects land on the limb

An effect NIF's root carries a `NiStringsExtraData` called **`AttachT`** whose
entries are either `NamedNode&<NodeName>` — hang me off that node — or engine
hints such as `MultiTechnique`, which name nothing. The merge ignored it and put
every donor branch under the target's root, so a Tesla arc meant for the calf
imported perfectly and sat at the actor's feet. That reads as "the effect didn't
import" when in fact it did.

`mergeDonor` reads `AttachT` now and parents the donor's tops under the named
node, failing loudly (rather than silently landing at the origin) when the target
has no such node. `nifMergeResult` reports `attachRequested` / `attachedTo`, and
`isEffect` for a donor that carries `AttachT` **at all** — which is the case
worth a warning, because those are exactly the effects that vanish to the origin.

**Half of them name nothing**, by design: the X-01 Tesla legs say
`NamedNode&LLeg_Calf_Armor2`, but its torso, arms and helmet say only
`MultiTechnique` because their attach node lives in the ESP's ARTO record, not in
the mesh. So `nifcli merge` gained **`--attach NODE`**, applying to the next
`--add`; without it the CLI says where the piece went and that it probably wanted
saying:

```bash
NifSkope -no-gui merge rig.nif --attach L_Pauldron --add arm_left_fx.nif -o out.nif
```

All six X-01 Tesla ArtObjects attach correctly this way — legs from their own
`AttachT`, the other four by override — adding 24 shapes with no duplicate names.

### WW_SHOT_TIME

Effects open over time; a Fallout 4 ArtObject at `t=0` is usually scaled or faded
to nothing, so a screenshot of the first frame is a screenshot of an empty
effect. `WW_SHOT_TIME=<seconds>` parks the scene clock before grabbing, stepping
in 1/30s increments rather than jumping, because particle systems integrate frame
to frame and cannot be evaluated at an arbitrary *t* the way a keyframe
controller can. At `t=1.5` the arcs are alive on both pauldrons, the right arm
and the right leg; at `t=0` only one is.

### A data problem in the source, not in the code

`x01tesla_helmet_fx.nif`'s root node is named **`PA_Edison_RPauldron_VFX`** — the
same name as `x01tesla_arm_right_fx.nif`'s root. Merged after it, that name
de-duplicates and the helmet effect folds into the right pauldron's branch: 6
blocks and 1 shape, against the right arm's 150 and 5. Renaming the helmet
effect's root in the source is the fix; nothing in the merge can guess that two
files meant different things by one name.

## 2026-07-30g — Load skeleton was folding the mesh up, and Outfit Studio poses on X-01

**Two bug fixes and a feature test.** See 07-30f for the first fix (string
indices); this is the second, larger one, plus the pose-import verification.

### The mesh really was moving, and my proof that it could not was wrong

07-30f ended with *"the mesh can't move: existing blocks are byte-identical
except one appended Children entry."* That is a proof about the **file**, not
about what NifSkope **evaluates**. `WW_SKELMERGE_TEST` measures the evaluated
skin instead — `Shape::skinVertex` for every vertex, before and after — and
reported **1871 of an outfit's 3147 vertices moving**.

Armour and clothing store their bones **flat** under the root, each carrying that
bone's **world** transform; the game re-parents them onto the real skeleton at
runtime. A skeleton NIF stores the same bones **nested** with **local**
transforms. Merging the two, the skeleton's parent-child edges land on the
target's flat bones, so `LArm_ForeArm1` stops being a root child and becomes a
grandchild of `LArm_Collarbone` *while still carrying its world transform* — and
is then composed on top of its new ancestors. The old edge was never cut either,
so the node had two parents at once.

The hierarchy is what the merge is **for**, so it stays; the transform is what
gets repaired. The stale edge is cut and each adopted node is rebased so its
world transform is exactly what it was: `local = newParentWorld⁻¹ × oldWorld`.
33 nodes on the reported outfit; **0 vertices moved**.

Two harness assertions then failed, and both were mine: *"posing LArm_ForeArm1
moves the same vertices"* (86 before, 331 after). Of course not — before the
merge every bone is a flat root child, so rotating one moves only its own
vertices; after it, the bone has the skeleton's children and drags the limb.
That is the entire point. The checks now assert a **superset**, and that the bone
has not become a transform on the whole mesh.

### Outfit Studio pose import, on the full X-01 rig

Requested test: all six X-01 Tesla parts on the power-armour skeleton, then the
three Outfit Studio pose XMLs.

| step | result |
|---|---|
| skeleton + helmet/torso/2 arms/2 legs | **28 shapes**, 29 bones shared, **0 duplicate names** |
| `PAActionPose.xml` | 48 bones posed, **0 not in this skeleton** |
| `PAGunOneHanded.xml` | 47 posed, 0 missing |
| `PAStandGeneric.xml` | 40 posed, 0 missing |

All three render correctly — the armour bends at its joints and holds together.

**Worth knowing:** these parts carry no hand mesh, so **32 of the posed bones
(both hands and all 30 fingers) move nothing visible**. The remaining pose is
fully effective: 12 bones drive geometry directly and the rest (spine,
collarbones, twist) propagate into bones that do.

### WW_SHOT

`WW_SHOT=<out.png>` loads a file, lets the scene settle, saves the viewport and
quits; `WW_SHOT_VIEW=front|left|top` picks the camera. Some things are only
checkable by looking, and `grabFramebuffer` renders offscreen — no window is
raised and no focus is taken, so it can run while the machine is in use.

## 2026-07-30f — Load skeleton was renaming the bones it imported

**BUG FIX.** Reported against an InstituteWorksuit outfit: load the human
skeleton in the Pose Manager and the rig folds into a heap.

### A NIF stores names as an index into its own string table

From NIF 20.1.0.3 on, node names and file paths are not text in the block — they
are an **index into the file's own header string table**. `mergeDonor` spliced
donor blocks with `saveIndex` / `loadAndMapLinks`, which remaps *block links* and
leaves everything else byte-for-byte. So every imported node arrived carrying the
donor's raw index, resolved against the **target's** table: donor index 36 and
target index 36 are two unrelated strings.

Measured on outfit + `skeleton.nif`: **32 bone names ended up on two nodes each.**
The imported hierarchy read

```
LArm_Collarbone → LArm_UpperTwist2_skin → LArm_ForeArm1 → LLeg_Toe1 → …
```

A rig binds bones **by name** — the pose library, mirroring, and this merge's own
de-duplication all do — so a pose aimed at `LArm_Hand` reached whichever of the
two nodes was found first. That is the heap.

Fixed by carrying the strings across explicitly: each donor block's header-string
values are collected in traversal order at serialize time and re-allocated in the
target's table after load. (`NifModel::updateStrings`, which `moveAllNiBlocks`
uses for the same problem, cannot be reused — it addresses the target item
through the *donor* item's pointer, which only works when blocks are moved rather
than re-created.)

### Why no test caught it

The merge's only automated coverage was `WW_MERGEARCH_TEST`, which merges a NIF
**into itself** to compare the file and in-memory paths. A file shares its own
string table, so that test could not have seen this — every index was already
correct. The blind spot was structural, not an oversight in the assertions.

`tests/merge/skeleton_merge_sweep.sh` covers the real case now: **9 pairs across
clothing, armour, a head, faceBones, two creature skeletons and an unskinned
weapon**, asserting that the merged node-name set is exactly the union of the two
inputs' (minus the donor root, which is deliberately not imported), that no name
repeats, and that the target shape's bone bindings are untouched. All 9 pass; all
of the skinned ones failed before the fix.

### And it now says so if it ever happens again

`NifMergeResult` carries `duplicateNames`. The Pose Manager raises a warning
naming them, and `nifcli merge` prints either `no duplicate bone names
introduced` or the list. The symptom — a rig that poses into a heap — says
nothing at all about the cause, so the cause is reported directly.

## 2026-07-30e — the Block List says what a block IS

**16 checks, 0 failures** (`WW_SUMMARY_TEST`), plus a rendered screenshot.

The Block List showed a number, a type and a name. A file is thirty rows of
`NiNode`, and which one is the mesh, how big it is, what a controller drives and
which texture a shader set points at were all one click away, thirty times over.
There is a **Summary** column now:

| block | summary |
|---|---|
| `0 NiNode` | 9 children |
| `2 BSXFlags` | 203 |
| `3 NiStringExtraData` | WEAPON |
| `5 NiTransformController` | → WEAPON |
| `10 BSTriShape` | 600 tris · 576 verts |
| `11 BSLightingShaderProperty` | 10mmPistol.BGSM |
| `12 BSShaderTextureSet` | 10mmPistol02_d.dds · +4 more |
| `3 bhkPhysicsSystem` | 7.0 KB |

Skinned meshes add `· skinned · 5 segments`; skins report their bone count,
keyframe data its key count. **Zero is never a row** — a skeleton is mostly leaf
bones, and "0 children" sixty times over buries the counts that matter.

### Badges, as colour rather than as pills

Statuses are red text in the same column, not chips: the house style for a data
view here is coloured text, and a defect belongs beside the thing it describes.
`missing texture` reuses `texturePathInfo` from 07-30d; `no geometry` catches a
shape with no triangles or no vertices.

**There were three, and one was wrong.** An `unreferenced` marker was written,
and the harness caught it firing on **48 of a vanilla pistol's 57 blocks**.
`getParentLinks()` holds the Ptr links a block *owns*, not the blocks pointing
*at* it — and the actual reverse relation, `rootLinks`, is built as "every block
nothing refers to" and then written back over the footer's Roots array, so in this
model **an orphan and a legitimate root are the same thing by construction**.
Deciding it needs a reverse index NifModel does not keep. Removed, with the reason
recorded next to the two that survived.

### Both list modes, which is where the second bug was

Hierarchy view goes through `NifProxyModel`, which had two columns and translated
its own column numbers into the model's. It has three now. A summary that worked
in only one of the two list modes would have been broken half the time, so the
harness checks the proxy's mapping too.

The third bug was quieter: `restoreState()` on a saved header carries its own
column visibility and **wins over anything set at construction**, so the column
came up backwards — visible in Block Details, where a per-block line has nothing
to say, and hidden in the Block List, where it is the entire point. There was
already a comment about exactly this for the Reference column; Summary is set back
on the same line now.

### Verification

`WW_SUMMARY_TEST` reads the column through `data()` on the real column index and
through the proxy, checks the status markers **in both directions** (mangle a
path, the owning block must go red; restore it, the marker must go), and asserts
no block in a vanilla file reads as unreferenced — which is how the wrong marker
was caught rather than shipped.

It also **grabs the widget offscreen and saves a PNG**, squeezing the sections to
the dock's real width first: a column that reads correctly through `data()` can
still be two pixels wide or off the right edge, and a screenshot of the columns
you *can* see is not evidence about the one you are checking. `grab()` renders
without showing, so it costs no focus.

## 2026-07-30d — a broken texture path now looks broken

**20 checks, 0 failures, 0 skips** (`WW_TEXCOLOR_TEST`).

### The commonest bug in the file had no tell

A texture path that resolves nowhere is the usual reason a mesh renders
untextured, and Block Details said exactly as much about a broken path as about a
working one — a string, in the same colour, either way. It is **red** now, with a
tooltip that names where it looked:

> `textures\Weapons\Foo\bar_d.dds`
> Not found in the configured resources.
> Checked textures/ with .dds .tga .png .bmp .nif .texcache

A path that does resolve gets its resolved location instead, and an empty slot
reads as an empty slot rather than as a missing file — an unused texture slot is
normal, not a fault.

One method answers both, `NifModel::texturePathInfo`, because the colour and the
tooltip must never disagree; and it tries the same extensions in the same order
the renderer does, so the details view cannot call a texture missing that the
viewport is happily drawing.

**Finding the texture fields at all** is the interesting part. nif.xml types the
classic slots as `FilePath`, but Bethesda's shader properties give theirs no
distinguishing type whatsoever — `BSShaderTextureSet`'s entire array is plain
`SizedString` — so those are recognised by where they sit and what they are
called, with the name rule left open-ended so a slot added to a later game's
shader property lights up without a code change. A same-named `Ref` field cannot
match, because the type gate runs first.

**Browse was already there.** `spChooseTexture` is an *instant* spell, so every
texture row has had a clickable icon in its Value column the whole time. The
tracker filed it as missing; it was not.

### The colour row has a colour on it

`ColorEdit` was four spin boxes. It leads with a **swatch that opens the picker** —
`ColorWheel::choose` already existed and was reachable from the settings pane
alone. Alpha draws over a checkerboard rather than over the dialog background,
because a 50%-alpha black otherwise just looks like a mid grey, which is a
different colour rather than a translucent one.

**HDR would have been a silent no-op.** `ColorWheel::choose( Color4 )` returns its
input untouched when any channel exceeds 1.0 — it has no way to show one — so on
an emissive colour the new button would have opened nothing and reported nothing.
The intensity is factored out, the hue is picked on the 0..1 wheel, and the factor
is multiplied back in: the picker edits the colour and leaves the brightness where
the file had it. Off-scale colours draw with a bright cap along the swatch's top
edge, so a clamped preview never passes itself off as the value.

### Verification

`WW_TEXCOLOR_TEST` reads colour and tooltip back through `data()` exactly as the
delegate does, rather than calling the helper directly. A path is then **mangled
in place** — whatever this machine's resources happen to contain, garbage must
resolve nowhere — which makes the negative case machine-independent; the positive
one reports a skip instead of a failure where no resources are configured. (Here
they were: 5 of 5 pistol textures resolve.)

The `FilePath` branch reads the **header string table**, which is what a Fallout 3
/ Skyrim `NiSourceTexture` takes and no file on this machine has, so the harness
reaches it by retyping a `NiFixedString` field in place and putting it back —
an untested branch made tested rather than assumed correct.

The swatch is clicked with a timer waiting to dismiss the modal dialog, which
proves the wiring end to end. What that cannot prove is the HDR scale round trip,
which needs a colour *changed* inside the dialog; it is unverified and said so.

## 2026-07-30c — the two greyed-out Separate entries, and what one of them can never be

`P > By Material` and `P > By Loose Parts` had been `setEnabled( false )` since the
Separate menu was written, with the comment *"not implemented yet (Blender parity
later)"*. Both now do something — but only one of them is the thing Blender does.

### By Loose Parts connects by POSITION, not by index

Two triangles are one piece when they share a vertex. Done the obvious way — union
by vertex *index* — a NIF shatters: the format splits vertices at every UV and
normal seam, so an outfit that is visibly one garment reports thirty "loose parts"
and the operator is useless on exactly the meshes people run it on. Connectivity is
taken over vertex **positions**, compared bit-exactly, which is what an exporter
writes for a seam pair — it duplicates the vertex, it does not recompute it. (`-0.0`
is folded onto `0.0` so the two spellings of zero land in one bucket, and bucket
membership is confirmed with an exact compare, so a hash collision can never weld
two vertices that are merely near each other.)

Verified as the definition rather than as a triangle count: after the split, **no
two output shapes share a vertex position**. That check is what "loose part" means.

### By Material could never have worked, so it is By Segment

A NIF shape carries exactly one shader property. The FO4 segment structures carry
no material either — `BSGeometryPerSegmentSharedData` is User Index, Bone ID and
cut offsets. Per-face material does not exist anywhere in the format, on any file,
so a By Material item was never a missing implementation; it was a menu row that
could not have been written. The segment array **is** the per-face grouping a NIF
has — body parts for a skinned FO4 mesh — so the entry separates by that instead,
and says so in its tooltip when a mesh has none.

### The count is in the menu, before the click

Both new entries ignore the selection and split the whole mesh, Blender-style, and
By Loose Parts on a seam-heavy mesh can make a lot of objects. The item reads
**By Loose Parts (+10)** — the honest place to say so is before the click, not in
the status bar after it. Zero disables the row, with a tooltip explaining why.

### One N-way core under all three

`separateSelection` was a two-way split with the undo commands, skin cloning,
segment rebuild and vertex compaction inline. That is now `separateShapes`, taking
a grouper that returns a per-triangle group id: group 0 stays in the source, every
other group becomes a sibling. Selection is a grouper returning 2 groups, so all
three variants share one implementation and cannot drift apart.

**Verified headlessly** (`WW_SEP_TEST`, new `WW_SEP_MODE` 0/1/2 and `WW_SEP_ANY`;
the harness now validates a LIST of outputs rather than a source/new pair, and
skips the skin and segment checks on shapes that have neither instead of failing
them):

| file | mode | result |
|---|---|---|
| 1stPersonFemaleVault111Suit (skinned, 5 seg) | Selection | 621 + 621, PASS + UNDO PASS |
| ” | Loose Parts | +1, disjoint, PASS |
| ” | Segment | +1, one segment each, PASS |
| InstituteWorksuit FOutfit (3147 v / 5158 t) | Loose Parts | **+10**, all disjoint, PASS |
| ” | Segment | **+4** (5 of 7 segments non-empty), PASS |
| 10mmPistol (unskinned, no segments) | Loose Parts | **+5**, PASS |
| ” | Selection | 422 + 423, PASS |

Every run: triangles conserved, no orphan verts, distinct skins per output, segment
ranges contiguous and covering, the menu's preview matching what the operator then
produced, and undo restoring the source and dropping all 40 appended blocks.

## 2026-07-30b — one scrubber, and P4 was not blocked after all

**7 rigs, 671 checks, 0 failures, 1 correct skip.**

### The recording moved to the timeline

A physics recording **is** a timeline: a run of poses at a fixed rate, scrubbed
back and forth to find the frame worth keeping. It had its own slider in the
Collision Manager while this application already has a timeline dock with a ruler,
a playhead and a keyframe view — two scrub bars that did not know about each other,
in a program whose whole point is that one of them is good.

Routed through `GLView::setSceneTime` rather than by teaching `TimelineWidget`
about physics. The dock already drives that slot and reads `sceneTimeChanged` back
for its range, so answering on the GLView side hands the timeline the recording
**with no changes to the widget at all** — and the moment the recording goes, the
scene answers again. The panel keeps Record and Live; the slider is gone.

Checked through the same slot the playhead uses, rather than by calling `seek()`
and hoping the two are wired together.

### P4's blocker was stale

`TO_BE_IMPLEMENTED.md` had *Compound / instance encoding — BLOCKED, needs reference
pairs to validate against.* The §4d encoder campaign supplied exactly that months
of work ago: every vanilla file is a reference pair, and the assembler writes
**compounds 27/27 and compound instances and children byte-identical**, inside the
470/470 byte-exact packfile result. The line survived because nobody came back to
it after the campaign closed.

Two items genuinely remain — per-triangle face-material painting, and
`hknpBSMaterialProperties` beyond the single-material table. Both are real features
wanting design input, so they are sized and left rather than started blind.

### Smaller

Two material pickers sat in one dock with nothing saying which was which. They are
**Material for new collision** (a default for whatever Create makes next) and
**Material of this body** (the selected one).

`physicspanel.cpp`'s constructor gave up its two self-contained parts —
`applyPhysicsPreset` touches no widget at all, and `buildToolButtons` is a table
and a loop nothing else refers to by name. The remaining ~55 locals are genuinely
shared with the sync closure and every connect; turning those into members is a
hundred-symbol rename that would buy navigation at the cost of the thing currently
passing 671 checks, so it was not done.

## 2026-07-30a — the zero-area triangles were ours

**39 files, 125,054 preview triangles, 0 degenerate.** Physics harness: 7 rigs,
647 checks, 0 failures.

The picker sweep turned up 11 to 15 zero-area triangles per rig and I left the
cause open. It was the decoder.

`synthSphere` built a UV sphere with a full ring of twelve vertices at *every*
phi including 0 and pi — so each pole was twelve coincident points, and every
triangle spanning two of them had no area. `synthCapsule` had it at both caps for
the same reason: its end rings are radius zero. **24 of every 144 triangles per
sphere or capsule**, drawn, ray-tested and counted against the collision budget
while covering nothing.

They surfaced as picker "misses" — a ray aimed at such a triangle's centroid hits
nothing, because there is nothing there. Möller–Trumbore was right and the
geometry was wrong.

The poles are one vertex each now, with a fan at each cap. The useful geometry is
identical: the alien skeleton's preview goes from 3308 triangles to 2924, which is
exactly its 16 capsule bodies × 24. `addFaceFan` drops degenerate output too, since
a hull may genuinely repeat a corner.

The `collision` CLI reports `preview triangles N, M DEGENERATE` per system, which
is what the corpus sweep above reads — the guard against this returning, and
against a hull arriving with a repeated corner.

## 2026-07-29z — the file's own friction and bounce, and less panel

**7 rigs, 647 checks, 0 failures, 1 correct skip.**

### The solver was ignoring what the file said

`HknpBodyPhys` decodes a friction and a restitution **per body** and the Collision
Manager's selected-body editor edits them. `RagdollSim::build` read neither: every
contact used one global number, so one panel above the simulator you could set a
body's friction to 0.9 and watch the sim keep using 0.5.

Measured across 37 actor skeletons before wiring it, because a comment in the
solver claimed Havok's restitution is 0 on every corpus body, and that justified a
default:

| | authored values |
|---|---|
| friction | 0.50 ×512, 0.30 ×127, 0.40 ×92, 0.60 ×82, 0.70 ×45, 3.00 ×24 |
| restitution | 0.30 ×284, 0.20 ×263, 0.10 ×115, 0.80 ×74, 0.00 ×71, 0.05 ×48 |

**The claim was never measured and is false** — 71 bodies of some 900 carry 0. Both
are read per body now, and the panel's controls became **multipliers**: the
arrangement `damping` has had all along, where the authored number is the truth and
the global adjusts it. Contacts combine the two bodies' coefficients as a geometric
mean; a ground contact combines the body's with the floor's own grip, which
previously ignored the body and slid a friction-3.0 body exactly like a 0.30 one.

Bounce defaults to what the file says rather than to nothing, which is energy
coming back out of every landing — so the harness gained a settling check. After
five seconds the fastest body is 0.00–0.01 m/s and nothing diverges.

The collision CLI reports both columns now. The only place either was visible was
the manager's editor, one body at a time, which is no way to learn what a corpus
carries.

### Punt is no longer a heavier Shoot

It set an impulse, and an impulse divided by mass is a velocity — so the same click
launched a 0.2 kg jaw and barely nudged a torso. `RagdollSim::shove` sets the
**speed**: what you point at leaves at the same rate whatever it weighs, which is
what a gravity gun feels like and the one thing an impulse cannot do. The spin
still comes from the offset, so punting a foot still turns the rig.

### Redundancy out

- **The Bodies list is gone.** It showed every body by name with a pin checkbox,
  one panel below a tree already listing exactly those bodies with bone, shape,
  layer, material, mass and state. The checkbox moved to the tree; the list went.
- **The status line** said `bhkRagdollSystem [7]: 18 bodies, 17 joints.` directly
  above a picker reading `bhkRagdollSystem [7]`. Counts alone now.
- **The picker only lists simulable systems.** It offered every `bhkPhysicsSystem`,
  and choosing a jointless one stopped the sim, failed, and said nothing — which is
  how a panel came to show `[7]` in its status and `[8]` in its picker.
- **Firmness and Strength** were one idea in two dials. One **Grip** drives both.

### Moved and collapsed

Options was eleven flat rows mixing gravity and the ground — changed while watching
— with sweeps and substeps, set once. Split into **World** (both panels), **Solver**
(manager) and a collapsed **Advanced** (manager). "Show the ground" moved beside the
ground toggle. "Put the floor back under the rig" was a full-width button doing a
revert; it is a small control on the row it reverts.

**Pause** and **Freeze** are genuinely different and never said which was which.
They are **Stop time** and **Stop motion**.

### A test that was testing the file

One rig failed "clicking a shape always hits something". The diagnostic named it:
PowerArmor's `skeleton_female_faceBones`, body 11, triangle 0, **edge 0.000 game
units**. A zero-area triangle, which Möller–Trumbore correctly rejects — there is no
surface there to hit. Aiming at it and demanding a hit tested the file, not the
picker. They turn out to be common rather than freakish: **11 to 15 per rig**.
Skipped and counted now.

Two dead ends worth naming. Re-running a rig through PowerShell with
backtick-quoted arguments launched NifSkope with **no file**, so the harness never
ran and the app sat idle looking like a hang — and a running instance holds
`release/NifSkope.exe`, so the next link failed and the sweep silently re-ran the
**old binary**.

## 2026-07-29y — a bouncing-ball icon, and one tool row instead of two

**7 rigs, 605 checks, 0 failures, 1 correct skip.**

### The icon

A ball bouncing off a floor: what collision is **for**. The cage-around-a-body it
replaces described the *data* — a hull drawn round a mesh — which is accurate and
says nothing about what the feature does.

Three marks, so it survives 16 px: a heavy floor, an arc, and a filled ball at the
top of the rebound. Monochrome like the rest of the set, with the arc a dimmed
shade of the same colour rather than a second hue.

Drawn twice. The first attempt gave the ball a radius of 8.5 against a full-width
arc, and at 16 px the curve was the subject and the ball a speck — the ball is the
thing that says "physics", the arc and the floor only place it.

**The harness photographs the toolbar now.** Icons are drawn in code and nothing
else here looks at one, which is how the previous collision icon shipped as a
wireframe cube indistinguishable at 16 px from the x-ray icon beside it.

### One tool row

The manager's Simulation section no longer repeats the tool picker or its
parameters. Which tool is active, and how hard it hits, is what you change most
often and reach for from the viewport, so it stays one click away in the top bar.
Two copies on screen at once was two controls for one setting, each able to show
the other as out of date for as long as it took a sync to run.

The widgets are still built in both — hidden, not omitted — so the single sync path
drives either panel without asking which host it is in.

### Renamed

**Create collision** is **Collision Creation**, **Test collision** is **Collision
Simulation**, on the switch and on the group box it reveals.

The split check had to change with it, and the new one uses `isVisibleTo()` rather
than `isHidden()`: it is the *row* that gets hidden, and a button inside a hidden
row still reports itself shown — the same trap that made the first control-count
check read 93 against 97.

## 2026-07-29x — the collision panel splits in two

**7 rigs, 599 checks, 0 failures, 1 correct skip.**

The Collision dropdown had grown to about forty controls behind a scrollbar, in a
popup that closes the moment you click the viewport you are trying to adjust.
That is two products under one button: the file's **collision data**, and a
**live simulator**. Almost all of the bulk was the simulator.

### One class, two sizes

`PhysicsSimPanel` (`src/ui/widgets/physicspanel.h`) is the whole thing as a
widget, in one of two modes. The alternative — a short panel and a long one —
is two implementations of the same controls that disagree the first time either
changes.

The mode selects which **sections** are laid out. Every widget is built either
way and the ones a mode does not offer are hidden, so the single sync path
refreshes all of them without asking which host it is in. Measured on the alien
skeleton: **26 controls in the dropdown against 69 in the manager**, counted by
what is genuinely on screen.

That count needed care. `isHidden()` is per-widget, so the children of a hidden
row still report themselves visible and the first version of the check came out
93 against 97 — a metric that would have passed whatever the split did. It walks
up to the panel now.

### Where things went

**The toolbar dropdown** keeps what you touch every few seconds: show collision,
run/stop, the six tools and the active tool's parameters, playback, presets. It
fits without scrolling, which is the entire point of a dropdown.

**The Collision Manager** gets the rest, and gets it in a dock that stays open
while you work — the world settings, the solver knobs, the body list, recording,
and capture.

The separate Collision dock added the day before is gone. It existed because the
menu covered the viewport, which is a real problem with a better answer than a
second dock beside the manager that does half of what the manager does.

### Create or Test, one at a time

The manager's bottom section now switches between **Create collision** and
**Test collision**. They are the two things you do with collision once you can
see it — author it, then throw something at it — and they are never wanted at
the same moment. Side by side they would each get half the width; stacked,
whichever one you are not using is scrolled past on every trip. A switch costs
one row. Which one you had is remembered, since that is a property of the job
rather than of the session.

The manager's copy is built with no Show Collision action, so that row hides
itself rather than sitting a few pixels below the manager's own identical
checkbox.

### Fighting a QMenu about size, and losing

Worth writing down, because it cost three attempts.

A `QMenu` sizes a `QWidgetAction` to the widget's cached hint and will hand it
**less** than the layout needs — which does not clip, it draws the rows on top
of one another. The old panel never showed this because it sat in a scroll area
with a minimum height, which forced the popup open to that size.

Pinning the panel's minimum to the worst case across all six tools fixed the
overlap and left a hand's depth of dead space under the shorter tools, because a
`QVBoxLayout` with slack and nowhere to put it spreads it evenly between the
rows. A trailing stretch fixed that. Sizing per-tool in `sync()` instead — which
runs on `aboutToShow`, before the popup is measured — got the height right and
still came up a row short, because the menu recomputes its own size afterwards
and wins.

It is back in a scroll area with a floor under it. The popup is that tall
whatever happens, and a tool whose parameters need more scrolls rather than
being quietly cut off.

## 2026-07-29w — the physics gun

**7 rigs, 587 checks, 0 failures, 1 correct skip.**

Physics Sim had a hand that could only drag in a plane. This is the rest of the
gesture set a physics gun actually has, plus the controls the panel was missing.

### Push, pull and turn

`m_grabDepth` was set once when the grab was made and never touched again, so a
drag was confined to a plane parallel to the screen — placing a hand in front of
a chest meant orbiting the camera to a side view first. **The wheel now reels the
held body in and out**, multiplicatively rather than in fixed steps, because 10 cm
is a crawl across a room and a lurch on a rat while a percentage of the current
distance moves the same fraction of the way whatever the scale. Shift is a fine
step.

`setDrag` was a pure point constraint, so a held bone was free to spin about the
grab: an arm dragged into position arrived at whatever angle the swing left it.
**Ctrl+drag now turns what the hand holds**, Ctrl+wheel rolls it, Shift snaps to
15°. It is solved through the same compliant XPBD path as the positional drag and
with the same firmness, so the two cannot fight, and the joints still get the last
word.

The rotation frame is captured **once**, when the gesture starts. Taking the
camera's live axes each time would let an orbit mid-rotate redefine which way is
up, and snapping would then quantise against a moving target. Snapping applies to
the accumulated angle, not to each increment — rounding every mouse delta rounds a
stream of half-degree moves to zero and the bone never turns at all.

**The beam** draws the line from the grip to the hand, a cross at the point on the
shape the hand actually has hold of, and a marker where the hand is pulling to.
Without it the two wheel gestures are controls with no visible state: you can feel
them working but a body pushed behind the one in front of it just looks like it
stopped responding. The gap between grip and target is the chain going taut, made
visible.

### Untangling, and getting back out

**The held bone can be excused from self-collision.** Self-collision is what stops
a thigh entering a pelvis and also what traps a limb already inside one — the
solver will not let it back out through the surface it is behind. Suspending it
for the one body in hand is the untangling tool; suspending it globally, the only
option before, changes every other body at the same time.

**Unpin all** (U). Pins accumulate one right-click at a time and the only way back
from four of them was a full reset.

### Balls

`RagdollSim::build` only ever made bodies from the decoded file. It can now take
**loose props**, and the Ball tool throws one along the view ray at a configurable
size, mass and speed. This is the honest test of a collision mesh: does anything
actually bounce off it.

A sphere, and the choice is forced rather than lazy. The exact narrow phase is
segment-to-segment plus a radius, so a sphere collides correctly with every
capsule and sphere in a rig, while a box is a point set with no faces between its
corners (`exactPair`) and would be excluded from body-on-body contact altogether.
A crate that fell through the ragdoll it was thrown at would be worse than no
crate.

A prop joins the same body list the rig uses, with generated icosphere geometry in
the same mesh list, so drawing, picking, grabbing and pinning need no special
case — which is what makes a thrown ball something you can then catch and throw
again. Two things had to be kept honest: the no-collide filter's keys encode a
pair as `min * stride + max`, so the stride is frozen at build and props are never
looked up in it; and `looseBodies()` excludes props, or one ball would report a
clean ragdoll as a kit.

**Punt** is the gravity gun's other half: a heavy shove along the view, or a yank
toward the camera. Distinct from Shoot, which delivers a round's impulse where it
lands; this acts along the *view*, so a punt sends a body away from you rather
than into the floor.

### Bounce

Restitution is new in the solver, as a velocity pass after the position solve —
which is where XPBD puts it, and not merely where it was convenient.

It cost two wrong attempts to get there. The position solve exists to remove the
overlap, so **the contact cannot be found again afterwards**: a ball that has just
landed is resting exactly on the plane, penetrating by zero, and re-deriving
contacts there finds none. Recording them on the *last* sweep fails for the same
reason one level down — the earlier sweeps have already pushed the body out. The
contacts are recorded on the **first** sweep, which sees the impact as it arrived,
and the pass reflects the velocity captured at the top of the substep rather than
the post-solve one, which by then is zero.

| dropped 0.9 m | rebound |
|---|---|
| bounce 0 | **0.000 m** |
| bounce 0.8 | **0.537 m** |

Only contacts that were closing bounce. A body resting on the floor is in contact
on every substep for ever, and reflecting that would feed it energy from nothing
and walk it off the ground.

### Record and scrub

A ragdoll settles in about two seconds and the one frame worth keeping goes past
in a sixtieth of one. The only way back to it was to reset and try to catch it
with the pause key, which is a game of reflexes rather than a tool. **Record keeps
every stepped pose** (20 s, oldest dropped) and the scrub slider moves through
them. Scrubbing pauses — running on from a frame you went back to would overwrite
the rest of the recording, which is not what going back to look at something
means.

### Capture pose

The one control here that writes to the file, and the reason the mode is more than
a toy: drop a rig, let it settle, keep the result.

Each node is moved by the **rigid difference** its body underwent since the rest
pose, not set to the body's absolute transform. A body sits at its centre of mass
and a node at its origin, so the two frames differ by a constant offset — and in a
difference that offset cancels, which makes this correct without first having to
establish what the offset is. World transforms are read before anything is
written, because node transforms are stored as locals and writing a parent moves
its children.

It goes through `nifSnapshotOp`, so it undoes.

### The panel

Everything above is reachable from the Collision dropdown, along with the knobs
that were built and never exposed:

- **body-on-body friction**, which is not the floor's and is why limbs slid
  against each other
- **damping**, the difference between a rig that swings for ever and one that
  settles
- **sweeps** and **substeps** — the stats overlay has always reported joint error
  and there was nothing to *do* about a bad number
- **gravity direction**, as a tilt and a heading rather than a vector nobody wants
  to normalise by hand

Plus **presets** (zero-G, slow motion, drop and settle, ice, stiff and stable),
a **body list** with each bone's pin as a checkbox — on a 39-body rig, pinning a
named bone meant finding it with the cursor, which is the picking problem faced
again for a job that needs no aiming at all — a **system picker**, since `start()`
takes the first jointed system and a skeleton file carries several with no way to
tell which one you were looking at, and the **shortcut legend**, which existed
before the panel did and was invisible unless you already knew it.

Layout defects from bungo's screenshot, fixed: gravity read `m/s2` and now reads
`m/s²`; the tool row wrapped 3+1 leaving Wind alone and is now two rows of three;
the ground height had no unit; the checkboxes were mixed into the label/value grid
and each sat at a different indent depending on whether it spanned one cell or
two, which is what made the left edge look ragged — they have their own column
now; and the dead "17 bodies, 16 joints." line names the system it is describing.

**And it docks.** As a menu it closed the moment you clicked anywhere else,
including the viewport, so every option was set blind and verified by reopening
the menu. The same widget moves into a dock and back — it is never duplicated, so
the two cannot drift. The scroll area is what moves, never the `QWidgetAction`'s
own widget, which is the trap in reparenting a menu's contents.

### Two test bugs worth naming

The wheel check compared the hand with where it started — after a notch out *and*
a notch back in. That is a round trip which lands exactly there, so a working
push/pull failed for being reversible. It samples between the two now.

The bounce test dropped its ball over the origin, where it landed on the ragdoll
instead of the floor and never reached the height being watched for. Both runs
measured a rebound of exactly zero, which reads as "restitution does nothing" when
the ball had simply never hit the ground. It drops clear of the rig now and
measures from the ball's own lowest point rather than an assumed contact height.

A third was not a test bug: capture pose leaves the model modified, and NifSkope
prompts on close when either the window's modified flag or the undo stack's clean
index says something is outstanding. The harness quit into a modal dialog with
nobody to answer it and hung with an empty report. It undoes its own capture —
which is also the only check that the tooltip's promise of undo is true — and
tells the window so.

## 2026-07-29v — clicking a collision shape now picks that shape

**All rigs pass, 0 failures.**

Clicking a collision mesh often selected the wrong bone. The picker was testing
`RagdollSim`'s **sphere set**, not the geometry on screen — a capsule reduced to
its two end spheres, a polytope to its bare vertices at radius zero (clamped to
2 cm so it could be hit at all). Click the middle of a limb and the nearest sphere
was frequently some neighbour's.

It intersects the drawn triangles now. The earlier reasoning for the sphere set —
that a pick should not land where the physics has nothing — does not survive
contact with the problem: a body is rigid, so every point on it is a real place to
apply a force, and a picker that disagrees with what is on screen is simply wrong.

| | hit point off the body it reported |
|---|---|
| sphere set | up to **5.08** game units (brahmin), 3.48 (mirelurk king) |
| drawn triangles | **0.00** on every rig |

### It took four metrics to measure one bug

Worth writing down, because three of them looked reasonable and said nothing.

1. *"Ray at a body's centre hits that body"* — 30 of 39. Suggestive, but a centre of
   mass often sits outside its own shape, so it conflated two faults.
2. *"Aim at a triangle on body b, get body b"* — 88 of 195 before, 76 after. Both
   pickers look equally bad, because a rig's shapes **overlap**: a neighbour
   genuinely in front is the right answer, and counting it wrong buried the
   difference.
3. *"How far is the hit from the nearest triangle centroid"* — 6.20 against 6.86,
   no signal. Collision hulls have large triangles, so this measured triangle size.
4. *"How far is the hit from the triangle itself"* — 0.00 against 5.08. The
   property that was actually broken, and the only one of the four that separates
   the two implementations.

The first three were not wrong about the code; they were wrong about what to
measure. A metric that cannot distinguish the fixed case from the broken one is
worth no more than no metric at all, and it takes longer to admit.

## 2026-07-29u — the root drags, and freezing works from the hand

**All rigs pass, 0 failures.**

### The COM could barely be moved

The drag's force limit was measured against **the held body's** weight. That is
wrong for a hub: pulling the root pulls all 39 bodies, and 25x the weight of one
of them is nowhere near enough to shift the lot. A limb felt fine, because most of
what hangs off a limb is the limb — which is why it looked right until the COM was
tried.

Measured against the whole rig's mass now. That alone widened the worst joint
separation from 4 mm to 19 mm, so the fix is not to weaken the hand again but to
**let the joints keep up with it**: a live drag gets three times the solver sweeps,
because what tears a chain is running out of sweeps, not the force. Only while
something is held, so an idle rig costs nothing.

**0.0036 m on the brahmin and 0.0012 m on the mirelurk king** — tighter than before
the hand was strengthened, and the root moves.

### Freezing applies to the bone in hand

Right-click during a drag now freezes or unfreezes **the bone being held**,
wherever the cursor is pointing. A dragged limb rarely stays under the pointer, so
asking the ray to agree would freeze a neighbour.

The grab is kept through the toggle, which was a correction: the first version
dropped it on freeze, and then the second right-click had nothing to unfreeze and
had to hunt for the bone with the cursor — exactly the aiming problem the feature
exists to avoid. Freeze to leave a bone where it is, right-click again and the drag
picks it straight back up. A pinned body ignores the drag, so holding a frozen one
simply does nothing until it is released.

## 2026-07-29t — the right button only pins, and the bone in hand says its name

**All rigs pass, 0 failures.** Real bone names came back on every one: `Spine4` on
the brahmin, `SPINE1` on the mirelurk king, `COM` on the humanoids.

### The secondary button was doing three things at once

Pinning was put on right-click last change without checking what that button
already did. It does plenty: a right-click **release places the 3D cursor**, a
right-**drag zooms the camera**, and a right-click opens the **context menu**. So a
successful pin also moved the 3D cursor, and a missed one moved the cursor *and*
the camera.

In this mode the secondary button now does one thing. The press is consumed
whether or not it hits anything, the drag is swallowed so it cannot zoom, the
release is swallowed so it cannot place the cursor, and the context menu is
suppressed. Orbiting is still the middle button and zooming is still the wheel,
so nothing was actually lost.

Checked by measuring the camera distance across a right-drag and requiring it not
to move.

### The bone in hand says what it is

A drag now labels the body it is holding, at the body. On a 39-body rig knowing
you have *a* limb is not the same as knowing you have that one.

The name has to come from the NIF: the packfile has none at all. `hkaSkeleton`'s
bone name pointers are null on all 804 corpus bones — the ragdoll's skeleton copy
identifies bones by index and carries no strings — so the only place a name exists
is the node each `bhkNPCollisionObject` targets. Matched by the system block the
collision object points at, because a skeleton file holds several systems and body
indices restart in each.

## 2026-07-29s — the chain goes taut: a drag can no longer tear a ragdoll open

**All rigs pass, 0 failures.** Two changes, and the second is the interesting one.

### Pin is the right mouse button

It was a tool of its own, which meant nailing a bone down cost a trip to the
toolbar and back — and it is almost always done *during* a drag, to hold what you
have. Right-click now pins or unpins whatever it hits, with any tool active, and
pinned bodies draw in red-orange the way a secondary selection reads in Edit Mode.

### A hand is not infinitely strong

Pull a limb away from a pinned root and the rig came apart: **1.90 m of ball-socket
separation** on a brahmin, measured. A joint is a point constraint, so any
separation at all is the rig tearing rather than a chain going taut.

My first diagnosis was wrong. I thought the drag was solved after the joints and so
got the last word, reordered it, and the number moved from 1.90 m to 1.86 m —
nothing. The reorder is kept because joints-last is the right order regardless, but
it was not the fault.

The actual cause: the drag was a near-rigid positional constraint, and simply
out-pulled joints that are deliberately weakened by mass splitting. No ordering
fixes that, because the drag re-applies every iteration.

**The fix is to give the hand a finite strength.** The correction the drag applies
is a position impulse — displacement is `dLambda * invMass`, so `dLambda = F·h²` —
which means capping the accumulated multiplier caps the force. Once the chain is
taut the hand pulls at its limit and the whole rig follows.

**1.86 m → 0.0041 m.** Expressed as a multiple of the held body's own weight rather
than in newtons, so it means the same thing on a 0.2 kg jaw and a Liberty Prime
torso, and exposed as **Strength** under the Grab tool. 0 removes the limit and
restores the tearing, which is occasionally what you want to see.

### The throw test was measuring gravity

Comparing the released speed of a bone let go while moving against one let go while
still failed on the feral ghoul at 1.97 against 1.02 m/s. The "still" case is not
still: holding for 20 frames lets the rig fall, and most of that 1.02 was honest
gravity. Gravity contributes nothing along the hand's direction of travel, so the
measurement is that component now — which is exactly what the throw adds and
nothing else.

## 2026-07-29r — the floor grips, Grab is one tool, and Physics Sim lets go

**51 of 51 checks on 6 rigs, 1 correctly skipped, 0 failures.**

### The mode was genuinely stuck, and it was structural

Physics Sim was added as a seventh viewport mode **without the change signal every
other mode has**. Two consequences, both reported: Pose and the three paint modes
never left it, so the sim kept running underneath them; and Object and Edit did
leave it but nothing the mode button watches changed, so the button kept saying
"Physics Sim" and the mode looked stuck.

Fixed at the root rather than by sprinkling calls: `GLView` emits
`physicsSimModeChanged`, and **the exclusivity now lives in the mode setters
themselves** — `setEditMode`, `setPoseMode` and the three paint setters all leave
Physics Sim when switched on. Two of six call sites had already forgotten it,
which is exactly the argument for putting the rule where it cannot be forgotten.

Tested by entering all four other modes from a RUNNING sim, which is the case that
broke.

### The floor had no friction at all

Not a missing setting — a missing branch. The ground contact `continue`d straight
past the Coulomb correction, so body-on-body contacts had friction and the floor
had none: a ragdoll landed and slid for ever. Ground friction is applied now,
shared by the same contact count as the normal correction (eight vertices on the
floor must not brake eight times), and exposed as **Floor grip**: 0 is ice, 1
stops a rig where it lands.

### Drag and Throw are one tool

They were the same gesture differing only at the moment of release, which is not a
choice worth making in advance. **Grab** now does whichever the hand was doing —
let go while moving and it is thrown, let go while still and it drops.

The test for it was wrong twice over. Comparing how far the bone coasts conflates
the throw with gravity, and on the feral ghoul the still-release case travelled
*further* because the bone was left swinging and kept falling. What the tool
promises is that the hand's velocity survives the release, so that is what is
measured now, at the moment of release before a step can muddy it.

### The simulation is the scene, so it is drawn like one

While the sim runs the preview is near-black, like unselected geometry in Edit
Mode, and **orange is reserved for the bone actually in hand**. Amber everywhere
left nothing to say which one you grabbed.

### Loading a file resets the mode and the camera

Staying in Edit or Pose or Physics Sim across a load points the mode at blocks
that no longer exist, and keeping the camera frames the new file from wherever the
last one happened to be looked at — which on a differently sized model is often
nowhere near it.

## 2026-07-29q — visible shots, a real projectile, a floor you can see

**45 of 45 checks on 6 rigs, 1 correctly skipped, 0 failures.**

**Shooting is visible now.** It was an invisible hitscan: the only evidence a shot
happened was the ragdoll twitching, which cannot tell a miss from a broken tool.
A hit leaves a tracer and an impact mark that fade over 0.6 s.

**And there is a real projectile.** Optional, and configurable: speed, mass,
radius, and whether gravity pulls it on the way. It travels — measured, not
assumed, because a round that teleported to its target would pass every impulse
check while being exactly the thing the feature exists to avoid. The test fires at
20 m/s and counts 11 frames in flight before it connects.

Collision is a **swept** test against the segment it covers each frame, not a
point test at the far end: a 60 m/s round crosses a metre in a 60th of a second,
which is most of a limb. The sweep reuses the picker the mouse tools aim with, so
a shot can only hit what a click could have hit.

**The ground draws as a solid surface**, sized from the rig's own footprint so it
is neither a postage stamp under Liberty Prime nor a runway under a cat. An
invisible plane that a ragdoll lands on looks like a bug.

**Per-tool parameters**, shown only for the active tool: grab firmness, shoot
impulse, the projectile's four settings, blast radius and strength, wind strength.
These were hardcoded — selecting Wind gave you 40 N and no way to say otherwise.

**And the three things built but never wired**: joint-limit highlighting (bodies
outside their limits draw in red, off by default because a rig whose authored pose
already breaks a limit would light up from the start), a ground reset to put the
floor back under the rig, and wind strength.

### The icon was already there

There was a `collision` icon in the set — a wireframe cube — so the one I added
was unreachable dead code and the button had been showing the old one the whole
time. Replaced the real one: a cage around a solid body. The cube was two offset
squares, which at 16 px is indistinguishable from the x-ray icon, also two offset
squares.

### Two hours lost to a stale build

Adding members to `PhysicsPreview` changes its size, and `GLView` embeds it by
value — so translation units compiled against the old header disagreed about
`GLView`'s layout and the app died with an access violation inside Qt. I bisected
three files before remembering this is a KNOWN failure of these qmake projects and
the first thing to rule out, not the last.

`make clean` fixed the crash. `make` then kept relinking a stale binary even after
the sources were touched, because the Makefile's own dependencies were out of date
— **re-running `qmake6` is the actual fix**, and it is the same note. Twice in one
session, both already written down.

## 2026-07-29p — a Collision button on the top bar

The physics-sim controls existed only as keyboard shortcuts, which is fine once
you know them and invisible until then: nothing told you 5 was Blast, or that the
ground height could be moved at all. They now live in a **Collision** dropdown on
the View toolbar, after the animation controls and behind a separator.

**Greyed out when the file has no collision.** Verified both ways: the button is
enabled on a ragdoll and greyed on `Alien_Body.nif`, which carries no bhk block at
all. A panel of controls that cannot do anything is worse than one that is plainly
unavailable.

The panel, top to bottom: **Show collision in viewport**, then Run/Stop with a
body and joint count, the six tools as a button group, Pause / Step / Freeze /
Reset, and the options — gravity and its strength, a speed slider, ground and its
height, self-collision, angular limits, stats overlay. Everything below the
visibility box is disabled until the sim is running, and Step is live only while
paused.

**The visibility box drives the EXISTING `aShowCollision` action** rather than
having a switch of its own. The Render toolbar and the settings dialog already
toggle that one, and a second copy would let the two disagree about whether
collision is being drawn. Checked in both directions, because a panel that only
wrote to the action would go stale the moment the toolbar was used.

Every control reads its state back from `PhysicsPreview` rather than caching it,
and the panel re-reads on open, so a keyboard shortcut and a click cannot
disagree.

Also a **collision icon** — a wireframe hull around a solid body, which is what
collision looks like in the viewport. It was borrowing the gizmo glyph.

**34 of 34 checks on 6 rigs, 1 correctly skipped, 0 failures**, including the
panel opening, both directions of the visibility sync, and screenshots of the
panel idle and running.

One harness trap worth recording: `QToolButton::showMenu()` spins its own event
loop and does not return until the menu is dismissed, so calling it from a test
simply hangs. `QMenu::popup()` is the non-blocking form.

## 2026-07-29n — Physics Sim: six tools and the options

Everything bungo asked for. Six interaction tools and the full option set, all
driven through the real event path and verified in a window on the second
monitor: **7 rigs pass 30 of 30 checks each, 2 correctly skipped, 0 failures.**

**Tools** (keys 1-6, one active at a time like a paint tool):

| | |
|---|---|
| **Drag** | spring grab, let go where it lies |
| **Throw** | spring grab, released carrying the hand's velocity |
| **Shoot** | 12 kg m/s impulse along the view ray, at the point it hits |
| **Pin** | nail a body in place, click again to free it |
| **Blast** | radial impulse, 2 m radius, falling off linearly |
| **Wind** | steady 40 N along the view direction while held |

Shoot, Blast and Throw apply **impulses to velocity**, not corrections to the
pose. An impulse is momentum; pushing the pose instead would put the energy in
through the constraint solve and let the joints cancel most of it on the same
substep. Shoot lands its impulse where the ray hit rather than at the centre of
mass, which is what makes a shot to a leg twist it.

**Options:** freeze (stop all motion but keep solving, so a settled heap holds),
single-step (`.` while paused), gravity on/off and strength (`G`), time scale down
to 0.01x, ground on/off and height, self-collision and angular-limit toggles, and
a stats overlay. Every number in the overlay was already computed by the step and
thrown away.

### Three tests that were wrong, not three bugs

Each of these reported a failure against code that was behaving correctly, and
each was worth fixing properly rather than loosening a threshold:

**Pin "failed" because the test clicked one body and asked about another.** A ray
aimed at body b's centre often hits a different body first — limbs overlap. The
helper returns the body the PICKER lands on now.

**Pin also "failed" on the root**, which `build()` has already pinned, so clicking
it correctly *un*pinned it. Asserted as a toggle now, not as "becomes pinned".

**Gravity-off went through two wrong measurements.** "It should barely move" is
false: the authored pose has bodies overlapping — the brahmin has 18 pairs
touching at rest — and the contact solve pushes them apart regardless. Comparing
total drift is also false, and power armour proved it by drifting *more* with
gravity off (193 against 149), because gravity holds it against the floor while
the push-apart has nothing to settle it. What the option promises is that nothing
accelerates downward, so the measurement is the drop in mean height and nothing
else.

### Still to come

**Capture pose** — hand the settled ragdoll to the Pose Manager. It is the one
item from the list not in this change, on purpose: everything here is
non-destructive, and that one writes to the model, so it gets its own change and
its own scrutiny. The binding it needs is already understood —
`worldOf(node) * rest^-1` per `bhkNPCollisionObject`.

## 2026-07-29m — Physics Sim viewport mode

The mode bungo asked for. It sits alongside Object, Edit and Pose in the mode
menu: the ragdoll runs live, dragging a bone pulls it with a spring, Space
pauses, R resets, Escape leaves. Nothing is written back to the file, so leaving
puts everything exactly as it was.

Verified in a real window, on the second monitor, with `WW_PHYSICS_TEST`:
**7 rigs pass 17 of 17 checks each, 2 correctly skipped, 0 failures.**

### Built to be testable

`PhysicsPreview` owns the sim, the grab and the posed geometry and is GL-free and
widget-free, so the only part that genuinely needs a window is the event plumbing
in GLView. The simulated pose goes out through `setCollisionPreview`, the channel
the collision tools already draw through, so nothing in the render path had to
learn about ragdolls.

The harness posts **real QMouseEvents and QKeyEvents at the GLView** rather than
calling the preview directly. The controller underneath is already covered
headlessly by the drag-spring and pick self-tests; what only a window can exercise
is that a press reaches the picker, that a move reaches the drag, and that Space
and R are not swallowed by another binding first. Calling the controller would
have tested the tested part and skipped the rest.

`WW_WINDOW_AT=x,y` places the window before it is shown and skips `raise()`, so a
run lands on a second monitor and never takes focus. Moving it after `show()` is
not the same thing: it appears on the primary monitor for a frame and then jumps,
which is the interruption the whole arrangement exists to avoid.

### Two bugs only the screenshots caught

**The viewport was not following the simulation.** Every check passed — the pose
moved 449 units in a second — and the two screenshots were pixel-identical. The
harness was stepping the solver directly and bypassing the per-frame tick that
pushes the pose to the preview, so the drawn geometry was whatever mode entry had
set and never changed again. The checks now measure `GLView::collisionPreview()`,
what is actually **drawn**, not what the solver thinks.

**The ragdoll fell out of the world.** With that fixed the next screenshot showed
an empty grid: no ground plane, so a brahmin free-falls 6.4 m in the first second
and is gone. The preview now puts a floor just under the rig at entry, the same
placement the headless `simulate` uses, and the shot shows what it should — the
thing dragged sideways, collapsed, and lying in a heap on the grid.

Neither was visible in a passing test report. Both were obvious in a picture.

### A refusal is not a failure

`CreateABot` and `Robot` carry single-body physics systems with no constraints —
there is nothing to simulate and refusing is correct. The first harness counted
that as nine failures, which would have made a corpus run unreadable; it reports a
skip now, and the mode itself explains the situation in a dialog rather than
sitting in a mode that does nothing.

### Known rough edge

A ray through a body's centre of mass hits geometry for 30 of the brahmin's 39
bodies. The other nine have their com outside their own shapes, which is ordinary
for a curved limb. It affects nothing a user does — they click on the limb, not on
its centre of mass — and is recorded because the harness reports it and the number
would otherwise look like a bug later.

## 2026-07-29k — ray picking, and the identity the viewport will lean on

Second half of the Physics Sim mode's testable part. `RagdollSim::pick` returns
the nearest body along a ray together with the hit point **in body space**, which
is exactly what `setDrag` takes — so picking and grabbing compose without the
caller redoing the transform.

**It tests against the solver's own sphere set, deliberately.** Picking against
the drawn geometry instead would let a user grab a limb at a point the physics
does not have, and the drag would then pull on a spot the body cannot feel. A
polytope's faces are precisely where the two representations differ. The cost is
that a hull is only grabbable near its vertices, so every point gets a 2 cm
minimum radius — a pick wants to be forgiving where a contact does not, and a
polytope contributes its vertices at radius zero, which nothing could ever hit.

The conversion is the awkward part and worth naming: shape points are in BONE
space while `x` is the centre of mass, so a point sits at `x + q*(p - com)`.
Dropping the `com` shift offsets every pick by most of a limb's length.

### Tested as a property, not a fixture

For each body, fire a ray at one of its own shape points from just outside that
point's radius, along each of the three axes. No hand-written coordinates to go
stale, and it holds for every rig at once: **2571 rays across all 37 vanilla rigs,
every one hit a body.**

The ray starts close on purpose. Fired from far away it would legitimately hit
whatever is in front, and what is under test is that the transform chain and the
sphere intersection agree — not occlusion order. So a *different* body coming back
is not counted wrong; only no body at all is.

Alongside it, the identity the viewport actually depends on:
`toWorld(pick.body, pick.localPoint) == pick.worldPoint`. A transform error there
would grab the right body in the wrong place, which is the kind of bug that looks
like bad physics. Worst error over all 37 rigs is **9.05e-06 m** — float
round-off.

**Still not done:** the viewport glue — mode switch alongside Object/Edit, mouse
to ray, Space to pause, R to reset. Both halves it sits on top of are now built
and measured; what remains is on-screen behaviour that needs a window.

## 2026-07-29j — the drag spring, built and measured without a window

First half of the Physics Sim viewport mode. The solver already had `setPinned`
and `setPosition` — its own header called them "what dragging a bone will use" —
but a pin is the wrong mechanism, and the spring that replaces it is the part that
can go numerically wrong, so it is the part worth building where it can be
measured. `simulate --drag N --drag-spring` exercises it headlessly.

**A pin is infinitely stiff: it teleports.** The grabbed body carries the whole rig
rigidly and every joint downstream gets a step-sized correction it did not ask
for, so limbs snap rather than swing. `setDrag` adds a compliant positional
constraint solved alongside the joints, so the rig lags, swings and settles the
way its own joints do. The grab point is in body space, so dragging a limb by its
end rotates it instead of sliding it.

### Firmness is dimensionless, and that is the point

XPBD's natural parameter is compliance in m/N. It is physically meaningful and the
wrong knob here: at a fixed compliance the same grab that barely moves a 5 kg limb
is rigid on a 0.2 kg one, so a drag would feel different on every bone and
completely different on Liberty Prime. Firmness runs (0, 1] and is converted to a
compliance per solve using the body's own generalised inverse mass, which holds the
FEEL constant instead.

Worst tracking lag on the brahmin's body 5 over a 0.138 m circular pull at two
seconds a lap, as a percentage of the radius:

| firmness | 1.0 | 0.9 | 0.5 | 0.2 | 0.05 |
|---|---|---|---|---|---|
| worst lag | 0.0% | 0.5% | 4.9% | 16.6% | 54.6% |

All of them land on the target exactly once the pull stops. 0.9 is the default: a
grab that visibly gives without ever feeling loose.

### Three things I got wrong, all caught by measuring

**Reusing the shared helper double-counted the mass.** `applyPositional` performs
its own 1/w solve, so handing it a `dLambda` that already carries the
1/(w + alpha) factor divides by the inverse mass twice — a 3x overshoot that put
the brahmin at 185 m/s and 250,000 J. The first, far too soft, default hid it
completely by making `dLambda` tiny. The impulse is applied directly now, and the
comment says why the shared path could not be used.

**A plausible-looking compliance was 70x too soft.** 1e-4 m/N reads like a firm
grab and lagged 63% of the pull.

**The lag was measured before the step**, which reads how far the target moved that
frame rather than how well the grab tracked it — it came out identical (25.0%) for
firmness 1.0 and 0.2, which is what sent me looking. Measured after the step, the
curve above is monotonic and sensible.

I had also written that firmness table into the header comment *before* measuring
it, with invented numbers. They are the measured ones now.

37 of 37 vanilla rigs dragged by spring without a blow-up. Solver self-tests still
green: every rig bounded within 25%, the box still settles on the plane.

**Not done:** the viewport half — mode switch, ray-picking a bone, Space and R. It
is on-screen behaviour and needs a window, which is bungo's call.

## 2026-07-29i — §4d closed, and the fix verified through the path a user takes

The compile-every-collision-type campaign is done, and the shared-vertex fix is
confirmed end to end rather than only in the CLI.

**Decompiling the SetDressing billboard** — `Havok/Decompile All Compiled
Collision`, the spell a user actually reaches for — now yields an editable mesh of
**710 vertices and 1089 triangles**, which is the corrected decode exactly. Before
yesterday's bound it would have produced 565 / 797, with vertices in the wrong
places. The file's second, single-section mesh is unchanged at 64 / 32, which is
the right answer for a mesh that shares nothing.

That is the check that matters: the round-trip sweeps could never have caught the
shared-vertex bug, because the bytes reassemble perfectly either way. Only
something that *consumes* the decoded geometry can.

### What §4d being "done" means, precisely

Objects whose content is fully modelled are written from the model — both packfile
roots, body records, capsules, spheres, convex polytopes, compounds, mass
properties, `hkaSkeleton`, ragdoll and limited-hinge constraints, the position
motor, the breakable wrapper. Objects whose content is not reconstructible are
written from their stored bytes with their fixups — compressed meshes and their
data objects, compound shape data, material tables, scaled-convex wrappers, and
constraint kinds with no encoder of their own.

The distinction is measured rather than asserted: `--roundtrip` assembles every
file twice, once with the stored bytes and once with everything re-derived, and
reports both numbers.

**Compressed-mesh derivation is deliberately not part of it.** Rewriting an
unedited mesh from decoded geometry would requantize it and change the file for no
gain. An edited mesh does not want a rewrite either — it wants a fresh one, which
is what `hknpEncodeCompressedMesh` already does and what the in-game validation
covered. There is no third case, so there is nothing left to build here.

Cloth remains deferred, as agreed.

## 2026-07-29h — a quarter of all collision meshes were decoding wrong

Auditing the sticky-flag bug found yesterday turned up the thing it was hiding.
**32 of 127 sampled compressed meshes use shared vertices, and every one of them
was decoding with an unbounded index.** A SetDressing billboard went from
565 v / 797 t to 710 v / 1089 t — geometry that was missing from the viewport,
from the simulator's contacts and from anything reading the decode.

### The sticky flag, everywhere it was

`Reader::ok` is sticky by design, and **every structural loop in the decoder was
written `while ( i < count && r.ok )`** — seven of them: compressed-mesh sections,
their primitives, skeleton bones, ragdoll constraint bindings, the root's shape
list, compound instances and compound children. One out-of-range read anywhere
silently truncated every list decoded after it.

There is now a `Reader::Scope` that clears `ok` on entry and restores it on exit,
declared at the top of each per-item loop body. `ok` then means "THIS item read
cleanly" rather than "nothing has failed since the file was opened", which is also
what the atom-chain walk actually wants — a bad read there really does invalidate
the rest of that one chain, and no others.

A separate `everFailed` records that a file had a bad read at all, with the offset
of the first, and `collision` reports it. The alternative is what used to happen:
a truncated list, no error, and geometry missing that nobody had reason to look
for.

### What the flag was hiding

The billboard's first bad read landed at +0x58c08 in a 23,680-byte file — 15x past
the end, so an offset was being computed from a field read wrong, not an
off-by-one.

A section's `+0x4c` packs its first shared-vertex index in the high 24 bits. **Its
low byte is not a shared count.** On all 7 sections of that billboard it equals
`numPacked` at +0x58 exactly, while the real per-section shared range is the gap
to the next section's first index. Reading it as a count let section 6 ask for
index 174+74 of a 178-entry array, walk off the end into the packed-vertex data,
and take a vertex position as a shared index.

My first attempt at a guard used that low byte, which is why it changed nothing.
Bounding against each array's OWN count — the hkArray counts at +0x78 and +0x98 —
is exact and needs no interpretation of the low byte at all.

Both faults compounded: the garbage index put vertices in the wrong place, and the
`&& r.ok` guard then abandoned the rest of that section's primitives.

### Scope

Multi-section compressed meshes are 29 of 127 in the sample and every one of them
shares vertices. They are architecture and set dressing — the collision most of
the game is built from.

Assembly sweep unchanged at 470/470 byte-exact with no refusals, ragdolls 69/69,
per-shape sweep clean, simulator unaffected. None of those would have caught this:
the bytes round-trip perfectly either way, because the fault was in reading them,
not in writing them.

## 2026-07-29g — 470 of 470, and a sticky flag that was hiding shapes

The last two refusals are gone, and chasing the second one turned up a decode bug
with far more reach than the assembly. A 714-file stride sample now reassembles
**470 of 470 packfiles byte-identical, zero refusals, zero structural
differences** — up from 465 of 465 with two refusals, because the fix recovered
three systems that had never fully decoded.

### `hknpScaledConvexShape`

A 112-byte wrapper with the usual shape header — flags at +0x10, convex radius at
+0x14, material at +0x18 — a pointer to the real shape at +0x30 and a scale at
+0x40. One packfile in 714, in a SCOL, and it had never decoded at all, so its
body had no shape and the system rendered nothing.

It now decodes as its child's geometry scaled, and writes as the wrapper followed
by the child, which brings its own `hkRefCountedProperties` and mass properties
along — so the encoder recurses rather than repeating that chain.

**The scale is inferred.** +0x40 holds (0.341, 0.357, 0.341) with 0.5 in the w
lane, which is the slot and the w-tagging every other hknp object uses for one,
but it has not been checked against a rendered mesh. Writing does not depend on
it: the wrapper goes back byte for byte either way.

The first attempt wrote the child and dropped the wrapper, its
`hkRefCountedProperties` and its mass properties — three objects of five — because
the wrapper's own shape carries the child's geometry and so looked like an
ordinary convex polytope to every test in the chain. It has to be checked first.
The same thing then bit the per-shape round-trip, which compared a re-derived
polytope against the 112-byte wrapper; that check now skips wrappers, since the
assembly is what covers them.

### `Reader::ok` is sticky, and every structural loop was gated on it

One out-of-range read anywhere leaves the flag false, and every later
`while ( i < n && r.ok )` stops without saying so. A 12-instance compound on a
billboard decoded 11 children — the twelfth silently absent from the shape list,
from the viewport and from the body's attribution, with no error anywhere. It only
surfaced because the assembler refused to write a compound whose child count did
not match its instance count.

A child's decode is bounded work; if it reads badly that is its problem and not a
reason to abandon the children after it. The flag is saved and restored around
each child now, on every exit from the loop body including both `continue`s. The
billboard decodes 12 shapes, all attributed.

That is the second time an integrity check earned its keep by refusing rather than
guessing — the counts had to disagree for anyone to notice.

### A shape with no geometry is still a shape

A compressed mesh that decodes to no triangles used to be dropped outright. Its
bytes are captured either way, so it is kept now: it draws nothing, exactly as
before, and it writes.

Per-shape sweep unchanged: no mismatches, polytopes 143/143, compounds 14/14.
Ragdolls 69/69. Simulator unaffected.

## 2026-07-29f — physics systems assemble too: 266 of 266 rebuilt byte for byte

`hknpEncodeSystem` and `hknpEncodePhysicsSystemData`. Ragdolls are 37 files; this
root covers everything else — architecture, interiors, set dressing, props. A
403-file stride sample of the 34,985-file mesh tree now decodes and reassembles
**266 of 266 packfiles byte-identical to what Havok wrote**, zero structural
differences, one clean refusal. Ragdolls stay at 69/69 in the same build.

The ragdoll root DERIVES from `hknpPhysicsSystemData`, so most of the work was
already done. What was not:

- **The header is 0x80, not 0x90.** The ragdoll root's extra bone-to-body
  descriptor at +0x80 is the whole difference, and payloads start immediately
  after either way. 50/50 and 6/6.
- **An empty array still writes `count 0` with the `0x80000000` flag.** Skipping
  the descriptor entirely leaves three zeroed words a static system does not have.
- **`dyn_motion` and `dyn_inertia` have independent counts.** One vanilla prop
  carries an inertia entry and no motion entry at all, so deriving both from the
  motion index writes an array the file lacks and shifts every offset after it.

### Local fixups follow the same reflection walk as globals

Proved on ragdolls that GLOBAL fixups are emitted in member declaration order
rather than by offset. The local table is ordered the same way, which a sort
matched on every ragdoll and on no compressed-mesh system: inside a
`CompressedMeshShapeData` the section array's own fixup lands between its +0x50
and +0x60 members, because an array member contributes its payload's fixups where
the member is declared. So the decode now records table order and the assembler
does not sort. One rule, three tables, and the earlier agreement was luck.

### Carrying an object's bytes without its fixups writes a null pointer

Written down for the compound shape data last entry, then repeated twice: a
compressed-mesh shape has two local fixups of its own at +0x68 and +0x80 with
varying targets, and `hknpBSMaterialProperties` has one at +0x10. Both are now
recorded per object rather than assumed, as `dataLocal` already was.

### Five fields that were "constant" because 37 actor skeletons agreed

Every one of these was a measured constant in the ragdoll corpus and wrong on
ordinary props, so all five are now recorded rather than derived:

| field | ragdolls | elsewhere |
|---|---|---|
| `cinfo+0x18` | `0x00010080` on all 140 | 0 on all 66 static, and 9-to-2 split on dynamic |
| `dyn_inertia+0x00` | `(motionIndex, 1)` | `0xffff` |
| `dyn_inertia+0x2c` | `1.0f` | 0 |
| `body_props+0x0e` | 0 | `0x0020` |
| `body_props+0x10` | `0xff00` | `0xff02` |

The last two are why `body_props` is now carried whole and patched, the same
contract constraints have had since 07-28x — chasing the next word one at a time
is not a strategy.

### Two decode conveniences that a writer must not persist

`layer` substitutes a useful default when the file stores 0, because 0 means
"unidentified" and shows the user nothing. Writing that back changes the file, so
the stored word is kept separately. The first fix used `packedFilter ? ... : ...`
and still failed on the one road prop whose filter really is 0 — a stored zero is
not "nothing was decoded", so there is an explicit flag now.

### Hashes come from the file

A writer that only knew a built-in class table refused any packfile containing a
class it had never sampled, and `hknpStaticCompoundShape` and
`hkpBallAndSocketConstraintData` are both real and both rare enough that ten
minutes of corpus scanning did not turn either up. The decode now records the
file's own `__classnames__` hashes and the assembler prefers them. They cannot be
wrong, and the built-in table is only needed when authoring something new.

Compressed meshes, their data objects, material tables and constraint kinds with
no encoder of their own all rewrite from stored bytes. That is not a substitute
for understanding them; it is what makes an edit to one joint limit leave the rest
of the file alone.

`--roundtrip -o` now writes one file per system. A NIF can hold several, and
writing them all to one name leaves only the last — which is how the Gorilla
skeleton's failing static system got diffed against its healthy ragdoll and
appeared to pass.

Per-shape sweep unchanged: no mismatches, compounds 27/27, polytopes 193/193.
Simulator unaffected. One refusal in 266: a system whose body names no shape.

## 2026-07-29e — the packfile assembly: all 37 vanilla ragdolls rebuilt byte for byte

`hknpEncodeRagdoll` and `hknpBuildPackfile`. **All 37 vanilla ragdolls are now
decoded and reassembled into files byte-identical to the ones Havok wrote.**

This is what the nine object encoders were missing. On their own they produce
bytes nothing can load, and nothing tested the class-name table, the object order,
the three fixup tables or the section headers, because no single object holds them.

### The layout rules, all measured on 37 files

- `__classnames__` at 0x100, entries `u32 hash | 0x09 | name | NUL`, padded to 16
  with 0xff. `hkClass`, `hkClassMember`, `hkClassEnum`, `hkClassEnumItem` always
  lead — which is what puts the root's own name at offset 75 in every vanilla file
  — and the rest follow in **order of first use** walking `__data__` (37/37).
- `__types__` empty; objects back to back, each padded to 16; root first,
  `hkaSkeleton` last.
- Local and virtual fixups ascend by source. **Global fixups do not** — they are
  grouped by source object and, within an object, in *member declaration* order.
  An array member contributes its payload's fixups where the member is declared,
  so the root's skeleton pointer at +0x78 is written *after* the fixups for the
  array whose payload sits at +0x7c0. Sorting by offset puts it in the wrong place.
- Each fixup table is padded to 16 with 0xff and carries **no sentinel entry**.
  This wrote a full `0xffffffff` entry instead, which reads the same to a parser
  but is 20 bytes more than any vanilla file — the first thing that stopped a
  rebuild from matching. A section-header name is likewise NUL-padded with 0xff in
  byte 0x13 alone, not 0xff-filled.

Those last two live in the shared helpers, so the compressed-mesh writer now
matches vanilla too. Its output changes by those bytes; a reassembled ragdoll being
byte-identical to Havok's own file is the strongest evidence available that the new
form is the right one.

### Three things the corpus said that I had wrong

**`+0x60` is not "entirely zero"** — yesterday's entry says it is. Every one of its
slots is patched by a global fixup, so the *bytes* are zero and the *contents* are
the ragdoll's shape list. Reading raw bytes and not the fixup table is the same
class of miss as reading `%.3f` and not the mantissa.

**Its order is the NIF's, not the ragdoll's.** It holds the same shape set as the
body cinfos — a bijection on all 37 — but in a different order on 36 of them. The
order is the sequence the `bhkNPCollisionObject` blocks appear in the NIF, exact on
35/37 (the two others are parts kits whose spare bodies have no node at all). So it
is *not derivable from the packfile*: a writer inside a NIF has it, a standalone
rewrite does not, which is why the decode now records it. A depth-first walk of the
bone tree reproduces it on 32 of 34 — another near-miss that would have been wrong.

**`cinfo+0x12` is not a material index.** It is the body's slot in that shape list,
the exact inverse permutation, on all 37. The decoder has been reading it as an
index into the body-material table; on ragdolls, which carry no such table, that
resolves to nothing and stays invisible. Flagged, not yet chased on physics systems.

### Nine per-body words that were not modelled

`dyn_inertia +0x30` is a world-space position distinct from the body's own on 848
of 857 bodies (0.67 m apart at the median, tracking skeleton scale up to 12.9 m on
Liberty Prime); `+0x40` is a unit quaternion on all 857 and also not the body's
orientation. `HknpBodyPhys` gains `motionCom` and `motionOrientation`, the two w
lanes beside them, and `motionIndex`, which was computed and thrown away before.

### Assembled twice, because the two runs answer different questions

Shapes now carry `rawData`, the same contract constraints have had since 07-28x. A
capsule's core box is derived from (segment, radius, roll) and cannot survive a
float round trip — about 750 differing bytes on an 18-capsule ragdoll, worst vertex
error 1e-06 m. Editing one joint limit should not perturb every capsule in the file.

So `--roundtrip` builds each file both ways. With the stored bytes: **37/37
byte-exact**. With every shape re-derived: 2/37 byte-exact, and on the other 35
*every differing byte lands inside a shape object* — none in the root, the
skeleton, the constraints, the fixup tables or the headers. That second number is
the honest measure of how much is reconstructed rather than copied, and the
classification is what proved the assembly right before the copies existed.

Mass properties carry their bytes for the same reason: a packed vector whose three
mantissas are all zero keeps whatever exponent Havok's arithmetic landed on, and
zero mantissas record no magnitude, so 24 of 269 corpus objects hold a value that
is genuinely unrecoverable.

### Compounds

The 5 ragdolls whose bodies carry compound shapes went from a clean refusal to
byte-exact. Three things they needed, none of which the decode kept, because it
flattens a compound into one shape per instance: the compound's **owning body**;
its **children** as such (every child carries the same body, so the body alone
cannot separate them, and the first child would otherwise be taken for the body's
whole shape); and its `hknpDynamicCompoundShapeData`, which is carried whole — 224
bytes for 2 instances, 288 for 3, 352 for 4, a dozen non-zero words and no reading
established for any of them.

Object order around a compound is compound, then children, then the shape data —
**data last, though its pointer at +0xC0 sits below the child pointers at +0xD0+**,
the same inversion as the root's skeleton pointer. Its own local fixup at
+0x10 → +0x40 is recorded rather than assumed: carrying an object's bytes and
dropping its fixups writes a null pointer, which is what the first attempt did.

Corpus sweep unchanged: 700 files, no mismatches, compounds 29/29, polytopes
269/269, spheres 4/4, capsules 78/78. Simulator unaffected.

**Still open:** compressed meshes, which still only decode.

## 2026-07-29d — hknpRagdollData layout measured; the "+0x80 trap" is now explained

The ragdoll root's layout, **verified on all 37 with zero violations**. No encoder
for it yet — deliberately, see the end.

Seven hkArrays, each a pointer patched by a LOCAL fixup at `+d`, count at `+d+8`,
`count|0x80000000` at `+d+12`:

| desc | contents | count | stride | pad |
|---|---|---|---|---|
| +0x10 | body_props | bodies | 0x50 | — |
| +0x20 | dyn_motion | bodies | 0x40 | — |
| +0x30 | dyn_inertia | bodies | 0x70 | — |
| +0x40 | cinfo | bodies | 0x60 | — |
| +0x50 | constraints | **bones − 1** | 0x18 | 16 |
| +0x60 | all zero | bodies | 0x08 | 16 |
| +0x80 | bone → body index | **bones** | 0x04 | 16 |

Payloads run back to back from +0x90 and the object ends where the last one does.
A global fixup at +0x78 reaches the `hkaSkeleton`.

**The "+0x80 bone-count trap" the spec has warned about since 4a is now concrete.**
Most arrays are per-BODY, but +0x80 is per-BONE and the constraint count is
**bones − 1, not bodies − 1**. On 34 of 37 ragdolls bones and bodies are equal, so
the distinction is invisible — and my first verification pass assumed body count
throughout and reported "3 violations" without saying what they were. The three are
`SkeletonRef` (48 bodies, 11 bones), `skeletonSentryBodyPart` (24 / 9) and
`TurretMountedSkeleton` (5 / 4): parts kits whose extra collision bodies sit outside
the ragdoll hierarchy, which is the same thing the simulator's `looseBodies()`
already handles. Distinguishing the two counts takes the violations to zero.

That is the fourth time today a corpus has looked uniform because the sample could
not separate two readings. It is also why the trap was worth a named warning.

The two arrays nobody had identified: +0x80 is the identity map `0..n-1` on all 37,
and +0x60 is entirely zero.

**No encoder, on purpose.** This object carries 54 global fixups on a mid-sized
ragdoll — one per shape, one per constraint, one for the skeleton — and writing it
usefully means writing the packfile assembly around it, not another standalone
object. That is the next real piece of work, and it now has a fully measured target.
Every *other* object a ragdoll needs already writes.

## 2026-07-29c — hkaSkeleton: 37/37 from scratch, after formatted output lied again

`hknpEncodeSkeleton`. **37 / 37 byte-exact, rebuilt entirely from the decoded bones
with no source bytes fed back** — the first object here that reconstructs from
modelled data alone rather than from a preserved template.

Layout, all 37 with no exceptions: a zero header, three hkArray descriptors at
+0x18/+0x28/+0x38 (pointer, count at +8, `count|0x80000000` at +12), parent indices
as `hkInt16` at +0x90, bone records at `align16(0x90 + 2n)` at 16 bytes each, and the
reference pose at 48 bytes each. Total exactly `pose + 48n`. **A bone's name pointer
is null on all 804** — the ragdoll's skeleton copy identifies bones by index and
carries no strings, so there is no string table and no fixups beyond the three array
pointers.

**Two things I had recorded wrongly, and one of them is a repeat offence.**

- **The reference-pose scale is not (1,1,1).** 767 of the 804 bones carry
  **0.99999994**, one ULP below unity; only the 37 roots carry exactly 1. My probe
  printed it as `%.4f`, saw "1.0000", and I wrote it down as unity — which is
  precisely the mistake I had documented ninety minutes earlier for the compound's
  `w` slots, and then made again. Formatted output has now concealed four separate
  fields in this format: the compound `w` payloads, its negative zero, the
  mass-properties exponent, and this.
- **The header region +0x48..+0x8f holds four negative zeros.** An earlier probe
  scanned only as far as it already understood and reported the header constant. The
  compound had the same shape of miss at +0x70. Scanning to the end of the object is
  now the rule, not the exception.

Both are fixed at the source: `HknpBone` gained `scale` and the two pose `w` lanes,
and the header constants are written explicitly.

Every collision object in a vanilla ragdoll now round-trips:

| object | result |
|---|---|
| `hkaSkeleton` | **37 / 37** (from scratch) |
| `hkpRagdollConstraintData` | 521 / 521 |
| `hkpLimitedHingeConstraintData` | 246 / 246 |
| compounds | 10 / 10 |
| polytopes | 68 / 68 |
| spheres | 30 / 30 |
| mass properties | 68 / 68 |
| capsules | 819 / 819 structure |

Self-tests green. `hknpRagdollData` and the packfile assembly are what remain.

## 2026-07-29b — The remaining constraint objects; every constraint type now writes

Three more encoders, and with them **every constraint object in a vanilla ragdoll**:

| object | result |
|---|---|
| `hkpLimitedHingeConstraintData` | **246 / 246** rewrite, 244 / 246 from template alone |
| `hkpRagdollConstraintData` (07-29a) | 521 / 521 rewrite, 520 / 521 from template |
| `hkpPositionConstraintMotor` | **37 / 37**, and it takes no parameters at all |
| `hknpBreakableConstraintData` | **3 / 3**, one float |

The hinge decomposes exactly like the ragdoll type: chain `SET_LOCAL_TRANSFORMS`,
`SETUP_STABILIZATION`, `ANG_MOTOR`, `ANG_FRICTION`, `ANG_LIMIT`, `TWO_D_ANG`,
`BALL_SOCKET` plus 8 bytes of alignment tail, with `ANG_MOTOR` and `TWO_D_ANG`
carrying no varying field anywhere. Fields: friction +0xf0, hinge min/max
+0xfc/+0x100, tau a constant 1.0 (ragdoll limits use 0.8). Unlike the ragdoll type
its pivotA is real — a hinge sits off the bone origin, where a ragdoll joint is at it.

**Both template-alone shortfalls are fully accounted for**, which is the part worth
saying: the hinge misses exactly 2 and the ragdoll exactly 1, and those are precisely
the objects carrying the minority flag byte at +0xb0/+0x118 (`17000000`/`05000100`
against the usual `17000100`/`05000000`). The measured distribution is 244 / 2 and
520 / 1. Nothing is unexplained.

`hkpPositionConstraintMotor` is worth a line for being the simplest object in the
format: 48 bytes, twelve words, **not one of which varies across all 37**. The
encoder takes no arguments.

The two trivial templates were checked by rebuilding them from the constants as
written in the C++ and diffing against vanilla — 37/37 and 3/3 — rather than
assuming the transcription was right, since a mistyped hex word would otherwise sit
there silently until something loaded it.

Self-tests green; corpus sweep clean, no mismatches.

## 2026-07-29a — Ragdoll constraints: the atom chain, and what a rewrite can't invent

`hknpEncodeRagdollConstraintData`. **521 / 521 byte-exact** rewriting from the source
bytes; **520 / 521** built from the measured template alone.

**The constraint objects are fixed-size templates, which makes the ragdoll far
smaller than it looked.** Inventory over 37 ragdoll packfiles:
`hkpRagdollConstraintData` is 416 bytes on all 521, `hkpLimitedHingeConstraintData`
304 on all 246, `hkpPositionConstraintMotor` 48 with **zero varying words** — a pure
constant — and `hknpBreakableConstraintData` 48 with one. Only `hkaSkeleton` and
`hknpRagdollData` scale with bone count.

**The atom chain is one fixed sequence per type**, identical on every instance:

    hkpRagdollConstraintData (521/521, fills 416 exactly)
      SET_LOCAL_TRANSFORMS(144) SETUP_STABILIZATION(16) RAGDOLL_MOTOR(96)
      ANG_FRICTION(16) TWIST_LIMIT(32) CONE_LIMIT(32) CONE_LIMIT(32) BALL_SOCKET(16)

    hkpLimitedHingeConstraintData (246/246, + 8 bytes of alignment tail)
      SET_LOCAL_TRANSFORMS(144) SETUP_STABILIZATION(16) ANG_MOTOR(40)
      ANG_FRICTION(16) ANG_LIMIT(16) TWO_D_ANG(16) BALL_SOCKET(16)

So the writer emits atoms rather than patching a blob, and the template is built
from ~15 named constants attached to the atoms they belong to — `RAGDOLL_MOTOR` has
no varying field anywhere in the corpus, `CONE_LIMIT` carries the -100 sentinel for
the bound a cone doesn't have, the limits are two floats each.

**Two honesty notes, both about what the test actually proves.**

The 521/521 rewrite starts from `rawData`, so it proves the field *offsets* — write
a rotation row to the wrong place and the bytes diverge — but the constants come
along for the ride. So the encoder is run a second time with `rawData` cleared, to
test what a newly authored constraint would really get. That second number is the
honest one, and it is what turned up the next finding.

**The w lanes of the rotation basis vectors are SIMD residue, not data.** They hold
values in [-1, 1] of the same magnitude as the rotation itself, the third row is
zero almost everywhere, and one vanilla pivot w holds outright garbage
(`0x98d3b2b5`). Havok ignores them. A freshly authored constraint therefore *cannot*
be byte-identical to vanilla and does not need to be — 518 of 521 differ in nothing
else. They are reported as their own category, like the mass properties' inert
exponent.

The single genuine template miss is one object using the minority flag byte at
+0xb0 and +0x190 (`17000000`/`05000100` against the usual `17000100`/`05000000`).

Everything else re-verified unchanged: compounds 10/10, polytopes 68/68, spheres
30/30, mass properties 68/68, capsules 819/819. Self-tests green.

## 2026-07-28z — The compound encoder, and two things that print as constants

`hknpEncodeCompoundShape`, plus `HknpSystem::compounds` — decoding flattens a
compound into one shape per instance, which is what drawing wants, so nothing
represented the compound itself for a writer to reproduce.

**A compound is the first shape that cannot be written as a self-contained blob.**
All 60 corpus child-pointer slots hold **raw zero**: the binding to children and to
the CompoundShapeData lives entirely in the packfile's fixup tables. So the encoder
returns the bytes *and* an `HknpCompoundFixups` telling the caller which slots it
must still patch once the children have been placed. That is a real architectural
difference from the primitives, and it is what a packfile writer will need.

Layout, 14 corpus compounds with no exceptions: `0xD0 + count * 0x80` bytes,
instance hkArray at +0x60 (count at +0x68, `count | 0x80000000` at +0x6c, pointer
patched by a LOCAL fixup to +0xD0), CompoundShapeData via a GLOBAL fixup at +0xC0,
instances at +0xD0 with a 0x80 stride.

**Two fields I called constants were not, and both hid behind their own printed
form.** The byte round trip caught each; a float comparison would have passed.

- **The header from +0x70 to +0xCF.** My first scan stopped at +0x70 — the end of
  the instance-array descriptor — and concluded the rest was zero. It holds an
  **AABB** at +0x80/+0x90 and more besides. An encoder built on that first pass
  would have written every compound with no bounds.
- **The instance `w` slots.** I printed them as `%.3f`, saw `0.500` on all 60, and
  recorded a constant. They are 0.5 with a payload in the low mantissa bits, the
  way a hull vertex carries its index — values run 64/66/70, multiples of 16 up to
  1440, and small integers. Then, having fixed that, I wrote `0.0f` into row 1's
  slot with a comment saying *this one really is a flat zero*; three instances carry
  **negative zero**, which prints as `0.000` and compares equal to `0.0f`.

Both are now carried verbatim rather than reconstructed, the same treatment the
mass properties' major-axis frame gets. The AABB is decoded out for use;
`headerTail` and `wPayload` are honestly labelled undecoded.

Full corpus sweep, 700-file stride over the 34,985-file tree — **no mismatches of
any type**:

| type | result |
|---|---|
| compounds | **29 / 29 byte-exact** |
| polytopes | **269 / 269 byte-exact** |
| spheres | **4 / 4 byte-exact** |
| mass properties | 245 + 24 inert-exponent = **269 / 269** |
| capsules | **78 / 78 structure byte-exact** |

Actor skeletons: 10 / 68 / 30 / 68 / 819, all clean. Self-tests green.

## 2026-07-28y — Testing the encoders outside the data they were built from

Every number so far came from 39 actor skeletons. TO_BE_IMPLEMENTED says plainly
that Architecture, SetDressing, SCOL and Landscape are unmeasured and are where the
hulls and compounds actually live, so I ran `--roundtrip` over a **700-file stride
sample of the whole 34,985-file mesh tree**.

It found a real bug immediately: **8 polytopes failed, in exactly the categories the
skeletons don't cover** — Architecture, Landscape, SetDressing, Vehicles.

**Cause: the encoder was writing the wrong material.** `HknpShape::materialCRC` is
the *effective* material — for a convex shape owned by a body that names one, the
decoder resolves the body's material over the shape's own, which is what a user
should see. It is not what sits at shape+0x18. On actor skeletons the two agree, so
the mistake was invisible; on static architecture they differ. The stored value is
now kept separately as `shapeMaterialCRC` and all three encoders write that.

This is the second time this stretch that a sample too narrow to separate two
readings quietly confirmed the wrong one — the capsule's AABB-vs-OBB was the first,
and the `+0x20` "constant" was the third. The pattern is clear enough to state as a
rule: *a corpus that cannot distinguish two hypotheses is not evidence for either.*

After the fix, over the 700-file sample:

| type | result |
|---|---|
| polytopes | **269 / 269 byte-exact** |
| spheres | **4 / 4 byte-exact** |
| mass properties | 245 byte-exact + 24 inert-exponent = **269 / 269** |
| capsules | **78 / 78 structure byte-exact**, worst vertex error 4.8e-07 m |

Actor skeletons re-verified unchanged (68 / 30 / 68 / 819); self-tests green.

Coverage caveat, since it is the whole point of this entry: 114 of the 700 sampled
files carry an encodable shape, and compressed meshes and compounds are still only
*decoded*, not written. The sample is a stride, so it is proportional rather than
exhaustive.

## 2026-07-28x — The polytope encoder: 68 of 68 byte-exact

`hknpEncodeConvexPolytopeShape`. Every collision object type in the actor skeletons
now round-trips:

| type | result |
|---|---|
| polytopes | **68 / 68 byte-exact** |
| mass properties | 64 byte-exact + 4 inert-exponent = **68 / 68** |
| spheres | **30 / 30 byte-exact** |
| capsules | **819 / 819 structure byte-exact**, worst vertex error 9.9e-07 m |

The round trip turns out to have been *unblocked all along*: 07-28v listed the array
padding as a blocker, but the decode was already keeping every padded slot — all
`nv` vertices and all `np` planes, spares included. The only thing genuinely dropped
was the `+0x10` flag word, now kept as `HknpShape::shapeFlags`. Worth recording,
because I had filed a blocker against something the code already did.

**The one real find was the vertex padding tag.** At 50/68 I diagnosed the failures
rather than guessing, and all 18 came from a single cause: I wrote each padding
slot's own slot number into its `w` index tag, which is the obvious reading of "w
carries the vertex index". Vanilla instead makes every padding slot a **duplicate of
the last real vertex — position and tag both**. That is 18 of 18 of the polytopes
carrying padding, and it means the real vertex count is recoverable from the face
loops (highest index + 1), so the encoder needs no extra input to reproduce it.

Guessing would have been tempting here and would have cost more: the natural fix —
carrying the raw `w` bytes through the decode — would have worked, added per-vertex
state, and hidden the fact that the rule is derivable.

So of the four polytope blockers, **two are closed** (mass properties, padding
contents) and two remain, and both only bite when synthesizing *new* geometry rather
than rewriting existing: `minHalfAngle`'s exact quantization (semantics known, 67%
exact) and the `+0x20` major-axis packing.

Self-tests green.

## 2026-07-28w — Mass-properties writer, and minHalfAngle identified

Two of the three polytope blockers moved.

**`hknpEncodeShapeMassProperties` is in, with an hkPackedVector3 *writer*** — the
inverse of the decoder's, and the reusable half of this. `frexp` gives exactly the
exponent wanted: its mantissa is in [0.5, 1), so every component divided by 2^E
lands in range without searching for a scale. **68 of 68 vanilla objects round-trip**
— 64 byte-identical, 4 differing *only* in the exponent of an all-zero vector.

Those 4 are reported as their own category rather than folded into either column.
A packed vector with three zero mantissas keeps whatever exponent Havok's arithmetic
landed on (−45 in one vanilla centre of mass, where this writes −96); the decoded
vector is identical, and the original exponent is genuinely unrecoverable because
zero mantissas record no magnitude. Calling that a pass would hide a real difference;
calling it a failure would imply something is wrong.

**`minHalfAngle` is no longer an unknown quantity.** It is the sharpest edge on the
face — the minimum over the face's edges of the angle to the neighbouring face,
halved and quantized over 0..90° into a byte:

    byte = round( halfAngle / 90° × 255 )

Correlation 0.9985 over 2120 faces; **67% exact, 87% within 1.** Two refinements
that mattered: shared edges have to be keyed by vertex *position*, not index, since
the padded vertex arrays carry duplicates; and all 76 polytopes turn out to be closed
manifolds, which rules out missing neighbours as the cause of the residual. Taking
the minimum over *all* faces rather than edge-adjacent ones changes nothing (68.0%
vs 67.4%), so that is not it either. The remaining error is systematically positive
— Havok's value is conservatively lower than the true minimum — and the last step is
not cracked. Semantics known, exact quantization not.

**A correction.** The header claimed the `+0x20` major-axis field was "one constant
value in every file seen". Over 76 objects it takes **23 distinct values** — the
earlier sample was simply too small, exactly the way the capsule's core box looked
like an AABB until tilted capsules turned up. Its packing is not decoded either: as
four int16 over 32768 the norm sits near √3 rather than 1, and the largest component
does not reliably saturate. So it is carried as opaque bytes, which is honest, and
means there is still no defensible default for a *new* polytope.

Capsules 819/819 and spheres 30/30 unchanged; self-tests green.

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
