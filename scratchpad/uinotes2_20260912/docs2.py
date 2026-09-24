# The DONE2 delivery's documents: rulings 07:3x (lit state white) and 08:2x
# (keys outside the range greyed).  Anchors exactly once, LF only, no partial
# writes.
import io, os, sys

ROOT = "E:/Projects/NifskopeWWE_ui"
EDITS = []       # (path, old, new)  -- old "" means append


def E(path, old, new):
    EDITS.append((path, old, new))


SP = "scratchpad/uinotes2_20260912/"

# ============================================================ WW_CHANGES_ENTRY
E(SP + "WW_CHANGES_ENTRY.md",
  """**Both light up when the mode is on**, in the palette's `accent` (#f0a54a, whose
name in `skinVars[]` is "selection accent, active toggles"), with
`accentDisabled` for a toggle that is on but cannot be clicked. Play and Play
backwards deliberately do not: they already say they are running by swapping to
the pause bars. Loop does not either, and that is a measurement, not a taste —
the render toolbar owns that shared action's icon and re-skins it on every
refresh of its popup.
""",
  """**Both light up when the mode is on, in white.** The lit ink is `textBright`
(#f2f3f5), the palette's brightest ink, over the plain `text` (#e6e8eb) of an
unlit one; a toggle that is on but cannot be clicked draws in `text`, brighter
than the `textMuted` of a disabled off one, so it does not read as off. The
first cut lit them in the `accent` orange and bungo overruled it on the picture:
"Why do these turn yellow when selected? the buttons, keep them white". The step
between the two inks is deliberately small — 33 of a possible 765 in summed RGB
— because the signal the eye reads is the blue `:checked` plate under the glyph;
the ink only has to stop contradicting it. Play and Play backwards deliberately
do not light: they already say they are running by swapping to the pause bars.
Loop does not either, and that is a measurement, not a taste — the render
toolbar owns that shared action's icon and re-skins it on every refresh of its
popup.
""")

E(SP + "WW_CHANGES_ENTRY.md", "",
  """
**Keys outside the play range are greyed out.** In the dope sheet every key
diamond whose frame falls outside the Start..End range is drawn dimmed, so the
picture says which keys the playback still takes into account. The dim is the
darkened band's own arithmetic applied to the ink instead of to the ground —
the `animOutOfRange` token composited over the key's colour at Blender's alpha
(155/255, `ANIM_draw_framerange`) — so a greyed diamond looks exactly as though
the out-of-range wash had been painted across it: `animKey` #bfbfbf becomes
#595a5c. A SELECTED key that falls out of the range keeps its own colour, dimmed
the same way (#ffbe33 becomes #725a25, #ff8c00 becomes #724611), so it still
reads as selected while it says "ignored". Nothing else about such a key changes
— same size, same shape, still clickable and draggable — and the diamonds follow
the ruler grips live, because dragging one already repaints the sheet. The
darkened band behind them is untouched.

This is a stated divergence from Blender: Blender darkens the out-of-range
background and leaves the keys as bright as the ones inside. bungo asked for the
keys themselves: "also, grey out diamond keyframes out of animations start and
end range, to indicate they're not being taking into consideration anymore".
""")

# ================================================================ HANDOFF_BLOCK
E(SP + "HANDOFF_BLOCK.md",
  """**Where it stands.** The two gizmo switches in the animation dock's transport row
are icons in ruling 4's set and light up in the accent when their mode is on;
Ctrl+A in the Animations list keeps every row; the `lodl_open` crash lane ROADS4
handed back is measured and is NOT the UI's.""",
  """**Where it stands.** The two gizmo switches in the animation dock's transport row
are icons in ruling 4's set and light up in WHITE (`textBright`) when their mode
is on — ruling 07:3x, "keep them white", after the first cut lit them in the
accent; every key diamond outside the play range is greyed out (ruling 08:2x);
Ctrl+A in the Animations list keeps every row; the `lodl_open` crash lane ROADS4
handed back is measured and is NOT the UI's.""")

E(SP + "HANDOFF_BLOCK.md",
  """**The exe.** `E:/Projects/NifskopeWWE_ui/release/NifSkope.exe`, **2026-09-12
06:55:52, 21,850,624 B**, make exit 0, `make -q` says nothing left to build,
`res/style.qss` and `release/style.qss` identical.""",
  """**The exe.** `E:/Projects/NifskopeWWE_ui/release/NifSkope.exe`, **2026-09-12
08:47:03, 21,867,008 B, md5 `1b41b4f9407732f00fd9d02399dac5e3`**, make exit 0
(`BUILD4-RC=0` in `build_main4.log`), PE header `MZ` read back before the exe was
run, `res/style.qss` and `release/style.qss` identical. It replaces the 06:55:52
/ 21,850,624 B exe this lane's first delivery shipped.""")

