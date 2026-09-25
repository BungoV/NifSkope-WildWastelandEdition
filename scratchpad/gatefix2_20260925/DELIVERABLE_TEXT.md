## HANDOFF text

**2026-09-25 03:5x GATEFIX2 (lane, Opus 5.5) -- DONE.** Branch gatefix2-20260925 (from b1cd5bc): bab6129 +
the scratchpad commit. Not merged. Exe 3a4d1e5d = b1cd5bc unmodified; no source code changed.
- native_lighting.sh is green again: 21 checks, 0 failures, twice. Its two gate (a) reds were not a generator
  defect and not a stale baseline. The harness rendered under bungo's saved settings, and in those settings
  "Vertex Color" is unticked in the Lighting shading mode's Material Contributions. That made the .BTR water
  shape draw pure white.
- No baseline moved. Under the new settings scope all four legacy frames are byte-identical to the 09-16
  baselines. The exe those baselines were measured on (before_vt1) and before_cellview4 both give the same
  red under a copy of his settings.
- Reds: his Contributions value seeded into the scope gives 21/2, the exact reported failure. A baseline with
  one flipped byte gives 21/1.
- Kept green on 3a4d1e5d: lodgen_native 32/0, lodgen_native_baseline PASS, lodgen_btofree 30/0 (pin before_gatefix1, generators-differ path).
- Eleven other render spells still set no settings scope (listed in DONE.md). Any red on them: check his
  settings first.

## WW_CHANGES text

- **2026-09-25 GATEFIX2 (lane, Opus 5.5): native_lighting gate green again, no code changed.**
  - `tests/spells/native_lighting.sh` runs every window in its own `WW_SETTINGS_SCOPE`, wiped before each
    window and at exit, and seeded with `Settings/Version=1`.
  - Why the scope: the harness was reading the operator's saved view. His Lighting-mode "Vertex Color"
    contribution was off (`GLView/Display/Contributions/2` = 0x00184b00; bit 0x80 alone moves the picture),
    so the .BTR water drew white and gate (a) failed on every exe back to the one the baselines came from.
  - Why the seed: an empty scope is a first install. The settings dialog then saves every pane's value,
    including Background 46,46,46, and the check's coverage masks count the background as terrain
    (gates b, d, e and f fail).
  - `SEED_REG=<file.reg>` imports a chosen profile into the scope, for red controls.

## MISTAKES text

- **2026-09-25 GATEFIX2: a profile fix was applied to one gate and not its sibling.** GATEFIX1 found on
  09-24 that native_open measured bungo's saved settings (the .BTR water drew white) and scoped that one
  spell. native_lighting renders the same .BTR the same way and stayed unscoped, so it went red for the
  same reason a day later. **Rule:** when a harness defect is found, grep every spell with the same shape
  (`WW_RENDER_SHOT` without `WW_SETTINGS_SCOPE`) and fix or list them in the same lane.
- **2026-09-25 GATEFIX2: an empty settings scope was taken for "defaults".** An empty scope is a first
  install, and a first install writes the settings dialog's widget values (Background 46,46,46). The first
  scoped run went from 2 failures to 8. Seed `Settings/Version=1` before each window.

## Skill review

- **ww-stale-gate-attribution:** used, and it fitted. Its section 5 already warned that GUI gates read the
  user's profile. Amended: test the baseline's own exe first; bisect a COPY of the profile imported into a
  scope (never his key); an empty scope is a first install, so seed Settings/Version; the list of render
  spells still without a scope.
- **nifskope-ww-worktree-build:** worked as written (main's objects, make -n 0, REVISION objects deleted).
- **nifskope-ww-lodgen:** not loaded; no bake flag or lodgen source was touched.
- No new skill. The profile bisection went into ww-stale-gate-attribution rather than a new page.
