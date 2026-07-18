# Create Skin (bind to node) — headless test gauntlet

Automated end-to-end test for the Rigging spell "Create Skin (bind to node)"
and the donor pipeline running on its output. Found and now guards against
the 2026-07-18b corruption (see WW_CHANGES.md).

The scripts import `tools/rigging_prototype/nifparse.py`; the paths are baked
in, run them from anywhere with Python 3.

## Test asset

Any skinned FO4 mesh works as the seed; the gauntlet strips its skin at byte
level to make the unskinned target, then uses the untouched original as the
transfer donor — so a Nearest-Vertex transfer must reproduce the original
weights exactly.

```sh
cp <fo4 meshes>/X01_ArmLeft.nif work/donor.nif
python strip_skin.py work/donor.nif work/target.nif
python nifscan.py work/target.nif      # sanity: first shape unskinned
```

## Run (PowerShell)

The WW_CREATESKIN_TEST hook (nifskope_ui.cpp) drives the real spell through
NifSkope::castSpell, auto-answering its dialogs (QInputDialog accepted,
QMessageBox answered Yes/Ok, progress dialogs left alone), saves
`<input>_skinned.nif`, and quits. Log: `release/ww_createskin_test.log`.

```powershell
$env:WW_CREATESKIN_TEST='1'
# optional: bind to a non-parent node by name substring (strong variant —
# the mesh must not move for ANY choice)
$env:WW_TEST_PICKNODE='ForeArm1'
# optional: donor for the second stage; also saves <input>_transferred.nif
$env:WW_TEST_DONOR='...\work\donor.nif'
release\NifSkope.exe work\target.nif
```

## Verify

```sh
# Stage A: structure, attribute preservation, weight-1.0 records, triangles,
# and the must-not-move proof (boneWorld*skinToBone applied to every vertex
# must equal shapeWorld applied to it)
python verify_createskin.py A work/target.nif work/target_skinned.nif

# Stage B: transferred weights vs the donor's (Nearest Vertex on identical
# geometry). Expect ~2% outliers at most: coincident seam vertices tie-break
# onto position twins bound to a different bone — inherent to any
# position-based mapping, not a defect.
python verify_createskin.py B work/donor.nif work/target_transferred.nif
```

Green run (X01_ArmLeft, 961 verts): A = PASS with max deviation 0.000000
(parent bind) / 8e-6 (rotated non-parent bind); B = 940/961 exact, 21
coincident-twin ties.
