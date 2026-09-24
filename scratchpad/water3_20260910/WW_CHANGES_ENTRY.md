Text for `WW_CHANGES.md` (the director splices it — that file is MIXED and its
2026-09 entries at the top are LF-only; assert the CR count is unchanged,
19,020 as of 2026-09-09).

---

### 2026-09-10a — marking water direction by hand (lane WATER3) — BUILD PENDING

bungo, 2026-09-09: *"in nifskope, have the player mark the water direction in a
smart way"*, *"lakes have no flow if they're not connected to rivers, then
rivers end up at sea"*, *"different water colors for different bodies of
water"* — *"or at least an ID for them"*.

**A Water Marking dock** (`src/watermarkpanel.cpp`, in the Workspaces dropdown)
over a new model (`src/watermark.cpp`). The worldspace is drawn from above, one
stored plane at a time — Body ID as a hashed hue, Flow as direction-hue, Shore
as a ramp — and a drag along a river is a CONSTRAINT on its direction. Strokes,
pins, a source/outlet pin pair that means "the path between these", a still-water
mark for a lake nothing feeds, per-body class, water form, colour override and
name.

**The strokes are the SOURCE and the planes are DERIVED.** The store is written
in WORLD coordinates (spec §3.7), so a stroke survives a re-derivation at
another sample rate; nothing in the tool edits a plane. `save()` rewrites the
file from the body table onward: the table, the name blob and the stroke store
re-encoded, the flow plane re-derived tile by tile, and the body-ID and shore
planes copied through VERBATIM with their absolute offsets rebased — because
nothing a marking tool does can move a body's shape. The original is renamed
`.bak-watermark`, which is the way back.

**Measured before anything was compiled**
(`scratchpad/water3_20260910/repro_flow.py`, through the independent decoder):
the flow plane really is a pure function of the body-ID plane and the body
table — **0 mismatches on 37,748,736 texels**, floor 25,114 (turning body 3 by
40 direction steps makes the same comparison report exactly the Charles's own
texel count). That is what makes "undo is byte-identical to the file you
started from" a property and not a hope.

**The spec's pre-registered gate numbers did not survive their audit**
(`ww-spec-gate-audit`, `scratchpad/water3_20260910/audit_spec.py`).
`spec_water.md` marks "body 233, the Charles, 25,112 texels" and refutes on
"body 136": under the rule lane WATER2 shipped, ids are assigned by descending
area over 346 bodies, so the Charles is **body 3** (25,114 texels, same cells)
and the marsh is **body 2** (29,312). The harness does not hard-code either — it
picks the largest river and the largest other non-sea body, and prints which.

**`spec_water.md` is current**, 557 → 735 lines: 346 bodies everywhere, the
SIZE-guarded merge clause, §3.8's stride refusal reversed (it had the
forward-compatibility rule backwards), §7's G5 and G6 restated with the numbers
that hold, §5 rewritten as built, and a provenance footer rebuilt from scratch
whose 38 line numbers were re-derived from their anchors (38 moved, 0 missing,
0 ambiguous).

**Divergence from the spec and from Blender, both stated.** The spec said the
3-D water plane would be the canvas; it is a top-down map in the dock, because
`src/glview.cpp` was another lane's file and because a river reach eleven cells
long is a fact about the map. Blender's grease pencil is the reference for the
gestures; the eraser removes a WHOLE stroke (a stroke is one constraint), there
is no tablet pressure (the Width row is the only source, so a stroke is
reproducible from the file), and pan/zoom are Blender's.

**Confidence is not the spec's second harmonic fill.** That converges to 15
everywhere — a harmonic function with one Dirichlet value and no other boundary
condition is that constant. It is a geodesic distance inside the body's mask,
halving every stroke width, which decays with distance and fades where a body
widens, which is what the spec's own sentence asked for.

**NOT BUILT, NOT RUN, NOTHING PROVEN.** `Fallout4.exe` was up at the gate check.
Four new files (`src/watermark.{h,cpp}`, `src/watermarkpanel.{h,cpp}`, 3,533
lines) pass `g++ -fsyntax-only` with the real `Makefile.Release` flags and
nothing else. `NifSkope.pro` carries them; the three hook-ups to
`src/nifskope.cpp` (one include, one call) and `src/nifcli.cpp` (the
`--water-mark-selftest` verb) are written as a refusing patch script and NOT
applied. Harness `tests/spells/water_mark.sh` (model half floor 14 checks, dock
half floor 16) has never executed. Resume:
`scratchpad/water3_20260910/PENDING.md`.

**Owed, and named:** the before/after flow picture of the Charles (the "before"
already exists at the exact framing); gate P4's angle comparison; Barrier and
Merge strokes, which need the classifier re-run and so belong to the writer;
the writer reading the stroke store, without which a full re-bake from the ESM
discards a user's marks; and retiring the plane packer's TWIN into shared code.
