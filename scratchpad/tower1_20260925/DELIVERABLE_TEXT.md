## HANDOFF text
TOWER1 (2026-09-25 22:26-23:10, branch tower1-20260925, commits 0aa2465b + e872a753, not merged): the grey
downtown Boston towers are a real bake defect, not a tint. Our lodgen drops the material swaps that vanilla bakes
into its LOD atlas. A REFR's XMSP (or the base's MODS) points at an MSWP whose rows name LOD materials, e.g.
hittechextalod01 -> hittechextalod07 (cream). We place every building with its base's own LOD .bgsm.
Measured base-colour luma, vanilla vs ours, at the 08 camera: tower A 0.458/0.185, B 0.367/0.193, and the control
C 0.123/0.133. Offline, applying vanilla's swap to our models lands on vanilla to the third decimal: A 0.378 vs
0.377, B 0.380 vs 0.379. Whole map: 21,064 of 184,069 placements carry a swap that names their LOD material
(Fallout4.esm refs only), across 41 distinct pairs. The top pair is decomainlod -> decomainblod at 8,062.
Picture: scratchpad/tower1_20260925/pics/TOWER1_vanilla_vs_ours.png, untracked.
Proposed fix, not built: parse XMSP, MODS and MSWP in esmdata. Key the library by (base, swap) and substitute
BNAM -> SNAM on the LOD shapes. Add a bake census line. The refuter is in DONE.md section 4.
GREY1's per-placement multiplier is not the fix for these towers.

## WW_CHANGES text
2026-09-25 TOWER1 (measurement only, no code): found that far-LOD buildings lose vanilla's material swaps (MSWP
via REFR XMSP / base MODS). Those swaps are why downtown towers render darker blue-grey instead of vanilla's cream
and orange. 21,064 Commonwealth placements are affected. Fix proposed in scratchpad/tower1_20260925/DONE.md s4.

## MISTAKES text
2026-09-25 23:1x TOWER1 (for GREY1's record): GREY1 concluded that "the swap never reaches the LOD; that is
vanilla's own trait". It had compared ours against our OWN per-model LOD textures, never against vanilla's CK atlas.
Vanilla's atlas holds the swapped colourways, and applying the swap reproduces vanilla to the third decimal.
Rule: a "vanilla does it too" verdict needs vanilla's shipped output as the reference, not our inputs.
2026-09-25 23:0x TOWER1: I assumed a stock .bto is drawn chunk-local like a .btr and framed it with the /4
camera. 14 of 18 BTO pictures came back blank (the same 11,411 B file every time). NifSkope draws a .BTO in
world space. Rule: print every render's byte size, and prove the frame with an auto-fit .cam before deriving a
camera.
