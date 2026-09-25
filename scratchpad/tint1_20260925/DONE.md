DONE -- lane TINT1, 2026-09-25 (clock read 21:04). Branch tint1-20260925 (worktree E:\Projects\NifskopeWWE-tint1), cut from main d5764fbe. Not pushed, not merged.

The v5 object library, which keeps vertex colour, is baked and installed. It is proven to carry the colour: it draws in the viewer, and the before/after pictures differ.

The census answers bungo's question differently from how it was put. Almost no building LOD model carries a HUE in its vertex colour. The hue sits on trees (blasted maples). Buildings carry white, or a grey shade at most. So the grey buildings he sees are NOT fixed by keeping vertex colour. See section 5.

## 1. Skills loaded

**Loaded:**
- ww-whole-map-lod-bake
- nifskope-ww-lodgen
- ww-lodo-version-bump
- nifskope-ww-render-shot
- ww-whole-map-picture
- fo4-nif-vertex-channel-census
- mo2-mod-content-census
- nifskope-ww-worktree-build
- search-lean

**Written this lane:**
- nifskope-ww-render-shot: "A lit native picture hides vertex colour unless something forces it".
- nifskope-ww-worktree-build: section 8, "A freshly linked exe can be exec-blocked for minutes".

## 2. What was built

### Census
Tools: census.py, g2p_census.py, locate.py, matnames.py, g2p_bgsm.py, bto_vc.py. Every model is read in place through his MO2 stack (BAKE1's resources.txt), over the installed library's 184,431 placements.

| Group | Shapes | Meshes | Placements (slot 0) | Share |
|---|---|---|---|---|
| Colour channel + Vertex_Colors flag | 172 | 166 | 30,046 | 16.3% |
| of which real HUE (R, G, B not all equal) | 14 | | 6,539 | 3.5% |
| of which grey shade only | 32 | | ~3,586 | |
| of which white only (mostly BNS trees, alpha = wind) | 126 | | | |

Top models:
- TreeBlasted02_LOD_1: 4,055 placements, hue.
- Buildings with hue are a handful only: BathHouse wings (10 placements), the Amphitheater, CovWallExLrg01 and VltGearDoorRoomExt02 (1 each).
- The full ranked list is in census_models.tsv.

Where the building tint is NOT:

| Place checked | Result | Control |
|---|---|---|
| NIF palette flag (SLSF1 bit 4) | 0 of 3,125 shapes | |
| bGrayscaleToPaletteColor on placed-LOD .bgsm | 0 of 2,848 | 282 of 6,616 vanilla BGSMs have it |
| Colour channel in the shipped stock .bto (vanilla and BNS) | none | |

Building LOD materials are shared atlases (decomainlod, hittechextalod01, sidingkitalod ...).

### Safety gate: the FO4CS reader (read-only, wt-debris1\src\ImprovedLOD)

- ImprovedLODLodo.h:39 `inline constexpr std::uint32_t kLodoVersion = 4;`
- ImprovedLODLodo.h:224-225 `if (version != kLodoVersion) return refuse(std::format("version {}; this reader knows {}", version, kLodoVersion));`
- ImprovedLODLoad.h:175-176 pushes the note. ImprovedLODRuntime.cpp:182-183 logs `[ImprovedLOD] {}: {}`.
- The module defaults to enabled=false.

Verdict: it refuses cleanly. Install was allowed. FO4CS was not edited.

### Build
- Main plus SEAM1's v5 writer: run copy b72cef2a.
- Viewer fix 61d920ab, "give the vertex row a colour field for a .lodo v5 library colour", in src/lodinative.cpp:
  - Before the fix, the library colour was written into a vertex layout with no colour field, so it went nowhere.
  - The fix exe is 880f5056.

### Re-bake
- Object library only (not the VT, not the .lodl), with BAKE1's object settings, into the lane scratchpad.
- Ran 18:59:35 to 20:03:54, rc 0.
- The new .lodo is v5, 6,933,236 B, with 52,925 colour rows.

### Install (20:13:47 to 20:13:50)
- 17 files into mods\FO4CSLOD\FO4CSLOD\Commonwealth: .lodo, .lodi and the 15 legacy _n card arrays.
- Backups are in replaced/. The sha1 before, after and expected for each file are in install_record.tsv; all match.
- The .lodb was KEPT as BAKE1's, because an objects-only record would drop its VT line.
- VT and .lodl sha1 are unchanged. The folder still holds 3,677 files.
- native-verify rc 0: v5, cardCount 79, 52,925 colour rows, 166 colour meshes, 184,431 instances.

## 3. Gates

### (a) G1: v5 with no colour equals v4 apart from the version word
RED. Every difference is attributed.

- **.lodo:** 27 bytes differ after stripping the stream.

  | Offset | What changed | Why |
  |---|---|---|
  | 0x04 | version word | expected |
  | 0xB8 | loadOrderHash | CORE.esp grew 14,270 -> 14,766 B at 14:32 |
  | 0x28 | cardCorpusHash | the 15 legacy _n arrays moved by a few hundred bytes each |
  | 2 vertex self-AO bytes | 228 -> 201 on WaterTowerConcord01_LOD and WaterTowerGeneric01_LOD | see split below |
  | CRCs | | follow from the above |

- **.lodi:** only lodoIdentity and headerCrc32 changed (both allowed), plus loadOrderHash at 0x90 (CORE.esp).

**Split, from a small bake of cells -20,17..-17,20 with four exes:**

| Run | Exe |
|---|---|
| A | mine |
| B | mine again |
| C | BAKE1's exe |
| D | this tree without SEAM1's commit (08bf7589) |

- A = B, byte-identical (the bake is deterministic).
- **The _n arrays** follow the 4 non-SEAM1 commits (c21eb26a, a6e5e8de, 44805f8f, 59a0dd33): D equals A on them, C differs.
- **The water-tower self-AO byte** follows SEAM1's build: D has 228, like C.
  - strip(A.lodo) against D differs only at 0x04, the two CRCs and that one vertex byte.
  - That mesh has NO colour flag.
  - Best reading, not proven: the AO caster is inline (lodgenao.h:160) and is compiled into lodofile.cpp with -O3 -march=haswell. SEAM1's edit to that file moved the float code, and one grazing ray flipped (27/255 is one ray).
  - Refuter: a build of SEAM1's lodofile.cpp with -ffp-contract=off that still gives 201.

### (b) G2: the coloured models carry their rows
RED, attributed.
- 166 VERTEX_COLOUR meshes and 6 VERTEX_ALPHA meshes.
- Flag and source agree both ways, with 0 disagreements.
- 1 row disagreement: TreeFirForest04Gr_LOD_0 has 2 black vertices that no triangle uses. The emitter drops those by design (ad482f49).

### (c) Spells

| Spell | Exe | Result |
|---|---|---|
| lodgen_native | b72cef2a | 32/0 PASS |
| lodgen_loadorder | b72cef2a | 24/0 PASS (its red rung fails 2, as it must) |
| lodgen_cardlink | b72cef2a | PASS |
| lod_generation (GUI) | 880f5056 | 128/0 PASS |
| lodl_channels | 880f5056 | 54/0 PASS; run because the fix touches lodinative.cpp; main-tree fixtures by absolute path |

### (d) Counts
- 3,670 object files, the same names as installed. 3,652 are byte-identical.
- 18 differ: .lodo, .lodi, .lodb and 15 _n arrays.
- Placements: 184,431 = 184,431.

### Pictures (pics/vc_on/)
Before = replaced/ v4 pair; after = installed v5. Same exe (880f5056) and same camera for both, WW_LODL_AO=1.

| Picture | Pixels differing (of 2,598,400) | Diff image |
|---|---|---|
| Boston 08_boston_oblique | 1,040 | 08_boston_oblique_diff.png |
| Amphitheater close | 5,389 | amphitheater_diff.png |
| Blasted maple close | 4,814 | maple_diff.png |
| Most-tinted window (cells -16,21..-9,28, 350 hue placements) | 13,404 | hue_window_diff.png |

**Red runs on the way:**
- Every lit render was byte-identical before and after, even with the viewer fix. The cause is that the lit path uses vertex colour only with Scene::DoVertexColors, and a headless run inherits that option from the saved UI state.
- Proof with doctor.py (every colour row set to pure red, CRCs and lodoIdentity recomputed, written to red/):

  | Render of the red pair | Result |
  |---|---|
  | flat | 185,943 red pixels |
  | lit | 0 pixels changed |
  | lit with WW_LODL_AO=1 | 137,332 pixels changed |

- bungo's own NifSkope window shows the tint only if its Vertex Color toggle is on.

## 4. Exe sha1 and commits

**Exes:**

| Exe | sha1 | Role |
|---|---|---|
| run/release | b72cef2a60d00fd186525cfc302b0e7eaec01a5e | baked, ran 3 spells |
| release (tree) | 880f5056c0513064da00cfab26428b0f1ceaa0b8 | viewer fix; pictures, 2 spells |
| run_nov5 | 08bf758989ba5f4900b8c0d4c567f1ef97e4dd85 | determinism split only |

**Commits (text only, by explicit path):**
- 4a52ef53
- b8668e9a
- 1a15ac28
- 61d920ab (code)
- 1df4888c
- fe775dba

## 5. What the final bake needs

1. **The FO4CS reader must learn v5.** It refuses v5 today, so in game the installed library loads NO objects through ImprovedLOD if that module is on (it ships off). This is owed and comes last, by standing order.
2. **Merge 61d920ab with SEAM1's commit.** Without it, the NifSkope viewer draws v5 colour as white.
3. **The grey buildings need a different fix.**
   - Their LOD NIFs carry no hue in vertex colour, no palette flag and no .bto colour.
   - Look next at the atlas diffuse textures, and at whether those textures resolve in the bake (a missing texture draws grey).
   - Refuter for "vertex colour is not the building tint": a building LOD NIF with a hue channel that the census missed.
4. **The _n card arrays and CORE.esp's size** move the hashes between BAKE1 and now. A fresh whole bake settles that; nothing to fix.

## 6. Skill review

- **ww-lodo-version-bump:** right about the byte layout. It lacked the viewer check, and the new render-shot section now covers it.
- **nifskope-ww-render-shot:** it missed that the lit path inherits the vertex-colour option. That cost about an hour and a wrong "viewer defect still open" reading. Now written in.
- **nifskope-ww-worktree-build:** it missed the exec block on new exes (19 tries of rc 126 over 40 minutes). Now written in.
- **fo4-nif-vertex-channel-census:** the tools did the census with 0 refusals. Keep as is.
- **ww-whole-map-picture:** the BAKE1 cameras were reused as they stand. Keep as is.
