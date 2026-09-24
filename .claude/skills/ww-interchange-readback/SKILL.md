---
name: ww-interchange-readback
description: Gate a writer that exports OUR data into somebody else's format — glTF, FBX, OBJ, a .hkx clip, a JSON sidecar — where "it opens" is not proof. Covers the four-gate set (structural validator, independent read-back, content counts, the third-party importer), the rule for picking an invariant that is a property of the DATA and not of the textbook, the sabotage floor under each gate, and using the vendor's own shipped file as the control so a red gate names the fixture instead of the code. Use whenever the deliverable is a file another program has to read.
---

# The read-back gate for an interchange writer

Written from lane HKX4b (NifSkope WW, glTF 2.0 export of a Fallout 4 character
plus a Havok clip, 2026-09-10), which shipped 3,234 checks and found four real
defects, two of them in code that had already produced a file that opened fine
in Blender.

**The premise.** A file that loads in the target application is worth almost
nothing as evidence. Blender opened the very first export of this lane, drew a
character, and played the animation — with every bone's `x` and `z` swapped,
because the writer's NIF reader had three unsequenced reads in one argument
list. Nothing about "it opens" could ever have caught that. What caught it was
a second reader that had never seen the first reader's code.

## 1. Four gates, and none of them substitutes for another

| gate | question | what it cannot see |
|---|---|---|
| **structural validator** | is this a legal file of that format? | whether it says what the source said |
| **independent read-back** | does it still say what the source said? | whether the format is legal |
| **content counts** | vertices, indices, frames, weight sums vs the source | anything about layout |
| **the real importer, headless** | does the vendor's own reader agree? | nothing you can debug from |

Run all four. The fourth is the only one that is not your code judging your
code, and it is also the one that tells you least when it fails — so it goes
last and it is never the only gate.

## 2. The independent read-back is the centre of gravity

* **It shares NO code with the writer.** Not a helper, not a constant, not the
  struct. Write it in the other language if the writer is C++ — a Python reader
  of the format plus a Python reader of the source is the cheapest true
  independence you can buy.
* **It undoes the writer's declared conversion and compares against the
  SOURCE**, field by field, not against a golden file. A golden file locks in
  today's bug; the source cannot.
* **It reports the worst error per gate as a number**, always, green or red.
  "R4 234 channels, 23 frames: worst translation 4.20e-6 units, rotation
  2.34e-6 deg" is a result. "PASS" is not.
* **The reader you lean on must have been RUN.** The lane inherited a Python
  NIF reader from its own earlier run and discovered, on first execution, that
  it crashed on the first shader block of the first fixture — it had been
  written, committed to the report as machinery, and never executed once. A
  gate script with no run behind it is a draft.

## 3. Pick an invariant that is a property of the DATA, not of the textbook

This is the expensive one, and it is the reason this page exists.

The textbook invariant for skinning is: in the bind pose, skinning returns the
mesh unchanged. The lane pre-registered it, and it failed by **1.727 metres**
on every shape. The gate was wrong, not the writer: Fallout 4's body meshes
store their vertices with the origin at the top of the head, and the skin is
what stands the model on its feet.

The procedure when a pre-registered invariant goes red everywhere at once:

1. **Suspect the invariant before the code** when the residual is the same for
   every element. A uniform error is a convention you have not read yet; a
   scattered one is a bug.
2. **Measure the residual on the VENDOR's own shipped file**, through the same
   path. If Bethesda's file gives the same constant, the constant is the format.
3. **Restate the gate as the weaker, true property.** Here: every joint matrix
   must be one and the same rigid transform (spread ≤ tol), its rotation must
   be the declared axis conversion, and its translation is *reported* as the
   file's storage offset rather than required to be zero. That still fails on a
   transposed matrix, a shifted joint list, a dropped axis node and a
   mis-composed hierarchy — everything the textbook version caught — and it
   does not lie about the data.
4. **Write the number into the contract**, with the file it was measured on.
   The next lane must not rediscover it.

Never loosen a tolerance until a gate passes. Either the invariant is true and
the code is wrong, or the invariant is wrong and must be restated with a
measurement behind it.

## 4. The sabotage floor, one per gate, built BEFORE the green run

CONSTITUTION 4: a check that cannot fail is not a check. Two kinds:

