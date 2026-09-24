# docs/LODGEN_TERRAIN_VT.md -- what lane ROADS4 changed, ALREADY APPLIED

This is a record, not a pending splice. The doc is edited in the tree; nothing
is committed. Line endings verified LF-only by Python byte counts before and
after (CR 0, 2,142 LF before, 2,278 after, 145,454 bytes).

Applied by three scripts kept beside this file:
`patch_doc.py`, `patch_doc2.py`, `patch_doc3.py`. Each asserts its anchor occurs
exactly once and refuses otherwise.

| # | section | change |
|---|---|---|
| 1 | **1a.5**, the colour bullet | said *"`--road-detail 0` is the default"*. Now says the default is **1.0** from 2026-09-12, quotes bungo's ruling, and points at 1a.5c as the record of what detail 0 does rather than as a description of current behaviour. |
| 2 | **1a.5e**, NEW | the whole finding: `materials/Landscape/Ground/` shapes inside the road NIFs, 36.1 % and 24.9 % of the road plane, the two-tone and boundary-gradient tables with vanilla and the displaced floors beside them, the folder-not-name discriminator, `--road-ground-paint` with its refutation table, the road-opacity evidence and its refusal, and the two closed hypotheses (no skirt-only texels; vertex alpha carries no signal, and the -0.792 was an instrument). |
| 3 | **1a.5d item 7** | ROADS3's *"`--road-detail` was re-tested and stays 0"* is kept -- the measurement still holds -- with bungo's override noted on the same line, so no one reads it as current policy. |
| 4 | **1a.8**, `--roads-legacy` | the token now also means `--road-ground-paint 1`, and the note that `--road-detail 1` has stopped being a no-op in the other direction. |
| 5 | **5**, the CLI table | two new rows, `--road-detail F` and `--road-ground-paint F`, both with **no value recommended** and the reason. |
| 6 | **Provenance**, NEW `### ROADS4, 2026-09-12` | the exe and the rung, the eleven variant bakes and where they live, every instrument script by path, the substitution in the width comparison stated out loud, the one red harness row with its number, the three source files with sha256/bytes/lines, and seven line anchors. |

Nothing in sections 2, 3, 4 or 6 was touched.
