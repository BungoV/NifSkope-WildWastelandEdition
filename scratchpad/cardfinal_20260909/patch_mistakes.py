#!/usr/bin/env python3
"""Lane CARDFINAL's entries, prepended under the file's header (newest at the top)."""
import sys

P = 'MISTAKES.md'
s = open(P, encoding='utf-8').read()
MARK = 'Newest at the top.\n'
if s.count(MARK) != 1:
    print('header marker matched %d times' % s.count(MARK))
    sys.exit(1)

ENTRY = """
## 2026-09-09 -- lane CARDFINAL pre-registered a gate threshold it had not measured

- **What was done:** the new per-frame transition check in
  `tests/spells/lodgen_octahedral.sh` was written as *"no frame's quad is
  displaced by as much as a quarter of the card's own half extent"*. A quarter is
  a round fraction and nothing measured it.
- **What was true instead:** the maple's TOP view legitimately sits **239 units**
  off centre, 28.8% of the card's half extent, because a canopy's plan view is
  not centred on the trunk. The check failed on the first real bake and the
  number it failed on was correct geometry.
- **How it was found:** the gate ran and printed 28.8% beside a bound of 25%.
- **The rule:** a gate's threshold is derived from the law, from the file's own
  arithmetic, or from an independent instrument -- never picked as a round
  fraction because it "sounds safe". The replacement is exact: for every frame,
  `|offset| + that frame's own half box` must not exceed the union half-extent
  the bake recorded, and for at least one frame it must reach it. (Related, and
  already in this file from lane CARDFIT3: carrying a tolerance from a brief
  into a gate without measuring whether the instrument can meet it.)

## 2026-09-09 -- lane CARDFINAL sent a patch script through a bash heredoc

- **What was done:** a Python patch for `tests/spells/lodgen_octahedral.sh` was
  piped through `python - <<'PYEOF'` while carrying apostrophes and backslashes
  in its replacement text. The shell died with `unexpected EOF while looking for
  matching quote` and the patch never ran.
- **What was true instead:** the `nifskope-ww-lodgen` skill says outright that no
  text carrying a backslash or an apostrophe goes through a heredoc at all, and
  that patch scripts are written with the Write tool and then run. It cost four
  occurrences in one lane on 2026-09-09 before this one.
- **How it was found:** the command failed loudly. It could have failed quietly:
  a heredoc that parses but eats one backslash produces a patch that applies
  cleanly and writes broken Python into a harness, which is the failure mode the
  skill was written for.
- **The rule:** patch scripts go in `scratchpad/<lane>/patch_*.py` through the
  Write tool, are run as files, and assert their anchor count before replacing.
  Nothing else. (Second entry for this rule in two days.)

## 2026-09-09 -- lane CARDFINAL found two gates that had been pinned to stale constants

- **What was done (by earlier lanes, found here):** two checks in the card gates
  restated a measurement rather than a law, and both went stale under laws that
  moved afterwards:
  1. `lodgen_octahedral.sh` bounded the short-axis air at *"at most one rung, 16
     texels"*, a budget calibrated on the UNION of the frames' silhouette boxes.
     Per-frame positioning collapses that union onto the widest single view, so
     the same model reads 28 texels where it read 29 and the budget tips over its
     own boundary with nothing having got worse.
  2. `lodgen_card_arrays.sh` required the `--card-half-aux` payload to land
     *"within two points of the claimed 46.4%"*, measured on 2026-09-06. The aux
     mip chain came down with its halved gap that morning and the whole chain lost
     a level that evening, so the true figure is 42.9%.
- **How it was found:** both failed on the first run after this lane's change, and
  in both cases the output was right and the check was out of date.
- **The rule:** a gate states the LAW and derives its expectation from the same
  inputs the writer used, so that changing the law changes the check in the same
  edit. A remembered percentage or a texel budget calibrated on one bake is a
  measurement pinned in the shape of a rule, and it goes stale silently. Both are
  now derived: the aspect ladder is checked as "the smallest rung that does not
  crop, and one rung narrower would have", and the half-aux saving as the exact
  byte count the class, the gap and `auxDiv` account for.
"""

s = s.replace(MARK, MARK + ENTRY, 1)
open(P, 'w', encoding='utf-8', newline='').write(s)
b = open(P, 'rb').read()
print('written; CR %d LF %d' % (b.count(b'\r'), b.count(b'\n')))
