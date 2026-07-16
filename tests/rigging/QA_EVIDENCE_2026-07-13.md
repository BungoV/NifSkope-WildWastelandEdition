# Rigging release QA evidence — 2026-07-13

All runs used the native NifSkope spell and test executable. Blender and other DCC tools
were not involved. External Fallout 4 assets were read-only; generated NIFs were written
only under `tests/rigging/out/`.

## Automated fixture suite

Release build: PASS.

```text
PASS blocks=76 nodes=70 bones=69 remap=20268 stepsUndo=4 combinedUndo=1
multishape=clean topology=3229 vertices=1690 standardOnly=clean donorOverlay=clean
weightHeatmap=clean weightPaint=clean paintModes=clean cancel=clean
reject=clean progressCancel=clean summary=clean distanceGuard=clean
topologyGuard=clean rollback=clean workspace=Rigging workspaceFlow=clean
fixtures=immutable meanL1=1.28738e-06 maxL1=0.00038147
combinedMeanL1=1.28738e-06 combinedMaxL1=0.00038147
```

## Passing production assets

| Target | Donor | Snap | Bones | Remap | Mean / max L1 | Dominant | Validation |
|---|---|---:|---:|---:|---:|---:|---:|
| BigBeard01_Hairline | BaseFemaleHead_faceBones | Caution | 8→69 | 8,112 | 0.343375 / 1.88278 | 83.58% | clean |
| BaseMaleHead | BaseMaleHead_faceBones | Close | 10→69 | 20,352 | 2.25823e-06 / 0.000274658 | 99.88% | clean |
| MaleBody | MaleBody | Close | 58→58 | 18,276 | 1.70321e-07 / 6.10352e-05 | 100% | 0→0 |
| F_Torso_Lite | F_Torso_Mid | Caution | 8→8 | 384 | 0.0439682 / 0.116577 | 100% | 0→0 |

Every passing case completed normalized-weight checks, `Validate FO4 Skin`, save/reload,
and byte-exact Undo/Redo. The standard-only fixture and MaleBody case specifically prove
that RemapData is regenerated from the final packed float16 skin; face-bone cases prove
that the original standard skin is captured before sculpt-weight transfer.

## Expected preflight rejections

| Target | Donor | Geometry evidence | Binding evidence | Result |
|---|---|---|---|---|
| Vault111 suit (first person) | FemaleBody | median snap 73.9%, Poor | incompatible `RArm_UpperFat_skin` rest pose | rejected before mutation |
| FemaleBody | FemaleGhoulBody | median 24.6%, p95 85%, Poor | incompatible `Chest_skin` rest pose | rejected before mutation |
| OldMaleBody | MaleBody | median 0.254%, p95 0.679%, max 0.908%, Close | incompatible `Chest_skin` rest pose | rejected before mutation |

These are safety-guard passes: geometry quality alone does not override an incompatible
shared-bone rest pose.

## Remaining manual check

Load generated copies in Fallout 4 and verify visual deformation plus FaceGen
morph/customization behavior. This cannot be established by the native structural suite.

The audited binary distribution is archived as
`dist/NifSkope-WW-Rigging-2026-07-13.zip`. It contains the Release application and runtime,
this evidence file, the implementation plan, and the change log; the test-only integration
executable is intentionally excluded.

The later visualization increment (read-only donor overlay and selected-bone heatmap) is
archived separately as `dist/NifSkope-WW-Rigging-Visuals-2026-07-13.zip`; its native suite
adds `donorOverlay=clean weightHeatmap=clean` to the result above.

The manual-paint increment adds a target-only cyan viewport brush and Add/Subtract/Replace/
Smooth operations with Weight, Strength, and Radius controls. The native suite applies
three distinct strokes, verifies normalized four-slot FO4 records and one Undo entry per
stroke, then restores the starting model byte-for-byte. Its release archive is
`dist/NifSkope-WW-Rigging-WeightPaint-2026-07-13.zip`.

The paint-mode selector increment verifies the exact Object/Edit/Vertex Paint/Weight
Paint/Segment Paint ordering, confirms that the future Vertex Paint and Segment Paint
entries are visible but disabled, and checks Weight Paint activation plus Object Mode
exit through the real application window. Its release archive is
`dist/NifSkope-WW-Rigging-PaintModes-2026-07-13.zip`.
