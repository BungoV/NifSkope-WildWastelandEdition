#!/usr/bin/env python3
"""The second occurrence of the same mistake, in the same lane. The constitution
says repeating an entry that is already in the file is its own entry."""
import sys

P = 'MISTAKES.md'
s = open(P, encoding='utf-8').read()
MARK = 'Newest at the top.\n'
if s.count(MARK) != 1:
    print('header marker matched %d times' % s.count(MARK))
    sys.exit(1)

ENTRY = """
## 2026-09-09 -- lane CARDFINAL made the unmeasured-threshold mistake TWICE in one lane

- **What was done:** an hour after writing the entry below about picking a round
  fraction for a gate, the same lane extended
  `scratchpad/cardfinal_20260909/transition_bounds.py` to per-frame positioning
  by REUSING lane CARDFIT3's `RATIO = 4.0`. That 4x was measured for
  `card.center`, a single point, against the zeroed-offset control. It was
  applied to `d + max|offset|`, a PESSIMISTIC scalar bound that assumes every
  frame's offset points straight away from the model's bound centre.
- **What was true instead:** two of three bases came out at 2.6x and 3.0x and
  "failed" a threshold nothing had measured, on numbers that are correct.
- **How it was found:** the gate printed FAIL beside a ratio it had computed
  itself.
- **The rule, restated because restating it did not prevent it:** a threshold
  carried from one quantity to a DIFFERENT quantity is an unmeasured threshold
  even when the number itself was once measured. The per-frame row now says only
  what the instrument can support -- the most displaced frame's quad lies inside
  the model's own bound sphere, with the control that the same quad anchored at
  the pivot does not, and the ratio to the pivot reported rather than
  thresholded -- and the 4x stays where CARDFIT3 measured it.
- **What would have caught it earlier:** writing the control BEFORE the check.
  Both times, the check was written first and its control was reached for
  afterwards; both times the control is what showed the check was the wrong
  shape.
"""

s = s.replace(MARK, MARK + ENTRY, 1)
open(P, 'w', encoding='utf-8', newline='').write(s)
b = open(P, 'rb').read()
print('written; CR %d LF %d' % (b.count(b'\r'), b.count(b'\n')))
