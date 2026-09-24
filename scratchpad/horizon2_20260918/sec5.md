
## 5. The neighbours

All on the final exe (23:47:33, sha1 `b349f807…`), Fallout4 checked DOWN as its
own command before each batch.

| harness | standing | this lane | log |
|---|---|---|---|
| `lodgen_horizon.sh` | 22 ok / 1 failed | **24 ok / 2 failed / 0 skipped** | `gate_horizon_after2.log` |
| `lodi_v7.sh` | 12 / 0 | **12 ok, 0 failed, 0 skipped** | `nb_lodi_v7.log` |
| `lodl_channels.sh` | PASS | **54 checks, 0 failures -- PASS** | `nb_lodl_channels.log` |
| `lodgen_slab.sh` | 16 / 0 | **16 checks, 0 failures -- RESULT PASS** | `nb_lodgen_slab.log` |
| `native_open.sh` | 17 / 0 / 2 | **17 checks, 0 failures, 2 skipped -- PASS** | `nb_native_open.log` |
| `render_shot.sh` | 82 / 0 | **82 checks, 0 failures -- PASS** | `nb_render_shot.log` |
| `lodl_open.sh` | 23 / 0 | **23 checks, 0 failures -- PASS** | `nb_lodl_open.log` |
| `lodgen_native.sh` | PASS | **RESULT PASS** (rc=0) | `nb_lodgen_native.log` |

Seven of the eight are exactly their standing counts. The eighth is the lane's
own gate and it is the one that moved.

### 5.1 `lodgen_horizon.sh` 22/1 -> 24/2, and the brief asked for 23+/0

G6 adds the two ok lines and the red control:

```
ok   G6 the sheet stands up to the raw-input witness -- 10 receivers x 16 bins = 160;
     mean |sheet - witness| 8.15 deg (bar 9.00); bias +0.87 deg (bar +-2.00);
     over 2 deg 78.8%; worst 67.7 deg at FOOT2 bin 4
red  G6 control: the PRE-FIX sheet  FAIL -- mean 11.90 > 9.00; bias +8.59 outside +-2.00 (EXPECTED)
ok   G6 the fixture still carries the placements -- witness mean 43.34 deg, terrain-only mean 10.86 deg
```

The two failures are the two G3 lines, and they are the reds this lane chose to
leave standing with their numbers rather than re-tune (section 4.2):

```
FAIL G3 horizonRefute agrees with the reference cast -- worst of the eight sun positions
     89.01% over 4161 samples, floor 97%            (51.69% before this lane)
FAIL G3 vhorRefute agrees with the reference cast -- worst of the eight sun positions
     95.96% over 2449 samples, floor 97%            (97.73% before this lane)
```

Both controls are still red, which is what makes those numbers readable at all:
`horizonRefute` control 50.70%, `vhorRefute` control 59.81%.

So the brief's target -- **23+ ok and 0 failed** -- is met on the ok side and
NOT on the failed side, and the honest reading is in 4.2: G3's reference calls
`LodgenHorizonField::maxAlong` (`src/lodghorizonrefute.h:107,112`), so it is not
a floor between the march and the truth, it is a floor between the march and
itself. Moving it to 97% is a matter of making the two agree, which this lane
can do at any time by tuning growth, and which would mean nothing. What the
brief wanted from that number -- evidence the sheet is right -- is in G6 and in
the lit ground of section 7, both of which are new and both of which can fail.

### 5.2 Two neighbours FAILED inside G5 and PASS standing alone -- the cause, measured

The first full run reported `lodl_channels.sh rc=1` and `native_open.sh rc=1`.
Neither is a regression, and it is not a matter of opinion:

| run | `lodl_channels.sh` | `native_open.sh` |
|---|---|---|
| inside G5 (the gate's env) | rc=1 | rc=1 |
| standalone, gate variables unset, private `OUT` | **rc=0, 54/0** | **rc=0, 17/0/2** |
| standalone, the gate's `OUT` + `SHEETS`/`V8DIR`/`V8VT`/`WAYBACK`/`REFLOG` re-exported | **rc=1, 54 checks 3 failures** | **rc=1, 17 checks 3 failures** |

Same exe, same working tree, three runs: the failure follows the ENVIRONMENT,
not the build. The failing checks name the cause themselves --

```
FAIL (a) mask-a: the note line says ABSENT by name -- ... terrain mask-a from the MASK SHEET'S A ...
FAIL (b) mask-a: absent, so its render is IDENTICAL to the default (380174 px differ, must be 0)
FAIL the lit terrain looks like the .BTR of the SAME cells (NCC 0.1387 >= 0.45)
```

`lodl_channels.sh` expects its OWN fixture, one with no mask sheet, and `G5`
hands it `SHEETS=` this lane's v8 containers, which do carry one; `native_open.sh`
is handed this lane's bake in place of the one it builds. `G5` exports `OUT`,
`SHEETS`, `V8DIR`, `V8VT`, `WAYBACK` and `REFLOG` to every neighbour it runs, and
those are the names the neighbours read for their own fixtures.

The same export does something worse and quieter: the neighbours use `OUT` as
their own working directory, so running G5 **deletes the gate's own evidence** --
`witness.log`, `witness_control.log`, `hz_*.png`, every per-neighbour log the
loop redirects into `$OUT/<script>.log`. That is why the G5 summary lines in
`gate_horizon_after2.log` carry an rc and an empty tail. The numbers in the
table above therefore come from the standalone re-runs, each with its own `OUT`,
and every one of them is named beside its row.

**Owed, named, not fixed here** (it is another lane's file and outside this
lane's one-file scope): G5 should run each neighbour with `env -u` on the
fixture names and a per-neighbour `OUT`, or it will keep reporting other
people's harnesses as broken and eating its own evidence while doing it.
