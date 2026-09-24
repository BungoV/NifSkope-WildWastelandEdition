
---

## 4. Mistakes

1. **I grepped a binary with `strings` and believed the empty answer.** `strings`
   is not installed in this MSYS bash, so my first scan of a `.BTO` for texture
   paths returned nothing and I nearly wrote down that the file names no
   textures. Caught by re-running the scan in Python
   (`re.finditer(rb'[ -~]{8,}', data)`), which found
   `data\Textures\Terrain\Commonwealth\Objects\Commonwealth.LodgenObjects.DDS`
   immediately. A tool that is missing and a tool that finds nothing look the
   same on this shell; check the tool exists before trusting a negative.
2. **My first `shot.sh` waited on any `NifSkope.exe`.** That would have
   deadlocked the lane against its own headless bakes. The GUI slot is about GUI
   instances, so the wait now reads the command line through `wmic` and ignores
   anything with `-no-gui`.
3. **The first grid stretched every render into a square cell.** A 2800x1730
   render squeezed into a square changes every slope in the picture -- a
   measurement picture that lies about geometry. Fixed by letterboxing; the fix
   is in `pics/common.py:grid()` and applies to every picture.
4. **I rendered the building close-ups at ortho half-width 520** and got one wall
   across the whole frame at 98.4% coverage. Re-rendered at 1300 (oblique) and
   700 (top down) so the cluster and its surroundings are both in frame.
5. **I built picture 3's card panels before reading why the look bake had no
   cards.** The right order was source first: `--no-identity` gates the whole
   manifest (red 5), which is a finding, not an obstacle. I got there, but only
   after a 510-second bake that produced nothing usable for the picture.
6. **My DDS reader could not read our own card sheets.** It handled the
   uncompressed DX10 the `_msn` cache writes but refused DX10 BC3 (dxgi 77),
   which is what the packed card sheets are. Fixed by decoding the colour block
   in `pics/common.py:_bc_colour()` rather than by dropping the panel.

## 5. Skill review

Skills read: `lodgen-region-bake`, `nifskope-render-harness`, `lodgen-card-bake`.

* **The render skill's first floor fired and was worth the whole lane.** "Prove
  the pixels come from the file you think they do." The `.BTR` being
  byte-identical across all six attribution bakes is what lets section 1.5 say
  every visible difference comes from the sheets and not the geometry, and the
  magenta-vs-grey gate is what lets picture 3 say the pale far ring is the
  atlas rather than a missing texture. Both of those started as skill steps I
  nearly skipped.
* **The render skill has no entry for "the file draws its own payload".** It
  assumes a render shows a surface. Two of this fork's landed features write data
  into vertex colour, and NifSkope multiplies vertex colour into the diffuse, so
  the default bake cannot be photographed as it looks. The skill should say: if
  the bake writes vertex-colour payloads, either bake a look-only arm or state
  the payload; never present a payload render as a look render. That is red 1
  and it cost this lane a fourth bake.
* **The render skill should name `WW_LOD_CHANNEL` as the AO view.** `--ao-grey`
  exists in lodgen and writes baked AO into R, G and B, but it needs a REBAKE.
  `WW_LOD_CHANNEL=3` draws the same byte flat from a file already on disk, in
  one render. Nothing in either skill points at it; I found it in
  `src/lodgenmanager.cpp:1795-1803` and the shader.
* **The card skill does not say a card is not geometry.** It describes the bake
  and the sheets but never states that the chunk file names only the merged
  object atlas, so "show me an impostor" cannot mean "render the chunk and point
  at one". A line saying what a consumer has to do with the `C` lines would have
  saved this lane an hour.
* **The region-bake skill should carry the switch couplings.**
  `--terrain-object-ao` refuses to run with `--lodl` (a named exit 2),
  `--no-terrain-identity` does not touch the object payload, `--no-identity`
  takes the card arrays with it, and `--msn-cache` beats `--erosion` to the
  normal sheet. Four couplings, none of them in the skill, all of them found by
  running the thing.
* The resource-root shape (`<root>/data/Textures/...` AND `<root>/Textures/...`,
  a junction between them) is not in any skill and is needed by every render of
  a bake output. `make_res.sh` in this lane's folder is the procedure; it should
  become a skill.
