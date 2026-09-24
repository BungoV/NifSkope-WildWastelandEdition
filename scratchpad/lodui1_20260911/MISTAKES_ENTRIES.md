## 2026-09-11 -- lane LODUI1: a heredoc halved a backslash the skill had just warned about

**What was done.** A batch of edits to `tools/bake_impostor_cards.sh` was
attempted through a `python - <<'EOF'` heredoc whose anchor contained
`tr -d '\r'`. The anchor counted 0 and the script refused.

**What was true instead.** The file was fine. A Bash heredoc halves
backslashes, so the anchor arrived as `tr -d 'r'`. The
`ww-anchored-hookup` skill says this in its own section 1 -- *"written with the
WRITE TOOL (never a heredoc: a quoted heredoc halves backslashes)"* -- and it
had been read in full ten minutes earlier.

**How it was found.** The script's own `assert count == 1` refused instead of
writing, which is what that assert is for.

**The rule that prevents it.** Reading a skill is not the same as applying it.
Anything carrying a backslash goes through the Write or Edit tool, and a
one-off anchor count is not an exception to that -- the skill already says so
and it now has a case of its own to point at.

## 2026-09-11 -- lane LODUI1: indentation typed from a Read, not from the bytes

**What was done.** Three Edit-tool replacements in `src/nifskope_ui.cpp` failed
with "String to replace not found", each time because the anchor was indented
one tab too deep or too shallow.

**What was true instead.** The Read tool prefixes every line with its number
and a TAB, and that tab reads as part of the indentation. Statements at six
tabs looked like seven.

**How it was found.** The Edit tool refused, three times, and a
`python -c "print(repr(line))"` settled it in one call.

**The rule that prevents it.** Before anchoring on an indented line in a deeply
nested file, print its exact bytes (`repr`) once; never count tabs off a Read.
Better: for more than one edit in such a file, write the splice script and let
it assert `count(anchor) == 1`, which is what caught it here.

## 2026-09-11 -- lane LODUI1: a floor PREDICTED instead of measured

**What was done.** `tests/spells/lodgen_panel_run.sh` shipped with a check-count
floor of 128, arrived at by counting the `check(` calls in the new block by eye.
The gate ran, every one of its 125 checks passed, and the spell failed on its
own arithmetic.

**What was true instead.** The block adds 9 checks, not 12.

**How it was found.** The first run of the spell: `125 checks, 0 failures /
PASS`, then `FAIL: only 125 checks ran, floor is 128`.

**The rule that prevents it.** A floor is a MEASUREMENT of the first green run,
written back into the spell with the exe and the date beside it -- never a
count taken off the source. (`nifskope-ww-build-verify` says the verdict is a
number; this is the same rule applied to the floor itself.)

## 2026-09-11 -- lane LODUI1: a picture offered as proof that could not show the thing

**What was done.** The panel grab (`WW_LODGEN_SHOT`) was taken and the lane
moved on. The image showed Source, Plugins, Resources and an empty progress
map -- the settings pane opens scrolled to the top and the splitter gives the
map most of the height -- and **not one** of the rows the lane changed.

**What was true instead.** The rows were there; they were below the fold.

**How it was found.** By looking at the file, which is the only way, and which
CONSTITUTION 5 requires before a picture is offered.

**The rule that prevents it.** A grab is arranged for what it has to show
before it is taken -- scroll the pane to the rows under test and give them the
height -- and the lane LOOKS at the image before counting it as a deliverable.
A picture that cannot contain the defect is not proof of the fix.

## 2026-09-11 -- lane LODUI1: two checks whose premise the lane changed, not re-aimed

**What was done.** The panel's object pass gained a second head (the native
row, ticked by default under FO4CS). `WW_LODGEN_TEST`'s check *"the chunk range
is greyed while no chunk output is selected"* was left as it was and turned red
on the first gate run, because the panel now opens with an object output
ticked.

**What was true instead.** The panel was right and the check's premise was
stale. A second check, this lane's own, asked the FO4CS summary to name `.lodt`
while the terrain pyramid was unticked -- also a stale premise, written the
same hour.

**How it was found.** The first gate run: `116 checks, 2 failures`.

**The rule that prevents it.** `ww-retire-a-surface` section 6 already says a
harness that drove a changed surface is re-aimed, retired or made to refuse in
words, per harness, and that the pre-change baseline is recorded first. The
half that was missing here is the inverse sweep: after changing a DEFAULT or a
visibility rule, grep the harness for every check whose text states the old
premise ("while no X is selected", "the panel opens with") and re-aim it in the
same edit -- before the build, not after the red.
