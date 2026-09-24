#!/usr/bin/env python3
"""fix_wording.py -- two sentences in the changelog entry that were written
before the rule-C guard and before G4 measured which way the stride refusal
goes. Both are in WW_CHANGES.md and in its source copy under scratchpad.
"""

PAIRS = [
    ("""2. **A bridge in which exactly one side inherits the worldspace type is
   accepted only when the inheriting side is the SMALLER of the two.** Without
   that, the exact test lets the 21.5 M-texel sea absorb a painted marsh that
   passes within two texels of it, and the whole Commonwealth ocean comes out
   named `ExtMarshScumWater` (21,587,443 texels, 332 components, five forms).
   Rule C's merge already states the direction; this is the same direction
   across a gap.""",
     """2. **A merge in which exactly one side inherits the worldspace type is
   accepted only when the inheriting side is the SMALLER of the two** -- in
   rule C's ADJACENT merge and in rule D's bridge, both. Without it in the
   bridge, the exact test lets the 21.5 M-texel sea absorb a painted marsh that
   passes within two texels of it and the whole Commonwealth ocean comes out
   named `ExtMarshScumWater` (21,587,443 texels, 332 components, five forms);
   without it in rule C, the known-answer control's sea takes the form of the
   painted river reach it TOUCHES at its own height, which is how the second
   half of this was found. Rule C already states the direction ("an inheriting
   component is merged INTO the painted one"); the clause is what makes it
   safe."""),
    ("""file by the harness: a section bit over an empty rate or offset, a record stride
longer than this reader knows, a record whose `id` is not its index + 1, and a
plane naming a body past the table.""",
     """file by the harness: a section bit over an empty rate or offset, a record
stride SHORTER than the 48 this reader knows (a LONGER one is the
forward-compatible case and strides past the fields it does not know, which is
the rule the `.lodm` sidecars already use), a record whose `id` is not its
index + 1, and a plane naming a body past the table."""),
]

for path in ('WW_CHANGES.md', 'scratchpad/water2_20260909/WW_CHANGES_ENTRY.md'):
    raw = open(path, 'rb').read()
    cr0 = raw.count(b'\r')
    s = raw.decode('utf-8')
    for old, new in PAIRS:
        assert s.count(old) == 1, '%s: %d matches' % (path, s.count(old))
        s = s.replace(old, new, 1)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr0
    open(path, 'wb').write(out)
    print('  %-46s CR %d unchanged' % (path, cr0))
print('ok')
