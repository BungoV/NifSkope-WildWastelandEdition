# Rigging integration test

This native Qt integration test exercises the complete Fallout 4 Rigging workflow
against the small fixtures in `fixtures/`. It covers the four advanced operations,
the atomic one-dialog operation, Undo/Redo, save/reload, byte-exact rollback,
cancellation (including the progress dialog during geometry mapping), a multi-shape
donor, mismatched target/donor vertex counts, the atomic pre-transfer summary, and
the real Rigging workspace dock. The workspace test loads a read-only cyan donor geometry
overlay, adjusts its hide/show lifetime, and enables a selected-bone weight heatmap; both
must leave serialized target bytes and Undo state unchanged and clear on a real edit.
It then activates the real manual-paint controls, verifies brush radius propagation, and
drives Add, Subtract, and Replace strokes through the same viewport signals used by mouse
painting. Each stroke must add exactly one Undo entry, keep every FO4 vertex normalized
with valid bone indices, refresh the heatmap, and restore the complete model byte-for-byte
after all three Undo operations. The workspace must reacquire its target after that
whole-model restore before the primary transfer test continues (`weightPaint=clean`).
Summary assertions cover both the baseline and
varied fixtures, including geometry counts, new binding counts, donor-shape choice,
and the expected remap-data size. A generated half-precision fixture displaces every
target vertex to verify that the geometry diagnostic reports `Poor`; rejecting that
default-Cancel confirmation must leave the model byte-for-byte unchanged with no
Undo entry. The native-window portion also clicks the primary Rigging workspace
button, checks the manager's post-transfer 69-bone refresh and single application
Undo entry, then undoes back to the original 10-bone model. A generated donor with an
out-of-range triangle index verifies topology rejection before mutation. GUI save
dialogs are always rejected, close prompts choose Discard, and the runner byte-compares
both versioned fixtures at shutdown so the suite cannot silently overwrite its inputs.

From PowerShell at the repository root:

```powershell
.\tests\rigging\run.ps1 -Build
```

Subsequent runs can reuse the executable:

```powershell
.\tests\rigging\run.ps1
```

The dedicated `RiggingIntegration.pro` project uses isolated objects under
`GeneratedFiles/RiggingIntegration` and writes logs and derived NIFs under
`tests/rigging/out/`. The test requires a native graphical session because the
workspace smoke check creates NifSkope's OpenGL window; Qt's `minimal` platform is
not sufficient for that check.

The native-window smoke test also checks the viewport selector's exact mode order,
requires Vertex Paint and Segment Paint to remain disabled placeholders, and exercises
Weight Paint entry and Object Mode exit through the selector (`paintModes=clean`).

## Real-asset mode

The same executable can validate an external standard-skinned target with either a face-bone
or standard-skeleton donor against a vertex-compatible vanilla reference. It runs the actual
atomic spell, checks normalized weights and remap size, compares post-transfer validation
with the original target's baseline, validates again after reload, proves byte-exact
Undo/Redo, and reports L1 error plus dominant-bone agreement:

```powershell
.\tests\rigging\run.ps1 `
  -RealTarget 'E:\path\StandardSkinnedTarget.nif' `
  -RealDonor 'E:\path\FaceBoneDonor.nif' `
  -RealReference 'E:\path\VanillaTarget_faceBones.nif' `
  -RealOutput 'E:\path\GeneratedTarget_faceBones.nif'
```

`-RealOutput` is optional and defaults to `tests/rigging/out/real_result.nif`. The target,
donor, and reference are read-only; all generated data goes to the output path. The mode
requires mean L1 ≤ 0.5 and at least 75% dominant-bone agreement with the reference.

Recorded vanilla results:

- female head → BigBeard01 hairline: 676 vertices, `Caution`, mean L1 0.343375,
  83.58% dominant agreement;
- male head self-surface: 1,696 vertices, `Close`, mean L1 2.25823e-06,
  99.88% dominant agreement;
- male body standard-only self-surface: 1,523 vertices, `Close`, mean L1 1.70321e-07,
  max L1 6.10352e-05, 100% dominant agreement, validation 0→0;
- female Combat Armor Mid torso → Lite torso: 32 target vertices, `Caution`, mean L1
  0.0439682, max L1 0.116577, 100% dominant agreement, validation 0→0.

The standard-only case is also covered by the versioned fixture suite as
`standardOnly=clean`; it verifies one-step Undo and that RemapData matches the exact packed
post-transfer skin. Incompatible production candidates are expected to stop safely: tested
Vault111/FemaleBody and FemaleBody/FemaleGhoulBody pairs classified `Poor`, while
OldMaleBody/MaleBody was geometrically `Close` but rejected for an incompatible shared-bone
rest pose. Each rejection occurred before mutation.

The complete dated matrix, including exact suite output and rejected-candidate metrics, is
archived in [`QA_EVIDENCE_2026-07-13.md`](QA_EVIDENCE_2026-07-13.md).

## Manual release QA

The versioned fixtures exercise the motivating FO4 head/face-bone structure and many
generated failure/topology variants, but they are not a substitute for broader asset
coverage. Before a production release, run the primary workspace action on copies of:

- a hair or beard with a genuinely different surface from its donor head (automated
  BigBeard01 hairline coverage is complete; visual/in-game inspection remains);
- a headgear/outfit mesh already skinned to the standard skeleton (automated female Combat
  Armor Mid→Lite coverage is complete; visual/in-game inspection remains);
- a non-head skinned mesh with different vertex and triangle counts.

For each pair, record the Close/Caution/Poor snap classification, inspect the transferred
weights and bone bounds, exercise Undo/Redo, save and reopen the result, run **Validate
FO4 Skin**, and verify the asset in Fallout 4. Entirely unskinned targets are outside the
current Phase 3A backend and should remain disabled rather than being used as QA inputs.
