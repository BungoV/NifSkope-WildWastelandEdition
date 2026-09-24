p = '.claude/skills/nifskope-ww-render-shot/SKILL.md'
s = open(p, encoding='utf-8', newline='').read()

ANCH = "## The run hangs and writes nothing: the Save Confirmation dialog (2026-09-09, lane NOPROMPT)"
assert s.count(ANCH) == 1

NEW = """## The run hangs and writes nothing, second cause: NO FILE ON THE COMMAND LINE (2026-09-18, lane LODIV7)

**`WW_RENDER_SHOT` only arms when a file is on the command line.** The hook hangs
off `completeLoading`, and `src/nifskope_ui.cpp:22056` is explicit about it:

```cpp
if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_RENDER_SHOT" ) )
```

Setting the variables is not the arming condition and never was. An exe launched
as `"$EXE" --port "$PORT"` with `WW_RENDER_SHOT`, `WW_LODL_OBJECTS` and
`WW_LODL_CHANNEL` all set and no scene **never loads, never renders, never quits,
and never prints a reason** -- and because the one-instance rule means the tree
holds exactly one NifSkope, that one wedged process blocks every later gate in
the run. It cost lane LODIV7 its pictures and its whole G4/G5 half.

The three parts of the fix, and all three belong in every driver that starts the
exe, not just the one that was bitten:

1. **Pass the scene.** For a native far-field picture that is the `.lodl`, and it
   is not optional. The `.lodi` goes in `WW_LODL_OBJECTS`, the sheets in
   `WW_LODL_SHEETS`, the window in `WW_LODL_REGION` -- but the document the exe
   OPENS is the `.lodl` and it is a positional argument.
2. **Wrap it in `timeout`.** `timeout 600 "$EXE" --port "$PORT" "$(winpath "$LODL")"`.
   A stuck window is not a failed test, it is a failed TREE. And a wedged process
   may be beyond the session's own permission to kill: `Stop-Process`, `taskkill`
   and `CloseMainWindow` were all refused by the classifier in LODIV7, so the
   lane could not clean up after itself and closed on `PENDING.md`.
3. **Assert the artefact after the launch**, in the driver:
   `[ -s "$OUT/$1.png" ] || echo "  (no picture for $1, exit $?)"`. A harness
   whose output is a file must say in its log whether or not the file arrived.
   An absent line reads exactly like a passing one.

The shape that works, from `tests/spells/lodi_v7.sh`:

```sh
shot () {  # shot <tag> <channel> <lodi>
	WW_LODL_OBJECTS="$(winpath "$3")" \\
	WW_LODL_SHEETS="$(winpath "$SHEETS")" \\
	WW_LODL_REGION="$REGION" WW_LODI_LEVEL=0 WW_LODI_SLOT=0 \\
	WW_LODL_CHANNEL="$2" \\
	WW_RENDER_SHOT="$(winpath "$OUT/$1.png")" \\
	WW_RENDER_SIZE="$SIZE" WW_RENDER_CENTER="$CX,$CY,$CZ" \\
	WW_RENDER_ORTHO="$ORTHO" WW_RENDER_VIEW="$VIEW" WW_RENDER_FLAT=1 WW_RENDER_CLEAN=1 \\
	WW_WINDOW_AT=1960,40 \\
		timeout 600 "$EXE" --port "$PORT" "$(winpath "$LODL")" > "$OUT/$1.log" 2>&1
	[ -s "$OUT/$1.png" ] || echo "  (no picture for $1, exit $?)"
}
```

**Telling the two hang causes apart without guessing:** the Save Confirmation
hang has a window with a message box on it; this one has a window with an empty
document, or no window at all yet. The log is the faster tell -- the dialog hang
has loaded the file and logged it, this one has logged nothing after startup.

"""

s = s.replace(ANCH, NEW + ANCH)
open(p, 'w', encoding='utf-8', newline='').write(s)
print('skill section added')