E(SP + "HANDOFF_BLOCK.md",
  """**Gates on that exe.** animws **224 / 0** (2 skips, both fixture skips) against a
210 / 1 rung;""",
  """**Gates on that exe.** animws **236 / 0** (2 skips, both fixture skips — the
same two) against 224 / 0 before rulings 07:3x and 08:2x and a 210 / 1 rung;""")

E(SP + "HANDOFF_BLOCK.md",
  """**Three files changed, all pure LF**: `src/animworkspace.cpp` (121,358 B),
`src/animworkspacetest.cpp` (153,971 B), `tests/spells/animws.sh` (12,251 B).
Byte counts and the not-touched list are in
`scratchpad/uinotes2_20260912/CHANGED_FILES.txt`.""",
  """**Six files changed, all pure LF**: `src/animworkspace.cpp` (122,142 B),
`src/animworkspacetest.cpp` (162,827 B), `src/animdopesheet.cpp` (40,890 B),
`src/animdopesheet.h` (12,302 B), `tests/spells/animws.sh` (13,928 B), and the
skill `.claude/skills/ww-toggle-lit-gate/SKILL.md` (6,646 B) — plus
`.claude/skills/nifskope-ww-build-verify/SKILL.md` amended in the first
delivery. `src/animdopesheet.*` are the two outside the first delivery's five:
ruling 08:2x lives in the sheet's painter, and its dim is exposed as
`AnimDopeSheet::dimOutOfRange()` so the harness measures the pixels against the
painter's own arithmetic instead of re-deriving the constant. Byte counts, CR
counts and the not-touched list are in
`scratchpad/uinotes2_20260912/CHANGED_FILES.txt`.""")

E(SP + "HANDOFF_BLOCK.md", "",
  """
5. **This session could not EXECUTE `release/NifSkope.exe` from its bash shell,
   and the exe is fine.** From 08:47 on, `execve` of that path returned EACCES
   ("Permission denied", exit 126) — and so did `cmd /c` from the same shell,
   while the very same bytes copied to another name in the same folder ran
   normally, an older exe at a different name ran normally, and PowerShell
   launched `release/NifSkope.exe` itself without complaint. It is the path, in
   this shell, not the binary: md5 of the shipped file equals md5 of the copy the
   harness ran. If a lane hits it again, copy the exe beside itself and point the
   spell at it with `EXE=`; do not rebuild chasing a corrupt binary.
""")

# ================================================================ CHANGED_FILES
E(SP + "CHANGED_FILES.txt",
  """M   src/animworkspace.cpp                                121358     0    3155
M   src/animworkspacetest.cpp                            153971     0    2811
M   tests/spells/animws.sh                                12251     0     199
M   .claude/skills/nifskope-ww-build-verify/SKILL.md      17832     0     270
A   .claude/skills/ww-toggle-lit-gate/SKILL.md             5202     0     107

    All five are untracked or unstaged in this copy's git index, which is an old
    checkout; the A/M column says what this lane did to the file on disk.
""",
  """M   src/animworkspace.cpp                                122142     0    3166
M   src/animworkspacetest.cpp                            162827     0    2940
M   src/animdopesheet.cpp                                 40890     0    1271
M   src/animdopesheet.h                                   12302     0     269
M   tests/spells/animws.sh                                13928     0     221
M   .claude/skills/nifskope-ww-build-verify/SKILL.md      17832     0     270
A   .claude/skills/ww-toggle-lit-gate/SKILL.md             6646     0     125

    All seven are untracked or unstaged in this copy's git index, which is an old
    checkout; the A/M column says what this lane did to the file on disk.

    THE TWO OUTSIDE THE FIRST DELIVERY'S FIVE are src/animdopesheet.cpp and
    src/animdopesheet.h, both added by ruling 08:2x (2026-09-12 08:2x, "grey out
    diamond keyframes out of animations start and end range"): the greying is a
    decision the sheet's painter makes, and the header carries one new public
    static, AnimDopeSheet::dimOutOfRange( QColor ), so the harness measures the
    painted pixels against the painter's own arithmetic rather than re-deriving
    the 155/255 constant on its own. Both were pure LF before and after; the
    counts above were taken in Python after the last write, and the counts they
    replace (121358/3155, 153971/2811, 12251/199, 5202/107) were this lane's
    first delivery.
""")

