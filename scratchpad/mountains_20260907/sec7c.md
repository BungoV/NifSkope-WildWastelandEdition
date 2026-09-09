### 7.3 Full-corpus level-4 sweep — LANDED, and it confirms section 2.2

`python corpus.py`. First it validates the coarse mip rather than assuming it,
against a full mip-0 decode on 20 random level-4 tiles:

    === mip 4 validated against mip 0 on 20 random level-4 tiles ===
       delta lum      mean -0.05   max |0.24|
       delta meanSat  mean -0.0051 max |0.0249|

**Tolerance: lum within 0.25, meanSat within 0.025.** Every number below carries
that.

Then all **2304** level-4 diffuse tiles and all 2304 `_msn` tiles were decoded
and each tile tagged with how many of its 16 cells the ESM walk found textured.
Full per-tile table in `level4.csv`; four 48x48 ASCII maps (luminance,
saturation, `_msn` relief, textured-cell count) in `corpus_out.txt`.

    === whole corpus, split by the ESM texture boundary ===
    group                       tiles          lum         meanSat        lumStd      msn relief
    TEXTURED (all 16 cells)       214   66.19+-14.92  0.1595+-0.0328   8.98+-2.17    33.5+- 7.9
    MIXED                          67   63.14+-14.13  0.1798+-0.0426   8.28+-2.07    36.5+- 9.5
    UNTEXTURED (0 cells)         2023   71.06+-11.01  0.1885+-0.0249   5.85+-3.79    18.4+-19.2

Against section 2.2's 40-vs-40 sample at mip 3 (textured lum 66.4 / sat 0.155,
untextured lum 71.0 / sat 0.191) the full corpus agrees to **0.2 lum and 0.005
saturation**. `lumStd` and `msn relief` come out lower here purely because mip 4
is one halving blurrier than mip 3; the *ratio* between the groups is what
matters and it is preserved.

Three things the full sweep adds:

1. **The boundary is real and visible in the maps.** The luminance map shows one
   compact block of high-contrast, structured terrain (characters `#*+=`) in the
   upper-left-of-centre, surrounded on all sides by a uniform `:-` field. The
   textured-cell map (map D) has the same footprint. They are the same region.
2. **The outer region is genuinely flatter even in vanilla** — `lumStd` 5.85 vs
   8.98, and `msn relief` 18.4 vs 33.5. So Bethesda's own far LOD is lower
   contrast than the playable area. But note the standard deviations: relief
   `18.4 +- 19.2` means the outer region is *wildly* varied — some outer tiles
   are genuinely flat plains and some are full mountains. A rebake that replaces
   all of it with one value destroys that variety, which is exactly the "flat
   silhouette" complaint.
3. **The outer region really is the more saturated half of the worldspace**
   (0.1885 vs 0.1595), now on 2023 tiles rather than 40. The saturation map also
   shows a distinct high-chroma band well outside the textured box on the
   eastern side — content that exists only in the shipped LOD textures and
   nowhere in the ESM.
