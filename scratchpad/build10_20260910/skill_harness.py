#!/usr/bin/env python3
"""Lane BUILD10 -- one rule added to `ww-test-harness-add`, section 2.

`tests/spells/water_flow.sh` reports a GREEN gate as red because its loop does
`grep -F "<name>" | head -1` and the harness prints an informational line that
begins with the same words, immediately above the ok line.  Two lanes have now
paid for this shape.  The remedy is one clause and it belongs beside the spell
that the section already describes.
"""
import os

P = r"E:\Projects\Claude\.claude\skills\ww-test-harness-add\SKILL.md"
ANCHOR = "## 3. Read the WIDGETS, not the private members\n"

TEXT = """### 2b. The gate loop's own grep takes the FIRST line, and that is usually not the gate

A spell that reads its gates back by name (which it must -- a gate that did not
run has to be a failure, not a pass) is normally written:

```bash
line="$(grep -a -F "$g" "$LOG" | head -1)"
printf '%s' "$line" | grep -aq '^  ok ' || { echo "FAIL: gate '$g' is red"; ... }
```

and a harness that prints a NUMBER before its verdict --

```
F8 the solve: 0.319 s, 1493 iterations, residual 9.26e-10, stroke agreement 0.819
  ok   F8 the solve of body 3 runs under 1.0 s with a residual below 1e-8 (0.319 s, ...)
```

-- makes that loop take the informational line, find no leading `  ok `, and
report a green gate red. `tests/spells/water_flow.sh` has done it since it was
written (2026-09-10, found on its first run by lane BUILD10). **Filter to the
check lines BEFORE taking the first one:**

```bash
line="$(grep -a -F "$g" "$LOG" | grep -aE '^  (ok|FAIL) ' | head -1)"
```

`tests/spells/water_weights.sh` is the reference. And a floor of the form
"N gates green" must be arithmetic on the gates that are EXPECTED green: the
same file registered 19 gates, predicted two of them red, and then set the floor
at 18.

"""


def main():
    b = open(P, "rb").read()
    cr = b.count(b"\r")
    a = ANCHOR.encode("utf-8")
    n = b.count(a)
    print("anchor count=%d CR=%d bytes=%d" % (n, cr, len(b)))
    assert n == 1
    assert b.count(b"### 2b.") == 0, "already added"
    out = b.replace(a, TEXT.encode("utf-8") + a)
    assert out.count(b"\r") == cr
    open(P, "wb").write(out)
    print("wrote SKILL.md %d -> %d bytes, CR %d unchanged" % (len(b), len(out), cr))


if __name__ == "__main__":
    main()
