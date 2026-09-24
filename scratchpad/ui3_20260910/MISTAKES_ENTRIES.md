<!-- Lane UI3, 2026-09-10. TEXT ONLY, appended to MISTAKES.md by the lane
     itself (CONSTITUTION 2, append-only). LF-only, no CR. -->

## 2026-09-10 -- lane UI3 -- a Bash heredoc ate the backslashes the skill had already warned about

**What was done.** The first patch of `tests/spells/water_ui.sh` went through
`python - <<'PYEOF'`. Its second anchor is a shell line-continuation --
`"...(R4) the skin emits one padding" \` -- and the anchor counted **0** while
the file was perfectly fine. Two minutes were spent doubting the tabs, the
line endings and the file itself.

**What was true instead.** `ww-anchored-hookup` section 3a says it in as many
words: *"a one-off anchor COUNT through a Bash heredoc halves backslashes ...
anything carrying a backslash goes through the Write or Edit tool."* The skill
had been loaded in this same session, twelve minutes earlier.

**How it was found.** Writing the same substitutions into
`scratchpad/ui3_20260910/patch_spell.py` with the Write tool and running it:
all five anchors matched once, first try.

**The rule that prevents it.** No patch, and no anchor COUNT, through a
heredoc -- ever, not even "just to check". A script file written with the Write
tool costs one extra call and cannot be wrong about its own bytes. Loading a
skill is not the same as obeying it: the trap it names has to be checked
against the command being typed, at the moment it is typed.

## 2026-09-10 -- lane UI3 -- the hook-up's own marker assertion was written from an assumption

**What was done.** `hookup.py` asserted that its marker string
(`wwBarRowBoxQss`) appears in the target exactly `n + 1` times after the edit.
`--apply` refused. The replacement text names the marker THREE times -- the
definition and two call sites -- so the assertion was arithmetic about a number
nobody had counted. A second version, written to survive a `--revert`, made the
same shape of error again and refused a second time.

**What was true instead.** The honest form is
`after == before - old.count(marker) + new.count(marker)`: derive it from the
two texts rather than from a belief about how many times a name is used.

**How it was found.** The script's own assertion, before it wrote anything --
which is the point of writing the check into the script. Cost: two refused runs
and no damage to the file.

**The rule that prevents it.** A refusing script's own predicates are code and
get the same treatment as the code they guard: derive every count from the
bytes in hand, never from a recollection of what was written a minute ago.

## 2026-09-10 -- lane UI3 -- the first build moved the buttons and forgot what hangs off them

**What was done.** The row's button sheet made every tool button in the top
strip the row's height, and the gate went green at 1 px: 4 of 4 at 35, y 0.
The PICTURE showed something the gate could not: the viewport header's Global,
snap and grid buttons had grown a dropdown arrow in their bottom-right corner,
because a QToolButton's default `menu-indicator` sits there and those buttons
-- unlike the four boxed ones in the top row -- carry no rule that centres it.
Invisible at 25 px, obvious at 35.

**What was true instead.** Changing a widget's HEIGHT moves everything the
style positions RELATIVE to that height. `wwBoxedButtonQss` had always centred
the indicator, which is exactly why the four buttons the ruling was about
looked right and the ones nobody was looking at did not.

**How it was found.** The before/after grab, at a 2x crop of the second row
(`scratchpad/ui3_20260910/images/cmp_header.png`) -- CONSTITUTION 5, "counts do
not see a ragged column". It cost a second build in the same session.

**The rule that prevents it.** When a rule changes a widget's box, list the
subcontrols the style places against that box -- indicator, arrow, chevron,
separator -- and state them in the same sheet. And look at the picture before
believing the count, not after.
