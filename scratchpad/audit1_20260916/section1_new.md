### 1.6 Three checks added this lane that are RED on the audited exe

These are not board rows in 1.1 -- they did not exist when the board was run.
They are the before-proofs the brief requires for step 6: each one is a check
written against a defect found in the diff review (section 4), each is red on
the audited exe now, and each is the thing that must go green after the fix and
red again on a broken build. They are recorded here so the board can be read
against the same gate files afterwards.

| gate | new check | before, on this exe | the defect it pins (section 4) |
|---|---|---|---|
| `lodgen_incremental.sh` | arm **(g)** `--incremental` with no value must refuse, name the switch, and write nothing | **RED**: `rc=0, 15 file(s) written under the out-dir`; the run silently full-baked | C8: a switch that takes a value, spelled last with no value, parses to an empty string |
| `lodgen_defaults.sh` | phase **(f)** a `--land-guide` value that is not one of the six | **RED**: the warning says `; off stands` while the bake that is written is the DEFAULT one, byte for byte | C4: the warning text names a fallback the code does not take |
| `lodgen_native.sh` | section **14**, a floor plus four doctored cases through `--native-verify` | **RED**, 4 of 4: both wrap cases are refused by the WRONG rule (`pad byte ... is not zero`, not the bounds), and both version-5 aggregate cases are ACCEPTED | C1 the payload-bounds add that wraps, C2 the aggregate rules gated on version 4 in a file that is version 5 |

Both of the first two carry their own floor, so neither can pass vacuously.
Phase (f)'s floor is `--land-guide off is a different bake from the default`,
which is green -- the two bakes really are distinguishable, so the third check
is comparing against something that can move. Arm (g) asserts the file COUNT
under the out-dir, not just the exit code, so a refusal that still wrote half a
bake would fail it.
