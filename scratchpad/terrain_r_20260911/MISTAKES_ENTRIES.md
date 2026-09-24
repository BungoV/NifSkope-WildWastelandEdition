## 2026-09-11, lane TERRAIN-R: a byte cost was reported from arithmetic instead of from the bake, and it was wrong

**What was done.** The cover's-home proposal was drafted with Option B (the
ground cover in the colour sheet's alpha) costing "+46,240 bytes a tile on every
cover-free tile, +14.3%", derived by adding up sheet sizes on paper.

**What was true instead.** Nothing. The two options cost **exactly the same** —
746,752 / 374,016 with cover and 655,456 / 327,776 without, identical in all four
numbers. The cover format is selected **per tile** by the `COVER` bit, so a
cover-free tile is BC1 whichever sheet nominally carries the alpha, and a cover
tile is BC3 on exactly one sheet either way.

**How it was found.** By baking both arms before writing the proposal down,
because the brief asked for a cost and CONSTITUTION 4 says a claim about our
output is a number against an artefact.

**The rule that prevents it.** A size, a count or a cost that will be shown to
bungo is read off a file, never added up from a table — and when the arithmetic
and the file disagree, the file is right and the arithmetic goes in the report as
what it was.

## 2026-09-11, lane TERRAIN-R: a ceiling check that could not fail

**What was done.** The ring-0 gate's ceiling — "the bake compared against itself
reads exactly 0" — was written as `max(abs(px[k][0] - px[k][0]) ...)`, which is
zero for every possible input.

**What was true instead.** It measured nothing at all, and it would have printed
`ok` on a container that could not be decoded.

**How it was found.** Reading the line back while writing the report, against
CONSTITUTION 4's rule that a check which cannot fail on its input is not a check.

**The rule that prevents it.** A ceiling is built the same way a floor is: it
must be capable of reading non-zero. The fix re-opens the container, re-decodes
the sheet through the same path and differences the two arrays over all 73,984
texels, so a decode that drifted would show.

## 2026-09-11, lane TERRAIN-R: a mutation named for a rule it never reached

**What was done.** The new `maskSheetMissing` mutation renamed the mask sheet to
`emissive` to prove that a container with no mask sheet is refused. It was
refused — by the COVER-CARRIER rule, because renaming the sheet left its
differing `dxgiFormatCover` behind, and that rule runs first.

**What was true instead.** The missing-mask rule was never executed, so the
battery had a mutation whose name was a claim it did not test.

**How it was found.** Reading the refusal TEXT rather than the refusal count, in
the same pass that added the by-name matcher.

**The rule that prevents it.** Every mutation in a validator battery is matched
against the TEXT of its own refusal, not against "it was refused" — and when a
mutation trips an earlier rule, the mutation is widened until only the rule it is
named for can fire.

## 2026-09-11, lane TERRAIN-R: an extension built by dropping five characters and adding four

**What was done.** The PBRM candidate for a material-less landscape texture was
built as `lodm.left( lodm.length() - 5 ) + "pbrm"` — which drops `.lodm` and
appends `pbrm` without the dot, producing `NF_Dirt01_dpbrm`. Every PBRM lookup
on that path missed silently and the census kept reading `maskPbrm 0`.

**What was true instead.** The fixture's five `.pbrm` files were on disk and
readable the whole time.

**How it was found.** The census. `maskPbrm 0` with five fixture files present is
a contradiction, and the field exists precisely so that it is one — which is the
three rules of 2026-09-04 21:33 working as intended.

**The rule that prevents it.** A path built by string surgery is printed once
during the first run that uses it, or a census field that must move is checked
against a fixture whose answer is known BEFORE the field is believed.

## 2026-09-11, lane TERRAIN-R: a heredoc ate a backslash, twice, on a machine where that is already written down

**What was done.** Two patch scripts were passed to python through a `<<'EOF'`
heredoc from the Bash tool. One died on a `'\'` inside a string; the other
silently failed its anchor assert because `\\n` in the C++ source arrived halved.

**What was true instead.** `nifskope-ww-build-verify` already says it: *"Patch
with a script file, never a heredoc or `python -c`. Heredocs halve backslashes"*,
dated 2026-09-06, twice.

**How it was found.** The assert fired, which is why every patch script in this
lane asserts `count(anchor) == 1` before every replace.

**The rule that prevents it.** The one already written: a patch is a file written
with the Write tool. This lane used heredocs for short, backslash-free text and a
file for everything else, and the two failures were both in the first category
creeping into the second.