# ====================================================================== REPORT
E("scratchpad/lane_uinotes2_report.md", "",
  """

## 7. Ruling 07:3x -- the lit state is white, not the accent

bungo, over `icons_before_after.png`, verbatim: **"Why do these turn yellow when
selected? the buttons, keep them white"**.

**The token, named.** ON is **`textBright`, `#f2f3f5`** -- the brightest ink in
`skinVars[]` (`src/nifskope_ui.cpp:321`), one line below `text` `#e6e8eb`. OFF
stays `text`, as before. The two disabled pixmaps moved with them: Disabled/Off
is still `textMuted` `#aeb3ba`, and **Disabled/On is now `text`** -- brighter
than the muted ink a disabled OFF toggle draws, so a toggle that is on but cannot
be clicked does not read as off, and dimmer than the enabled white, so it still
reads as disabled. `accent` and `accentDisabled` are gone from
`wwTransportToggleIcon()` entirely.

**The step is small on purpose, and that is written beside the code.**
`dist(text, textBright)` is 12 + 11 + 10 = **33 of a possible 765**. The state
signal the eye actually reads is the QSS `:checked` plate under the glyph
(`bgBtnDown` `#355f86`, from `wwBoxedButtonQss()`); the ink only has to stop
contradicting it. That sentence is in the doc comment so the next person does not
"fix" the small gap.

**What the gate had to change, and why the threshold moved twice.** Gate (q)
compared the ON mean to `accent` with `dist <= 40` and required `dist(off, on) >=
60`. Both numbers were sized for a colour that is no longer there. Now:

* `dist(off, on) >= 20` -- it still has to move, floored at a fraction of the
  33 the two tokens are apart, not at a constant inherited from the accent;
* **both** means are pinned to their own token, `text` for off and `textBright`
  for on;
* the Stop floor is untouched: Stop has no On pixmap, and the same arithmetic
  must still report it moved 0. It does.

The pinning tolerance had to be measured rather than assumed. With the tolerance
written as a summed distance of 2 the gate went **red on the first run**: the
pose glyph is a ring and is nearly all antialiased edge, and the mean is taken
over pixels whose alpha is merely over half, where un-premultiplying costs up to
1 per channel. Its ON mean read `#f1f2f4` against `#f2f3f5` -- one short on each
channel, summed distance 3 -- while the auto-key dot, which is solid, read the
token exactly. The fix is not a looser number but the right measurement: the
tolerance is **2 on every channel** (`max(|dr|,|dg|,|db|)`), which is what "the
mean ink is the token" means, and a summed test would have had to be loosened to
6 to say the same thing.

**Measured on the shipped exe** (`animws_done2d.log`, gate (q)):

```
Pose     off=#e6e8eb on=#f1f2f4 moved=30 offWhiteMaxCh=1 offPlainMaxCh=0
AutoKey  off=#e5e8eb on=#f2f3f5 moved=34 offWhiteMaxCh=0 offPlainMaxCh=1
                                        (textBright #f2f3f5, text #e6e8eb)
```

**And the picture, because the icon can be right while the button is wrong.**
That is the mistake this lane already made once at 06:35, so the ink was read
back off `transport_2x_on.png` itself. Reading every pixel that differs from the
`:checked` plate gives a blend (`#5a6571`, `#5e6771`) -- a 16 px glyph is mostly
edge, which is exactly why the accent build's same measurement read `#61584b` and
not `#f0a54a`. Taking the glyph's CORE instead (the pixels furthest from the
plate) gives, for both toggles:

```
transport_2x_on.png    pose #f2f3f5   auto-key #f2f3f5     (textBright)
transport_2x_off.png   pose #e6e8eb   auto-key #e6e8eb     (text)
```

The button really does show the white. `measure_lit_core.py` beside the pictures
does that measurement on its own; `compose_icons.py` now captions the composite
from the same number instead of from a constant, so the picture and this report
cannot disagree.

**The skill was amended where it said accent** (`ww-toggle-lit-gate`): the
frontmatter, the reference implementation, the bullet that justified
`accent`/`accentDisabled`, and checks 3 and 4. Two new bullets say what this
ruling cost: *ask which ink, do not assume the accent* (it was overruled the same
day it shipped), and *a small ink step is fine when the PLATE carries the state,
so size the thresholds to the tokens you chose and never leave a floor sized for
a colour you no longer use*. Check 4 now spells out the per-channel tolerance and
why summing will not do.


## 8. Ruling 08:2x -- keys outside the range are greyed

bungo, verbatim: **"also, grey out diamond keyframes out of animations start and
end range, to indicate they're not being taking into consideration anymore"**.

**The token, named, and the arithmetic.** The dim is
**`animOutOfRange` (`#17191c` dark, `#cfcfcf` light) composited over the key's
own colour at 155/255** -- Blender's own alpha from `ANIM_draw_framerange`, the
same one the darkened band already lays over the background. So a greyed diamond
is exactly the colour it would have been if the out-of-range wash had been
painted OVER it instead of under it. One token, one constant, and it is right in
the light theme for free.

It lives in one public static, `AnimDopeSheet::dimOutOfRange( const QColor & )`
(`src/animdopesheet.h`), so the painter and the harness cannot drift apart: the
gate compares the painted pixels to the painter's own function rather than
re-deriving 155/255 on its own.

**Every state is dimmed the same way.** The diamond lambda now takes the key's
frame and runs the colour it was going to use -- plain, "other selected", or
active -- through `dimOutOfRange()` when the frame is outside
`rangeStart()..rangeEnd()`. A selected key that has fallen out of the range keeps
its selection colour, dimmed, so it still reads as selected while it says
"ignored". Nothing else changes: same size, same shape, still hit-tested, still
draggable. **It follows the grips for free**, because dragging one already calls
`setRange()`, which calls `update()` -- the gate proves that rather than assuming
it.

**The comment that said the opposite is gone.** `paintEvent` carried a paragraph
justifying the band being painted band-by-band partly because "a key outside the
range then stays as bright as one inside, which is what Blender does". That is
still true of Blender and it is now a stated divergence, in the same place, with
bungo's sentence quoted.

**The gate**, `(k8b)`, on the 93-frame fixture (every frame is a key):

| with the range at | frame 5 | frame 30 | frame 80 |
|---|---|---|---|
| 10..50 | `#595a5c` greyed | `#bfbfbf` plain | `#595a5c` greyed |
| 10..90 (End moved, no reload) | `#595a5c` still greyed | -- | `#bfbfbf` plain again |
| 0..92 (**the floor**) | `#bfbfbf` | `#bfbfbf` | `#bfbfbf` |

Every sample is compared to the EXACT colour, not to "darker than", so a wash of
the wrong strength fails too; the floor row is what shows the comparison able to
go the other way on the same run. Selected-and-out-of-range is measured in the
same block: the active key reads `#725a25` (`animKeySel` `#ffbe33` dimmed) and the
other selected one `#724611` (`animKeySelOther` `#ff8c00` dimmed), both exactly
what `dimOutOfRange()` says, and the three greyed states are still three
different colours.

The two zoom passes of gate (k) carry the same expectation now, which is why
pass 2, whose range is 25..40, expects its ACTIVE key at frame 20 to be greyed.
That is not a workaround: it is the first place the new rule bit, and the
expectation is computed from the pass's own range.

**Picture:** `images/after/sheet_range_dim.png`, the sheet at 1:1, 1054x96, range
10..50, with the greyed diamonds either side and two selected-but-out-of-range
keys among them.

**One thing the new gate broke, and how it was caught.** Adding (k8b) turned gate
(n) into a SKIP -- "the COM row is scrolled out of the sheet" -- because (k8b)
grabs the sheet and pumps the event loop several times over. The run still said
`0 failures`; only the skip count moved, from 2 to 3. A gate forces the state it
measures, so (n) now scrolls the COM row in the way (k) already does for the thigh
row and says so in the log (`the COM row was scrolled out of the sheet; scrolled
in to row index 2, centre y now 53`), and its menu check is measured again.


## 9. The DONE2 delivery

**The exe:** `E:/Projects/NifskopeWWE_ui/release/NifSkope.exe`, **2026-09-12
08:47:03, 21,867,008 B**, md5 `1b41b4f9407732f00fd9d02399dac5e3`, `make` exit 0,
`MZ` read back before it was run. `qmake` was not re-run, as instructed. Two
`make` rounds went into it: the first landed both rulings (9 translation units --
the dope-sheet header pulled `nifskope_ui.cpp` in with it), the second rebuilt
only `animworkspacetest.cpp` for the per-channel tolerance.

**animws: 236 checks, 0 failures, 2 skips** -- against 224 / 0 / 2 before these
two rulings. The 12 new checks are (k8b)'s eleven (ten plus its picture) and
gate (q)'s new "the unlit ink is still the plain text ink". Both skips are the
two fixture skips this lane has had from the start: no `skeleton.hkx` in this
copy, and the vanilla 10mmPistol NIF has no `NiControllerSequence`.

**Changed, all pure LF, CR 0 every one:**

| file | bytes | LF |
|---|---|---|
| `src/animworkspace.cpp` | 122,142 | 3,166 |
| `src/animworkspacetest.cpp` | 162,827 | 2,940 |
| `src/animdopesheet.cpp` | 40,890 | 1,271 |
| `src/animdopesheet.h` | 12,302 | 269 |
| `tests/spells/animws.sh` | 13,928 | 221 |
| `.claude/skills/ww-toggle-lit-gate/SKILL.md` | 6,646 | 125 |

`src/animdopesheet.cpp` and `src/animdopesheet.h` are the two outside the first
delivery's five, listed in `CHANGED_FILES.txt` with their counts and the reason.

**Pictures re-taken:** `transport_2x_off.png`, `transport_2x_on.png`,
`icons_before_after.png` (2736x298) and `icons_zoom_4x.png`, plus the new
`sheet_range_dim.png`.

**One thing worth knowing before the next lane builds.** From 08:47 this
session's bash shell could not EXECUTE `release/NifSkope.exe` -- `execve`
returned EACCES, and so did `cmd /c` from the same shell -- while the identical
bytes under another name in the same folder ran, an older exe ran, and PowerShell
launched `release/NifSkope.exe` itself. It is the path in this shell, not the
binary: the harness was run against a byte-identical copy (`EXE=`), md5 verified
equal both ways, and the copy was deleted afterwards. Nobody should rebuild
chasing a corrupt exe over it.
""")

