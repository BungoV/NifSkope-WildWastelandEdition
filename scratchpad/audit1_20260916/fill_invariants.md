| # | invariant (section 3's row) | reader | before (audited exe) | after (fixed exe) | moved |
|---|---|---|---|---|---|
| 1--3 | `.lodl` subsample, cell range, water | `lodgen_lodl_pyramid.py` | 7 checks / 0 | **7 / 0** | no |
| 4 | `.lodt` tiles | `lodgen_vt_check.py tiles` | 7 / 0 per container | **7 / 0** per container | no |
| 5 | `.lodt` borders are the neighbour's content | same, `border` | 4 / 0 per container | **4 / 0** per container | no |
| 6 | `.lodt` georef | same, `georef` | 2 / 0 per container | **2 / 0** per container | no |
| 7--8 | `.lodo`/`.lodi` pair, and the v3 refusal | `lodgen_native_decode.py` | 6 / 0 per tree, 5 trees | **6 / 0** per tree, 5 trees | no |
| 9 | `.lodi` fields, every word lane | `lodgen_native_fields.py` | 35 / 2 and 39 / 2 | **35 / 2** and **39 / 2** | no (the 2 are `e1`/`g1`, 3.2) |
| 10 | `.lodo` ladder, spheres, cones, the cut partition | `lodgen_native_cut.py` | 15 / 0 and 17 / 0 | **15 / 0** and **17 / 0** | no |
| 11 | `.lodm` cards | `lodgen_lodm_check.py` | 24 / 0 on 23 cards | **24 / 0** on 23 cards, and **100 / 0** on all 99 sidecars | the READER widened -- see below |
| 12 | FO4CSLOD layout | direct | everything under `FO4CSLOD/<ws>/` | **20 files** under `FO4CSLOD/Commonwealth/`, nothing outside `--out-dir` / `--tex-dir` | no |
| 13 | `.lodb` record: sections, hashes, written last | `lodgen_bakerec_gate.py` | 33 / 0 | **24 + 8 + 1 = 33 / 0** | no |
| 14 | INCR1 `.lodj` caches against the `.lodi` | `lodj_sweep.py` | 5 / 0, `cache 3526, .lodi 3526` | **5 / 0**, `cache 3526, .lodi 3526` | no |
| 15 | INCR1 byte identity | `cmp` | 55 of 55 | **55 of 55** (6.4) | no |

**Nothing in the decoders moved.** That is the result the section is for: seven
edits in readers, refusals and messages, and every independent reader in the
tree reads the same numbers off the same files it read before the build.
