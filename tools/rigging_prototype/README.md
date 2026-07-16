# Rigging prototype — validated reference implementation

Verified Python + C++ prototypes for the Bone & Weight Transfer feature
(`../../BONE_WEIGHT_TRANSFER_PLAN.md`). Every algorithm here is checked against
vanilla FO4 data before being ported to NifSkope C++. Kept in-repo so the
validated foundation survives the scratchpad.

Test data: vanilla FO4 unpacked meshes under
`.../CharacterAssets/` (BaseFemale/MaleHead, Beards). Paths are hardcoded with
Windows separators (MSYS path-conversion only rewrites argv, not string literals).

## Files
- `nifparse.py` — FO4 NIF header/block-table walker (bsver 130). Exposes blocks,
  strings, sizes. Foundation for the rest.
- `nifshape.py` — loads a `BSSubIndexTriShape`: positions (half/full), triangles,
  per-vertex skin as `{boneName: weight}`, bone-name list. numpy.
- `transfer.py` — closest-point-on-triangle + barycentric weight transfer
  (`vertex` and `face` modes), top-4 prune + renormalize. **Validated:** self-
  transfer exact (meanL1 1e-4, 100% dominant); head→hairline ~83% vs vanilla
  hand-authored (legitimate divergence — see plan §7.1).
- `invbind.py` / `invcheck.py` — skin-to-bone (inverse-bind) transform math.
  **Validated:** `skinToBone_k = inv(boneWorld_k) @ meshBindWorld`, matrices
  as-stored, `world = parent@local`; reproduces vanilla `BSSkin::BoneData`
  transforms to ~1e-5 across female/male head, beard, hairline (plan §7.2).
- `encode_test.py` — `CustomizationRemapData` encoder. **Validated:** regenerates
  the vanilla blob **byte-for-byte** from a mesh's standard-skeleton skin.
- `compare.py` — mesh-skin vs RemapData comparison (research helper).
- `skincore.cpp` — C++ port of the core math (closest-point-on-triangle, 4x4
  multiply/inverse, inverse-bind). **Validated:** all tests pass vs Python
  (closest-point machine precision, inverse-bind 1e-6). Maps to NifSkope
  `Vector3`/`Matrix4` on integration.
- `skintransfer.cpp` — C++ end-to-end transfer reading `case.txt` (dumped by
  `dump_case.py`). **Validated:** reproduces the Python head→hairline transfer
  **exactly** (meanL1 0.00000, 100% dominant match, 676 verts).

## Build/run (C++, from PowerShell)
The UCRT64 g++ must run inside its shell environment:
```
C:\msys64\usr\bin\bash.exe -lc 'export PATH=/ucrt64/bin:$PATH; cd <dir> && \
  g++ -O2 -std=c++17 skincore.cpp -o skincore.exe && ./skincore.exe'
```
`skintransfer.cpp` needs `case.txt` first: `python dump_case.py` (not copied here;
regenerate from scratchpad or re-derive from nifshape+transfer).

## Status
All risky skin math (weights, bone transforms, RemapData) is de-risked in Python
**and** the core is ported+verified in C++. Remaining = NifModel read/write, block
assembly, skin-partition regen, and the Rigging workspace UI — done in the NifSkope
Qt build. See the plan's §8 phasing.
