
## B8 The harness chain, the two reds it found, and what they were

The chain is `scratchpad/land1_20260912/b8_chain.sh`, run through the MSYS2
shell with `PY` **named** -- MSYS2's own python has no numpy and an empty number
reads exactly like a render regression (`docs/MISTAKES.md`, ROADS1). Logs under
`logs/hb_*.txt` (first run) and `logs/hc_*.txt` (after the fix).

### B8.1 First run, on the 08:42:33 exe -- two reds, and both were mine

| harness | Part A's exe (07:42:22) | Part B's exe (08:42:33) | verdict |
|---|---|---|---|
| `lodgen_identity` | PASS | PASS | unchanged |
| `lodgen_terrain` | 26 / 0 | 26 / 0 | unchanged |
| `lod_generation` | 116 / 0 (floor 116) | 116 / 0 | unchanged |
| `lodgen_terrain_vt` | 41 / 1 | 41 / 1 | **inherited**, identical failing line |
| `lodgen_ground_cover` | 29 / 5 | 29 / 5 | **inherited**, identical five lines |
| `lodgen_terrain_pbrm` | 14 / 0 | 14 / 0 | unchanged |
| `lodgen_native` | 18 / 0 | **18 / 1** | **NEW RED** |
| `lodgen_roads` | 11 / 0 *(A cleared R5)* | **11 / 1** | **NEW RED** |
| `lodl_open` | 23 / 0 | 23 / 0 | unchanged |
| `animws` | rc 0 | rc 0 | unchanged |

The two inherited reds were compared **line by line**, not by count: the same
four lines in `lodgen_terrain_vt` (V9c, the E/W seam) and the same five in
`lodgen_ground_cover` (C1, C2 x3, C6a...), so nothing of this lane's is hiding
behind a matching total.

```
FAIL R1 two --no-roads runs are byte-identical (1 of 10 differ)
     differs: ./obj/Commonwealth.lodb
```
```
DIFFER Commonwealth.lodb
26 stock files compared, 1 differ
FAIL the stock bake is byte-identical with and without --native
```

### B8.2 One root cause, and it was a sentence I wrote in the docs

`--incremental` writes `<out-dir>/<WS>.lodb` on **every** bake. That is the
right design -- you cannot diff against a ledger nobody wrote -- and it silently
changed the contract of every gate in the tree that says *two bakes of this are
byte-identical*. Both reds are the ledger's `switches` field:

* `lodgen_roads.sh` R1 bakes `--no-roads` twice, into `roadOff/` and
  `roadOff2/`. The `bake()` helper gives each run its own directory, so the two
  argument vectors differ in `--out-dir`, `--tex-dir` and **`--vt`**. The first
  two are on the digest's skip list. `--vt` was taken OFF it earlier this
  session.
* `lodgen_native.sh` check 5 bakes the same region with and without
  `--native <dir> --native-mesh-report <file>` and asserts the stock outputs are
  identical. They are -- except for a ledger recording whether a `.lodo` was
  written beside them.

Both follow from one sentence I put in `docs/LODGEN_LEDGER_FORMAT.md` section 3:
*"A flag that changes the **output** belongs in the digest even when it does not
change the **inputs**."* It reads like conservatism, and over-rebaking really is
free of correctness risk, which is what makes it seductive. It is not free: it
is paid in false refusals nobody can explain, and here it was paid in two
harness reds that look exactly like a broken bake.

The test is narrower than the sentence. The ledger tracks the per-chunk
`.BTO`/`.BTR`/`.DDS` files it lists and nothing else, so the question is **can
this flag make a TRACKED CHUNK stale?**

* `--vt` can -- the sheets come from the pyramid instead of the stock per-chunk
  composite, a different picture from the same inputs. Its **argument** cannot:
  it is a place to put the pyramid, exactly like `--out-dir`'s. One skip list
  could only take both or neither, so there are two lists now, and `--vt` keeps
  its token and loses its path.
* `--native` cannot. The pair goes to its own directory. It leaves the digest
  with `--native-mesh-report`.

### B8.3 The fix found a defect the gate had not

Taking `--native` out of the digest raised the question of what *should* happen
to `--incremental --native`, and the answer was ugly: `lodgenNativeActive()`
collects one `NativePlacement` per drawn reference and one lighting sample per
vertex **inside the chunk pass** (`lodgen.cpp:3784` and `:4069`). A filtered
chunk list therefore writes a `.lodo`/`.lodi` pair holding only the chunks that
happened to be dirty.

That is the `--atlas` case word for word, with one difference that matters: **a
quarter-sized atlas looks like a quarter-sized atlas.** A pair built from a
quarter of a region loads, decodes, passes
`--native-verify --native-verify-corpus`, matches its own three staleness hashes
and is simply missing most of the worldspace. It was reachable until now.
`--native` joins `--atlas`, `--arrays` and `--impostors` on the whole-region
refusal, with its own sentence.

Gate B1's dependency map had this wrong in both directions and both corrections
are in B1.6: it named the merge as whole-region when the merge is a per-file
loop, and it did not name `--native` at all.

### B8.4 Second run, on the 09:21:04 exe -- both cleared

