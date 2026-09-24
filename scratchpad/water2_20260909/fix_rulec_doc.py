#!/usr/bin/env python3
"""fix_rulec_doc.py -- rule C's stated merge in the contract needs the same
direction clause the bridge's does. The known-answer control found it there,
and the doc was written before that run.
"""
P = 'docs/LODGEN_BTD_FORMAT.md'
raw = open(P, 'rb').read()
assert raw.count(b'\r') == 0
s = raw.decode('utf-8')

OLD = """merge       a component whose type is the worldspace default (0xFFFF) joins the
            same-height PAINTED component it TOUCHES; with more than one
            candidate, the largest, and the body is flagged AMBIGUOUS."""
NEW = """merge       a component whose type is the worldspace default (0xFFFF) joins the
            same-height PAINTED component it TOUCHES, and only when it is the
            SMALLER of the two; with more than one candidate, the largest, and
            the body is flagged AMBIGUOUS. REFUSED when the inheriting side is
            the larger -- an ocean does not become an unpainted reach of the
            river it happens to touch."""
assert s.count(OLD) == 1
s = s.replace(OLD, NEW, 1)

OLD2 = """**Why the bridge is asymmetric, measured.** Rule C already states the direction —
an INHERITING component joins the painted one — because an unpainted reach of a
river is the river. Applied to a GAP without that direction, and with an exact
shore test, the Commonwealth's ocean absorbs a painted marsh that passes within
two texels of it and the whole 21,587,443-texel body comes out named
`ExtMarshScumWater`. With the clause, the sea keeps its own form, every large
painted body survives intact, and there is **not one** body where "the painted
majority" and "the majority counting inherited area" disagree — the two readings
of the form rule coincide, which they do not without it. Scripts:
`scratchpad/water2_20260909/bridge_exact.py`, `bridge_effect.py`,
`bridge_variants.py`."""
NEW2 = """**Why BOTH merges are asymmetric, measured.** Rule C states the direction — an
INHERITING component joins the painted one — because an unpainted reach of a
river is the river. Neither merge said what happens when the inheriting side is
the OCEAN.

* In the BRIDGE, with an exact shore test, the Commonwealth's ocean absorbs a
  painted marsh that passes within two texels of it and the whole
  21,587,443-texel body comes out named `ExtMarshScumWater`.
* In rule C's ADJACENT merge the same thing happens wherever the two actually
  TOUCH. The Commonwealth hides it (its ocean never touches a painted body at
  its own height, it only ever passes near one); the known-answer control does
  not, and that is where it was found.

With the clause in both, the sea keeps its own form, every large painted body
survives intact, and there is **not one** body where "the painted majority" and
"the majority counting inherited area" disagree — the two readings of the form
rule coincide, which they do not without it. Three guards were measured, in both
merges, end to end; the size clause is the only one that leaves zero such bodies.
Scripts: `scratchpad/water2_20260909/bridge_exact.py`, `bridge_effect.py`,
`bridge_variants.py`, `merge_guard_variants.py`."""
assert s.count(OLD2) == 1
s = s.replace(OLD2, NEW2, 1)

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('ok, %d bytes' % len(out))
