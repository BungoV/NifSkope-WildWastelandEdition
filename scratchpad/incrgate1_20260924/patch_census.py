import os
P = r'E:\Projects\NifskopeWWE-incrgate1\tests\spells\lodgen_census_check.py'
src = open(P, 'rb').read()
cr0 = src.count(b'\r')


def rep(s, a, b):
    assert s.count(a) == 1, (a, s.count(a))
    return s.replace(a, b)


src = rep(src,
          b"""            c['lodo.level%d.meanError' % lv] = float(m.group(4))
""",
          b"""            c['lodo.level%d.meanError' % lv] = float(m.group(4))
        # THE LADDER OFF (the default since the authored-only library): the line
        # still prints `(group 4, ...)`, the target a ladder WOULD be built at,
        # while the file writes ladderGroup 0 -- "0 iff the LADDER flag is clear"
        # (NATIVE sec 3, header 0xCD). So the claim about the FILE is 0, and the
        # printed target is a setting of a pass that did not run. Lane INCRGATE1,
        # 2026-09-24: the first v4 run of this checker (a default Sanctuary pair)
        # read `lodo.ladderGroup 4 / 0 RED` on a correct file.
        if c.get('ladder.on') == 0 and 'lodo.ladderGroup' in c:
            c['bake.ladderGroupTarget'] = c['lodo.ladderGroup']
            c['lodo.ladderGroup'] = 0
""")
src = rep(src,
          b"""NOT_DERIVABLE = {
""",
          b"""NOT_DERIVABLE = {
    'bake.ladderGroupTarget': 'the grouping target of a ladder that was not built (the file carries 0)',
""")
assert src.count(b'\r') == cr0
open(P + '.tmp', 'wb').write(src)
os.replace(P + '.tmp', P)
print('patched, CR', cr0)
