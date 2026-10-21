# NifSkope WW lanes: planned, no yes yet (future pile; split from LANES.md)
A row moves to LANES.md section 3 the moment bungo says yes.

- From the MW4 deep dive (10-21, sources explsmoke\SOURCES.md, GitHub survey explsmoke\GITHUB.md):
  - EXPLSMOKE voxel smoke for explosions + smoke grenades. Preview in NifSkope first, then FO4CS. Look + timeline in a
    .nifx beside the effect NIF (appear/peak/fade/gone on the NIF's animation clock, resolution + detail per preset,
    density/spread curves); .recx only per-record overrides. Fills rooms + corridors, blocked by cell collision, doors by
    open/closed state. Bullets carve cylinders several times bullet width (per caliber) that refill; click-to-shoot preview.
    Shadows, self-shadow, light shafts, lit signs shine in. Research note first.
  - FXRES1 per-effect resolution by distance + overlap (>60 % less VFX cost in MW4) + order-independent transparency
    (cheap blend for soft effects, few-layer exact for glass)
  - VISBUF1 tiny far triangles through a visibility buffer (no 2x2 quad waste), big ones forward; after PERF2
  - VRS1 variable rate shading in the cell view (NVIDIA GL path); after PERF2; picture pair if any look change
  - Meshlets + GPU-driven drawing (one call, GPU culls per piece + per patch; facing test on true faces only, never
    on two-sided; on/off frames must match pixel for pixel)
  - FOGRAY raymarched fog: PARKED by bungo 10-21, we stick to VOLAIR1 fog
- TREELEAF1 leaves break into flecks; far-LOD object shadows; far geometry LOD in the cell view (look change -> bungo)
- KIND2TYPE, BAKEUI1 (needs BAKEINCR1), RECXFIX-NIFSKOPE, FOGMAP1 water fog, "near cells stop drawing with TERRAIN=4+SYNC" bug
- PLUGINCELLED in-app plugin/cell editor (09-19); PRECOMBAKE bake precombines/previs ourselves (09-25); screen-size LOD
  selection (09-11); denser shore LOD water (09-04); door-timed air volumes (10-16); worse Far Harbor fog (10-15)
- VAULTDUST1 camera-local motes/snow/ash/leaves; UMBRA1 door state + multi-block; GRASSMB1 not-ported list
- Editor (not cell/LOD): Skeleton Manager 2-4, Block Details table, collision P4 + cloth, UV ops, 3 animation gaps


- FOGTRICUBIC1: tricubic (4x4x4) read of the fog grid when applying it; softer edges, maybe a coarser grid (from diharaw/volumetric-fog, MIT). Visible change: PNG pair first.
