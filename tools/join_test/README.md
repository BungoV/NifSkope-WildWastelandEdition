# Rigging-aware Join (Ctrl+J) — headless test

End-to-end test for object-mode **Join** (`GLView::joinSelectedObjects`) on FO4
skinned, segmented `BSSubIndexTriShape` meshes. Guards the rig merge: bone lists
unioned, per-vertex Bone Indices remapped, segments concatenated, and the
superset color fill.

## Test asset

Any FO4 mesh with **two or more `BSSubIndexTriShape`s** that share a structural
vertex layout works (they may differ in vertex colors — that exercises the
superset fill). `x01tesla_torso.nif` has both a group of colourless shapes and
one coloured shape.

## Run (PowerShell)

The `WW_JOIN_TEST` hook (nifskope_ui.cpp) builds the scene, object-selects the
active plus every compatible `BSSubIndexTriShape`, runs the real
`joinSelectedObjects()`, verifies invariants on the live model, optionally saves,
and quits. Log: `release/ww_join_test.log`.

- `WW_JOIN_TEST=1` — active = biggest shape (Phase A: same-format merge).
- `WW_JOIN_TEST=2` — active = richest vertex format (exercises the color fill:
  colourless sources merged into a coloured active must come out opaque white).

```powershell
$env:WW_JOIN_TEST='2'
$env:WW_TEST_SAVE='E:\path\to\work\torso_joined.nif'
release\NifSkope.exe E:\path\to\work\torso.nif
```

Headless `grabFramebuffer` is slow to init and the app hangs on quit afterward
(a GL-teardown nuisance, not a failure) — poll the log for `done`, then kill.

The log ends in `PASS` when: merged `Num Vertices`/`Num Triangles` == the sums of
the participants; the skin bone list == the union of their bone NiNodes with
`Num Bones` consistent across Instance/Bones/BoneData; **every per-vertex Bone
Index < merged Num Bones**; segments cover `[0, Num Triangles)` contiguously with
Σ Num Primitives == triangles; and (mode 2) the appended verts are opaque white.

## Verify (byte level)

```sh
python verify_join.py work/torso_joined.nif [expectedVerts] [expectedTris]
```

Reparses the saved file and re-checks every `BSSubIndexTriShape`: skin counts
consistent, all bone indices in range, segments cover all triangles with no
orphaned Segment Data, and `Data Size == verts*stride + tris*6`.
