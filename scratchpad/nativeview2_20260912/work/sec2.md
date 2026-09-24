
The spell was RUN, not just written. On `release/NifSkope.exe` 12:15 it returns
**14 checks, 0 failures, 0 skips, PASS** (`release/ww_native_lighting.log`, copy
kept at `work/native_lighting_new2.txt`). Its first run FAILED all four of gate
(a)'s legacy frames, and the cause was a defect in the spell rather than in the
code under test — section 5 carries it.

---

## 2. Build and chain

### The game check

`tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` was run as its own
command and read before every build and before every exe launch in this lane.
It returned **rc=1** every time — no Fallout 4, no NifSkope, including none of
bungo's own. Nothing was ever killed. The exe was renamed aside at link time
(`release/NifSkope.before_nativeview2.exe`) and is still on disk. **No build
mutex was created and none was left behind**, as the charter instructed.

### The exe at launch, verified before the rung copy

The charter said the exe on disk would be DEFAULTS1's, 2026-09-12 21:52:25,
22,280,192 B. `ls` before the rung copy read exactly that, and the rung copy
still carries those bytes:

```
-rwxr-xr-x 22280192 Sep 16 11:40 release/NifSkope.before_nativeview2.exe
```

(the mtime is the copy's own; the size is DEFAULTS1's to the byte).

### The chain, each link with its evidence

| link | evidence |
|---|---|
| game check its own command, before build and before launch | rc=1 every time |
| rename aside, never kill | `release/NifSkope.before_nativeview2.exe` on disk, 22,280,192 B |
| `make -j2` gated on **make's own rc** | `BUILD-RC=0` (the skill's one-liner: rc is captured before any grep) |
| exe newer than EVERY changed file | 172 files under `src res tools tests` from `git status --porcelain`; the exe is newer than **170**. The two exceptions are `tests/spells/native_lighting.sh` (12:18) and `tests/spells/native_lighting_check.py` (12:03) — the gate itself, never compiled and never linked. Every file under `src/` and `res/` is older than the exe. |
| stale-object check for every header touched | **this lane touched no header.** `src/gl/renderer.cpp` and `res/shaders/fo4_default.frag` only. As a belt-and-braces run, `GeneratedFiles/.obj/renderer.o` (11:54:43) was checked against every header `renderer.cpp` includes: **0 stale**. |
| `make -n` zero compile lines | `make -n` rc=0, 4 lines of output, `Nothing to be done for 'first'`; **0** lines matching `g++`/`gcc`/`moc`/`uic`/`rcc` |
| `cmp` the stylesheet copy after the link | `cmp res/style.qss release/style.qss` — **identical**, 11,097 B |
| `cmp` the shader copy after the link | `cmp res/shaders/fo4_default.frag release/shaders/fo4_default.frag` — **identical**, 21,646 B |

**Exactly one object recompiled** — `GeneratedFiles/.obj/renderer.o`, at
11:54:43, from `src/gl/renderer.cpp` at 11:54:26. So the exe on disk is the
exe that was there at launch plus this lane's diff and nothing else.

### The exe on disk now

```
-rwxr-xr-x 22288896 2026-09-16 11:54:47.914509800 +0200 release/NifSkope.exe
```

**22,288,896 B, 2026-09-16 11:54:47** — 8,704 B larger than the rung.

### The existing harnesses

| harness | before (rung exe) | after (new exe) | verdict |
|---|---|---|---|
| `tests/spells/render_shot.sh` | 82 checks, 0 failures | **82 checks, 0 failures** | count kept, PASS |
| `tests/spells/lodl_open.sh` | 23 checks, 0 failures | **23 checks, 0 failures** | count kept, PASS |
| `tests/spells/native_open.sh` | **14 checks, 1 failure, 2 skipped, FAIL** | **14 checks, 1 failure, 2 skipped, FAIL** | count kept; the failure is **pre-existing** |
| `tests/spells/native_lighting.sh` (new) | — | **14 checks, 0 failures, 0 skips, PASS** | floor 14, measured |

**`native_open.sh`'s failure, run on both exes and settled by measurement.**
It first came back FAIL on the new exe, which is the sort of thing that should
be treated as red until proved otherwise. It was proved otherwise: the rung exe
was run against the same spell, with the pre-change fragment shader restored in
`release/shaders/` so the rung ran as it did at 11:40 (that restored file is
`work/fo4_default.rung.frag`, and it comes back at **20,045 B / 547 LF**, the
before-size to the byte, so the reconstruction is exact). The rung fails the
**same check with the same number**:

```
RUNG  IOU 0.8179   FAIL the .lodi scene covers the same pixels as the .BTO (IoU 0.8179 >= 0.95)
NEW   IOU 0.8179   FAIL the .lodi scene covers the same pixels as the .BTO (IoU 0.8179 >= 0.95)
```

It is a silhouette-coverage check on OBJECTS — the `.lodi` scene against the
chunk's own `.BTO`. It has nothing to do with normals: both frames are drawn by
shapes the census reports as `msn=0`, and gate (a) already showed those frames
byte-identical across the two exes. **It is somebody else's red, it was red
before this lane started, and this lane did not touch it.** It belongs to
whoever owns native object placement; section 4 lists it as owed.

The same run turned up something worth keeping, in `native_open`'s own gate (d),
which correlates the lit `.lodl` against the `.BTR` of the same cells:

| `native_open.sh` (d) | rung | new |
|---|---|---|
| NCC, lit terrain vs the `.BTR` of the SAME cells (bar 0.45) | 0.6008 | **0.8583** |
| mean abs colour difference (bar 48) | 35.821 | **21.132** |
| NCC against a DIFFERENT chunk's `.BTR` (refuter) | 0.2023 | 0.2303 |
| NCC against the same `.BTR` mirrored in Y (floor, bar 0.10) | -0.0242 | -0.0477 |

That gate was written by an earlier lane, with its bar and its two refuters
already fixed, and it was not part of this lane's registered set. It says the
native terrain now looks substantially MORE like Bethesda's own `.BTR` of the
same ground than it did — 0.60 to 0.86 — while both refuters stay where they
were. It is corroboration from a gate this lane did not design, which is the
kind worth more than the kind you design yourself.
