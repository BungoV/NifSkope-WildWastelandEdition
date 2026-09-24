# BUILD PENDING (2) -- GLTFEXPORT1, ruled defaults

Code-only lane. Nothing below has been run: the ruling landed after my last build, and
IMPOSTORSHOW owns the build slot. R0 is RE-BASED and R14/R15 are PRE-REGISTERED and UNRUN.

Run after IMPOSTORSHOW's build, from an MSYS2 UCRT64 shell, cwd
`E:/Projects/NifskopeWildWastelandEdition`.

## 0. The gate before the gate

```
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?
```

Its OWN command, before EVERY build and every exe run. Fallout4 up = no build, no exe run,
and never a wait-loop -- report and stop. A NifSkope with no `--port` is bungo's: never kill
it; at link time rename the exe aside instead.

## 1. Build (only if IMPOSTORSHOW has not already built)

```
export PATH="$PATH:/e/Tools/GIT/cmd"   # or the link dies: Error 127: git: command not found
make -j8 release 2>&1 | tail -40
ls -l --time-style=full-iso release/NifSkope.exe
```

The rung already exists and is never deleted:
`release/NifSkope.before_gltfexport1.exe`, 22,837,760 B,
sha1 `251ecc6fdc7991d2b64e4a8e3fbc3baa3b1e6ac2`.

## 2. The CLI gate -- 16 rows

```
bash tests/spells/gltf_export_options.sh 2>&1 | tail -60
```

Green is `==== 0 row(s) not as registered ====`.

Rows that have never run in this form:

* **R0** now exports through `--legacy-defaults` and must be byte-identical to the rung exe.
  Its floor is one MORE flag changing the bytes. If R0 fails, the six flipped defaults are
  not reachable from `--legacy-defaults` -- read `gltfExportLegacyOptions()` in
  `src/gltfexportopts.cpp` field by field against `GltfExportOptions` in `src/gltfexportopts.h`
  before touching anything else. R1..R13 also run through `run()`, which injects
  `--legacy-defaults` first, so each of them still measures ONE option.
* **R14** ruled defaults, `runNew()`, no option flag at all. Registered:
  `legacy_defaults=0 skeleton_nodes=129 skeleton_auto_missed=0 joints_identical=yes`
  `metres_per_unit=1 helpers_dropped=15 nodes=117 animations=1 tracks_matched=78`,
  at least one copied `.dds`, and the Blender arm = 1 armature / 0 empties.
* **R15** the static `$STATIC`
  (`Meshes/SetDressing/AlarmClock/AlarmClock.nif`): rc 0, `skeleton_auto_missed=1`,
  `skeleton_nodes=0`, nodes < 60. Floor = the human body on the same machine finds one.

Env the script honours: `ESMROOT` (default
`X:/Programs/Steam/steamapps/common/Fallout 4/Data`), `STATIC`, `DATA`, `OUT`.

## 3. The dialog gate

```
bash tests/spells/gltf_export_dialog.sh 2>&1 | tail -30
```

Launch NifSkope only with `--port <unused>` and `WW_WINDOW_AT=1960,40`, one instance ever,
absolute `E:/...` paths. Check (a) now asserts the six ruled values field by field, that
`--legacy-defaults` parses to exactly `gltfExportLegacyOptions()`, and that a flag AFTER it
wins. Checks (c)/(d) run five states, the first two being the ruled defaults and legacy.
Was `17 checks, 0 failed` before the ruling; the count will have moved.

## 4. The body-build gate (untouched by the ruling, re-run for regression only)

```
bash tests/spells/body_build.sh 2>&1 | tail -30
```

## 5. Owed rulings, which must NOT be silently closed

* **Root motion.** The ruling text says "root motion keep-on-bone ... stay as they are".
  Those two clauses disagree: the code default is and remains `RootMotion::Strip`, which is
  what "stay as they are" means and what R0's rung bytes contain. If bungo meant
  `RootMotion::Root`, that is a ONE-LINE change in `src/gltfexportopts.h` plus a new R-row --
  do not make it without his word.
* `--legacy-defaults` has no dialog row, by instruction. The dialog gate asserts
  row-for-flag equality for every OTHER flag; if someone later adds the row, the
  exclusion list in `src/gltfexportdialogtest.cpp` is where it is spelled out.

## 6. State at handoff

`g++ -fsyntax-only` rc=0 on `src/gltfexportopts.cpp`, `src/gltfexportchar.cpp`,
`src/gltfexportdialogtest.cpp`, `src/gltfexportdialog.cpp`, `src/nifcli.cpp`.
`bash -n tests/spells/gltf_export_options.sh` ok. No `make` and no exe run since the ruling.

Shared files touched, smallest hunks: `src/nifcli.cpp` (two keys added to the MEASURED line).
Skill text updated identically in three trees:
`E:/Projects/Claude/.claude/skills/nifskope-ww-gltf-character-export/SKILL.md`,
`E:/Projects/NifskopeWildWastelandEdition/.claude/skills/nifskope-ww-gltf-character-export/SKILL.md`,
`E:/Tools/AISkills/nifskope-dev/references/nifskope-ww-gltf-character-export.md`
(sha1 `264a959c34ad3a4c6403d496671d475fffe1d270`, 7590 B, all three identical).
