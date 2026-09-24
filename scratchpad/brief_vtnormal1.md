# Lane VTNORMAL1 -- the terrain pyramid's normal comes from bungo's upscaled _msn sheets, downsampled per level

Director brief, 2026-09-23 09:3x. Model: Opus 5.5. QUEUED behind DEFAULTS2 (build slot). Folder scratchpad/vtnormal1_<date>/.

RULING UPDATE (bungo 2026-09-23 09:4x, supersedes the size part below): "Or we can go with middle, but for the bake,
full should also be allowed" -> the pyramid's finest texel density is a BAKE CHOICE with three values: 32 u/texel
(0.457 m, ~2.2 GB whole Commonwealth), 16 u/texel (0.229 m, ~7-8 GB) = THE DEFAULT, and 8 u/texel (0.114 m, his
sheets' native, ~30 GB) = allowed. Find whether today's flags can already express 16 (e.g. --vt-content 512 at
--vt-finest 2 = 8192/512) or need a change; one clear flag + a panel row with the three values, default 16; measure
the real whole-Commonwealth size and bake time of each (estimate from a region, say which). His sheets are
downsampled to whichever density is chosen (at 8 u/texel: used as-is).

RULING (bungo 2026-09-23): pyramid uses his upscaled normal + slope sheets "Downsampled" (the ~2 GB option: pyramid
sizes unchanged, finest 32 u/texel = 0.457 m; NOT the 29.7 GB VT.1 option; the 16 u/texel middle was offered, not taken).

Facts (BLENDEDGES1, 2026-09-23): `--msn-cache DIR` is read ONLY in lodgenWriteChunkSheets (src/lodgen.cpp ~7684); the
.lodt pyramid builds its normal from heights (~11395-11415), r 0.991 vs height-normal, 0.580 vs his sheet. His sheets:
E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals/Textures/Terrain/Commonwealth/*_msn.DDS, 2,304 files,
16,777,364 B each, uncompressed R8G8B8A8 DX10, one mip, 2048^2 per dim-4 chunk (8 u/texel), R east, G up (his slope),
B north, A 255 (HANDOFF 2026-09-18 hotfix 4 + MSN ASSEMBLY). READ-ONLY: never write into that folder.

ADDED (bungo 2026-09-23 09:4x "didn't we have our normal maps and non-diffuse / base color textures be half the size
too as a toggle?"): that toggle exists for CARDS only (--card-half-aux, panel "Half-resolution normal, mask and emissive
sheets", QSettings LodGeneration/cardHalfAux, default off). Give the PYRAMID the same toggle: colour at the chosen
density, normal / mask / height / emissive at half each side; default OFF like the card one; its own panel row and
CLI flag; say how the .lodt tile table / index declares the per-sheet size (contract docs/LODGEN_TERRAIN_VT.md) and
whether that is a version bump; report the whole-Commonwealth size for each density with it on and off.

Jobs
1. Design first (progress.md): the pyramid's normal plane = his sheet, box-filtered in VECTOR space and renormalised
   (not averaged as bytes) down to each level's texel size; coverage of chunks without a sheet (fallback = heights,
   counted in the .lodm census as a named rule, e.g. normalMsnCache / normalHeights); row order (sheet vs NORTH-UP
   .lodt -- the Y-mirror trap is documented, prove orientation with a known feature); seams between chunks.
2. Implement in the VT bake; the chunk-sheet path unchanged. .lodt version bump only if the meaning of a sheet changes
   (it should not: still model-space normal). Census words in the index.
3. Gates: terrain_vt, the tiling gate, lodgen_perf as reached; a new row: with the cache, the pyramid's L02 normal over
   Sanctuary matches his sheet downsampled (r >= a bar you set BEFORE code, proven to fail on the rung, where it reads
   0.58) and without the cache it is byte-identical to the rung.
4. Picture: Sanctuary L02 normal "heights (old)" | "your upscaled, downsampled" | "vanilla _msn", plus the same
   lit (one sun) so the look is visible, not just the encoding.
5. How his bakes pick it up: the panel field for the cache dir -- does it persist, is it on in his profile; report,
   and make the panel remember the last dir if it does not (no hardcoded path in src/).

Rules: CONSTITUTION.md first; skill nifskope-ww-lodgen. Game down before build; rung first. Scratch outputs only.
Do not commit; text into DELIVERABLE_TEXT.md; DONE marker; report under 300 words, plain words.