# =================================================================== MISTAKES
E(SP + "MISTAKES_ENTRIES.md", "",
  """

## 5. A new gate turned an old one into a SKIP, and the failure count did not move

**What happened.** Gate (k8b), added for ruling 08:2x, grabs the dope sheet
eleven times and pumps the event loop between grabs. On the next run gate (n)
stopped measuring its right-click menu and said *"the COM row is scrolled out of
the sheet"* instead. The run still reported **0 failures**. The only number that
moved was the skip count, 2 to 3, and a skip is never a pass.

**How it was caught.** By diffing the whole check list against the previous run,
not by reading the totals: `224 / 0 / 2` to `236 / 1 / 3` looks like "one new
failure" until the two lists are put side by side and a green line has turned
into a SKIP.

**The fix.** Not to make (k8b) tidier -- a gate must not be fragile enough to
care. Gate (n) now scrolls the COM row into view exactly as gate (k) already does
for the thigh row, says in the log that it had to, and then measures. The state a
gate needs is a state it forces.

**The rule.** After adding a check, compare the SKIP LIST with the previous run's
as carefully as the failure count. A silent conversion from measured to skipped
is a regression that both numbers you usually read will hide.
""")


def main():
    byfile = {}
    for rel, old, new in EDITS:
        byfile.setdefault(rel, []).append((old, new))
    out, bad = {}, []
    for rel, edits in byfile.items():
        p = os.path.join(ROOT, rel)
        with io.open(p, "rb") as f:
            raw = f.read()
        if raw.count(b"\r"):
            bad.append("%s carries %d CR bytes" % (rel, raw.count(b"\r")))
            continue
        s = raw.decode("utf-8")
        for old, new in edits:
            if old == "":
                s = s.rstrip("\n") + "\n" + new
                continue
            n = s.count(old)
            if n != 1:
                bad.append("%s: anchor found %d times (want 1): %r" % (rel, n, old[:70]))
                continue
            s = s.replace(old, new, 1)
        out[rel] = s.encode("utf-8")
    if bad:
        for b in bad:
            sys.stderr.write("REFUSED: " + b + "\n")
        sys.exit(1)
    for rel, data in out.items():
        assert b"\r" not in data, rel
        with io.open(os.path.join(ROOT, rel), "wb") as f:
            f.write(data)
        print("%-52s %8d B  LF %d" % (rel, len(data), data.count(b"\n")))


main()
