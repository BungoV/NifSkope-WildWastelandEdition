#!/usr/bin/env python
"""BUILD1: the two lane reports say the skill was RECOMMENDED. It was written.
Correct both, so the director does not place it twice."""
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                     '..', '..'))
OLD = (b'**Recommended as a section of `nifskope-ww-build-verify` rather than a skill of\n'
       b'its own**, since\n'
       b'it belongs to the same chain, and the director should place it in BOTH skill\n'
       b'trees (CONSTITUTION 1a, the two-tree drift). I have not written it: this lane\n'
       b'owns no file under `.claude/skills`.\n')
NEW = (b'**WRITTEN**, as a section of `nifskope-ww-build-verify` rather than a skill of\n'
       b'its own, since it belongs to the same chain: "A successful build is not a\n'
       b'consistent one (2026-09-09)", in the LIVE tree\n'
       b'`E:\\Projects\\Claude\\.claude\\skills\\nifskope-ww-build-verify\\SKILL.md`, with the\n'
       b'grep-and-mtime check, the delete-the-object-and-relink fix, the qmake caveat\n'
       b'and the symptom to expect. The repo tree `<repo>/.claude/skills` holds only\n'
       b'`ww-control-calibration`, so there is no second copy to keep in step\n'
       b'(checked, CONSTITUTION 1a).\n')

for path in ('scratchpad/lane_noprompt_report.md',
             'scratchpad/lane_terrain_fix_report.md'):
    p = os.path.join(ROOT, path)
    b = open(p, 'rb').read()
    assert b.count(b'\r') == 0
    assert b.count(OLD) == 1, (path, b.count(OLD))
    b = b.replace(OLD, NEW)
    assert b.count(b'\r') == 0
    open(p, 'wb').write(b)
    print('corrected %s' % path)
