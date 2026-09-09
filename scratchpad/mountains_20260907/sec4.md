
---

## 4. LENS 4 — TOOLS (overseer, after the four lens agents were killed by the
## session limit; done personally)

### 4.1 What is actually installed on this machine — MEASURED

| thing | path | evidence |
|---|---|---|
| xLODGen 3.0.22.0 | `E:\Tools\xLODGen\` | `xLODGen.exe`, `xLODGenx64.exe`, `Edit Scripts\LODGen.exe`; version string in `LODGen_log.txt` header |
| DynDOLOD 3.0 Alpha-210 | `E:\Tools\DynDOLOD\` | `DynDOLODx64.exe`, `TexGenx64.exe` 3.0.0.210 |
| MO2 2.5.2, **Fallout 4 instance** | `C:\Users\bungo\AppData\Local\ModOrganizer\Fallout 4`, mods at `E:\Projects\Fallout 4 Mods` | `ModOrganizer.ini`: `gameName=Fallout 4`, `base_directory=E:/Projects/Fallout 4 Mods` |
| TexGen + DynDOLOD wired into that instance | same ini, tool entries 11 and 12 | `11\binary=E:/Tools/DynDOLOD/TexGenx64.exe`, `12\binary=E:/Tools/DynDOLOD/DynDOLODx64.exe` |
| a pre-DynDOLOD MO2 ini backup | `ModOrganizer.ini.bak-20260827-pre-dyndolod` | filename |

Game is **Fallout 4 1.11.221** (from the TexGen log's `Game: ... Version: 1.11.221`).

### 4.2 Did any of it ever produce FO4 terrain LOD? NO — measured

* `E:\Tools\DynDOLOD\Logs\TexGen_FO4_log.txt`, session **2026-08-29 00:04:23**,
  TexGen 3.0 Alpha-210 for Fallout4. It loaded the whole load order and then
  ended on `[00:00] Exit TexGen, check log or restart?` — it **exited without
  generating anything**. `E:\Tools\DynDOLOD\TexGen_Output\` is **empty**.
* `E:\Tools\xLODGen\LODGen_log.txt` (1.79 MB) contains only **FNV/FO3** runs —
  `Game Mode: TerrainFO3`, `Game Mode: FO3`, worldspaces `WastelandNV`,
  `adwDryWellsReloaded`, `DCworld*`, `DLC01*`, `DLC02*`. There is **no FO4
  terrain LOD run in any log on this machine.** Its output path was
  `C:\Output\`, which **no longer exists**.
* An FO4 *object* LOD run did happen at some point — `E:\Tools\xLODGen\Edit
  Scripts\FO4-AtlasMap-Commonwealth.txt` (18838 bytes) and
  `FO4-AtlasMap-SanctuaryHillsWorld.txt` exist — but object LOD is not terrain
  LOD and no terrain artefacts accompany them.
* `python scan_lod.py` walked `E:\Projects\Fallout 4 Mods` (the whole FO4 mod
  stack, ~90 mods, including `overwrite`), `E:\Tools\DynDOLOD`,
  `E:\Tools\xLODGen`, and the repo's `tests`, `scratchpad`, `scratch_water`,
  `heightmaps`, matching `<ws>.<4|8|16|32|64|128>.<x>.<y>[_msn].(dds|btr|bto)`:

        SCANNED  E:\Projects\Fallout 4 Mods                   0 LOD-named files
        SCANNED  E:\Tools\DynDOLOD                            0 LOD-named files
        SCANNED  E:\Tools\xLODGen                             0 LOD-named files
        SCANNED  ...\NifskopeWildWastelandEdition\tests       0 LOD-named files
        SCANNED  ...\scratchpad / scratch_water / heightmaps  0 LOD-named files
        === directories holding LOD-named files (0) ===

  The only `Textures\Terrain\<worldspace>` trees found hold **heightmaps**, not
  LOD tiles — e.g. `...\heightmaps\Textures\Terrain\Commonwealth\
  Commonwealth.HeightMap.-96.-96.95.95.-8316.44862.dds` (33.5 MB). That
  filename independently corroborates section 0's cell extent: **-96..95**.
* `X:\...\Fallout 4\Data` has **no loose `Textures\` or `Meshes\` directory** at
  all, so nothing was installed over the game either.

> **THEREFORE: the decisive comparison this brief asked for — measure a real
> rebake tile against vanilla — CANNOT BE MADE ON THIS MACHINE.** No rebaked
> FO4 terrain LOD exists here. Everything below about what xLODGen/DynDOLOD do
> is from their shipped documentation or is reasoning, and is labelled as such.
> **Section 1.2's green/blue transposition is a finding about bungo's own fork
> and is NOT evidence about xLODGen.**

### 4.3 What xLODGen's OWN documentation says — MEASURED (it is on this disk)

Source: `E:\Tools\xLODGen\Terrain-LOD-Readme.txt`. These are quotations, not
recollection.

**(a) The knob that targets exactly the region bungo is asking about.**

> "**Default diffuse size** - size of diffuse texture in case there are no
> texture layers for the LOD level, e.g the entire terrain LOD texture is the
> default landscape texture. **This is to minimize terrain textures of outer
> regions without landscape textures.** Setting None means no change from the
> Diffuse Size for the LOD level. Applies to all LOD levels."

> "**Default normal size** - size of normal texture in case there is no normal
> data for the LOD level. **This is to minimize terrain texture of outer regions
> were no terrain textures have been defined.** Setting None means no change
> from the Diffuse Size for the LOD level. Applies to all LOD levels."

This is a documented, region-specific down-sizing of **both** the diffuse and
the normal map for "outer regions" — the mountains outside the playable area,
by name. A tile reduced to a handful of texels is:

* **flatter** — no tonal variation left to have;
* **desaturated** — averaging many hues collapses toward grey;
* **darker** iff the default landscape texture is darker than the layer mix
  Bethesda baked (untested — see UNVERIFIED);

and, because it shrinks the **normal** map too, it removes precisely the thing
section 1.4 proved carries *all* the shading at distance. **This is my leading
candidate for the answer to the question bungo actually asked**, and unlike the
transposition it is region-specific by construction, which matches his
screenshots.

**(b) The tool documents its own output as darker than you want.**

> "**Diffuse Brightness, Contrast, Gamma** - modify intensity levels. **Might be
> required to counter darkening caused by textures\terrain\noise.dds** or the
> broken 'improved' snow shader of Skyrim SE. It would be better to adjust the
> average levels (best values seem to be different depending on game) of the
> used noise.dds texture instead..."

A documented darkening mechanism with a documented compensating slider. Note the
sentence names Skyrim SE for the snow half; whether FO4's terrain shader applies
`noise.dds` identically is **UNVERIFIED** here.

**(c) A direct knob on the VCLR multiply.**

> "**Vertex Color Intensity** - 1.00 = 100%, controls how 'strong' the vertex
> color overlay is painted on top of the terrain LOD textures."

Given VCLR's neutral is 255 and it multiplies in, this slider sits exactly where
a brightness regression would live.

**(d) The stated "native" normal size is HALF what Bethesda shipped.**

> "**Normal Size** - ... see the hint message for the size that is native to the
> data (**typically 256x256 for LOD4, 512x512 for LOD8** etc.)"

Measured, section 0: **vanilla FO4 ships 512x512 for LOD4** (and for LOD8, 16 and
32 — every terrain LOD texture in `Textures\Terrain\Commonwealth` is 512x512 BC3
with 10 mips, 349680 bytes). So the readme's "native for LOD4" is half vanilla's
resolution per axis, a quarter of the texels. A user who accepts the native hint
gets a coarser normal map than Bethesda shipped, hence less relief, hence
flatter. **Caveat, and it matters:** that sentence is generic across the nine
games xLODGen supports and may be quoting Skyrim's numbers; that it applies to
FO4 is **UNVERIFIED**. The vanilla 512x512 measurement is solid.

**(e) Other relevant controls, quoted.**

* "**Normal Rise steepness** - increase steepness for each mipmap level."
* "**Normal Bake normal-maps** - bake landscape normal map textures onto terrain
  normal. Use with higher normal resolutions for LOD4, 1024x1024 for example."
* Meshes: "**Quality**: higher settings equal less quality, less terrain detail."
  and "**Max Vertices**: can be used to limit max files size".
* "**Optimize Unseen**", "**Protect Cell Borders**", "**Hide Quads**".

**(f) A skip trap that silently mixes vanilla and rebaked tiles.**

> "If terrain LOD textures already exist in the output folder, their generation
> will be skipped. ... **If the diffuse or normal texture for a higher LOD level
> already exists, then all lower LOD levels that are covered by the higher LOD
> level are skipped as well.** So in order to re-generate a LOD level 4 texture,
> it is not enough to just delete the LOD level 4 files ... the higher LOD level
> diffuse and normal textures files need to be deleted as well."

A partial rebake therefore leaves a mixture, which is a plausible source of
"some of it changed and some did not".

**(g) The readme says NOTHING about normal-map channel order.** I have not
measured what xLODGen writes into `_msn`. See UNVERIFIED for the exact procedure
that would settle it in about ten minutes.
