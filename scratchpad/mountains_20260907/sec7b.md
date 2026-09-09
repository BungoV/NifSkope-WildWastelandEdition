
### 7.2 The outer tiles are genuinely per-cell content, verified on the outer region itself

The brief's item 3 checked md5 distinctness on nine tiles, mixed inside and
outside. I re-ran it on **120 random level-4 tiles drawn only from the fully
untextured outer region** (classified by the ESM walk, not by eye):

    outer level-4 tiles sampled: 120
    distinct diffuse md5: 120
    distinct _msn    md5: 120

**120 of 120 distinct, in both the diffuse and the normal map.** Nothing out
there is a repeated default fill. Bethesda authored real, individual terrain LOD
for a region whose source data contains no landscape textures at all — which is
the fact the whole answer turns on.

### 7.3 Full-corpus level-4 sweep — [RUNNING]

`corpus.py` is decoding all 2304 level-4 diffuse tiles and all 2304 `_msn`
tiles at mip 4, after first validating mip 4 against mip 0 on 20 tiles and
quoting the tolerance. It writes `level4.csv` (per-tile lum, meanSat, lumStd,
msn relief, and the count of ESM-textured cells in the tile) plus four 48x48
ASCII maps to `corpus_out.txt`. It had not finished when this report was
written. **It is a confirmation of section 2.2's 40-vs-40 sample, not a
dependency of any conclusion here** — if it disagrees with 2.2, 2.2 is what
should be doubted.
