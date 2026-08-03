# Handoff — NifSkope, Wild Wasteland Edition

**Read this first.** It is the short document: where things are, how to build,
what will bite you. [WW_CHANGES.md](WW_CHANGES.md) is the detailed history,
[WW_FEATURES.md](WW_FEATURES.md) is what the fork adds, and
[docs/TO_BE_IMPLEMENTED.md](docs/TO_BE_IMPLEMENTED.md) is the single backlog.

Updated **2026-08-03**, at the first public push. Branch `main`, build green,
working tree clean.

---

## State

Edition **0.2**, on upstream NifSkope 2.0.dev11 (fo76utils `develop` @
`f2587869`). 382 commits of fork work.

Everything in [WW_FEATURES.md](WW_FEATURES.md) is built, committed, and covered
by at least one harness. Nothing is half-landed. Two things are deliberately
parked and say so in the UI:

- **PBR rendering** — implemented, mode and toggle greyed out.
- **The four mapped features** — growing-op segment attribution, `NiPointLight`,
  `NiPSysColliderManager`, `NiPSysBombModifier`. Researched and written up in
  [docs/FOUR_FEATURES_PLAN.md](docs/FOUR_FEATURES_PLAN.md), not started, on the
  user's instruction.

One thing on the list is unfinished-by-request: the **Shading menu** is longer
than it should be. Simplification was raised and then deferred — do not touch it
without asking.

## Build

Windows, MSYS2 UCRT64. Release:

```bash
C:/msys64/usr/bin/bash.exe -c 'cd /e/Projects/ClaudeNifskope && PATH=/ucrt64/bin:/usr/bin:$PATH make -f Makefile.Release -j8'
```

Output is `release/NifSkope.exe`. That is always the correct binary; do not test
against anything else.

**After changing the include graph or adding data members to a widely-included
class, re-run qmake before trusting an incremental build:**

```bash
qmake6 -o Makefile NifSkope.pro
```

A stale dependency list once linked an old-layout `.o` and hard-crashed at
startup (`0xC0000005` inside `QHash::findNode`). More generally: **rule out a
stale incremental build before bisecting any access violation or heap
corruption.** `make clean` first. Identical source has crashed 6/6 incremental
and 0/12 clean.

Relink can also fail with the exe locked. Kill the straggler and relink; it is
not a code error.

### Version numbers — there are two, on purpose

| | where | what it is |
|---|---|---|
| `WW_VER` | `NifSkope.pro` | this fork's edition number (`0.2`) — title bar, About box |
| `VER` | `build/VERSION` | upstream lineage (`2.0.dev11`) |

`applicationName` stays `"NifSkope 2.0"` because it is the **QSettings key**;
renaming it would strand every existing user's settings. The edition name lives
on `applicationDisplayName`. `NifSkope::migrateSettings` compares against
`NIFSKOPE_VERSION`, so that one has to keep tracking upstream.

To cut 0.3: change `WW_VER` in `NifSkope.pro`. Nothing else.

### README.md is generated

`README.md` is produced at link time by `QMAKE_PRE_LINK` from
`build/README.md.in`, substituting `@VERSION@` and `@WWVERSION@`. **Edit the
`.in` file.** Editing `README.md` directly works until the next link, then
silently reverts.

## Layout

```
src/                    application
  glview.cpp            the 3D viewport — modes, modeling ops, selection, gizmos
  nifskope_ui.cpp       docks, toolbars, menus, workspaces, the WW_* harnesses
  gl/hknpdecode.cpp     compiled Havok collision, read
  gl/hknpencode.cpp     compiled Havok collision, written back
  uvtools.cpp           UV editing workspace
  unfucktools.cpp       Issue Manager
  nifcli.cpp            headless CLI
  wwskin.h              the palette — colours come from here, never literals
  ui/widgets/
    wwnumberfield.*     the one number field
    timeline*.cpp       animation timeline
    physicspanel.*      ragdoll / physics sim
lib/                    vendored deps (qhull, gli, meshoptimizer, libfo76utils)
tests/                  harness wrappers, one per feature area
tools/                  byte-level verifiers, corpus scripts, render regression
docs/                   plans, audits, research, CLI reference
```

No submodules. Everything is vendored, so `git clone` is complete.

Note: source comments cite plan docs by bare filename (`MODELING_TOOLS_PLAN.md`,
`TO_BE_IMPLEMENTED.md`, …). Those files now live under `docs/`. The names are
still unique — grep finds them.

## Testing

Harnesses are environment-gated code paths inside the real binary. Set the flag,
the app drives itself and writes `release/ww_*.log`, and a shell wrapper asserts
on it.

```bash
tests/spells/top_bar.sh
tests/spells/collision_undo.sh
tests/spells/scrub_uniform.sh
```

`tests/spells/_harness.sh` holds shared setup. 60 flags exist; `grep -rhoE
'WW_[A-Z_]+_TEST' src/ | sort -u` lists them.

Three rules that were learned the hard way:

1. **Run only the harnesses whose code path the change reaches**, and say why
   each is in the list. A blanket run opens a dozen windows and tells you
   nothing about most of them.
2. **A harness must force the state it measures**, never inherit persisted
   `QSettings`. A green suite went red with no code change because
   `GLView/Enable Animations` was left `false` in the registry.
3. **Measure with an invariant that fails on broken code, and prove it fails.**
   A proxy number that merely agrees with correctness is not evidence. Where
   possible the check is committed *before* the fix, so its failure is recorded
   against the unfixed binary.

`grabFramebuffer` does **not** repaint — pump `ogl->update()` +
`processEvents()` twice or you diff a stale frame.

## Landmines

**CRLF.** Four sources and one doc are CRLF: `src/glview.cpp`,
`src/gl/controllers.cpp`, `src/nifskope.cpp`, `src/spells/havok.cpp`,
`WW_CHANGES.md`. Python text-mode I/O flattens them into a 33,000-line junk
diff. Use an editor that preserves them, or binary I/O, and check
`git diff --numstat` before committing.

**Colours.** Never introduce a colour literal. `wwSkinColor("name")` from
`src/wwskin.h`, or add a token there. The same discipline applies to the number
field: `wwMakeScrubField` / `WwNumberField`, never a sixth private copy of the
scrub gesture — that is exactly how the five that got deleted came to exist.

**Menus.** `QMenu` paints the icon and the check indicator in the *same* column,
so an icon-bearing checkable item silently loses its checkmark. Checked rows are
styled by fill (blue background, orange text) instead.

**Event filters run before the target's own handler.** Insetting a spin box's
line edit on the *host's* resize gets undone; the correction has to hook the
editor's own `Resize`.

**`QToolButton::sizeHint()` under-reports** on stylesheet-styled buttons, because
QSS padding is not folded in. Take the max of the hint and font-metrics + padding
or the label elides.

## Conventions

- Solo repo: commit straight to `main`. No branches, no PRs.
- **Update [WW_CHANGES.md](WW_CHANGES.md) with every change batch, unprompted.**
  Newest entry at the top, dated, with the measurement that backs it.
- When a feature's design is uncertain, look up Blender's equivalent and follow
  it. State deliberate divergences.
- Do GUI verification via the harnesses rather than handing over a checklist.
  In-game validation belongs to the user.
- Fallout 4 only. Fallout 76, Skyrim and Starfield paths are inherited, not
  maintained, and not to be worked on.