* **For the structural validator**, a script that writes broken COPIES of a
  real output — JSON-only edits are enough and let one payload serve every
  mutant: a length that is four bytes long, a view that overruns, a
  misaligned offset, an accessor count off by one, a `min` a metre off the
  bytes, a dropped required `min`, an out-of-range index, a second parent, an
  unreachable subtree, a non-unit quaternion, a sampler length mismatch, an
  unknown enum. Twelve took twenty minutes and all twelve were refused.
* **For the read-back**, a `--sabotage KIND` flag that corrupts the data
  **after reading and before checking**, one kind per gate, and the runner
  asserts each goes red. Transpose the matrices; rotate the index arrays by
  one; conjugate the quaternions; shift the animation by a frame; read the
  unit scale as a plausible wrong constant; drop the axis-conversion node;
  move one element by one unit. Report which gates each one reddened — a
  sabotage that reddens the wrong gate means two gates are coupled.

Both floors go in the runner, so a later change that hollows out a gate is
caught by the gate's own floor rather than by nobody.

## 5. The vendor's own file is the control

Export one of the target application's SHIPPED assets through the same path and
gate it beside your fixture. It costs one command and it settles, permanently,
which of the two a red gate is about.

In this lane the assembled fixture failed R5 on exactly one bone at 2.865
units; the vanilla donor passed at 0.001. That turned "our export deforms the
character" into "these two Bethesda files disagree about where one toe is",
which is a sentence you can hand to the user. Without the control it would have
been a week of looking at the writer.

Register the fixture's known failures **by count** in the runner
(`verdict "G3 jog (2 registered)" 2 "$n"`) so they cannot rot into an unnoticed
pass, and make fixing the fixture require dropping the number back to zero.

## 6. Driving the third-party importer headless

* Run it with its factory settings (`blender --background --factory-startup
  --python check.py -- file.gltf`). The user's add-ons and preferences must not
  be able to change the answer, and `--background` opens no window.
* **Derive the expectation from the file, not from your notes.** Parse the
  output file's own JSON in the check script and compare the importer's meshes
  to it by NAME. A grand total is wrong: importers create data of their own
  (Blender adds an `Icosphere` as a bone display shape) and a total silently
  absorbs it.
* Expect the importer's units to differ from yours in ways that are correct:
  Blender reports an action's range in FRAMES at the scene's own fps, so a
  23-frame 30 fps clip reads `0.000..17.600` at 24 fps. Convert before you call
  it a failure.
* Deprecated accessors lie quietly. Blender 4.4+ moved fcurves into action
  layers and slots; `action.fcurves` still exists and returns a fraction of
  them (10 instead of 790). Count both ways and take the larger, and say in the
  script why.

## 7. Two writer defects this class of gate always finds

* **Empty arrays.** Most schemas forbid them (`EMPTY_ENTITY` in glTF). The
  fixtures never take the path — a scene with no mesh, an export with no
  animation — so only the validator on a synthetic case finds it. Build the
  writer to OMIT a section rather than emit it empty, and remember the trailing
  comma when you do.
* **Unsequenced side effects in an argument list.** `Vector3( r.f32(),
  r.f32(), r.f32() )` reverses on g++ and does not on MSVC. Grep every reader
  for two or more mutating calls inside one expression before trusting any of
  its numbers; it is a two-minute grep and it found three sites here.

## 8. What the contract page owes the gate

The page (`ww-contract-provenance` governs how it is written) must state, in
numbers the gate can read back: the unit and how it was measured; the axis
conversion and where it lives (a node, not baked into the data — that is what
lets the read-back compare against the source with no basis change); the
element order of every matrix; what is lost, named one by one; and every
constant the gate asserts. If the gate knows something the page does not, the
page is incomplete and the next lane will re-derive it.

## 9. Closing the ROUND TRIP (writer AND importer) -- see ROUNDTRIP.md

When the pair exists and someone asks whether an export/import is 1:1, read
`ROUNDTRIP.md` beside this file BEFORE measuring anything. It carries, from
lane BUILD8 (2026-09-10, FO4 .hkx <-> glTF 2.0): the trip must run through the
SHIPPING binary and "linked" is not "reachable"; one TSV currency and the six
comparisons that each accuse a different component; restricting a lossy leg by
NAME instead of loosening the bar; the two ways two correct contract pages fail
to compose; the renderer NOISE FLOOR that must be measured before any pixel
difference is attributed; counting colour differences PER CHANNEL (luminance
rounds a blue-only step of 1 to zero); and the third-party (Blender) leg, whose
scene frame rate re-times the clip and whose numbers are never quoted as ours.
