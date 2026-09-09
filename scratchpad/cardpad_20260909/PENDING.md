# PENDING -- lane CARDPAD, 2026-09-09 22:2x

The code is BUILT and gated (`release/NifSkope.exe` 2026-09-09 22:04:35, three
harnesses green). One step of the brief could not run: **the FO4CS sample set**.

## Why

`Fallout4.exe` came up (pid 18568) at ~22:22, and
`scratchpad/handoff_fo4cs/samples/make_samples.sh` refuses on its own game-up
guard. The refusal happened BEFORE the script's first `rm -rf`, so the shipped
sample set is intact and unchanged: `L4` 17 files, `L8` 22, `L16` 52, `L32` 52,
`vt` 12, `MANIFEST.md` 21:30:22. It still stands on lane CARDFIT3's card library.

## Resume, in order, game down (`tasklist | grep -i -E "Fallout4|NifSkope"` -> rc=1)

```bash
cd E:/Projects/NifskopeWildWastelandEdition/scratchpad/handoff_fo4cs/samples
bash make_samples.sh E:/Projects/NifskopeWildWastelandEdition/scratchpad/cardpad_20260909/cards_gap
python make_manifest.py
```

* the driver prints `rc=0` four times (dim 4, 8, 16, 32) then the VT block and
  `SAMPLES-DONE`;
* `make_manifest.py` re-reads every size from disk -- do not hand-edit MANIFEST.md.

## What that step also does, and is therefore also pending

`make_samples.sh` runs `lodgen --arrays --impostors <cards>`, which is what
CONVERTS the 19 baked PNG sets into `<id>_oct.lodm` + `_oct_{d,n,gsaos,g}.DDS`
and packs the card arrays. So `scratchpad/cardpad_20260909/cards_gap` today holds
**PNG sheets and sidecars only** (134 files, 19 complete sets, no zero-length
files). Nothing has yet converted THIS library, and therefore:

* the `card.gap` / `array.gap` keys and the gap-derived mip counts are proven
  only by `tests/spells/lodgen_octahedral.sh`, which does two real bakes and two
  real conversions at N=4 (73 checks, 0 failures) -- not on the 19-tree library;
* the card-array grouping's new `mipUnit` path is likewise proven only by
  `tests/spells/lodgen_card_arrays.sh`'s synthetic sets, which this lane did NOT
  run (it does not reach the bake; it should be run on resume).

## Check on resume

After the sample run, read back from any converted set:

```bash
python -c "import json,sys;d=open('<cards>/0003a28b_oct.lodm','rb').read();print(d[12:].decode())" | head -c 400
```

`card.pad` must be `[4,4]`, `card.gap` `[8,8]`, `card.mips` `4` on the 128x128
frame; the DDS header's mip count must equal `card.mips`.
