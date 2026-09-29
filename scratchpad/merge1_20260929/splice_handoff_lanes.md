### Lane lines, 2026-09-26..29 (spliced by MERGE1 from each lane's DELIVERABLE_TEXT / report; newest first)

- **WATER1** (branch water1-20260927 @ dfc243f5, merged): the .lodl is written with water bodies (version 3) by
  default; `--no-water-bodies` is the way back (version 2, byte for byte). In v3 the per-cell has-water bit is set only
  where water is over ground. Placed water that runs down a slope becomes a per-texel water surface (float plane,
  header 0x100, section bit 9); 306 placed water refs, 6 sloped used, 7 new bodies, every change confined to those
  cells; the NW hill streams (no water before) now have 4 sloped bodies drawn within 7.6 u of the file. The viewer
  draws every body as water at its height; the depth view (body height - ground) is the one non-flat view. Water LOD
  shapes lose their vertex colour (the depth bake was dropped at bungo's word; 84/84 non-water chunk blocks
  identical). Gates all PASS; 23 labelled pictures in scratchpad/water1_20260927/pics/labeled. NOT FLOWN.
- **GROUND1** (@ 625f0a44, merged): `.lodi` version 12 = ground contact per vertex (header 0x130/0x138, last payload,
  folded into indexCrc32 after the sky stream); the old one-byte-per-placement value is kept. Env
  `WW_LODGEN_NO_VERTEX_GROUND=1` = the old file. Boston .lodi +1,286,144 B (+25.3%). Recompute 99.9984% within 1
  PASS; ramp law PASS; the brief's two bars that contradict the law / compare different vertex sets FAIL and are
  recorded, not loosened. Doc section 4.16.
- **TIDY1** (@ d0860071 on ground1, merged): no black glow sheets (Boston 21 files, 50,064,092 B; installed 09-25
  whole map 90 MB), layers merged by identical texels (114 -> 106; whole map 399 -> 371), honest viewer labels. The
  one kept all-zero card sheet of 09-27 was BC1 truncating faint glow to zero (fixed in source). Gates 6/6 PASS.
- **IDENT1** (@ 470a2632, merged, fails recorded): identity by contact join (32 u, cap 4,096 u), one occluder box per
  building. Whole Commonwealth 21,140 groups, 511 boxes, street coverage 0.029 -> 0.560. Hub towers E 1 / W 4,
  Trinity 1 PASS. **FAIL: Diamond City 15 groups; row houses 1 group; 5 of 511 boxes poke > 1% (3 on a joint
  between two wall pieces).** Needs bungo's ruling (landmark/precombine join rule), not a code fix. Owed: file-wide
  ids (u32 group word), hill boxes (format bump), the 5 boxes. Pictures scratchpad/ident1_20260927/pics/.
- **TERR1** (@ 4c04abe8, merged): object normals stamped into the ground `_msn`; the ground sky (mask B) sees
  buildings, walls and bridges by a cosine law. Canyon mean 87.4 vs physical ray cast 98.8 (bar 70-114), every
  named street >= 52 (min 53.9), bias -6.0, open ground identical where no object is in reach (0 of 3,860), stamp
  confined to its own record (0 of 178,906). Ways back `--no-stamp-normals --no-sky-objects` = old sheets byte for
  byte. Still darker than physical in the deepest canyons (Theater 53.9 vs 92.3). No format change.
- **TILING5** (@ 995cffeb, merged, switches OFF): height-aware land blend and large-scale colour variation;
  `--land-height-blend on`, `--land-macro on` (macro amplitude 0 by measurement). Does NOT reach its own bars
  (transitions 7/14, band shape 3/7 + 7/7, repeat 5/7 + 6/7); keeps every grain gate green and no longer shifts
  brightness (+0.009). Recommendation kept OFF; next lever is the hex sampler's interior grain. Off = rung bytes.
- **FLAT2** (@ e8a10631, merged): a terrain tile sheet that is one value everywhere is stored as a 16-byte record +
  tile flag bit 2+k (5.96 of 19.98 GB = 29.8% of the installed bake). `.lodt` stays v2. On by default;
  `--no-collapse-uniform` = old bytes. Sea edge 48/180 collapsed, 54.9 -> 34.1 MB; Nuka-World 84/256, 78.1 -> 52.6
  MB; 0 texels differ after decode. FO4CS refuses a collapsed bake until its reader lands: the installed bake uses
  `--no-collapse-uniform`.
- **GPU1** (merged into night earlier, @ 8f58e7db): the bake measured per stage; faster identity join (109 s -> < 1 s)
  and card dilate with the same bytes; card normal sheets BC7-encoded on the GPU (on by default; Settings > NIF >
  LOD bake > Use GPU; `--no-gpu`), gated on the CPU's own error measure. Left: VT tile loop fan-out (~2,500 s of the
  whole map), hashing the bake record while writing (~450 s), the AO cast's one-thread tail (~150 s).
- **AO2** (spliced 2026-09-26, commit 422881d4): see its lines below in the 09-26 entries.
- **AUDIT1** (09-27, read-only, overseer scratch audit1/): audited the MAPS1 bake against the plan on 8 named points.
  Big faults: water never drawn as water, the v2 bake set has-water on all 36,864 cells, ground contact one grey
  per piece, identity cut at chunk lines and welded into streets. Its section 4 fault list drove WATER1, GROUND1,
  TIDY1, IDENT1, TERR1. No code.
- **MAPS1** (09-27, overseer scratch maps1/): one picture of every LOD map (79 panels) at the 08 Boston camera
  plus offline decodes; most channels right; emissive, scrappable, ground cover and v3 water planes on a v2 file
  were empty. bungo called most of the renders broken -> this campaign. Its list is the MERGE1 render list.
- **VAN1** (09-27, overseer scratch van1/): vanilla's shipped far LOD rendered at the same camera, clipped to our
  ground: mask match IoU 98.46%; mean brightness vanilla 106.2 vs ours AO on 98.4 vs AO off 117.1 over 1,899,746 px.
  Differences named (AO, water, terrain, objects), not neutralised.
- **CELL1** (09-26, overseer scratch cell1_20260926/): exported one downtown Boston LOD chunk at full detail and as
  the bake draws it (glb + census) and prepared an InstaLOD job folder (HOWTO.txt, profiles) for bungo to run
  himself. **Owed: bungo's InstaLOD runs and their import/validation.** No result exists yet.
