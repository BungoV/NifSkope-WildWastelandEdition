"""Lane CELLVIEW4B, fix 04 -- repair of this lane's OWN bug in fix03.

fix03 wrote a `sed` whose pattern has one capture group and whose replacement
asks for `\\2`.  GNU sed said so out loud --

    sed: -e expression #1, char 58: invalid reference \\2 on `s' command's RHS

-- and the variable came back empty, which sent the arm detection down the
`mosaic` branch on a BLEND run and printed `0 textured quads of 0` under the
label `[mosaic]`.  No row read the bad variable, so the gate still said PASS:
that is exactly the failure this project's own rule is about -- a census field
that is READ but never written, reported without anyone checking it moved.

The two wordings differ here too, so this takes two attempts rather than one
clever pattern:

    blend   ... (1 with VCLR), 1024 land quads drawn as 2234 passes ...
    mosaic  ... (1 with VCLR), 1024 quads over 6 landscape textures ...
"""
import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
REL = 'tests/spells/cell_pick.sh'
CHECK = '--check' in sys.argv

OLD = '''quadstotal=$(sed -n 's/.*(\\?[0-9,]* with VCLR)\\?, \\([0-9,]*\\) land quads.*/\\2/p' "$g" | head -1 | tr -dc '0-9')'''

NEW = '''quadstotal=$(sed -n 's/.*, \\([0-9,]*\\) land quads.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
if [ -z "${quadstotal:-}" ]; then
	quadstotal=$(sed -n 's/.*, \\([0-9,]*\\) quads over.*/\\1/p' "$g" | head -1 | tr -dc '0-9')
fi'''

ANCHOR2 = '''check "the passes-per-quad multiplier is stated, so the cost is measured not guessed" \\
	"$([ -n "${mult:-}" ] && echo 1 || echo 0)"'''

NEW2 = '''check "the passes-per-quad multiplier is stated, so the cost is measured not guessed" \\
	"$([ -n "${mult:-}" ] && echo 1 || echo 0)"
# A FLOOR UNDER THE SCRAPE ITSELF (lane CELLVIEW4B). Every row above reads a
# number out of one line of prose. When a rewording makes a pattern miss, the
# variable defaults and the row can still pass for the wrong reason -- which is
# how this lane shipped a broken `sed` inside a green run. This row fails when
# the scrape came back empty, so a future rewording is a RED row and not a
# silently wrong number.
check "the census scrape actually captured its fields (no silently-empty variable)" \\
	"$([ -n "${tex:-}" ] && [ -n "${quadstotal:-}" ] && [ -n "${bare:-}" ] \\
	   && [ -n "${layered:-}" ] && [ "${quadstotal:-0}" -eq 1024 ] && echo 1 || echo 0)"'''


def edit(anchor, new):
    path = os.path.join(ROOT, REL)
    with io.open(path, 'rb') as fh:
        raw = fh.read()
    cr_before = raw.count(b'\r')
    txt = raw.decode('utf-8')
    n = txt.count(anchor)
    print('%-22s %d  %r' % (REL, n, anchor[:56]))
    assert n == 1, 'anchor matched %d times, not once' % n
    out = txt.replace(anchor, new)
    if CHECK:
        return
    data = out.encode('utf-8')
    assert data.count(b'\r') == cr_before, 'CR count moved'
    with io.open(path, 'wb') as fh:
        fh.write(data)


edit(OLD, NEW)
edit(ANCHOR2, NEW2)
print()
print('CHECK ONLY, nothing written' if CHECK else 'APPLIED')
