## LOD terrain options: HYBRID (default) and DYNAMIC; painted ground blends into vanilla; projected decals; optional rule paint outside (lane TERRLIVE1, 2026-09-29; merged by MERGE2, 2026-09-29)

- `--terrain-option hybrid|dynamic` and a Terrain row in the LOD panel. HYBRID (default) keeps the 64/128/256 u
  texture levels; near and mid distance are drawn live from the .lodl. DYNAMIC writes no terrain textures.
  The FULL option (every baked level) is gone.
- The painted ground now blends into vanilla's own LOD diffuse over 8 km inside its edge, per texel; outside
  it, vanilla's colour is left untouched. No more dark outline or square steps at the painted edge.
- Every bake writes projected decals (.lodd/.lodg); `--decal-check` reads them back.
- `--terrain-preview <spec.json>` renders and times the options offscreen, with the same blend to vanilla.
- `--outside-paint vanilla|rule` and an "Outside paint" row in the LOD panel: optionally paint the ground outside
  our painted area with the worldspace's own landscape textures (chosen by slope, height and vanilla's colour),
  stored in a new `<ws>.lodr`; `--rule-check` reads it back. Off by default.
