# Lane PANEL1, 2026-09-12 — mistakes to add to the root MISTAKES.md (newest first)

## 2026-09-12 — A picture is a gate: three panel screenshots, none of which showed the change

Lane PANEL1 shipped `panel_before_rung.png`, `panel_after.png` and
`panel_min.png` as the evidence that fifty-seven new rows had landed in the LOD
Generation panel. All three show the same thing: Source down to Native object
files, the top of the settings. Every new row is below the frame.

The mechanism: the harness grabs `dock->grab()`, which renders the dock as laid
out -- and the settings live in a `QScrollArea`, so the picture is the VIEWPORT,
about 700 px of a column that is 2,897 px tall with the sections open. The
arrangement call even centres on the object head with a 300 px margin, which
puts the new rows just under the bottom edge.

Two tells were in the deliverable and neither was read: `panel_min.png` was
BYTE-IDENTICAL to `panel_after.png` (reported as a finding about the dock's
minimum width, which it also is -- but two pictures of a change being identical
is first a statement about the instrument), and the before/after pair differed
by 48 bytes of PNG for a change that added 34 number fields.

**The rule**: a picture is a gate, and a gate is read before it is shipped. Look
at the image, name the thing in it that the change made, and if you cannot,
the picture is not evidence yet. For any scrolling panel, grab the scroll area's
INNER widget (`findChild<QScrollArea*>(...)->widget()->grab()`), open every
folding section for the grab and fold it back afterwards -- the fold state is the
operator's and it persists in QSettings, so click it open and click it shut.

## 2026-09-12 — "At its default" has to be forced, not assumed: a harness that inherits QSettings measures the machine

Lane PANEL1's byte gate bakes one chunk from the panel with every new row at its
default, then bakes the same chunk from the command line and compares file by
file. Fourteen of fifteen files matched. The terrain mesh did not: 56,916 B from
the panel, 46,518 B from the command line.

The first suspect was the known `Simplification error` disagreement (the panel
opens on 32, the bake's own default is 128). Measured: `--simplify-error 32`
changes that file not at all on this chunk. Wrong suspect.

The real one: `Water subdivision`. Code default 3; this machine had 4 saved in
`HKCU\SOFTWARE\NifTools\NifSkope 2.0\LodGeneration\waterSubdiv` from earlier use
of the panel. The harness let the panel load it. `--water-subdiv 4` on the
command line makes the two trees byte-identical, all fifteen files — so the
"disagreement" was one number this machine remembered.

**The rule** (already house feedback, broken anyway): a GUI harness FORCES the
state it measures. Saving the operator's settings group and restoring it at the
end — which this gate did — protects his settings and does nothing whatever for
the measurement. Write each row's default into the widget before the baseline
bake, and LOG the rows whose saved value differed: that line is also the answer
to "why does the panel disagree with the command line".

**The shape to expect**: a panel-versus-command-line comparison that differs on
exactly one file, on a machine where somebody has used the panel before. Ask
what that machine has saved before you go looking in the generator.

## 2026-09-12 — A per-item gate that bumps one setting at a time needs each item's dependency

The per-row leg of lane PANEL1's byte gate moved one new row at a time off its
default, re-baked the same chunk, and compared the tree digest with the baseline
bake. It reported that 37 of 54 rows "reach nothing".

Most of that was the instrument. Erosion rounds cannot move a bake whose erosion
strength is 0. The land guide's scale, strength and slope cannot move a bake with
the guide off. The five water numbers cannot move a landscape file with water
bodies unticked. The gate had bumped each dependent row while its parent sat at
its default, so the answer was decided before the bake started.

**The rule, in two halves.**

*Dependencies*: a per-item gate carries, per item, the item it hangs off. Turn
THAT on, bake a LOCAL baseline, then move the item and bake again. Two bakes per
dependent item, and the answer means something.

*Unreachable items*: where the run genuinely cannot reach an item — no road on
this chunk, no card library armed, the module off in this module set, the row
hidden under this render target — the item carries a written reason in plain
language and is printed as a NAMED SKIP, with the spell that DOES read it. Never
a silent pass, never counted as a pass, never folded into a failure count either.

**The shape of the failure to expect**: a gate that condemns most of what it
measures. That is the shape of a broken reader, not of broken items — 56 of 57
one hour, 37 of 54 the next, both in this lane, both the instrument. Prove the
gate on ONE item by hand before believing it about fifty.

**And "on" is not always enough.** The fixed gate's own second run still
condemned two rows, and the command line proved it wrong a third time: stepping
a selector one place to turn the land guide "on" lands on `drag`, which does not
read the slope reference at all, and bumping a drainage RADIUS from 64 to 65
texels asks whether two bodies sit exactly 65 texels apart. The table therefore
carries, per item, the VALUE the dependency must take and — for a setting that
is a threshold rather than a dial — the value this item is moved to instead of
one step. Ask whether each setting is a dial or a threshold, and which MODE of
its parent reads it, before reading anything into a bake that did not move.

## 2026-09-12 — A check that measured a HIDDEN widget passed for four months by accident

`WW_LODGEN_TEST`'s "an unticked box can be seen (24+ levels against the ground)"
read `LodgenAtlasCheck`. Under the FO4 Community Shaders target that box is
HIDDEN, and the same log said so three lines above: `FO4CS: the object atlas
hidden: yes`. A hidden widget keeps its last geometry, so the grab was of
whatever happened to be painted at those coordinates. With four rows above it
that was another row's box: 32 levels, green. Lane PANEL1 put fifty-seven rows
above it, the coordinates landed on empty ground, and the check read 0 and went
red — on a change that touched nothing about check-box contrast.

**The rule**: a check that grabs pixels at a widget's coordinates must first
assert the widget is VISIBLE, and must name in the log which widget it measured.
`isHidden()` is not the test either, because a widget inside a folded section is
not `isHidden()`; `isVisible()` is. The check now walks the panel for the first
visible, unticked box inside the settings scroll and logs its object name, so
the next reader can see what was measured rather than trusting a number.

**The shape of the failure to expect**: a pixel check that goes red on a change
that could not possibly affect it. Look for a hidden or moved widget before
looking at the palette.

## 2026-09-12 — setProperty is a no-op when the value does not move

The harness hook that asks the panel to save its settings is a dynamic property:
`panel->setProperty( "wwSaveSettings", true )`, answered by an `event()` override
watching for `QEvent::DynamicPropertyChange`. It fired for the first row and for
none of the other fifty-six, and the gate reported "56 of 57 rows do not
round-trip through QSettings" — a clean, plausible, completely false result. The
rows were fine; the hook was deaf.

`QObject::setProperty` on a dynamic property whose stored value EQUALS the new
one returns early, without sending the event. The second `true` is not a change.

**The rule**: a property used as a doorbell carries a value that MOVES —
`setProperty( "wwSaveSettings", ++tick )`. And when a gate reports that nearly
every item fails, suspect the instrument before the items: 56 of 57 is the shape
of a broken reader, not of 56 broken rows.