| harness | before | after |
|---|---|---|
| `lodgen_native` | 18 checks / **1** failure, RESULT FAIL | 18 checks / **0**, **RESULT PASS** |
| `lodgen_roads` | 11 checks / **1** failure, RESULT FAIL | 11 checks / **0**, **RESULT PASS** |

Everything else on the chain is byte-for-byte the same verdict as B8.1,
inherited reds included: `lodgen_identity` PASS, `lodgen_terrain` 26/0,
`lod_generation` 116/0, `lodgen_terrain_vt` 41/1, `lodgen_ground_cover` 29/5,
`lodgen_terrain_pbrm` 14/0, `lodl_open` 23/0, `animws` rc 0, zero segfaults
anywhere.

### B8.5 The sweep the reds should have prompted BEFORE the build

Two harnesses found this. Nothing guaranteed they were the only two, so the
question was asked properly rather than left to the chain:
`grep -c "byte-identical|cmp -s" tests/spells/*.sh` names every gate in the tree
that compares two trees, and four of them bake with lodgen and are not on my
chain. All four were run (`logs/hd_*.txt`):

| harness | result | ledger seen |
|---|---|---|
| `lodgen_farring` | RESULT PASS | no |
| `lodgen_texture_arrays` | RESULT PASS | no |
| `lodgen_native_baseline` | **RESULT FAIL** | **yes** |
| `lod_channel_preview` | RESULT PASS | no |

`lodgen_native_baseline` is a different animal and its red is not a defect:

```
NEW      region/Commonwealth.lodb
25 files in the baseline, 26 baked, 1 differ
baseline exe 664e0de4... 2026-09-10T03:57:46; this exe 943b52db... 2026-09-12T09:21:04
```

**Zero hashes differ. One file is new**, and it is new because the feature
writes it. The tempting fix is `--write`, and it is wrong: that baseline is a
checked-in list of hashes from ONE NAMED BUILD of 2026-09-10, and re-freezing it
from a mid-lane exe would silently bless every other lane's drift since, which
is the one thing its own header forbids.

The harness already made this call once for the same reason -- the `.BTR` of the
region run is not hashed, "terrain has its own lanes and its own gates" -- so
`*.LODB` joins `*.BTR` in the excluded set, with the reasoning in the header.
The ledger has its own gates and they are stricter than this one: B4 checks its
magic, version, sort order, relative paths and determinism, and B3 compares it
**byte for byte, by name, on all eight arms**. Hashing it here would turn every
future ledger version bump into a red about the stock vertex writer. The
`exclude=BTR` profile string is deliberately **unchanged**, because `--check`
refuses a baseline written under a different profile and changing the string
would reject the checked-in file it is meant to compare against.

### B8.6 A stale object that the build gate could not see

Found while re-checking the tree after a merge landed in it mid-lane, and worth
writing down because it is the `exe -nt src` trap wearing a new costume.

`tools/ww_build.sh` gates on **the exe being newer than the sources**. Six files
arrived in the tree from another lane with their **mtimes preserved from the
source tree** (08:26-08:46), older than the 09:21:04 exe. So the gate passed,
`make` had nothing to say, and the objects told the truth:
`GeneratedFiles/.obj/animdopesheet.o` was **04:34:58** against an
`animdopesheet.cpp` of 08:32:23. `make -n` wanted six translation units.

**An exe newer than a source file is not an exe built from it.** The object
timestamps are the check -- which is what this lane has been saying since the
first Part B build -- and `make -n` is the two-second version of it. A copy that
preserves timestamps defeats every mtime gate in the chain at once, and a lane
that reads only the exe's own stamp will measure the wrong binary and never
know.

### B8.7 The shipping exe, re-verified

The rebuild at B8.6 produced the exe that ships (09:32:37, 21,951,488 B, sha1
`3e1914a0637b66f438d873e0230b1e8c04d7c806`). It carries **no lodgen change** the
09:21:04 exe did not -- `nifcli.o` is 09:20:58 and `lodgen.o` 08:32:06 in both --
but "it should be the same" is the claim this lane exists to distrust, so the
five harnesses that could speak to it were re-run on it (`logs/he_*.txt`):

| harness | on the shipping exe |
|---|---|
| `lodgen_identity` | **RESULT PASS** |
| `lodgen_roads` | 11 checks / 0 failures, **RESULT PASS** |
| `lodgen_native` | 18 checks / 0 failures, **RESULT PASS** |
| `lodgen_native_baseline` | 25 files in the baseline, 25 baked, **0 differ**, RESULT PASS |
| `animws` | 236 checks / 0 failures / **1 skip** |

`lodgen_native_baseline` reading `25 baked` rather than `26` is the exclusion
doing its job: the ledger is still written, and is still compared byte for byte
by gate B3 -- it is only this frozen stock-vertex guard that no longer hashes it.

One number does not match what was handed to me: the `animws` baseline was
quoted as 236 / 0 / **2 skips** and this run skips **1**. A skip is never a
pass, so a skip that turns into a real check is the harmless direction, and the
remaining one names itself (`10mmPistol.nif has no NiControllerSequence`). It is
another lane's harness and another lane's number; it is reported, not adjusted.

Zero segfaults on every run of every chain in this part.
