
### 6.3 Both directions, on the fixed exe

A fix that makes bad files refuse is half a fix; the other half is that the
good files still pass. `scratchpad/audit1_20260916/step6_verify.sh` asks both
questions of every fix, and its block A is the one that found F7. Block A
verifies five real pairs this lane baked (three default regions, the aggregate
bake, the mesh-report bake) and prints the `aggregateStride` each one reports;
block B doctors a copy of a real pair four ways with the tree's own
`tests/spells/lodgen_native_doctor.py` and requires each to be refused; block C
spells `--incremental` with no value and requires a refusal before any work;
block D spells the same switch properly and requires the incremental path to
still be taken. Verbatim, on `release/NifSkope.exe` sha1 `48f7f1ab`:

```
STEP6-VERBATIM
```

Read across it: the four doctored files that the audited exe accepted at rc 0
are refused by name; the five legitimate pairs still verify at rc 0; `aggreal`
reports the stride its bytes hold (48) while the three default v5 bakes, which
carry no aggregates, still report 0; a valued switch with no value costs 0 s
and writes 0 files instead of full-baking for 44 s at exit 0; and the same
switch spelled properly still replays every chunk from the cache.
