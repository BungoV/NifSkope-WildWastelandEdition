# LANE CARDWIDTH -- NOT PENDING ANY MORE (kept for the resume procedure)

**Superseded 2026-09-10 02:0x.** `Fallout4.exe` went DOWN at the lane's one build
check, the build ran (`release/NifSkope.exe` 02:07:48) and the whole chain in
`run.sh` ran with it. The results are in `scratchpad/lane_cardwidth_report.md`
section 15. Nothing below is owed; it is kept because the gate list and the
"what must NOT move" list are still the right ones to re-run after any further
change to the coverage contract.

Two things in the resume were WRONG and are fixed in `run.sh`: the sample-set
step has to run BEFORE the two steps that read a card `.lodm` (that file does not
exist until `lodgen card` has run), and the exe-newer sweep is over five files,
not two.

---

# LANE CARDWIDTH — BUILD PENDING, paste-able resume

`Fallout4.exe` was up (PID 12500) at this lane's single build check, so nothing
was built, nothing was re-baked and no exe was launched. Everything below is on
disk and syntax-checked; none of it has been compiled.

Read first: `scratchpad/lane_cardwidth_report.md` (the measurement that names the
cause, the law that was refused, the fix, the pre-registered fixture in §8).

## What changed, and what has never been compiled

| file | what |
|---|---|
| `src/nifskope_ui.cpp` | the octahedral bake's pass two re-encodes the coverage (`covFloor`/`covTest`/`covBase` + `coverageEncode`, ~22535), and the sidecar gains `coverage <floor> <test> <base>` (~22675) |
| `src/lodgen.cpp` | card functions ONLY: three fields on the card struct (~2292), the sidecar's `coverage` branch (~2576), the card `.lodm`'s `coverage` object (~2874), and the same object travelling with a `cardArray` layer (~8552, ~8623) |
| `tests/spells/lodgen_octahedral.sh` | F1–F5 in bake 4 and in the bake-1 `.lodm` block |
| `docs/LODGEN_CARD_SHEETS.md`, `docs/LODGEN_LODM_FORMAT.md` | the contract |
| `scratchpad/cardwidth_20260910/*` | the measurement scripts (all runnable with the game up), `transition2.py`, `run.sh` |
| `CHANGE_NEEDED.md` | `docs/LODGEN_IMPOSTOR_SPEC.md` still states the superseded rule; not this lane's file |

Nothing was committed (CONSTITUTION 8).

## The resume, in order

```bash
cd /e/Projects/NifskopeWildWastelandEdition
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?     # rc=1 or stop here
# build through nifskope-ww-build-verify: qmake, make's OWN exit code, the exe
# renamed aside if bungo's window holds it, the stylesheet copied at link time,
# then the staleness sweep over the five changed sources above
bash scratchpad/cardwidth_20260910/run.sh 2>&1 | tee scratchpad/cardwidth_20260910/run.log
```

`run.sh` does the whole chain: the four harnesses, the cube fixture quoted against
the pre-registration, the 19-tree re-bake, the sheet-level contract check, the
`.lodm` check, the transition on the fixed instrument, the FO4CS sample set and
its MANIFEST rows, and the three pictures.

## The gates, pre-registered

| gate | bar |
|---|---|
| `lodgen_octahedral.sh` | RESULT PASS, and F1–F5 green |
| F1 | the cube spans its predicted texels within **1.0** at the reader's threshold |
| F2 (floor) | the perspective control **exceeds** 1.0 on the same measurement |
| F3 | **0** texels of the base sheet with alpha in 1…159, on the cube and on all 19 trees |
| F4 (floor) | the DECODED alpha carries them, **> 0** |
| F5 | `coverage 16 128 160` on every tree sidecar and in every card `.lodm` |
| `lodgen_card_arrays.sh`, `lodgen_impostor_cards.sh`, `lodgen_identity.sh` | must not move |
| the transition | 12 of 12 card rows within 1 texel of centre and 2 % on both extents |
| the zeroed-offset control | 0 of 12 passing |
| F6, the extent floor | the dx-extent column moves by **≥ 2.5 points** between the card arm and the 5 %-wide arm on every row |
| the 19-tree re-bake | 19 baked, 0 failed |

## What is expected to move, and what is not

* The `alpha >= 16` set of every sheet is UNCHANGED by the re-encoding (a covered
  texel goes from ≥16 to ≥160, an uncovered one from 1…15 to 0), so bake 4's
  existing 2-texel check at 16/255, the gap and mip gates, and the sway/AO laws
  read the same numbers. If any of those move, the change did something it was
  not meant to.
* The DDS bytes DO change on every octahedral card. `lodgen_identity.sh` covers
  the byte-identity path for TERRAIN, not for cards; if it moves, read it.
* `_fs.DDS`, the legacy crossed card, is untouched.
