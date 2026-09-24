"""Lane CELLVIEW4B -- skill text into BOTH trees, byte-identical.

Two things:
  1. CELLVIEW4's earned addition to `ww-anchored-hookup` (it wrote "section 5c";
     the skill has 5a and no 5b, so it goes in as 5b and says so).
  2. This lane's own earned skill, `ww-gate-closed-sum`.

Both files are LF-only; the writes are bytes, and the sha1s are printed for
both trees so "equal" is measured, not assumed.
"""
import hashlib
import io
import os

TREES = [
    'E:/Projects/NifskopeWildWastelandEdition/.claude/skills',
    'E:/Projects/Claude/.claude/skills',
]

APPEND = """

### 5b. An edit can eat the next edit's anchor (lane CELLVIEW4, 2026-09-19)

*(CELLVIEW4's deliverable calls this "5c"; this skill has 5a and no 5b, so it is
filed as 5b. The text is unchanged.)*

Inserted text is source too, and a block you insert can contain a line that is
a later edit's anchor. The table was resolved against the file as it was, so
the count printed by --check says 1 and the apply then quietly edits the copy
you just inserted instead of the one you meant.

Two defences, and use both:

* count again AT APPLY TIME, against the text in hand, and assert exactly one
  match. Never "take the first hit" -- that is the failure, not the fix.
* order the table so an edit that NARROWS a line runs before the edit that
  inserts a block mentioning it.

And demonstrate the apply without applying it: run the same table through the
same apply function onto COPIES in the scratchpad, then check each copy's CR
delta and its brace/paren delta against what the inserted text carries. A
hook-up script whose `--apply` has never produced a byte is not evidence that it
works. (Found by lane CELLVIEW4, 2026-09-19: an inserted block carried its own
`if ( spec.terrain ) {`, and the same dry run caught a missing `#include` the
table had simply never had.)
"""

NEW = """---
name: ww-gate-closed-sum
description: Replace a gate row that re-implements one of the product's own rules with a closed sum over the categories the product NAMES in its census, so the row cannot be satisfied by editing the rule and cannot be quieted by an open-ended excuse. Use when a gate carries a Python (or shell) copy of a C++ predicate, when a check's pass depends on a list of expected exceptions, or when a change to the product turns a gate red and the tempting repair is to copy the new clause across.
---

# A gate that mirrors a rule rots; a gate that closes a sum does not

Lane CELLVIEW4B, 2026-09-19. `tests/spells/cell_open_check.py` carried a Python
copy of `isMarkerModel()` from `src/cellview.cpp`, with a docstring promising it
mirrored the C++ "element for element". The lane widened the C++ -- a marker at
the meshes root has no backslash before its name, so `markerxheading.nif` was
being drawn as an ordinary static -- and did not widen the copy. The gate went
red and accused the viewer of dropping three references it had deliberately
hidden.

The red was CORRECT behaviour by a STALE rule. That is the worst kind, because
the obvious repair -- paste the new clause into the copy -- leaves the trap
armed for whoever changes the rule next, and leaves the gate's promise ("element
for element") being kept by nothing but attention.

## 1. Recognise the shape

You are in this situation when any of these is true:

* a gate re-implements a predicate the product owns (a classifier, a filter, a
  name rule, a unit conversion);
* a check passes because a list of EXCEPTIONS explains the difference, and the
  list has an open-ended member -- "expected", "BA2-only", "not applicable",
  "these always fail";
* a product change turns the gate red and your first instinct is to edit the
  gate so it agrees.

## 2. Find what the product already NAMES

The product usually publishes its own accounting, because somebody made it
self-diagnosing. Look for the census line that lists categories:

    hidden: disabled 0, markers 12, deleted 0, no base 0

Those category names are a contract the product wrote about itself. They are
worth more to a gate than a copy of any one rule, because the product cannot
move a reference out of them without changing the line.

## 3. Make the row a closed sum

State it as an identity over totals, not over items:

    every reference the plugin draws and the scene does not
      == the sum of the categories the census names

Measured on downtown 5,-11: plugin 1438 drawable, scene 1426, gap 12; census
names 12. The properties that make this better than the mirror:

* **it cannot be satisfied by editing the rule.** Widening or narrowing the
  marker rule moves a reference between two NAMED categories; the sum is
  unchanged. So the gate stops being a hostage to the copy.
* **it has no open end.** A reference dropped for a reason nobody has thought of
  is not in any named category, and the sum breaks. The excuse it replaced --
  "9 more were dropped with no loose model, which is expected" -- would have
  swallowed exactly that.
* **it fails loudly with a number**, not a list of names to squint at:
  `12 missing, census names 11 -- 1 dropped for a reason the viewer does not
  state`.

Keep the per-item explanation as PRINTED OUTPUT beside the sum. A reader still
needs to know WHICH reference; they just should not be able to pass on it.

## 4. Two proofs before you believe it

Both, every time, and they are cheap:

* **It fails.** Perturb the census by one -- write a copy of the notes with
  `markers 12` changed to `markers 11`, run the checker against it, watch it
  say so. One sed and one run.
* **It moves.** Run it on two subjects with different answers. Sanctuary -20,7
  gives `0 == 0`, downtown gives `12 == 12`. A sum that is `0 == 0` everywhere
  you looked is a check that cannot fail, and you have not tested it.

## 5. What to do with the mirrored rule you leave behind

Fix it anyway -- the per-item lines are read by humans -- but rewrite its
docstring so it no longer PROMISES agreement. Promise nothing a script does not
check:

    Do not trust this comment that the two rules still agree -- the accounting
    row below measures it, and that row is why a stale copy of this function can
    no longer pass quietly.

## 6. When there is no census to close over

Then you have found the real gap, and the deliverable is the census line, not
the gate row. A product that cannot say why it declined to do something cannot
be checked on it by anybody -- see CONSTITUTION rule 10: a refusal states its
reason in words. Add the named categories to the product first; the gate row is
three lines once they exist.
"""


def put(rel, data):
    shas = []
    for t in TREES:
        p = os.path.join(t, rel)
        d = os.path.dirname(p)
        if not os.path.isdir(d):
            os.makedirs(d)
        with io.open(p, 'wb') as fh:
            fh.write(data)
        shas.append(hashlib.sha1(data).hexdigest())
        print('  %-52s %d B' % (p, len(data)))
    print('  sha1 %s  equal=%s  CR=%d' % (shas[0], shas[0] == shas[1],
                                          data.count(b'\r')))


# 1. the append
src = os.path.join(TREES[0], 'ww-anchored-hookup/SKILL.md')
with io.open(src, 'rb') as fh:
    raw = fh.read()
assert b'5b.' not in raw, 'already applied'
assert raw.count(b'\r') == 0, 'expected an LF-only skill'
print('ww-anchored-hookup:')
put('ww-anchored-hookup/SKILL.md',
    raw.rstrip(b'\n') + b'\n' + APPEND.encode('utf-8').replace(b'\r\n', b'\n'))

# 2. the new skill
print('ww-gate-closed-sum:')
put('ww-gate-closed-sum/SKILL.md', NEW.encode('utf-8').replace(b'\r\n', b'\n'))
