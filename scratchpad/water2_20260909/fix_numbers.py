#!/usr/bin/env python3
"""fix_numbers.py -- the census figures quoted before the rule-C guard landed.

The bridge guard was measured and written up first; the known-answer control
then found that rule C's ADJACENT merge needed the same clause, and the whole
census moved: 792 -> 793 rule-C bodies and 347 -> 346 final, 526 -> 528 merges
accepted, 14 -> 13 refused, and the class split with it. Four documents were
already carrying the earlier figures.

This is the "before putting two artefacts in one sentence, put their mtimes in
one table" rule in its documentation form: a number written down before the
last change is a stale number, whoever wrote it.
"""

OLD_DOC = """Measured on the Commonwealth: 805 components → 792 after rule C's merge (2 of
those had more than one candidate) → **347 bodies**, 526 bridge merges accepted
and **14 refused**; 1 sea, 114 rivers, 232 lakes; 117 bodies under 4 texels,"""
NEW_DOC = """Measured on the Commonwealth: 805 components → 793 after rule C's merge (12
merged, 1 refused because the inheriting side was not the smaller, 1 with more
than one candidate) → **346 bodies**, 528 bridge merges accepted and **13
refused**; 1 sea, 115 rivers, 230 lakes; 115 bodies under 4 texels,"""

OLD_CH = """Commonwealth, measured: 805 components -> 792 after rule C's merge (2 with more
than one candidate) -> **347 bodies**, 526 bridge merges accepted, 14 refused.
1 sea, 114 rivers, 232 lakes; 88 bodies at 64 texels or more; 117 under 4
texels, flagged TINY. Flow sources: none 134, form NAM0 181, bed 2, drain 30."""
NEW_CH = """Commonwealth, measured: 805 components -> 793 after rule C's merge (12 merged,
1 refused because the inheriting side was not the smaller, 1 with more than one
candidate) -> **346 bodies**, 528 bridge merges accepted, 13 refused.
1 sea, 115 rivers, 230 lakes; 89 bodies at 64 texels or more; 115 under 4
texels, flagged TINY. Flow sources: none 132, form NAM0 182, bed 2, drain 30."""

OLD_CPP = """ *  Measured on the Commonwealth: 347 bodies, 526 bridge merges accepted, 14
 *  refused, and the per-form texel totals identical to WATER1's to the texel."""
NEW_CPP = """ *  The known-answer control below (`lodl --water-selftest`) then found the same
 *  missing direction in rule C's ADJACENT merge -- the synthetic sea TOUCHES a
 *  painted river reach at its own height -- so the clause is in both merges.
 *
 *  Measured on the Commonwealth: 805 components -> 793 rule-C bodies -> 346
 *  bodies, 528 bridge merges accepted, 13 refused, and thirteen of the fifteen
 *  per-form texel totals identical to the read-only census's to the texel."""

OLD_RPT = """This lane produces **346 bodies, 528 accepted, 13 refused, 1 sea / 115 river / 230 lake**."""


def fix(path, pairs, cr_expect=None):
    raw = open(path, 'rb').read()
    cr0 = raw.count(b'\r')
    s = raw.decode('utf-8')
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, '%s: %d matches for %r' % (path, n, old[:60])
        s = s.replace(old, new, 1)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr0, '%s CR %d -> %d' % (path, cr0, out.count(b'\r'))
    open(path, 'wb').write(out)
    print('  %-32s CR %d unchanged' % (path, cr0))


fix('docs/LODGEN_BTD_FORMAT.md', [(OLD_DOC, NEW_DOC)])
fix('WW_CHANGES.md', [(OLD_CH, NEW_CH)])
fix('scratchpad/water2_20260909/WW_CHANGES_ENTRY.md', [(OLD_CH, NEW_CH)])
fix('src/lodtfile.cpp', [(OLD_CPP, NEW_CPP)])
fix('scratchpad/lane_water2_report.md',
    [('and the corrected rule gives **347 bodies**.',
      'and the corrected rule gives **346 bodies**.')])
print('ok')
