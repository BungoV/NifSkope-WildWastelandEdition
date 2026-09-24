# -*- coding: utf-8 -*-
import sys
P = '../specs_20260909/spec_water.md'
t = open(P, 'rb').read().decode('utf-8')
edits = []
def rep(old, new): edits.append((old, new))

rep("""The `.lodl` viewer already meshes a scene from the file and its plane picker
already offers `waterheight` and `watertype`
(`tests/spells/lodl_open_authority.py: plane_keys`). Two plane keys are added,
`bodyid` and `flow`, and the **water plane itself becomes the canvas**: the
tool draws on the meshed water surface, not on a 2D map, so a stroke lands on
the body the user is looking at.

Body ID renders as a categorical colour (the ID hashed to a hue), which is also
the picture that answers bungo's *"or at least an ID for them"* on sight.""",
"""The `.lodl` viewer already meshes a scene from the file and its plane picker
now offers `bodyid`, `flow` and `shore` beside `waterheight` and `watertype`
(lane WATER2, `src/btdterrain.cpp`).

**DIVERGENCE, AS BUILT.** This page said *"the water plane itself becomes the
canvas"* — the tool drawing on the meshed 3-D water surface. What shipped draws
on a **TOP-DOWN MAP inside the dock**, and the 3-D viewer only SHOWS the result
by reading the planes back. Two reasons, the first of them the honest one:

* the 3-D viewport is `src/glview.cpp`, which another lane held open while this
  was written, and lane WATER3's brief forbade touching it;
* it is the better canvas anyway. A river reach is eleven cells long, its
  direction is a fact about the MAP, and orbiting a terrain mesh to draw a line
  down it is precisely the gesture Blender itself replaces with a 2-D editor
  (the UV and Image editors) whenever the thing being edited is flat.

Marking in the 3-D viewport is therefore still OPEN, and it is a `glview.cpp`
lane, not a redesign: the model (`src/watermark.{h,cpp}`) takes world
coordinates and knows nothing about either canvas.

Body ID renders as a categorical colour (the ID hashed to a hue, so that two
touching bodies cannot come out the same colour), which is also the picture that
answers bungo's *"or at least an ID for them"* on sight.""")

rep("""| Blender GP | here | divergence |
|---|---|---|
| Draw: click-drag lays a stroke | **Stroke**: a polyline on the water plane; its tangent is the flow direction | none |
| a stroke's pressure sets thickness | the **Width** row sets the influence radius in world units | no tablet pressure: the panel's field is the only source, so the value is reproducible |
| Eraser | **Erase**: removes strokes under the cursor | none |
| Line/Arc tools | **Pin**: click, drag out an arrow = one point + one direction | Blender has no single-sample constraint; this is ours |
| Cutter (knife) | **Barrier**: a stroke across a body splits it in two | ours; the split is stored, not the result |
| Join | **Merge**: a stroke between two bodies joins them into one | ours |
| strokes live in a GP object | strokes live in the `.lodl`'s stroke store, in world coordinates | the file is the document |

A stroke is drawn on ONE body: the body under the first point. Points that
leave that body are kept (the store is what the user drew) and IGNORED by the
solve (the mask is the domain). The panel says so in words when it happens —
*"3 of 41 points fell outside body 233 and were ignored"* — rather than
silently trimming.""",
"""| Blender GP | here | built? | divergence |
|---|---|---|---|
| Draw: click-drag lays a stroke | **Stroke** (kind 0): a polyline on the map; its tangent is the flow direction | yes | drawn on the top-down map, not the 3-D surface (§5.1) |
| a stroke's pressure sets thickness | the **Width** row sets the influence radius in world units | yes | no tablet pressure: the panel's field is the only source, so the value is reproducible from the file alone |
| Eraser | **Erase**: click removes the stroke under the cursor | yes | Blender's eraser has a radius and rubs part of a stroke away; a stroke here is ONE constraint and half a constraint is not a smaller one |
| Line/Arc tools | **Pin** (kind 1): click, drag out an arrow = one point + one direction | yes | Blender has no single-sample constraint; this is ours |
| — | **Source pin** (kind 4) and **Outlet pin** (kind 5): the head and the mouth; the PAIR is the path between them | yes | ours, and it is bungo's *"a source pin + outlet pin = a path"*. The straight segment is the constraint; the harmonic fill bends it to the banks |
| — | **Still water** (kind 6): this body has no flow | yes | ours, and it is bungo's *"lakes have no flow if they're not connected to rivers"*. It is a MARK in the store, not a bit in the table, so it survives a re-derivation |
| Cutter (knife) | **Barrier** (kind 2): a stroke across a body splits it in two | **no** | the split needs the classifier re-run, which needs the plugin and the heightfield, not just the `.lodl`. The kind is reserved and the store carries it; nothing acts on it yet |
| Join | **Merge** (kind 3): a stroke between two bodies joins them | **no** | same reason |
| strokes live in a GP object | strokes live in the `.lodl`'s stroke store, in world coordinates | yes | the file is the document |
| pan and zoom | middle-drag pans, the wheel zooms about the cursor | yes | none — and the wheel over a NUMBER field still does nothing unless the field has focus, which is also Blender's rule |

A stroke is drawn on ONE body: the body under the first point. Points that
leave that body are kept (the store is what the user drew) and IGNORED by the
solve (the mask is the domain). The panel says so in words when it happens —
*"3 of 41 points fell outside body 3 and will be ignored by the solve; the
stroke is stored as drawn"* — rather than silently trimming. A stroke whose
FIRST point is dry is refused outright, in words, and stored nowhere: a
constraint that names no body constrains nothing, and storing it would leave the
next reader to work out why the planes did not move.""")

for old, new in edits:
    n = t.count(old)
    if n != 1:
        sys.stderr.write('REFUSED: %d matches for %r...\n' % (n, old[:70])); sys.exit(1)
    t = t.replace(old, new)
open(P, 'wb').write(t.encode('utf-8'))
nb = open(P, 'rb').read()
print('ok: %d edits, %d bytes, %d lines, CR=%d' % (len(edits), len(nb), nb.count(b'\n'), nb.count(b'\r')))
